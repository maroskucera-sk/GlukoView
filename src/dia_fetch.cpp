#include "dia_fetch.h"
#include "dia_carelink.h"
#include "dia_config.h"
#include "dia_diag.h"
#include "dia_glucose.h"
#include "dia_tls.h"
#include "dia_ui.h"
#include "dia_types.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include <ctype.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>
#include <time.h>
#include <string.h>

// Dual Libre+NS: mbedTLS SHA buffer ide z interného DRAM – po veľkom JSON často chýba.
static void diaFetchLogHeap(const char *tag) {
    const unsigned freeH = (unsigned)ESP.getFreeHeap();
    const unsigned intH = (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const unsigned psH = (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    Serial.printf("[heap] %s free=%u int=%u psram=%u\n", tag, freeH, intH, psH);
    esp_rom_printf("[heap] %s free=%u int=%u psram=%u\n", tag, freeH, intH, psH);
}

static void diaFetchBreathe(unsigned ms) {
    delay(ms);
    yield();
}

// Watchdog zvýši epoch → task nedokončí zvyšné sloty (žiadne prekryté TLS)
static volatile uint32_t s_fetchEpoch = 0;
static uint32_t s_fetchRunEpoch = 0;
/** Druhý slot ešte nemá číslo. Graf Libre počká, aby nevisel pred ním. */
static bool s_holdLibreGraph = false;

static bool diaFetchAborted() {
    return s_fetchRunEpoch != s_fetchEpoch;
}

/** Po nsFetchPump – diaFetchPoll obnoví face (IOB) bez čakania na ďalší cyklus. */
static volatile int s_nsPumpUiSlot = -1;

static void sha256_hex(const String &input, char *out_hex /* min 65 */) {
    unsigned char hash[32];
    mbedtls_sha256((const unsigned char *)input.c_str(), input.length(), hash, 0);
    for (int i = 0; i < 32; i++) {
        sprintf(out_hex + (i * 2), "%02x", hash[i]);
    }
    out_hex[64] = '\0';
}

static String getLibreBaseUrl(String region) {
    region.toLowerCase();
    region.trim();
    if (region.length() == 0 || region == "global" || region == "us") {
        return "https://api.libreview.io";
    }
    return "https://api-" + region + ".libreview.io";
}

// Civil dátum (UTC) → dni od 1970-01-01 (Howard Hinnant)
static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + yoe / 400 + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

/** UTC civil → unix timestamp bez zásahu do systémovej TZ. */
static time_t timegmUtc(int year, int month, int day, int hour, int minute, int sec) {
    if (month < 1 || month > 12 || day < 1 || day > 31) return 0;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || sec < 0 || sec > 60) return 0;
    const int64_t days = daysFromCivil(year, (unsigned)month, (unsigned)day);
    const int64_t secs = days * 86400LL + hour * 3600LL + minute * 60LL + sec;
    return (time_t)secs;
}

// FactoryTimestamp Libre je UTC: "5/21/2022 1:38:50 PM"
static long parseLibreFactoryTimestamp(const char *s) {
    if (!s || !*s) return 0;
    int month = 0, day = 0, year = 0, hour = 0, minute = 0, sec = 0;
    char ampm[4] = {0};
    int n = sscanf(s, "%d/%d/%d %d:%d:%d %3s", &month, &day, &year, &hour, &minute, &sec, ampm);
    if (n < 6) return 0;

    if (ampm[0] == 'P' || ampm[0] == 'p') {
        if (hour != 12) hour += 12;
    } else if (ampm[0] == 'A' || ampm[0] == 'a') {
        if (hour == 12) hour = 0;
    }

    return (long)timegmUtc(year, month, day, hour, minute, sec);
}

static void addLibreHeaders(HTTPClient &http, bool withAuth) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("product", "llu.android");
    http.addHeader("version", "4.16.0");
    http.addHeader("cache-control", "no-cache");
    http.addHeader("pragma", "no-cache");
    if (withAuth && libre_token.length() > 0) {
        http.addHeader("Authorization", "Bearer " + libre_token);
        if (libre_account_hash.length() > 0) {
            http.addHeader("Account-Id", libre_account_hash);
        }
    }
}

/** Libre nepodpisuje Let's Encrypt. Keď overenie spadne, druhý pokus bez neho, ako Nightscout. */
static int libreHttp(const String &url, bool post, const String *body, bool withAuth, String &payload) {
    payload = "";
    int httpCode = -1;
    for (int pass = 0; pass < 2; pass++) {
        const bool insecure = (pass == 0) ? tls_insecure : true;
        if (pass > 0) {
            if (httpCode > 0 || tls_insecure) break;
            Serial.println("Libre TLS zlyhal – retry setInsecure");
            diaFetchBreathe(300);
        }
        WiFiClientSecure client;
        configureTlsClient(client, insecure);
        HTTPClient http;
        http.setTimeout(post ? 15000 : 10000);
        if (!http.begin(client, url)) {
            httpCode = -1;
            continue;
        }
        addLibreHeaders(http, withAuth);
        httpCode = post ? http.POST(body ? *body : emptyString) : http.GET();
        if (httpCode > 0) payload = http.getString();
        http.end();
        if (httpCode > 0) break;
    }
    return httpCode;
}

static bool libreLogin() {
    diaDiagStep(dia_active_slot, DiaDiagStep::Login);
    if (libreUser.length() == 0 || librePwd.length() == 0) return false;

    for (int attempt = 0; attempt < 2; attempt++) {
        String base = getLibreBaseUrl(libreRegion);
        String url = base + "/llu/auth/login";

        JsonDocument body;
        body["email"] = libreUser;
        body["password"] = librePwd;
        String bodyStr;
        serializeJson(body, bodyStr);

        String payload;
        int httpCode = libreHttp(url, true, &bodyStr, false, payload);

        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("Libre login HTTP %d\n", httpCode);
            return false;
        }

        JsonDocument doc;
        if (deserializeJson(doc, payload)) return false;

        JsonObject data = doc["data"].as<JsonObject>();
        if (data.isNull()) return false;

        if (data["redirect"] == true) {
            String newRegion = data["region"] | "";
            if (newRegion.length() > 0) {
                libreRegion = newRegion;
                preferences.putString("llu_reg", libreRegion);
                Serial.printf("Libre redirect -> region %s\n", libreRegion.c_str());
                continue;
            }
            return false;
        }

        const char *stepType = data["step"]["type"] | "";
        if (strlen(stepType) > 0) {
            Serial.printf("Libre login requires step: %s\n", stepType);
            return false;
        }

        const char *token = data["authTicket"]["token"] | "";
        const char *userId = data["user"]["id"] | "";
        if (strlen(token) == 0 || strlen(userId) == 0) return false;

        libre_token = String(token);
        char hash[65];
        sha256_hex(String(userId), hash);
        libre_account_hash = String(hash);
        libre_patient_id = "";
        return true;
    }
    return false;
}

static String libreNorm(String s) {
    s.trim();
    s.toLowerCase();
    // zjednoduš porovnanie mien (medzery)
    while (s.indexOf("  ") >= 0) s.replace("  ", " ");
    return s;
}

static void libreRememberPatients(JsonArray arr) {
    String list;
    size_t n = arr.size();
    if (n > 8) n = 8;
    for (size_t i = 0; i < n; i++) {
        String id = arr[i]["patientId"] | "";
        String fn = arr[i]["firstName"] | "";
        String ln = arr[i]["lastName"] | "";
        id.trim();
        fn.trim();
        ln.trim();
        if (id.length() == 0) continue;
        String name = fn;
        if (ln.length()) {
            if (name.length()) name += " ";
            name += ln;
        }
        if (name.length() == 0) name = id;
        name.replace("\t", " ");
        name.replace("\n", " ");
        name.replace("\r", " ");
        if (list.length()) list += "\n";
        list += id;
        list += "\t";
        list += name;
    }
    if (list.length() == 0) return;
    libre_patient_list = list;
    if (dia_active_slot >= 0 && dia_active_slot < DIA_PROFILE_COUNT) {
        profiles[dia_active_slot].libre_patient_list = list;
        char key[12];
        snprintf(key, sizeof(key), "p%d_lpl", dia_active_slot);
        String prev = preferences.isKey(key) ? preferences.getString(key, "") : "";
        if (prev != list) preferences.putString(key, list);
    }
    if (dia_active_slot == 0) {
        String prev = preferences.isKey("llu_pats") ? preferences.getString("llu_pats", "") : "";
        if (prev != list) preferences.putString("llu_pats", list);
    }
}

/** Vyber index connection podľa preferencie (patientId, meno alebo číslo 1, 2…). -1 = nič. */
static int libreFindConnectionIndex(JsonArray arr, const String &prefIn) {
    const size_t n = arr.size();
    if (n == 0) return -1;

    for (size_t i = 0; i < n; i++) {
        const char *pid = arr[i]["patientId"] | "";
        const char *fn = arr[i]["firstName"] | "";
        const char *ln = arr[i]["lastName"] | "";
        Serial.printf("Libre conn[%u]: %s %s id=%s\n", (unsigned)i, fn, ln, pid);
    }

    libreRememberPatients(arr);

    if (n == 1) return 0;

    String pref = libreNorm(prefIn);
    if (pref.length() == 0) {
        Serial.println("Libre: viac pacientov, preferencia prazdna -> beriem [0]");
        return 0;
    }

    bool digits = true;
    for (unsigned i = 0; i < pref.length(); i++) {
        if (pref[i] < '0' || pref[i] > '9') digits = false;
    }
    if (digits && pref.length() <= 2) {
        int pick = pref.toInt();
        if (pick >= 1 && pick <= (int)n) return pick - 1;
    }

    // 1) Presné patientId
    for (size_t i = 0; i < n; i++) {
        String pid = arr[i]["patientId"] | "";
        if (libreNorm(pid) == pref) return (int)i;
    }

    // 2) Meno: "Jan", "Novak", "Jan Novak"
    for (size_t i = 0; i < n; i++) {
        String fn = libreNorm(arr[i]["firstName"] | "");
        String ln = libreNorm(arr[i]["lastName"] | "");
        String full = fn;
        if (fn.length() && ln.length()) full += " ";
        full += ln;

        if (pref == fn || pref == ln || pref == full) return (int)i;
        if (fn.length() > 0 && pref.indexOf(fn) >= 0 && ln.length() > 0 && pref.indexOf(ln) >= 0) {
            return (int)i;
        }
        if (full.length() > 0 && (full.indexOf(pref) >= 0 || pref.indexOf(full) >= 0)) {
            return (int)i;
        }
    }

    Serial.printf("Libre: preferencia '%s' nenajdena -> beriem [0]\n", prefIn.c_str());
    return 0;
}

// 1 = OK, 0 = auth (treba re-login), -1 = sieť/DNS (token nechať)
static void libreAppendGraph(bool *hasSrcDelta, float *srcDeltaMmol) {
    if (libre_patient_id.length() == 0) return;
    const uint32_t heapNow = ESP.getFreeHeap();
    if (heapNow < 70000 || diaFetchAborted()) {
        Serial.printf("Libre graph: skip (heap=%u)\n", (unsigned)heapNow);
        return;
    }
    diaDiagStep(dia_active_slot, DiaDiagStep::Graph);
    String gurl = getLibreBaseUrl(libreRegion) + "/llu/connections/" + libre_patient_id + "/graph";
    int gcode = -1;
    String gpayload;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (diaFetchAborted()) break;
        if (attempt > 0) diaFetchBreathe(400);
        else diaFetchBreathe(200);
        gcode = libreHttp(gurl, false, nullptr, true, gpayload);
        if (gcode == HTTP_CODE_OK) break;
    }
    if (gcode != HTTP_CODE_OK) {
        Serial.printf("Libre graph HTTP %d (hodnota z connections ostava)\n", gcode);
        return;
    }
    JsonDocument filter;
    filter["data"]["graphData"][0]["ValueInMgPerDl"] = true;
    filter["data"]["graphData"][0]["Value"] = true;
    JsonDocument gdoc;
    if (deserializeJson(gdoc, gpayload, DeserializationOption::Filter(filter))) return;
    gpayload = "";
    JsonArray gd = gdoc["data"]["graphData"].as<JsonArray>();
    if (gd.isNull() || gd.size() < 1) return;
    size_t n = gd.size();
    if (hasSrcDelta && srcDeltaMmol && !*hasSrcDelta && n >= 2) {
        float newest = gd[n - 1]["ValueInMgPerDl"] | gd[n - 1]["Value"] | 0.0f;
        float prev = gd[n - 2]["ValueInMgPerDl"] | gd[n - 2]["Value"] | 0.0f;
        if (newest > 0.0f && prev > 0.0f) {
            *srcDeltaMmol = mgdl_to_mmol(newest) - mgdl_to_mmol(prev);
            *hasSrcDelta = true;
        }
    }
    size_t start = (n > DIA_HISTORY_MAX) ? (n - DIA_HISTORY_MAX) : 0;
    float histAsc[DIA_HISTORY_MAX];
    int histN = 0;
    for (size_t i = start; i < n && histN < DIA_HISTORY_MAX; i++) {
        float v = gd[i]["ValueInMgPerDl"] | gd[i]["Value"] | 0.0f;
        if (v > 0.0f) histAsc[histN++] = mgdl_to_mmol(v);
    }
    if (histN > 0) {
        float histNf[DIA_HISTORY_MAX];
        for (int i = 0; i < histN; i++) histNf[i] = histAsc[histN - 1 - i];
        glucoseHistorySetNewestFirst(histNf, histN);
    }
}

static int libreFetchConnections() {
    diaDiagStep(dia_active_slot, DiaDiagStep::Connections);
    float mgdl = 0.0f;
    int trendArrow = 0;
    long ts = 0;
    bool hasDeltaField = false;
    float deltaFieldMgdl = 0.0f;
    size_t connCount = 0;

    {
        String base = getLibreBaseUrl(libreRegion);
        String url = base + "/llu/connections";
        diaFetchLogHeap("libre-pre");
        int httpCode = -1;
        String payload;
        for (int attempt = 0; attempt < 2; attempt++) {
            if (diaFetchAborted()) return -1;
            if (attempt > 0) diaFetchBreathe(400);
            if (ESP.getFreeHeap() < 70000) {
                Serial.printf("Libre connections: low heap %u – cakam\n",
                              (unsigned)ESP.getFreeHeap());
                diaFetchBreathe(600);
            }
            httpCode = libreHttp(url, false, nullptr, true, payload);
            if (httpCode < 0) Serial.println("Libre connections: begin fail");
            if (httpCode == HTTP_CODE_OK) break;
            if (httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == 403) break;
            Serial.printf("Libre connections HTTP %d (pokus %d/2)\n", httpCode, attempt + 1);
        }

        if (httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == 403) {
            Serial.println("Libre connections: auth fail");
            return 0;
        }
        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("Libre connections: network fail HTTP %d\n", httpCode);
            return -1;
        }

        JsonDocument doc;
        if (deserializeJson(doc, payload)) {
            Serial.println("Libre connections: JSON fail");
            return -1;
        }

        JsonArray arr = doc["data"].as<JsonArray>();
        if (arr.isNull() || arr.size() == 0) {
            Serial.println("Libre: ziadne connections (follower?)");
            return 0;
        }

        connCount = arr.size();
        int idx = libreFindConnectionIndex(arr, libre_patient_pref);
        if (idx < 0) return 0;

        JsonObject conn = arr[idx].as<JsonObject>();
        libre_patient_id = conn["patientId"] | "";

        const char *fn = conn["firstName"] | "";
        const char *ln = conn["lastName"] | "";
        Serial.printf("Libre vybrany pacient [%d]: %s %s (%s)\n",
                      idx, fn, ln, libre_patient_id.c_str());

        JsonObject gm = conn["glucoseMeasurement"].as<JsonObject>();
        if (gm.isNull()) {
            Serial.println("Libre: glucoseMeasurement prazdne");
            return -2;
        }

        mgdl = gm["ValueInMgPerDl"] | 0.0f;
        if (mgdl <= 0.0f) {
            Serial.println("Libre: ValueInMgPerDl=0");
            return -2;
        }

        trendArrow = gm["TrendArrow"] | 0;
        const char *factoryTs = gm["FactoryTimestamp"] | "";
        ts = parseLibreFactoryTimestamp(factoryTs);

        if (!gm["Delta"].isNull()) {
            hasDeltaField = true;
            deltaFieldMgdl = gm["Delta"].as<float>();
        }
    } // uvoľni TLS client + JSON pred graph requestom

    if (libre_patient_pref.length() == 0 && connCount == 1 && libre_patient_id.length() > 0) {
        libre_patient_pref = libre_patient_id;
        preferences.putString("libre_pref", libre_patient_pref);
    }

    bool hasSrcDelta = false;
    float srcDeltaMmol = 0.0f;

    if (hasDeltaField) {
        srcDeltaMmol = mgdl_to_mmol(deltaFieldMgdl);
        hasSrcDelta = true;
    }

    // Graf je len krivka. Keď ešte čaká druhý človek, číslo už máme a graf ide po ňom.
    if (s_holdLibreGraph) {
        Serial.println("Libre graph: odklad, ide dalsi slot");
    } else {
        libreAppendGraph(&hasSrcDelta, &srcDeltaMmol);
    }

    applyGlucoseReading(mgdl_to_mmol(mgdl), getLibreTrendSymbol(trendArrow), ts,
                        hasSrcDelta, srcDeltaMmol);
    return 1;
}

void fetchLibre() {
    if (libreUser.length() == 0 || librePwd.length() == 0) {
        markFetchError(FetchError::Config);
        return;
    }

    if (libre_token.length() == 0 || libre_account_hash.length() == 0) {
        if (!libreLogin()) {
            markFetchError(FetchError::Auth);
            return;
        }
    }

    int r = libreFetchConnections();
    if (r == 1) return;
    if (r == -2) {
        markFetchError(FetchError::Empty);
        return;
    }
    if (r < 0) {
        // DNS/TLS blip – NEmazať token (to spôsobovalo falošný „Libre down“ v duali)
        markFetchError(FetchError::Network);
        return;
    }

    libre_token = "";
    libre_account_hash = "";
    if (!libreLogin()) {
        markFetchError(FetchError::Auth);
        return;
    }
    r = libreFetchConnections();
    if (r == 1) return;
    if (r == -2) {
        markFetchError(FetchError::Empty);
        return;
    }
    if (r < 0) {
        markFetchError(FetchError::Network);
        return;
    }

    markFetchError(FetchError::Empty);
}

static String stripJsonQuotes(String s) {
    s.trim();
    if (s.startsWith("\"")) s = s.substring(1);
    if (s.endsWith("\"")) s = s.substring(0, s.length() - 1);
    return s;
}

static bool isInvalidDexcomUuid(const String &id) {
    if (id.length() == 0) return true;
    return id == "00000000-0000-0000-0000-000000000000";
}

static String getDexcomBaseUrl(String region) {
    region.toLowerCase();
    region.trim();
    if (region == "jp") return "https://share.dexcom.jp/ShareWebServices/Services/";
    if (region == "us") return "https://share2.dexcom.com/ShareWebServices/Services/";
    return "https://shareous1.dexcom.com/ShareWebServices/Services/";
}

static String getDexcomAppId(String region) {
    region.toLowerCase();
    region.trim();
    if (region == "jp") return "d8665ade-9673-4e27-9ff6-92db4ce13d13";
    return "d89443d2-327c-4a6f-89e5-496bbb0317db";
}

static long parseDexcomWT(const char *wt) {
    if (!wt || !*wt) return 0;
    const char *p = strstr(wt, "Date(");
    if (!p) return 0;
    p += 5;
    long long ms = atoll(p);
    if (ms <= 0) return 0;
    return (long)(ms / 1000);
}

static int dexcomPost(const String &endpoint, const String &body, String &outPayload) {
    String url = getDexcomBaseUrl(dexRegion) + endpoint;
    outPayload = "";
    int httpCode = -1;
    // Dexcom podpisuje Google Trust Services, nie Let's Encrypt. Keď overenie spadne, druhý pokus bez neho.
    for (int pass = 0; pass < 2; pass++) {
        const bool insecure = (pass == 0) ? tls_insecure : true;
        if (pass > 0) {
            if (httpCode > 0 || tls_insecure) break;
            Serial.println("Dexcom TLS zlyhal – retry setInsecure");
            diaFetchBreathe(300);
        }
        WiFiClientSecure client;
        configureTlsClient(client, insecure);
        HTTPClient http;
        http.setTimeout(15000);
        if (!http.begin(client, url)) {
            httpCode = -1;
            continue;
        }
        http.addHeader("Accept", "application/json");
        http.addHeader("User-Agent", "Dexcom Share/3.0.2.11 CFNetwork/711.2.23 Darwin/14.0.0");
        if (body.length() == 0) {
            http.addHeader("Content-Length", "0");
            httpCode = http.POST((uint8_t *)"", 0);
        } else {
            http.addHeader("Content-Type", "application/json");
            httpCode = http.POST(body);
        }
        if (httpCode > 0) outPayload = http.getString();
        http.end();
        if (httpCode > 0) break;
    }
    return httpCode;
}

// Max 2 úplné auth pokusy za beh NA SLOT (dual: A nesmie zablokovať B).
static uint8_t s_dexAuthFails[DIA_PROFILE_COUNT] = {0, 0};
static constexpr uint8_t kDexAuthMaxFails = 2;

/** 1 = session, 0 = účet odmietnutý, -1 = sieť / TLS (nemaže pokusy). */
static int dexcomLogin() {
    if (dexUser.length() == 0 || dexPwd.length() == 0) return 0;
    int si = dia_active_slot;
    if (si < 0 || si >= DIA_PROFILE_COUNT) si = 0;
    if (s_dexAuthFails[si] >= kDexAuthMaxFails) {
        Serial.printf("Dexcom Auth: slot %d stop after %u fails (lockout guard)\n",
                      si, (unsigned)kDexAuthMaxFails);
        return 0;
    }

    // Len region + read-only Publisher API – žiadny zápis / follower invite
    Serial.printf("Dexcom Auth: slot=%d region=%s user_len=%u tls_insecure=%d\n",
                  si, dexRegion.c_str(), (unsigned)dexUser.length(), (int)tls_insecure);

    String appId = getDexcomAppId(dexRegion);
    JsonDocument authBody;
    authBody["accountName"] = dexUser;
    authBody["password"] = dexPwd;
    authBody["applicationId"] = appId;
    String authStr;
    serializeJson(authBody, authStr);

    String accountPayload;
    int code = dexcomPost("General/AuthenticatePublisherAccount", authStr, accountPayload);
    if (code <= 0) {
        Serial.printf("Dexcom Authenticate HTTP %d\n", code);
        return -1;
    }
    if (code != HTTP_CODE_OK) {
        s_dexAuthFails[si]++;
        Serial.printf("Dexcom Authenticate HTTP %d slot %d (attempt %u/%u)\n",
                      code, si, (unsigned)s_dexAuthFails[si], (unsigned)kDexAuthMaxFails);
        return 0;
    }

    dexcom_account_id = stripJsonQuotes(accountPayload);
    if (isInvalidDexcomUuid(dexcom_account_id)) {
        s_dexAuthFails[si]++;
        Serial.printf("Dexcom: neplatne account ID slot %d (attempt %u/%u)\n",
                      si, (unsigned)s_dexAuthFails[si], (unsigned)kDexAuthMaxFails);
        dexcom_account_id = "";
        return 0;
    }

    JsonDocument loginBody;
    loginBody["accountId"] = dexcom_account_id;
    loginBody["password"] = dexPwd;
    loginBody["applicationId"] = appId;
    String loginStr;
    serializeJson(loginBody, loginStr);

    String sessionPayload;
    code = dexcomPost("General/LoginPublisherAccountById", loginStr, sessionPayload);
    if (code <= 0) {
        Serial.printf("Dexcom LoginById HTTP %d\n", code);
        return -1;
    }
    if (code != HTTP_CODE_OK) {
        s_dexAuthFails[si]++;
        Serial.printf("Dexcom LoginById HTTP %d slot %d (attempt %u/%u)\n",
                      code, si, (unsigned)s_dexAuthFails[si], (unsigned)kDexAuthMaxFails);
        return 0;
    }

    dexcom_session_id = stripJsonQuotes(sessionPayload);
    if (isInvalidDexcomUuid(dexcom_session_id)) {
        s_dexAuthFails[si]++;
        Serial.printf("Dexcom: neplatne session ID slot %d (attempt %u/%u)\n",
                      si, (unsigned)s_dexAuthFails[si], (unsigned)kDexAuthMaxFails);
        dexcom_session_id = "";
        return 0;
    }
    s_dexAuthFails[si] = 0;
    Serial.println("Dexcom Auth OK (session)");
    return 1;
}

enum class DexRead : uint8_t { Ok, NoData, SessionDead };

static bool dexcomApplyReadings(JsonDocument &doc) {
    if (!doc.is<JsonArray>() || doc.size() == 0) return false;

    float mgdl = doc[0]["Value"] | 0.0f;
    if (mgdl <= 0.0f) return false;

    String trend;
    JsonVariant trendVar = doc[0]["Trend"];
    if (trendVar.is<int>() || trendVar.is<long>() || trendVar.is<float>()) {
        int trendNum = trendVar.as<int>();
        static const char *trendNames[] = {
            "", "DoubleUp", "SingleUp", "FortyFiveUp", "Flat",
            "FortyFiveDown", "SingleDown", "DoubleDown", "NotComputable", "RateOutOfRange"
        };
        if (trendNum >= 1 && trendNum <= 9) trend = trendNames[trendNum];
    } else {
        trend = trendVar.as<String>();
    }

    const char *wt = doc[0]["WT"] | "";
    long ts = parseDexcomWT(wt);

    bool hasSrcDelta = false;
    float srcDeltaMmol = 0.0f;
    if (doc.size() >= 2) {
        float prevMgdl = doc[1]["Value"] | 0.0f;
        if (prevMgdl > 0.0f) {
            srcDeltaMmol = mgdl_to_mmol(mgdl) - mgdl_to_mmol(prevMgdl);
            hasSrcDelta = true;
        }
    }

    float hist[DIA_HISTORY_MAX];
    int histN = 0;
    size_t nDocs = doc.size();
    if (nDocs > DIA_HISTORY_MAX) nDocs = DIA_HISTORY_MAX;
    for (size_t i = 0; i < nDocs; i++) {
        float v = doc[i]["Value"] | 0.0f;
        if (v > 0.0f) hist[histN++] = mgdl_to_mmol(v);
    }
    if (histN > 0) glucoseHistorySetNewestFirst(hist, histN);

    applyGlucoseReading(mgdl_to_mmol(mgdl), getTrendSymbol(trend), ts, hasSrcDelta, srcDeltaMmol);
    Serial.printf("Dexcom OK readings=%u latest_mgdl=%.0f\n", (unsigned)histN, mgdl);
    return true;
}

static DexRead dexcomReadQuery(const String &pathAndQuery) {
    String payload;
    int httpCode = dexcomPost(pathAndQuery, "", payload);
    if (httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == 500) return DexRead::SessionDead;
    if (httpCode != HTTP_CODE_OK) {
        // Nelogovať sessionId z query
        Serial.printf("Dexcom HTTP %d ReadPublisherLatestGlucoseValues\n", httpCode);
        return DexRead::NoData;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return DexRead::NoData;
    if (dexcomApplyReadings(doc)) return DexRead::Ok;
    return DexRead::NoData;
}

static DexRead dexcomFetchLatest() {
    if (dexcom_session_id.length() == 0) return DexRead::SessionDead;

    String q = "Publisher/ReadPublisherLatestGlucoseValues?sessionId=" + dexcom_session_id +
               "&minutes=1440&maxCount=" + String(DIA_HISTORY_MAX);
    DexRead pub = dexcomReadQuery(q);
    if (pub == DexRead::Ok || pub == DexRead::SessionDead) return pub;

    Serial.println("Dexcom Empty: ucet nema vlastne merania (Publisher). Follower login nefunguje.");
    return DexRead::NoData;
}

void fetchDexcom() {
    if (dexUser.length() == 0 || dexPwd.length() == 0) {
        Serial.println("Dexcom Config: chyba user/pwd");
        esp_rom_printf("[dex] Config: empty user/pwd\n");
        markFetchError(FetchError::Config);
        return;
    }

    if (dexcom_session_id.length() == 0) {
        const int lr = dexcomLogin();
        if (lr < 0) {
            esp_rom_printf("[dex] Network: login\n");
            markFetchError(FetchError::Network);
            return;
        }
        if (lr == 0) {
            esp_rom_printf("[dex] Auth fail (login)\n");
            markFetchError(FetchError::Auth);
            return;
        }
    }

    DexRead got = dexcomFetchLatest();
    if (got == DexRead::Ok) {
        esp_rom_printf("[dex] OK\n");
        return;
    }

    // Jeden re-login pri mŕtvej session (počíta sa do auth fail limitu)
    if (got == DexRead::SessionDead) {
        dexcom_session_id = "";
        dexcom_account_id = "";
        const int lr = dexcomLogin();
        if (lr < 0) {
            esp_rom_printf("[dex] Network: relogin\n");
            markFetchError(FetchError::Network);
            return;
        }
        if (lr == 0) {
            esp_rom_printf("[dex] Auth fail (relogin)\n");
            markFetchError(FetchError::Auth);
            return;
        }
        got = dexcomFetchLatest();
        if (got == DexRead::Ok) {
            esp_rom_printf("[dex] OK after relogin\n");
            return;
        }
    }

    esp_rom_printf("[dex] fail got=%d\n", (int)got);
    markFetchError(got == DexRead::SessionDead ? FetchError::Auth : FetchError::Empty);
}

static bool nsLooksLikeSha1Hex(const String &s) {
    if (s.length() != 40) return false;
    for (size_t i = 0; i < s.length(); i++) {
        if (!isxdigit((unsigned char)s[i])) return false;
    }
    return true;
}

static void nsSha1Hex(const String &input, char outHex[41]) {
    unsigned char hash[20];
    mbedtls_sha1((const unsigned char *)input.c_str(), input.length(), hash);
    for (int i = 0; i < 20; i++) sprintf(outHex + (i * 2), "%02x", hash[i]);
    outHex[40] = '\0';
}

static bool nsIsGluroo(const String &url) {
    String lower = url;
    lower.toLowerCase();
    return lower.indexOf("gluroo.com") >= 0;
}

static time_t nsUtcFromParts(int year, int mon, int day, int hour, int min, int sec) {
    if (mon < 1 || mon > 12 || day < 1 || day > 31) return 0;
    int y = year;
    int m = mon;
    if (m <= 2) {
        y -= 1;
        m += 12;
    }
    const long days = 365L * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + day - 719469;
    return (time_t)days * 86400L + hour * 3600 + min * 60 + sec;
}

static time_t nsParseIsoUtc(const char *s) {
    if (!s || !s[0]) return 0;
    int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0;
    if (sscanf(s, "%d-%d-%dT%d:%d:%d", &year, &mon, &day, &hour, &min, &sec) < 6) return 0;
    time_t utc = nsUtcFromParts(year, mon, day, hour, min, sec);
    if (utc < 1700000000) return 0;
    const char *tpos = strchr(s, 'T');
    const char *tz = tpos ? strpbrk(tpos + 1, "+-") : nullptr;
    if (tz && (tz[0] == '+' || tz[0] == '-') && strlen(tz) >= 3) {
        int oh = 0, om = 0;
        if (sscanf(tz + 1, "%d:%d", &oh, &om) >= 1) {
            int off = oh * 3600 + om * 60;
            utc += (tz[0] == '+') ? -off : off;
        }
    }
    return utc;
}

static float nsNum(JsonVariant v, bool *ok) {
    if (v.is<float>() || v.is<int>() || v.is<double>()) {
        if (ok) *ok = true;
        return v.as<float>();
    }
    if (ok) *ok = false;
    return 0;
}

/** IOB/COB – až PO apply glykémie. Jeden TLS režim (ten čo prešiel na entries). */
static void nsFetchPump(const String &base, const char *apiSecretHdr, bool tlsInsecure) {
    const int slot = diaFetchCurrentSlot();
    if (slot < 0 || slot > 1) return;
    if (!show_pump) {
        diaPumpClear(slot);
        return;
    }
    if (nsIsGluroo(base)) {
        diaPumpClear(slot);
        return;
    }
    if (ESP.getFreeHeap() < 24000) {
        Serial.println("NS pump preskocena, malo heap");
        return;
    }

    String url = base + "/api/v1/devicestatus.json?count=1";
    if (nsToken.length() > 0) url += "&token=" + nsToken;

    int httpCode = -1;
    String payload;
    {
        WiFiClientSecure client;
        configureTlsClient(client, tlsInsecure);
        HTTPClient http;
        http.setTimeout(4000);
        if (http.begin(client, url)) {
            if (apiSecretHdr && apiSecretHdr[0]) http.addHeader("api-secret", apiSecretHdr);
            httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) payload = http.getString();
            http.end();
        }
    }
    Serial.printf("NS pump HTTP %d\n", httpCode);
    if (httpCode != HTTP_CODE_OK) {
        if (httpCode == HTTP_CODE_NOT_FOUND || httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == 403)
            diaPumpClear(slot);
        return;
    }

    JsonDocument filter;
    JsonObject f0 = filter.add<JsonObject>();
    f0["date"] = true;
    f0["created_at"] = true;
    f0["openaps"]["iob"]["iob"] = true;
    f0["openaps"]["iob"]["timestamp"] = true;
    f0["openaps"]["suggested"]["COB"] = true;
    f0["openaps"]["suggested"]["IOB"] = true;
    f0["openaps"]["enacted"]["rate"] = true;
    f0["openaps"]["enacted"]["timestamp"] = true;
    f0["loop"]["iob"]["iob"] = true;
    f0["loop"]["cob"]["cob"] = true;
    f0["pump"]["reservoir"] = true;
    f0["pump"]["battery"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
        diaPumpClear(slot);
        return;
    }
    payload = "";
    JsonObject row = doc.is<JsonArray>() ? doc[0].as<JsonObject>() : doc.as<JsonObject>();
    if (row.isNull()) {
        diaPumpClear(slot);
        return;
    }

    time_t stamp = 0;
    if (row["date"].is<long long>() || row["date"].is<long>())
        stamp = (time_t)(row["date"].as<long long>() / 1000);
    if (stamp <= 0) stamp = nsParseIsoUtc(row["created_at"] | "");
    if (stamp <= 0) stamp = nsParseIsoUtc(row["openaps"]["iob"]["timestamp"] | "");
    const time_t now = time(nullptr);
    if (stamp > 1700000000 && now > 1700000000 && (now - stamp) > 20 * 60) {
        Serial.println("NS pump stare, skryvam");
        diaPumpClear(slot);
        return;
    }

    DiaPumpStatus p;
    bool ok = false;
    JsonObject openaps = row["openaps"];
    JsonObject loop = row["loop"];
    float iob = nsNum(openaps["iob"]["iob"], &ok);
    if (!ok) iob = nsNum(loop["iob"]["iob"], &ok);
    if (!ok) iob = nsNum(openaps["suggested"]["IOB"], &ok);
    if (ok) {
        p.hasIob = true;
        p.iob = iob;
    }
    ok = false;
    float cob = nsNum(openaps["suggested"]["COB"], &ok);
    if (!ok) cob = nsNum(loop["cob"]["cob"], &ok);
    if (ok && cob >= 0) {
        p.hasCob = true;
        p.cob = cob;
    }
    ok = false;
    float rate = nsNum(openaps["enacted"]["rate"], &ok);
    if (ok && rate >= 0 && rate < 50) {
        p.hasBasal = true;
        p.basal = rate;
    }
    ok = false;
    float res = nsNum(row["pump"]["reservoir"], &ok);
    if (ok && res >= 0 && res < 500) {
        p.hasReservoir = true;
        p.reservoir = res;
    }
    JsonVariant batt = row["pump"]["battery"];
    int pct = -1;
    if (batt.is<int>() || batt.is<float>()) pct = batt.as<int>();
    else if (batt["percent"].is<int>() || batt["percent"].is<float>()) pct = batt["percent"].as<int>();
    if (pct >= 0 && pct <= 100) p.batteryPct = pct;

    p.valid = p.hasIob || p.hasCob || p.hasBasal;
    if (!p.valid) {
        diaPumpClear(slot);
        Serial.println("NS pump bez poli");
        return;
    }
    slotPump[slot] = p;
    Serial.printf("NS pump iob=%d cob=%d basal=%d\n", (int)p.hasIob, (int)p.hasCob, (int)p.hasBasal);
}

/** Epoch ms z NS riadku: date (ms/s) alebo dateString. */
static long long nsEntryDateMs(JsonObject row) {
    long long ms = 0;
    if (row["date"].is<long long>() || row["date"].is<long>() || row["date"].is<double>())
        ms = row["date"].as<long long>();
    // Niektoré uploadery dávajú sekundy namiesto ms
    if (ms > 0 && ms < 100000000000LL) ms *= 1000LL;
    if (ms > 0) return ms;
    const time_t iso = nsParseIsoUtc(row["dateString"] | "");
    return (iso > 0) ? ((long long)iso * 1000LL) : 0;
}

static float nsEntrySgvMgdl(JsonObject row) {
    float v = 0;
    if (row["sgv"].is<float>() || row["sgv"].is<int>() || row["sgv"].is<double>())
        v = row["sgv"].as<float>();
    if (v <= 0.0f &&
        (row["glucose"].is<float>() || row["glucose"].is<int>() || row["glucose"].is<double>()))
        v = row["glucose"].as<float>();
    return v;
}

void fetchNightscout() {
    if (nsUrl.length() == 0) {
        markFetchError(FetchError::Config);
        return;
    }
    if (diaFetchAborted()) {
        markFetchError(FetchError::Network);
        return;
    }

    const int histCount = DIA_HISTORY_MAX;

    String base = nsUrl;
    base.trim();
    while (base.endsWith("/") && base.length() > 8)
        base.remove(base.length() - 1);
    {
        String lower = base;
        lower.toLowerCase();
        if (base.length() && !lower.startsWith("http://") && !lower.startsWith("https://"))
            base = String("https://") + base;
    }
    // Classic API_SECRET: rovnaká hodnota aj ako SHA1 v api-secret (NS akceptuje plain aj hash)
    char apiSecretHdr[41] = "";
    if (nsToken.length() > 0) {
        if (nsLooksLikeSha1Hex(nsToken)) {
            strncpy(apiSecretHdr, nsToken.c_str(), 40);
            apiSecretHdr[40] = '\0';
        } else {
            nsSha1Hex(nsToken, apiSecretHdr);
        }
    }

    auto nsBuildEntriesUrl = [&](bool typed) {
        String u = base + "/api/v1/entries.json?count=" + String(histCount);
        // find[type]=sgv: bez kalibrácií/MBG (Gluroo môže filter ignorovať/odmietnuť → fallback nižšie)
        if (typed) u += "&find%5Btype%5D=sgv";
        if (nsToken.length() > 0) {
            u += "&token=";
            u += nsToken; // NS token je zvyčajne [A-Za-z0-9._-]
        }
        return u;
    };

    diaFetchLogHeap("ns-pre");

    int httpCode = -1;
    String payload;
    bool usedInsecure = tls_insecure;
    auto nsHttpGet = [&](const String &requestUrl) {
        httpCode = -1;
        payload = "";
        Serial.printf("NS GET url_len=%u secret_len=%u tls_insecure=%d\n",
                      (unsigned)requestUrl.length(), (unsigned)nsToken.length(), (int)tls_insecure);
        for (int pass = 0; pass < 2; pass++) {
            const bool insecure = (pass == 0) ? tls_insecure : true;
            if (pass > 0) {
                if (httpCode > 0 || tls_insecure) break;
                Serial.println("NS TLS zlyhal – retry setInsecure");
                diaFetchBreathe(400);
            }
            for (int attempt = 0; attempt < 2; attempt++) {
                if (diaFetchAborted()) break;
                if (attempt > 0) diaFetchBreathe(400);
                WiFiClientSecure client;
                configureTlsClient(client, insecure);
                HTTPClient http;
                http.setTimeout(12000);
                if (!http.begin(client, requestUrl)) {
                    httpCode = -1;
                    continue;
                }
                if (apiSecretHdr[0]) http.addHeader("api-secret", apiSecretHdr);
                httpCode = http.GET();
                if (httpCode > 0) {
                    payload = http.getString();
                    usedInsecure = insecure;
                }
                http.end();
                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == 403)
                    break;
                Serial.printf("NS HTTP %d (pass %d pokus %d/2)\n", httpCode, pass, attempt + 1);
            }
            if (httpCode > 0) break;
        }
        Serial.printf("NS → HTTP %d payload_len=%u\n", httpCode, (unsigned)payload.length());
    };

    nsHttpGet(nsBuildEntriesUrl(true));
    // Prázdne pole pri 200: skús bez find[type] (niektoré Gluroo/proxy)
    if (httpCode == HTTP_CODE_OK && (payload == "[]" || payload.length() < 3)) {
        Serial.println("NS typed empty – retry bez find[type]");
        nsHttpGet(nsBuildEntriesUrl(false));
    }

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            // Filter: menší heap než plný entries JSON (36× veľké objekty na Solo často NoMemory)
            JsonDocument filter;
            {
                JsonObject f0 = filter.add<JsonObject>();
                f0["sgv"] = true;
                f0["glucose"] = true;
                f0["direction"] = true;
                f0["date"] = true;
                f0["dateString"] = true;
                f0["type"] = true;
            }
            JsonDocument doc;
            DeserializationError error =
                deserializeJson(doc, payload, DeserializationOption::Filter(filter));
            payload = "";

            if (error) {
                Serial.printf("NS JSON %s\n", error.c_str());
                markFetchError(FetchError::Empty);
            } else if (!doc.is<JsonArray>() || doc.as<JsonArray>().size() == 0) {
                markFetchError(FetchError::Empty);
            } else {
                JsonArray arr = doc.as<JsonArray>();
                // Vždy najnovší podľa date – [0] nie je spoľahlivé (asc/desc, Gluroo, proxy)
                int bestIdx = -1;
                long long bestMs = -1;
                for (size_t i = 0; i < arr.size(); i++) {
                    JsonObject row = arr[i].as<JsonObject>();
                    if (row.isNull()) continue;
                    const char *typ = row["type"] | "sgv";
                    if (typ[0] && strcmp(typ, "sgv") != 0 && strcmp(typ, "glucose") != 0)
                        continue;
                    float v = nsEntrySgvMgdl(row);
                    if (v <= 0.0f) continue;
                    long long ms = nsEntryDateMs(row);
                    if (bestIdx < 0 || ms > bestMs) {
                        bestIdx = (int)i;
                        bestMs = ms;
                    }
                }

                if (bestIdx < 0) {
                    markFetchError(FetchError::Empty);
                } else {
                    JsonObject newest = arr[(size_t)bestIdx].as<JsonObject>();
                    float sgv_mgdl = nsEntrySgvMgdl(newest);
                    String direction = newest["direction"] | "";
                    long long ns_date_ms = bestMs > 0 ? bestMs : nsEntryDateMs(newest);

                    // História: zoradiť podľa date desc (newest first), nie podľa poradia API
                    struct NsHistItem {
                        long long ms;
                        float mmol;
                    };
                    NsHistItem items[DIA_HISTORY_MAX];
                    int nItems = 0;
                    for (size_t i = 0; i < arr.size() && nItems < DIA_HISTORY_MAX; i++) {
                        JsonObject row = arr[i].as<JsonObject>();
                        float v = nsEntrySgvMgdl(row);
                        if (v <= 0.0f) continue;
                        items[nItems].ms = nsEntryDateMs(row);
                        items[nItems].mmol = mgdl_to_mmol(v);
                        nItems++;
                    }
                    for (int i = 0; i < nItems; i++) {
                        for (int j = i + 1; j < nItems; j++) {
                            if (items[j].ms > items[i].ms) {
                                NsHistItem tmp = items[i];
                                items[i] = items[j];
                                items[j] = tmp;
                            }
                        }
                    }
                    float hist[DIA_HISTORY_MAX];
                    int histN = 0;
                    for (int i = 0; i < nItems; i++) hist[histN++] = items[i].mmol;
                    if (histN > 0) glucoseHistorySetNewestFirst(hist, histN);

                    bool hasSrcDelta = false;
                    float srcDeltaMmol = 0.0f;
                    if (histN >= 2) {
                        srcDeltaMmol = hist[0] - hist[1];
                        hasSrcDelta = true;
                    }

                    Serial.printf("NS ok sgv=%.0f idx=%d/%u ts_ms=%lld\n", sgv_mgdl, bestIdx,
                                  (unsigned)arr.size(), (long long)ns_date_ms);

                    // Najprv číslo na display – pump TLS nesmie zdržiavať čerstvú glykémiu
                    applyGlucoseReading(mgdl_to_mmol(sgv_mgdl), getTrendSymbol(direction),
                                       (long)(ns_date_ms / 1000), hasSrcDelta, srcDeltaMmol);

                    // IOB max ~raz za 90 s (stačí; inak predĺži celý fetch cyklus)
                    static uint32_t s_nsPumpAt[2] = {0, 0};
                    const int slot = diaFetchCurrentSlot();
                    const uint32_t nowMs = millis();
                    const bool pumpDue = show_pump && slot >= 0 && slot < 2 &&
                                         (s_nsPumpAt[slot] == 0 ||
                                          (nowMs - s_nsPumpAt[slot]) >= 90000UL);
                    if (pumpDue && !diaFetchAborted()) {
                        nsFetchPump(base, apiSecretHdr, usedInsecure);
                        if (slot >= 0 && slot < 2) s_nsPumpAt[slot] = nowMs;
                        s_nsPumpUiSlot = slot;
                    }
                }
            }
        } else if (httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == 403) {
            markFetchError(FetchError::Auth);
        } else {
            markFetchError(FetchError::Network);
        }
    } else {
        markFetchError(FetchError::Network);
    }
    diaFetchLogHeap("ns-post");
}

void fetchGlucoseData() {
    if (WiFi.status() != WL_CONNECTED) {
        markFetchError(FetchError::Wifi);
        return;
    }
    if (dsSource != "nightscout") diaPumpClear(diaFetchCurrentSlot());
    if (dsSource == "nightscout") {
        fetchNightscout();
    } else if (dsSource == "libre") {
        fetchLibre();
    } else if (dsSource == "dexcom") {
        fetchDexcom();
    } else if (dsSource == "guardian") {
        fetchGuardian();
    } else {
        markFetchError(FetchError::Config);
    }
}

// ==========================================
// Asynchrónny fetch (FreeRTOS) – UI ostáva v loop
// ==========================================

enum class FetchPendingKind : uint8_t { None = 0, Success, Error };

struct FetchPending {
    FetchPendingKind kind = FetchPendingKind::None;
    int slot = 0;
    float mmol = 0;
    char trend[24] = {0};
    long ts = 0;
    bool hasSourceDelta = false;
    float sourceDeltaMmol = 0;
    FetchError err = FetchError::None;
};

static TaskHandle_t s_fetchTask = nullptr;
static volatile bool s_fetchBusy = false;
static volatile bool s_fetchPause = false;
static volatile bool s_deferUi = false;
static volatile int s_fetchSlot = 0;
static uint32_t s_fetchStartedMs = 0;
static portMUX_TYPE s_fetchMux = portMUX_INITIALIZER_UNLOCKED;
static FetchPending s_pending;

bool diaFetchBusy() { return s_fetchBusy; }
bool diaFetchDeferUi() { return s_deferUi; }
int diaFetchCurrentSlot() { return s_fetchSlot; }

/** Ak fetch task uviazne (TLS), uvoľni busy + zneplatni beh (žiadne 2× TLS naraz). */
void diaFetchWatchdog(uint32_t nowMs) {
    if (!s_fetchBusy) return;
    if (s_fetchStartedMs == 0) return;
    if ((uint32_t)(nowMs - s_fetchStartedMs) < 90000UL) return;
    if (s_fetchEpoch != s_fetchRunEpoch) return;
    // Len tento slot. Druhý človek sa po návrate spojenia ešte spustí.
    s_fetchEpoch++;
    diaDiagSlow(s_fetchSlot, (uint32_t)(nowMs - s_fetchStartedMs));
    esp_rom_printf("[fetch] slot %d >90s – dalsi slot pokracuje, tento ide nabuduce posledny\n",
                   s_fetchSlot);
}

static bool s_fetchGotReading = false;
static bool s_showConnecting = false;
static bool s_slotWait[2] = {false, false};
static uint8_t s_fastNetRetries = 0;

static void diaFetchSyncWaitFlag() {
    s_showConnecting = s_slotWait[0] || s_slotWait[1];
}

bool diaFetchShowConnecting() {
    return s_showConnecting && s_fastNetRetries < 8;
}

bool diaFetchSlotWaiting(int slot) {
    if (slot < 0 || slot > 1) return false;
    return s_slotWait[slot];
}

void diaFetchOnConfigChanged() {
    s_fastNetRetries = 0;
    s_slotWait[0] = false;
    s_slotWait[1] = false;
    s_showConnecting = false;
}

void diaFetchEnqueueSuccess(float mmol, const char *trend, long unix_ts,
                            bool hasSourceDelta, float sourceDeltaMmol) {
    s_fetchGotReading = true;
    if (s_fetchSlot >= 0 && s_fetchSlot < 2) s_slotWait[s_fetchSlot] = false;
    diaFetchSyncWaitFlag();
    // Ulož IHNEĎ na fetch tasku – inak slot 1 prepíše pending a slot 0 ostane so starým ts
    // (obaja siví s rovnakým vekom). UI refresh necháme na loop (LVGL).
    const int slot = s_fetchSlot;
    long ts = unix_ts;
    if (ts <= 0) {
        time_t now = time(nullptr);
        if (now > 100000) ts = (long)now;
    }
    glucoseStoreReadingSlot(slot, mmol, trend ? String(trend) : String(""), ts,
                            hasSourceDelta, sourceDeltaMmol);
    diaDiagResult(slot, (int)FetchError::None);
    Serial.printf("[fetch] slot %d OK mmol=%.2f ts=%ld (api_ts=%ld)\n",
                  slot, mmol, ts, unix_ts);

    portENTER_CRITICAL(&s_fetchMux);
    s_pending.kind = FetchPendingKind::Success;
    s_pending.slot = slot;
    s_pending.mmol = mmol;
    s_pending.ts = ts;
    s_pending.hasSourceDelta = hasSourceDelta;
    s_pending.sourceDeltaMmol = sourceDeltaMmol;
    s_pending.err = FetchError::None;
    if (trend) {
        strncpy(s_pending.trend, trend, sizeof(s_pending.trend) - 1);
        s_pending.trend[sizeof(s_pending.trend) - 1] = '\0';
    } else {
        s_pending.trend[0] = '\0';
    }
    portEXIT_CRITICAL(&s_fetchMux);
}

void diaFetchEnqueueError(FetchError err) {
    if (s_fetchSlot >= 0 && s_fetchSlot < 2) {
        // Empty = server odpovedal, ale bez SG (senzor/offline) – to nie je „čakám na server“
        if (err == FetchError::Network || err == FetchError::Wifi)
            s_slotWait[s_fetchSlot] = true;
        else if (err == FetchError::Empty || err == FetchError::Auth || err == FetchError::Config)
            s_slotWait[s_fetchSlot] = false;
    }
    diaFetchSyncWaitFlag();
    const int slot = s_fetchSlot;
    diaDiagResult(slot, (int)err);
    // Blip / prázdny JSON / krátky WiFi: nechaj poslednú hodnotu farebnú
    if ((err == FetchError::Network || err == FetchError::Empty || err == FetchError::Wifi) &&
        slotReadings[slot].has_valid_reading &&
        !slotReadings[slot].data_is_stale) {
        Serial.printf("[fetch] slot %d blip err=%d – drzim posledne data\n", slot, (int)err);
        portENTER_CRITICAL(&s_fetchMux);
        s_pending.kind = FetchPendingKind::Error;
        s_pending.slot = slot;
        s_pending.err = err;
        portEXIT_CRITICAL(&s_fetchMux);
        return;
    }
    slotReadings[slot].err = err;
    Serial.printf("[fetch] slot %d ERR %d\n", slot, (int)err);

    portENTER_CRITICAL(&s_fetchMux);
    s_pending.kind = FetchPendingKind::Error;
    s_pending.slot = slot;
    s_pending.err = err;
    portEXIT_CRITICAL(&s_fetchMux);
}

void diaFetchPoll() {
    FetchPending local;
    portENTER_CRITICAL(&s_fetchMux);
    local = s_pending;
    s_pending.kind = FetchPendingKind::None;
    portEXIT_CRITICAL(&s_fetchMux);

    if (local.kind == FetchPendingKind::Success) {
        diaUiUpdateSlotFace(local.slot);
    } else if (local.kind == FetchPendingKind::Error) {
        if (!s_deferUi || local.slot == s_fetchSlot) {
            glucoseSyncGlobalsFromSlot(local.slot);
            last_fetch_error = local.err;
        }
        diaUiUpdateSlotFace(local.slot);
    }

    const int pumpSlot = s_nsPumpUiSlot;
    if (pumpSlot >= 0 && pumpSlot < 2) {
        s_nsPumpUiSlot = -1;
        // Glykémia už mohla prejsť cez Success; IOB dorazí o chvíľu neskôr
        diaUiUpdateSlotFace(pumpSlot);
    }
}

static void diaFetchTask(void *param) {
    (void)param;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        s_fetchRunEpoch = s_fetchEpoch;
        s_fetchGotReading = false;
        Serial.println("[fetch] start");
        diaFetchLogHeap("cycle");
        s_deferUi = true;

        // Libre/Dex skôr než Nightscout – čistejší heap pre Libre TLS
        int order[DIA_PROFILE_COUNT];
        int orderN = 0;
        auto isNs = [](int s) {
            const String &src = profiles[s].dsSource;
            return src == "nightscout" || src == "ns";
        };
        for (int pass = 0; pass < 2; pass++) {
            for (int slot = 0; slot < DIA_PROFILE_COUNT; slot++) {
                if (!diaProfileActive(slot)) continue;
                if (pass == 0 && isNs(slot)) continue;
                if (pass == 1 && !isNs(slot)) continue;
                order[orderN++] = slot;
            }
        }
        if (orderN > 1) {
            const int lead = diaDiagTakeLead();
            if (lead >= 0 && !isNs(lead)) {
                for (int i = 0; i < orderN; i++) {
                    if (order[i] != lead) continue;
                    const int v = order[i];
                    for (int j = i; j > 0; --j) order[j] = order[j - 1];
                    order[0] = v;
                    break;
                }
            }
        }

        bool retrySlot[DIA_PROFILE_COUNT] = {};
        auto runSlot = [&](int slot) {
            // Najprv s_fetchSlot – inak PushSlot(1) skipne (guard: slot != current)
            s_fetchSlot = slot;
            s_fetchStartedMs = millis();
            s_fetchRunEpoch = s_fetchEpoch;
            diaDiagStep(slot, DiaDiagStep::Begin);
            diaConfigPushSlot(slot);
            Serial.printf("[fetch] slot %d src=%s url=%s\n", slot,
                          profiles[slot].dsSource.c_str(),
                          profiles[slot].nsUrl.c_str());
            esp_rom_printf("[fetch] slot %d src=%s dxu=%u\n", slot,
                           profiles[slot].dsSource.c_str(),
                           (unsigned)profiles[slot].dexUser.length());

            portENTER_CRITICAL(&s_fetchMux);
            s_pending.kind = FetchPendingKind::None;
            portEXIT_CRITICAL(&s_fetchMux);

            // História slotu do globals pred fetchom
            glucoseHistoryClear();
            glucoseSyncGlobalsFromSlot(slot);

            fetchGlucoseData();
            // UI pri platnej (aj starej) hodnote chybu hneď zmaže. Snímka musí
            // vzniknúť tu, inak sa Emily z cache už neopakuje.
            retrySlot[slot] = !diaFetchAborted() &&
                              (slotReadings[slot].err == FetchError::Network ||
                               slotReadings[slot].err == FetchError::Empty);
            // História z JSON ostáva v globals – skopíruj do slotu pred poll
            {
                DiaSlotReading &sr = slotReadings[slot];
                if (glucose_history_count > 0) {
                    sr.history_count = glucose_history_count;
                    memcpy(sr.history, glucose_history, sizeof(sr.history));
                }
            }
            diaConfigPullSessionsSlot(slot); // len tokeny – nie dsSource (dual race)

            // Počkaj kým loop spracuje pending (max ~2 s)
            for (int w = 0; w < 40; w++) {
                portENTER_CRITICAL(&s_fetchMux);
                bool empty = (s_pending.kind == FetchPendingKind::None);
                portEXIT_CRITICAL(&s_fetchMux);
                if (empty) break;
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            diaFetchLogHeap("slot-done");
        };

        for (int oi = 0; oi < orderN; oi++) {
            const int slot = order[oi];
            // Visí len tento slot. Po návrate ide ďalší, cyklus sa kvôli nemu nekončí.
            s_holdLibreGraph = (oi + 1 < orderN);
            runSlot(slot);
            s_holdLibreGraph = false;
            // Dual Guardian: dlhšia pauza – 2× ťažký TLS download za sebou
            const bool dualCl = orderN > 1 &&
                                (profiles[slot].dsSource == "guardian" ||
                                 (oi + 1 < orderN && profiles[order[oi + 1]].dsSource == "guardian"));
            vTaskDelay(pdMS_TO_TICKS(dualCl ? 3000 : (orderN > 1 ? 1500 : 500)));
        }

        // Krivka Libre až keď má číslo aj druhý človek. Visí len graf, nie Guardian.
        for (int oi = 0; oi + 1 < orderN; oi++) {
            const int slot = order[oi];
            if (!profiles[slot].dsSource.equalsIgnoreCase("libre")) continue;
            if (slotReadings[slot].err != FetchError::None) continue;
            s_fetchSlot = slot;
            s_fetchStartedMs = millis();
            s_fetchRunEpoch = s_fetchEpoch;
            diaConfigPushSlot(slot);
            glucoseHistoryClear();
            libreAppendGraph(nullptr, nullptr);
            if (glucose_history_count > 0) {
                DiaSlotReading &sr = slotReadings[slot];
                sr.history_count = glucose_history_count;
                memcpy(sr.history, glucose_history, sizeof(sr.history));
            }
            diaConfigPullSessionsSlot(slot);
        }

        // Prvý slot často padne na studenom TLS / rozbitej PSRAM, druhý už prejde.
        // Bez tohto ostane cache (Staré, prázdny graf) až do ďalšej minúty alebo reštartu.
        if (!diaFetchAborted()) {
            for (int slot = 0; slot < DIA_PROFILE_COUNT; slot++) {
                if (!diaProfileActive(slot) || !retrySlot[slot]) continue;
                Serial.printf("[fetch] slot %d retry\n", slot);
                esp_rom_printf("[fetch] slot %d retry\n", slot);
                vTaskDelay(pdMS_TO_TICKS(1500));
                if (diaFetchAborted()) break;
                runSlot(slot);
            }
        }

        // Po fetche globals vráť na dieťa, ktoré je na obrazovke
        if (s_fetchGotReading) {
            s_fastNetRetries = 0;
        } else if (s_fastNetRetries < 8) {
            bool auth = false;
            for (int slot = 0; slot < DIA_PROFILE_COUNT; slot++) {
                if (!diaProfileActive(slot)) continue;
                const FetchError e = slotReadings[slot].err;
                if (e == FetchError::Auth || e == FetchError::Config) auth = true;
            }
            if (!auth) {
                s_fastNetRetries++;
                lastUpdate = millis() - updateInterval + 5000;
                esp_rom_printf("[fetch] siet, dalsi pokus o 5s (%u)\n", (unsigned)s_fastNetRetries);
            }
        }
        s_deferUi = false;
        s_fetchSlot = diaShownSlot();
        diaConfigPushSlot(s_fetchSlot);
        glucoseSyncGlobalsFromSlot(s_fetchSlot);
        s_fetchBusy = false;
        s_fetchStartedMs = 0;
        Serial.println("[fetch] done");
    }
}
void diaFetchInit() {
    if (s_fetchTask) return;
    // Predĺž TWDT – CareLink JSON inak zhltne CPU0 IDLE → TASK_WDT reboot
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
    {
        esp_task_wdt_config_t wdtCfg = {
            .timeout_ms = 30000,
            .idle_core_mask = (1 << 0) | (1 << 1),
            .trigger_panic = true,
        };
        esp_task_wdt_reconfigure(&wdtCfg);
    }
#else
    esp_task_wdt_init(30, true);
#endif
    // Stack: TLS + JSON; core 0, loop typicky core 1
    BaseType_t ok = xTaskCreatePinnedToCore(
        diaFetchTask,
        "diaFetch",
        28672,
        nullptr,
        1,
        &s_fetchTask,
        0);
    if (ok != pdPASS) {
        Serial.println("[fetch] task create FAILED – sync fallback");
        s_fetchTask = nullptr;
    } else {
        Serial.println("[fetch] task OK");
    }
}

bool diaFetchQuiesce(uint32_t timeoutMs) {
    s_fetchPause = true;
    const uint32_t start = millis();
    while (s_fetchBusy && (millis() - start) < timeoutMs) delay(50);
    if (s_fetchBusy) {
        esp_rom_printf("[fetch] quiesce timeout\n");
        return false;
    }
    return true;
}

void diaFetchResume() {
    s_fetchPause = false;
}

bool diaFetchRequest() {
    if (s_fetchPause || s_fetchBusy) return false;

    if (!s_fetchTask) {
        // Fallback: synchrónne (ak task nevznikol)
        fetchGlucoseData();
        return true;
    }

    s_fetchBusy = true;
    s_fetchStartedMs = millis();
    xTaskNotifyGive(s_fetchTask);
    return true;
}
