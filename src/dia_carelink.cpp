#include "dia_carelink.h"
#include "dia_carelink_store.h"
#include "dia_config.h"
#include "dia_diag.h"
#include "dia_glucose.h"
#include "dia_secret.h"
#include "dia_tls.h"
#include "dia_ui.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <string.h>

// CareLink JSON → PSRAM (internal heap ostane pre TLS / WiFi)
struct CarelinkPsramAllocator : ArduinoJson::Allocator {
    void *allocate(size_t size) override {
        return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    void deallocate(void *pointer) override { heap_caps_free(pointer); }
    void *reallocate(void *pointer, size_t new_size) override {
        return heap_caps_realloc(pointer, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
};
static CarelinkPsramAllocator s_clPsramAlloc;

/** Buffer v RAM parsovať po kúskoch, inak IDLE(CPU0) nestihne a TASK_WDT reštartuje. */
class DiaYieldBuf : public Stream {
public:
    DiaYieldBuf(const char *data, size_t len) : _p(data), _n(len) {}
    int available() override { return (_i < _n) ? (int)(_n - _i) : 0; }
    int read() override {
        if (_i >= _n) return -1;
        if ((_i & 0x3FF) == 0) vTaskDelay(1);
        return (uint8_t)_p[_i++];
    }
    int peek() override { return (_i < _n) ? (uint8_t)_p[_i] : -1; }
    size_t write(uint8_t) override { return 0; }

private:
    const char *_p;
    size_t _n;
    size_t _i = 0;
};

/** Veľký CareLink stream bez yield → IDLE(CPU0) nestihne → TASK_WDT → reboot. */
class DiaYieldStream : public Stream {
public:
    explicit DiaYieldStream(Stream &inner) : _in(inner) {}
    int available() override { return _in.available(); }
    int read() override {
        if ((++_n & 0x3FF) == 0) vTaskDelay(1); // ~každých 1 kB
        return _in.read();
    }
    int peek() override { return _in.peek(); }
    size_t write(uint8_t) override { return 0; }
private:
    Stream &_in;
    uint32_t _n = 0;
};

// Tokeny z jednorazového CareLink prihlásenia (logindata.json / browser OAuth).
// 1 konto + 2 pacienti = spoločné tokeny (B fallback na A).
// 2 kontá = samostatný login pre slot B.
static String clAccess;
static String clRefresh;
static String clClientId;
static String clClientSecret;
static String clMag;
static int clBoundSlot = -1; // ktorý slot je v pracovných premenných

static String clBaseCare;
static String clBaseCumulus;
static String clTokenUrl;
static String clCountry;
static String clUsername;
static String s_lastFetchedPatientId; // dual: B bez pref → iný pacient než A

static int s_loginTokenSlot = 0;
static SemaphoreHandle_t s_clRefreshMux = nullptr;
static uint32_t s_lastRefreshOkMs = 0;
static uint32_t s_lastRefreshPostMs = 0;
static volatile bool s_refreshInFlight = false;

bool carelinkRefreshInFlight() {
    return s_refreshInFlight;
}

static void carelinkRefreshMuxInit() {
    if (!s_clRefreshMux) s_clRefreshMux = xSemaphoreCreateMutex();
}

static bool carelinkTokensLock() {
    carelinkRefreshMuxInit();
    if (!s_clRefreshMux) return false;
    return xSemaphoreTake(s_clRefreshMux, pdMS_TO_TICKS(25000)) == pdTRUE;
}

static void carelinkTokensUnlock() {
    if (s_clRefreshMux) xSemaphoreGive(s_clRefreshMux);
}

/** Krátka značka refresh tokenu. Samotný token do tohto kľúča nedávame. */
static String carelinkRefreshMark(const String &tok) {
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < tok.length(); i++) {
        h ^= (uint8_t)tok[i];
        h *= 16777619u;
    }
    char b[9];
    snprintf(b, sizeof(b), "%08lx", (unsigned long)h);
    return String(b);
}

static void carelinkRefreshGuardKey(int slot, char *out, size_t n) {
    // Len token, ktory server vyjadrene odmietol (HTTP 400/401/403).
    // c%drtok a c%drtry sa ignoruju: tie dosku zamkli aj bez odmietnutia.
    snprintf(out, n, "c%drbad", slot <= 0 ? 0 : 1);
}

static void carelinkClearRefreshGuard(int slot) {
    char key[8];
    const int s = slot <= 0 ? 0 : 1;
    carelinkRefreshGuardKey(s, key, sizeof(key));
    if (preferences.isKey(key)) preferences.remove(key);
    snprintf(key, sizeof(key), "c%drtok", s);
    if (preferences.isKey(key)) preferences.remove(key);
    snprintf(key, sizeof(key), "c%drtry", s);
    if (preferences.isKey(key)) preferences.remove(key);
}

static bool carelinkRefreshAlreadySent(int slot, const String &tok) {
    char key[8];
    carelinkRefreshGuardKey(slot, key, sizeof(key));
    if (!preferences.isKey(key) || tok.length() < 10) return false;
    return preferences.getString(key, "") == carelinkRefreshMark(tok);
}

static void carelinkMarkRefreshSent(int slot, const String &tok) {
    char key[8];
    carelinkRefreshGuardKey(slot, key, sizeof(key));
    preferences.putString(key, carelinkRefreshMark(tok));
}

// Discovery path musí ostať ≤3.8 (3.9+ = legacy). Body appVersion ≥3.7 (xDrip #4318).
static const char *CARELINK_DISCOVERY =
    "https://clcloud.minimed.eu/connect/carepartner/v13/discover/android/3.8";
static const char *CARELINK_APP_VERSION = "3.8.0";

static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + yoe / 400 + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static time_t timegmUtc(int year, int month, int day, int hour, int minute, int sec) {
    if (month < 1 || month > 12 || day < 1 || day > 31) return 0;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || sec < 0 || sec > 60) return 0;
    const int64_t days = daysFromCivil(year, (unsigned)month, (unsigned)day);
    const int64_t secs = days * 86400LL + hour * 3600LL + minute * 60LL + sec;
    return (time_t)secs;
}

static const int kClNvsChunks = 6;
static bool s_clPlainSecret = false;
static bool s_clSecretFail = false;

/** Access JWT nešifruj (veľký) – šetrí NVS; refresh/secret áno. */
static bool nvsPutChunks(const char *prefix, const String &val) {
    const bool isAccess = (strncmp(prefix, "cla", 3) == 0 || strncmp(prefix, "c1a", 3) == 0);
    return carelinkNvsPutChunks(prefix, val, !isAccess);
}

static String nvsGetChunks(const char *prefix) {
    bool plain = false, fail = false;
    String out = carelinkNvsGetChunks(prefix, &plain, &fail);
    if (plain) s_clPlainSecret = true;
    if (fail) s_clSecretFail = true;
    return out;
}

static bool carelinkJwtUsername(const String &access, String &userOut, bool allowSub = true);

static void carelinkJournalPrefix(int slot, char *out, size_t n) {
    snprintf(out, n, "c%dq", slot <= 0 ? 0 : 1);
}

static void carelinkJournalLenKey(int slot, char *out, size_t n) {
    snprintf(out, n, "c%dqlen", slot <= 0 ? 0 : 1);
}

static void carelinkClearJournal(int slot) {
    char prefix[8];
    carelinkJournalPrefix(slot, prefix, sizeof(prefix));
    carelinkNvsClearPrefix(prefix, kClNvsChunks);
    char lenKey[12];
    carelinkJournalLenKey(slot, lenKey, sizeof(lenKey));
    if (preferences.isKey(lenKey)) preferences.remove(lenKey);
}

static bool carelinkWriteJournal(int slot, const String &refresh) {
    char prefix[8];
    char lenKey[12];
    carelinkJournalPrefix(slot, prefix, sizeof(prefix));
    carelinkJournalLenKey(slot, lenKey, sizeof(lenKey));
    if (preferences.isKey(lenKey)) preferences.remove(lenKey);
    nvsPutChunks(prefix, refresh);
    if (nvsGetChunks(prefix) != refresh) return false;
    preferences.putUInt(lenKey, refresh.length());
    return preferences.getUInt(lenKey, 0) == refresh.length();
}

static void carelinkNvsPrefix(int slot, char *a, char *r, char *cid, char *sec, char *mag, size_t n);

/** Po výpadku počas zápisu: kompletný journal prepíše živý refresh ešte pred POST. */
static void carelinkAdoptJournal(int slot) {
    char prefix[8];
    char lenKey[12];
    carelinkJournalPrefix(slot, prefix, sizeof(prefix));
    carelinkJournalLenKey(slot, lenKey, sizeof(lenKey));
    if (!preferences.isKey(lenKey)) return;
    const uint32_t expect = preferences.getUInt(lenKey, 0);
    s_clSecretFail = false;
    String journal = nvsGetChunks(prefix);
    if (s_clSecretFail) return;
    if (expect < 11 || journal.length() != expect) return;

    char ap[8], rp[8], cid[8], sec[8], mag[8];
    carelinkNvsPrefix(slot, ap, rp, cid, sec, mag, sizeof(ap));
    String live = nvsGetChunks(rp);
    if (live != journal) {
        nvsPutChunks(rp, journal);
        if (nvsGetChunks(rp) == journal) {
            esp_rom_printf("[carelink] slot %d journal obnoveny\n", slot);
        } else {
            esp_rom_printf("[carelink] slot %d journal zostava\n", slot);
            return;
        }
    }
    carelinkClearJournal(slot);
}

static void carelinkNvsPrefix(int slot, char *a, char *r, char *cid, char *sec, char *mag, size_t n) {
    // Slot 0: legacy cla/clr + aj c0*; slot 1: c1*
    if (slot <= 0) {
        snprintf(a, n, "cla");
        snprintf(r, n, "clr");
        snprintf(cid, n, "cl_cid");
        snprintf(sec, n, "cl_sec");
        snprintf(mag, n, "cl_mag");
    } else {
        snprintf(a, n, "c1a");
        snprintf(r, n, "c1r");
        snprintf(cid, n, "c1cid");
        snprintf(sec, n, "c1sec");
        snprintf(mag, n, "c1mag");
    }
}

static void carelinkClearSlotTokens(int slot) {
    char ap[8], rp[8], cid[8], sec[8], mag[8];
    carelinkNvsPrefix(slot <= 0 ? 0 : 1, ap, rp, cid, sec, mag, sizeof(ap));
    for (int i = 0; i < kClNvsChunks; i++) {
        char key[8];
        snprintf(key, sizeof(key), "%s%d", ap, i);
        if (preferences.isKey(key)) preferences.remove(key);
        snprintf(key, sizeof(key), "%s%d", rp, i);
        if (preferences.isKey(key)) preferences.remove(key);
    }
    if (preferences.isKey(cid)) preferences.remove(cid);
    if (preferences.isKey(sec)) preferences.remove(sec);
    if (preferences.isKey(mag)) preferences.remove(mag);
    carelinkClearRefreshGuard(slot <= 0 ? 0 : 1);
}

static bool carelinkLoadTokensFromNvs(int slot) {
    char ap[8], rp[8], cid[8], sec[8], mag[8];
    carelinkNvsPrefix(slot, ap, rp, cid, sec, mag, sizeof(ap));
    clAccess = nvsGetChunks(ap);
    clRefresh = nvsGetChunks(rp);
    clClientId = preferences.isKey(cid) ? preferences.getString(cid, "") : "";
    {
        String rawSec = preferences.isKey(sec) ? preferences.getString(sec, "") : "";
        if (rawSec.length() > 0 && diaSecretActive() && !diaSecretIsSealed(rawSec)) s_clPlainSecret = true;
        bool failed = false;
        clClientSecret = diaSecretOpen(rawSec, &failed);
        if (failed) s_clSecretFail = true;
    }
    clMag = preferences.isKey(mag) ? preferences.getString(mag, "") : "";
    return clAccess.length() > 20 && clRefresh.length() > 10 && clClientId.length() > 0;
}

bool carelinkTokensReady() {
    return clAccess.length() > 20 && clRefresh.length() > 10 && clClientId.length() > 0;
}

static bool carelinkSlotHasOwnTokens(int slot) {
    char ap[8], rp[8], cid[8], sec[8], mag[8];
    carelinkNvsPrefix(slot <= 0 ? 0 : 1, ap, rp, cid, sec, mag, sizeof(ap));
    String a = nvsGetChunks(ap);
    String r = nvsGetChunks(rp);
    String c = preferences.isKey(cid) ? preferences.getString(cid, "") : "";
    return a.length() > 20 && r.length() > 10 && c.length() > 0;
}

bool carelinkSlotOwnsTokens(int slot) {
    return carelinkSlotHasOwnTokens(slot);
}

bool carelinkTokensReadyForSlot(int slot) {
    if (carelinkSlotHasOwnTokens(slot)) return true;
    if (slot == 1) return carelinkSlotHasOwnTokens(0); // B môže zdieľať A
    return false;
}

static void carelinkPersistTokens();

static void carelinkRestoreTokenUrl();

void carelinkLoad() {
    carelinkRefreshMuxInit();
    diaSecretBegin();
    // Jednorazovo zhodiť falošné cXrbad zo starších FW (prázdna 401).
    // Skutočne mŕtvy token pri ďalšom refresh dostane invalid_grant a zamkne sa znova.
    if (!preferences.isKey("cl_gclr")) {
        carelinkClearRefreshGuard(0);
        carelinkClearRefreshGuard(1);
        preferences.putBool("cl_gclr", true);
        esp_rom_printf("[carelink] clear legacy refresh guards\n");
    }
    carelinkAdoptJournal(0);
    carelinkAdoptJournal(1);
    for (int slot = 0; slot <= 1; slot++) {
        s_clPlainSecret = false;
        s_clSecretFail = false;
        if (!carelinkLoadTokensFromNvs(slot)) continue;
        if (s_clSecretFail) {
            esp_rom_printf("[carelink] slot %d secret necitam, NVS necham\n", slot);
            continue;
        }
        if (s_clPlainSecret) {
            clBoundSlot = slot;
            carelinkPersistTokens();
            esp_rom_printf("[carelink] slot %d tokeny zasifrovane\n", slot);
        }
    }
    s_clPlainSecret = false;
    s_clSecretFail = false;
    carelinkLoadTokensFromNvs(0);
    clBoundSlot = 0;
    carelinkRestoreTokenUrl();
    Serial.printf("[carelink] tokeny A access=%u refresh=%u cid=%u B_own=%s\n",
                  (unsigned)clAccess.length(), (unsigned)clRefresh.length(), (unsigned)clClientId.length(),
                  carelinkSlotHasOwnTokens(1) ? "ano" : "nie");
    esp_rom_printf("[carelink] tokeny A access=%u refresh=%u cid=%u B_own=%u\n",
                   (unsigned)clAccess.length(), (unsigned)clRefresh.length(), (unsigned)clClientId.length(),
                   carelinkSlotHasOwnTokens(1) ? 1u : 0u);
}

void carelinkBindSlot(int slot) {
    if (slot < 0 || slot >= DIA_PROFILE_COUNT) slot = 0;
    const bool locked = carelinkTokensLock();

    // 1 Care Partner + 2 deti = jedna session (tokeny A). Dva loginy toho istého účtu
    // na A aj B by Auth0 rotation navzájom zabíjali – zjednoť na A.
    // Dve RÔZNE Care Partner kontá (rôzny username v JWT) nechaj oddelené.
    if (slot == 1 && carelinkSlotHasOwnTokens(1) && carelinkSlotHasOwnTokens(0)) {
        char ap0[8], rp0[8], cid0[8], sec0[8], mag0[8];
        char ap1[8], rp1[8], cid1[8], sec1[8], mag1[8];
        carelinkNvsPrefix(0, ap0, rp0, cid0, sec0, mag0, sizeof(ap0));
        carelinkNvsPrefix(1, ap1, rp1, cid1, sec1, mag1, sizeof(ap1));
        String r0 = nvsGetChunks(rp0);
        String r1 = nvsGetChunks(rp1);
        String a0 = nvsGetChunks(ap0);
        String a1 = nvsGetChunks(ap1);
        String u0, u1;
        // sub (uuid) sa nesmie použiť: iný login toho istého účtu má iné sub a starý
        // refresh by sa odoslal po rotácii A. To je presne revoke celej relácie.
        const bool userA = carelinkJwtUsername(a0, u0, false);
        const bool userB = carelinkJwtUsername(a1, u1, false);
        const bool differentUser = userA && userB && u0.length() > 0 && u1.length() > 0 &&
                                   !u0.equalsIgnoreCase(u1);
        if (!differentUser) {
            Serial.printf("[carelink] B token zmazany (%s) – 1 konto / 2 pacienti\n",
                          (userA && userB) ? u0.c_str() : "konto nepotvrdene");
            esp_rom_printf("[carelink] drop slot B refresh\n");
            carelinkClearSlotTokens(1);
        } else {
            Serial.printf("[carelink] B ma ine konto %s – oddelene tokeny OK\n", u1.c_str());
        }
    }

    // Už v RAM: ten istý slot, alebo B zdieľa tokeny A (bez opakovaného NVS load JWT)
    if (carelinkTokensReady()) {
        if (clBoundSlot == slot || (slot == 1 && clBoundSlot == 0 && !carelinkSlotHasOwnTokens(1))) {
            if (locked) carelinkTokensUnlock();
            return;
        }
    }

    const String prevAccess = clAccess;
    if (carelinkLoadTokensFromNvs(slot)) {
        clBoundSlot = slot;
    } else if (slot == 1 && carelinkLoadTokensFromNvs(0)) {
        clBoundSlot = 0; // zdieľané konto A
        Serial.println("[carelink] slot B pouziva tokeny A");
    } else {
        clAccess = "";
        clRefresh = "";
        clClientId = "";
        clBoundSlot = slot;
    }
    // Rovnaký access token → nechaj discovery cache (ináč dual A/B zbytočne rediscover)
    if (clAccess != prevAccess || clAccess.length() == 0) {
        clBaseCare = "";
        clBaseCumulus = "";
        clTokenUrl = "";
    }
    if (locked) carelinkTokensUnlock();
}

static void carelinkPersistTokens() {
    int slot = (clBoundSlot >= 0) ? clBoundSlot : s_loginTokenSlot;
    if (slot < 0) slot = 0;
    if (slot > 1) slot = 1;
    char ap[8], rp[8], cid[8], sec[8], mag[8];
    carelinkNvsPrefix(slot, ap, rp, cid, sec, mag, sizeof(ap));
    nvsPutChunks(rp, clRefresh);
    nvsPutChunks(ap, clAccess);
    preferences.putString(cid, clClientId);
    if (clClientSecret.length() == 0) {
        preferences.putString(sec, "");
    } else if (diaSecretActive()) {
        String sealed = diaSecretSeal(clClientSecret);
        if (diaSecretIsSealed(sealed)) preferences.putString(sec, sealed);
        else esp_rom_printf("[carelink] client secret seal fail\n");
    } else {
        preferences.putString(sec, clClientSecret);
    }
    preferences.putString(mag, clMag);
    clBoundSlot = slot;
}

bool carelinkSaveFromJson(const String &json, int slot) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        Serial.println("[carelink] JSON tokenov sa nepodarilo precitat");
        return false;
    }
    const char *access = doc["access_token"] | "";
    const char *refresh = doc["refresh_token"] | "";
    const char *cid = doc["client_id"] | "";
    if (strlen(access) < 20 || strlen(refresh) < 10 || strlen(cid) == 0) {
        Serial.println("[carelink] v JSON chýba access_token / refresh_token / client_id");
        return false;
    }
    if (slot < 0 || slot > 1) slot = 0;
    const bool locked = carelinkTokensLock();
    clAccess = access;
    clRefresh = refresh;
    clClientId = cid;
    clClientSecret = doc["client_secret"] | "";
    clMag = doc["mag-identifier"] | "";
    clBoundSlot = slot;
    carelinkClearRefreshGuard(slot);
    carelinkPersistTokens();
    s_lastRefreshOkMs = 0;
    clBaseCare = "";
    clBaseCumulus = "";
    clTokenUrl = "";
    if (locked) carelinkTokensUnlock();
    Serial.printf("[carelink] tokeny ulozene slot=%d\n", slot);
    return true;
}

// --- Browser login (Auth0 Care Partner, bez PC / Node) ---
static String urlEncode(const String &s);
static bool clGetJson(const String &url, JsonDocument &doc, int &code, bool withAuth = true);
static bool discoverEndpoints();
static void carelinkRestoreTokenUrl();

static String s_loginTokenUrl;
static String s_loginClientId;
static String s_loginRedirect;
static String s_loginState;
static uint32_t s_loginStartedMs = 0;

bool carelinkBrowserLoginPending() {
    return s_loginTokenUrl.length() > 0 && s_loginClientId.length() > 0 &&
           (millis() - s_loginStartedMs) < 10UL * 60UL * 1000UL;
}

static String carelinkRandomHex(int bytes) {
    String out;
    out.reserve(bytes * 2);
    for (int i = 0; i < bytes; i++) {
        uint8_t b = (uint8_t)(esp_random() & 0xFF);
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", b);
        out += buf;
    }
    return out;
}

static String extractQueryParam(const String &urlOrQuery, const char *key) {
    String q = urlOrQuery;
    int qi = q.indexOf('?');
    if (qi >= 0) q = q.substring(qi + 1);
    int hash = q.indexOf('#');
    if (hash >= 0) q = q.substring(0, hash);
    String needle = String(key) + "=";
    int start = 0;
    while (start < (int)q.length()) {
        int amp = q.indexOf('&', start);
        String part = (amp < 0) ? q.substring(start) : q.substring(start, amp);
        if (part.startsWith(needle)) {
            String val = part.substring(needle.length());
            val.replace("%3A", ":");
            val.replace("%2F", "/");
            val.replace("%3D", "=");
            val.replace("%3F", "?");
            val.replace("%26", "&");
            val.replace("+", " ");
            return val;
        }
        if (amp < 0) break;
        start = amp + 1;
    }
    return "";
}

/** Načítaj Auth0 SSO (client_id, redirect, token URL, voliteľne authorize URL). */
static String carelinkAuthBase(const char *host, int port, String prefix) {
    if (prefix.endsWith("/")) prefix.remove(prefix.length() - 1);
    if (prefix.startsWith("/")) prefix.remove(0, 1);
    String base = "https://" + String(host) + ":" + String(port);
    if (prefix.length() > 0 && prefix != "/") base += "/" + prefix;
    return base;
}

static bool carelinkLoadAuth0Sso(const String &regionIn, String &errOut, String *outAuthorizeUrl) {
    errOut = "";
    if (outAuthorizeUrl) *outAuthorizeUrl = "";
    String region = regionIn;
    region.toUpperCase();
    region.trim();
    if (region != "US") region = "EU";

    JsonDocument disc;
    int code = 0;
    if (!clGetJson(CARELINK_DISCOVERY, disc, code, false)) {
        errOut = "Discovery CareLink zlyhalo (sieť).";
        return false;
    }

    String ssoUrl;
    for (JsonObject c : disc["CP"].as<JsonArray>()) {
        String r = c["region"] | "";
        r.toUpperCase();
        if (r != region) continue;
        const char *key = c["UseSSOConfiguration"] | "";
        if (strlen(key) > 0) ssoUrl = c[key] | "";
        break;
    }
    if (ssoUrl.length() == 0) {
        errOut = "Region " + region + " nie je v discovery.";
        return false;
    }

    JsonDocument sso;
    if (!clGetJson(ssoUrl, sso, code, false)) {
        errOut = "SSO konfigurácia nedostupná.";
        return false;
    }

    const char *clientId = sso["client"]["client_id"] | "";
    const char *redirectUri = sso["client"]["redirect_uri"] | "";
    const char *scope = sso["client"]["scope"] | "";
    const char *audience = sso["client"]["audience"] | "";
    const char *host = sso["server"]["hostname"] | "";
    int port = sso["server"]["port"] | 443;
    String prefix = sso["server"]["prefix"] | "";
    const char *authPath = sso["system_endpoints"]["authorization_endpoint_path"] | "";
    const char *tokenPath = sso["system_endpoints"]["token_endpoint_path"] | "";

    if (strlen(clientId) == 0 || strlen(redirectUri) == 0 || strlen(host) == 0 || strlen(tokenPath) == 0) {
        errOut = "Tento región nepoužíva Auth0 login (nepodporené).";
        return false;
    }

    String base = carelinkAuthBase(host, port, prefix);

    s_loginClientId = clientId;
    s_loginRedirect = redirectUri;
    s_loginTokenUrl = base + tokenPath;
    preferences.putString("cl_reg", region);

    if (outAuthorizeUrl) {
        if (strlen(authPath) == 0) {
            errOut = "Chýba authorization endpoint.";
            return false;
        }
        s_loginState = carelinkRandomHex(16);
        s_loginStartedMs = millis();
        *outAuthorizeUrl = base + authPath +
                           "?client_id=" + urlEncode(s_loginClientId) +
                           "&response_type=code" +
                           "&scope=" + urlEncode(String(scope)) +
                           "&redirect_uri=" + urlEncode(s_loginRedirect) +
                           "&audience=" + urlEncode(String(audience)) +
                           "&state=" + urlEncode(s_loginState);
    } else {
        // Len výmena kódu – predĺž platnosť „pending“ slotu
        s_loginStartedMs = millis();
    }
    return true;
}

bool carelinkBeginBrowserLogin(const String &regionIn, int tokenSlot, String &outAuthorizeUrl, String &errOut) {
    outAuthorizeUrl = "";
    s_loginTokenSlot = (tokenSlot <= 0) ? 0 : 1;
    if (!carelinkLoadAuth0Sso(regionIn, errOut, &outAuthorizeUrl)) return false;
    Serial.printf("[carelink] browser login ready slot=%d\n", s_loginTokenSlot);
    return true;
}

int carelinkPendingTokenSlot() {
    return s_loginTokenSlot;
}

bool carelinkCompleteBrowserLogin(const String &callbackOrCodeOrJson, String &errOut) {
    errOut = "";
    String raw = callbackOrCodeOrJson;
    raw.trim();
    if (raw.length() == 0) {
        errOut = "Prázdny vstup.";
        return false;
    }

    // Alternatíva: celý logindata.json
    if (raw.startsWith("{")) {
        if (!carelinkSaveFromJson(raw, s_loginTokenSlot)) {
            errOut = "JSON tokenov je neplatný.";
            return false;
        }
        s_loginTokenUrl = "";
        return true;
    }

    String code = extractQueryParam(raw, "code");
    if (code.length() == 0) {
        // možno samotný kód bez URL
        if (raw.indexOf("://") < 0 && raw.indexOf('=') < 0 && raw.indexOf(' ') < 0 && raw.length() > 10) {
            code = raw;
        }
    }

    // Token exchange nepotrebuje state – ak pending vypršal, znova načítaj SSO
    if (!carelinkBrowserLoginPending()) {
        String reg = preferences.isKey("cl_reg") ? preferences.getString("cl_reg", "EU") : "EU";
        if (!carelinkLoadAuth0Sso(reg, errOut, nullptr)) {
            if (errOut.length() == 0) errOut = "Najprv spusti krok 1 (Prihlásiť CareLink).";
            return false;
        }
    }
    String err = extractQueryParam(raw, "error");
    if (err.length() > 0) {
        String desc = extractQueryParam(raw, "error_description");
        errOut = "CareLink: " + err + (desc.length() ? (" – " + desc) : "");
        return false;
    }
    if (code.length() < 8) {
        errOut = "V texte nie je authorization code. Vlož celý odkaz com.medtronic… alebo code=…";
        return false;
    }

    String body = "grant_type=authorization_code" +
                  String("&client_id=") + urlEncode(s_loginClientId) +
                  "&code=" + urlEncode(code) +
                  "&redirect_uri=" + urlEncode(s_loginRedirect);

    WiFiClientSecure client;
    configureTlsClient(client, true);
    HTTPClient http;
    http.setTimeout(25000);
    if (!http.begin(client, s_loginTokenUrl)) {
        errOut = "Token endpoint begin fail.";
        return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "Dalvik/2.1.0 (Linux; U; Android 10; Nexus 5X Build/QQ3A.200805.001)");
    int httpCode = http.POST(body);
    String payload = (httpCode > 0) ? http.getString() : "";
    http.end();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[carelink] code exchange HTTP %d\n", httpCode);
        errOut = "Výmena kódu zlyhala (HTTP " + String(httpCode) + "). Skús znova od kroku 1.";
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        errOut = "Neplatná odpoveď tokenov.";
        return false;
    }
    const char *access = doc["access_token"] | "";
    const char *refresh = doc["refresh_token"] | "";
    if (strlen(access) < 20 || strlen(refresh) < 10) {
        errOut = "Odpoveď bez access/refresh tokenu.";
        return false;
    }

    const bool locked = carelinkTokensLock();
    clAccess = access;
    clRefresh = refresh;
    clClientId = s_loginClientId;
    clClientSecret = "";
    clMag = "";
    clBoundSlot = s_loginTokenSlot;
    carelinkClearRefreshGuard(s_loginTokenSlot);
    carelinkPersistTokens();
    s_lastRefreshOkMs = 0;
    if (locked) carelinkTokensUnlock();
    clBaseCare = "";
    clBaseCumulus = "";
    clTokenUrl = "";
    s_loginTokenUrl = "";
    s_loginClientId = "";
    s_loginRedirect = "";
    s_loginState = "";
    Serial.printf("[carelink] browser login OK – tokeny ulozene slot=%d\n", s_loginTokenSlot);
    return true;
}

static String urlEncode(const String &s) {
    String out;
    out.reserve(s.length() * 3);
    const char *hex = "0123456789ABCDEF";
    for (size_t i = 0; i < s.length(); i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

static bool b64UrlDecode(const String &in, String &out) {
    String b64 = in;
    b64.replace('-', '+');
    b64.replace('_', '/');
    while (b64.length() % 4) b64 += '=';
    size_t olen = 0;
    int rc = mbedtls_base64_decode(nullptr, 0, &olen, (const unsigned char *)b64.c_str(), b64.length());
    if (rc != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && rc != 0) return false;
    if (olen == 0 || olen > 4096) return false;
    unsigned char buf[4096];
    if (mbedtls_base64_decode(buf, sizeof(buf), &olen, (const unsigned char *)b64.c_str(), b64.length()) != 0) {
        return false;
    }
    out = "";
    out.reserve(olen);
    for (size_t i = 0; i < olen; i++) out += (char)buf[i];
    return true;
}

/** Username z access JWT. allowSub=false pri porovnaní slotov: uuid nie je účet. */
static bool carelinkJwtUsername(const String &access, String &userOut, bool allowSub) {
    userOut = "";
    int d1 = access.indexOf('.');
    int d2 = access.indexOf('.', d1 + 1);
    if (d1 < 0 || d2 < 0) return false;
    String payload;
    if (!b64UrlDecode(access.substring(d1 + 1, d2), payload)) return false;
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
        const char *user = doc["token_details"]["preferred_username"] | "";
        if (strlen(user) == 0) user = doc["preferred_username"] | "";
        if (strlen(user) == 0) user = doc["nickname"] | "";
        if (strlen(user) == 0) user = doc["email"] | "";
        if (allowSub && strlen(user) == 0) user = doc["sub"] | "";
        if (strlen(user) > 0) {
            userOut = user;
            return true;
        }
    }
    const char *keys[] = {"preferred_username", "nickname", "email", "sub"};
    for (const char *key : keys) {
        if (!allowSub && strcmp(key, "sub") == 0) continue;
        String pat = String("\"") + key + "\":\"";
        int at = payload.indexOf(pat);
        if (at < 0) continue;
        at += pat.length();
        int end = payload.indexOf('"', at);
        if (end <= at) continue;
        userOut = payload.substring(at, end);
        if (userOut.length() > 0) return true;
    }
    return false;
}

static bool decodeAccessClaims(long &expOut) {
    expOut = 0;
    int d1 = clAccess.indexOf('.');
    int d2 = clAccess.indexOf('.', d1 + 1);
    if (d1 < 0 || d2 < 0) return false;
    String payload;
    if (!b64UrlDecode(clAccess.substring(d1 + 1, d2), payload)) return false;
    JsonDocument doc;
    if (deserializeJson(doc, payload)) return false;
    expOut = doc["exp"] | 0;

    // Auth0 JWT často nemá token_details.country – len preferred_username / email
    String userFromJwt;
    if (carelinkJwtUsername(clAccess, userFromJwt)) clUsername = userFromJwt;

    const char *country = doc["token_details"]["country"] | "";
    if (strlen(country) == 0) country = doc["country"] | "";
    if (strlen(country) > 0) {
        clCountry = country;
        clCountry.toUpperCase();
    } else if (clCountry.length() == 0) {
        String reg = preferences.isKey("cl_reg") ? preferences.getString("cl_reg", "EU") : "EU";
        clCountry = (reg == "US") ? "US" : "SK";
    }

    Serial.printf("[carelink] jwt user=%s country=%s exp=%ld\n",
                  clUsername.c_str(), clCountry.c_str(), (long)expOut);
    // Stačí platný JWT; username doťahujeme z /users/me ak chýba
    return expOut > 0;
}

static bool s_lastHttpHtml = false;

/** Portál, filter alebo captive stránka. To nie je odmietnutie CareLink tokenu. */
static String carelinkReadSnippet(HTTPClient &http) {
    WiFiClient *stream = http.getStreamPtr();
    String out;
    out.reserve(200);
    const uint32_t deadline = millis() + 800;
    while (out.length() < 200 && (int32_t)(deadline - millis()) > 0) {
        if (!stream || stream->available() <= 0) {
            if (!http.connected()) break;
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        while (stream->available() > 0 && out.length() < 200) {
            const int c = stream->read();
            if (c < 0) break;
            out += (char)c;
        }
    }
    return out;
}

static bool carelinkBodyIsHtml(const String &body) {
    String h = body;
    h.trim();
    h.toLowerCase();
    if (h.length() == 0) return false;
    return h.startsWith("<") || h.indexOf("<html") >= 0 || h.indexOf("<!doctype") >= 0;
}

/** Auth0 naozaj zahodil refresh. Prázdne telo alebo HTML filter ním nie je. */
static bool carelinkOAuthRejected(const String &body) {
    if (carelinkBodyIsHtml(body)) return false;
    return body.indexOf("invalid_grant") >= 0 || body.indexOf("invalid_token") >= 0 ||
           body.indexOf("access_denied") >= 0;
}

static void addClHeaders(HTTPClient &http, bool jsonBody) {
    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "Dalvik/2.1.0 (Linux; U; Android 10; Nexus 5X Build/QQ3A.200805.001)");
    if (jsonBody) http.addHeader("Content-Type", "application/json");
    if (clMag.length() > 0) http.addHeader("mag-identifier", clMag);
    if (clAccess.length() > 0) http.addHeader("Authorization", "Bearer " + clAccess);
}

/** withAuth=false: discovery/SSO – expirovaný Bearer nesmie zablokovať token URL. */
static bool clGetJson(const String &url, JsonDocument &doc, int &code, bool withAuth) {
    s_lastHttpHtml = false;
    WiFiClientSecure client;
    configureTlsClient(client, true); // CareLink cert chain sa mení; čítame len s tokenom
    HTTPClient http;
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) return false;
    if (withAuth) addClHeaders(http, false);
    else {
        http.addHeader("Accept", "application/json");
        http.addHeader("User-Agent", "Dalvik/2.1.0 (Linux; U; Android 10; Nexus 5X Build/QQ3A.200805.001)");
    }
    code = http.GET();
    bool ok = false;
    if (code == HTTP_CODE_OK) {
        DiaYieldStream ystream(http.getStream());
        ok = deserializeJson(doc, ystream) == DeserializationError::Ok;
    } else {
        if (code == 401 || code == 403) s_lastHttpHtml = carelinkBodyIsHtml(carelinkReadSnippet(http));
        Serial.printf("[carelink] GET %d %s\n", code, url.c_str());
        if (s_lastHttpHtml) esp_rom_printf("[carelink] GET html, nie auth\n");
    }
    http.end();
    vTaskDelay(pdMS_TO_TICKS(10));
    return ok;
}

static void carelinkPersistTokenUrl() {
    if (clTokenUrl.length() > 8) preferences.putString("cl_turl", clTokenUrl);
    if (clBaseCare.length() > 8) preferences.putString("cl_bcare", clBaseCare);
    if (clBaseCumulus.length() > 8) preferences.putString("cl_bcum", clBaseCumulus);
}

static void carelinkRestoreTokenUrl() {
    if (clTokenUrl.length() == 0 && preferences.isKey("cl_turl"))
        clTokenUrl = preferences.getString("cl_turl", "");
    if (clBaseCare.length() == 0 && preferences.isKey("cl_bcare"))
        clBaseCare = preferences.getString("cl_bcare", "");
    if (clBaseCumulus.length() == 0 && preferences.isKey("cl_bcum"))
        clBaseCumulus = preferences.getString("cl_bcum", "");
}

void carelinkForgetEndpoints() {
    clBaseCare = "";
    clBaseCumulus = "";
    clTokenUrl = "";
}

static bool discoverEndpoints() {
    diaDiagStep(dia_active_slot, DiaDiagStep::Discover);
    carelinkRestoreTokenUrl();
    if (clBaseCare.length() > 0 && clBaseCumulus.length() > 0 && clTokenUrl.length() > 0) return true;
    if (clCountry.length() == 0) {
        String reg = preferences.isKey("cl_reg") ? preferences.getString("cl_reg", "EU") : "EU";
        clCountry = (reg == "US") ? "US" : "SK";
    }

    JsonDocument disc;
    int code = 0;
    // Bez Bearer: po USB reconnecte access často expirovaný a 401 by zabilo discovery
    if (!clGetJson(CARELINK_DISCOVERY, disc, code, false)) return false;

    String region;
    for (JsonObject c : disc["supportedCountries"].as<JsonArray>()) {
        if (!c[clCountry].isNull()) {
            region = c[clCountry]["region"] | "";
            break;
        }
    }
    // Auth0 token bez country → fallback na EU/US z loginu
    if (region.length() == 0) {
        String want = preferences.isKey("cl_reg") ? preferences.getString("cl_reg", "EU") : "EU";
        want.toUpperCase();
        for (JsonObject c : disc["CP"].as<JsonArray>()) {
            String r = c["region"] | "";
            if (r.equalsIgnoreCase(want)) {
                region = r;
                break;
            }
        }
    }
    if (region.length() == 0) {
        Serial.printf("[carelink] krajina %s nie je v discovery\n", clCountry.c_str());
        return false;
    }

    String ssoUrl;
    for (JsonObject c : disc["CP"].as<JsonArray>()) {
        if (String(c["region"] | "") != region) continue;
        clBaseCare = c["baseUrlCareLink"] | "";
        clBaseCumulus = c["baseUrlCumulus"] | "";
        const char *key = c["UseSSOConfiguration"] | "";
        if (strlen(key) > 0) ssoUrl = c[key] | "";
        break;
    }
    if (clBaseCare.length() == 0 || clBaseCumulus.length() == 0 || ssoUrl.length() == 0) return false;

    JsonDocument sso;
    if (!clGetJson(ssoUrl, sso, code, false)) return false;
    const char *host = sso["server"]["hostname"] | "";
    int port = sso["server"]["port"] | 443;
    String prefix = sso["server"]["prefix"] | "";
    const char *tokenPath = sso["system_endpoints"]["token_endpoint_path"] | "";
    if (strlen(host) == 0 || strlen(tokenPath) == 0) return false;
    clTokenUrl = carelinkAuthBase(host, port, prefix) + tokenPath;
    carelinkPersistTokenUrl();
    esp_rom_printf("[carelink] token url ok\n");
    Serial.printf("[carelink] region %s user %s\n", region.c_str(), clUsername.c_str());
    return true;
}

// Auth0: druhe odoslanie toho isteho refresh tokenu zrusi celu relaciu.
enum class ClRefreshResult : uint8_t { Ok = 0, Transient, Held, Invalid };

static bool carelinkJsonField(const String &payload, const char *key, String &out) {
    String pat = String("\"") + key + "\":\"";
    int at = payload.indexOf(pat);
    if (at < 0) return false;
    at += pat.length();
    int end = payload.indexOf('"', at);
    if (end <= at) return false;
    out = payload.substring(at, end);
    return out.length() > 10;
}

static bool carelinkRefreshStored(int slot) {
    carelinkPersistTokens();
    char ap[8], rp[8], cid[8], sec[8], mag[8];
    carelinkNvsPrefix(slot, ap, rp, cid, sec, mag, sizeof(ap));
    if (nvsGetChunks(rp) == clRefresh) return true;
    Serial.println("[carelink] NVS refresh mismatch – zapisujem znova");
    nvsPutChunks(rp, clRefresh);
    nvsPutChunks(ap, clAccess);
    return nvsGetChunks(rp) == clRefresh;
}

static bool carelinkCommitNewRefresh(int slot, const String &access, const String &refresh) {
    if (access.length() < 20 || refresh.length() <= 10) return false;
    clAccess = access;
    clRefresh = refresh;
    const bool journalOk = carelinkWriteJournal(slot, refresh);
    bool stored = false;
    for (int attempt = 0; attempt < 3 && !stored; attempt++) stored = carelinkRefreshStored(slot);
    if (stored) {
        carelinkClearJournal(slot);
        carelinkClearRefreshGuard(slot);
        s_lastRefreshPostMs = 0;
        s_lastRefreshOkMs = millis();
        esp_rom_printf("[carelink] refresh saved\n");
        return true;
    }
    if (journalOk) {
        s_lastRefreshOkMs = millis();
        esp_rom_printf("[carelink] refresh v journali\n");
        return true;
    }
    return false;
}

static ClRefreshResult refreshAccessToken() {
    diaDiagStep(clBoundSlot, DiaDiagStep::Refresh);
    if (!carelinkTokensLock()) return ClRefreshResult::Transient;
    ClRefreshResult result = ClRefreshResult::Transient;
    const int slot = (clBoundSlot <= 0) ? 0 : clBoundSlot;

    if (clRefresh.length() == 0 || clClientId.length() == 0) {
        esp_rom_printf("[carelink] refresh: missing token/cid\n");
        result = ClRefreshResult::Invalid;
        goto done;
    }
    // Po USB reconnecte je clTokenUrl v RAM prázdne – NIE Invalid (to by ukazovalo „odhlásený“)
    if (clTokenUrl.length() == 0) {
        carelinkRestoreTokenUrl();
        if (clTokenUrl.length() == 0) {
            // discoverEndpoints robí HTTP – pusti zamok, inak deadlock s fetchom
            carelinkTokensUnlock();
            const bool disc = discoverEndpoints();
            if (!carelinkTokensLock()) return ClRefreshResult::Transient;
            if (!disc || clTokenUrl.length() == 0) {
                esp_rom_printf("[carelink] refresh: no token URL yet\n");
                result = ClRefreshResult::Transient;
                goto done;
            }
        }
    }
    // Portal aj druhe dieta musia pockat na prave prebiehajucu obnovu.
    if (s_lastRefreshOkMs != 0 && (millis() - s_lastRefreshOkMs) < 45000UL && carelinkTokensReady()) {
        Serial.println("[carelink] refresh skip (cerstvy)");
        result = ClRefreshResult::Ok;
        goto done;
    }
    if (WiFi.status() != WL_CONNECTED) {
        esp_rom_printf("[carelink] refresh skip, wifi down\n");
        result = ClRefreshResult::Transient;
        goto done;
    }
    if (carelinkRefreshAlreadySent(slot, clRefresh)) {
        // Zamok na stary refresh: skus journal (novy token po rotacii).
        // NIKDY znova nePOSTuj ten isty refresh – Auth0 by zrusil celu rodinu.
        carelinkAdoptJournal(slot);
        carelinkLoadTokensFromNvs(slot);
        if (!carelinkRefreshAlreadySent(slot, clRefresh) && clRefresh.length() > 10) {
            esp_rom_printf("[carelink] held → journal novy refresh\n");
            result = ClRefreshResult::Transient;
            goto done;
        }
        Serial.println("[carelink] refresh uz bol odoslany, druhy krat by zrusil relaciu");
        esp_rom_printf("[carelink] refresh not resent\n");
        result = ClRefreshResult::Held;
        goto done;
    }
    if (s_lastRefreshPostMs != 0 && (millis() - s_lastRefreshPostMs) < 90000UL) {
        esp_rom_printf("[carelink] refresh wait, predosly pokus bez odpovede\n");
        result = ClRefreshResult::Transient;
        goto done;
    }

    {
        String body = String("grant_type=refresh_token&refresh_token=") + urlEncode(clRefresh) +
                      "&client_id=" + urlEncode(clClientId);
        if (clClientSecret.length() > 0) {
            body += "&client_secret=" + urlEncode(clClientSecret);
        }

        WiFiClientSecure client;
        configureTlsClient(client, true);
        HTTPClient http;
        http.setTimeout(30000);
        if (!http.begin(client, clTokenUrl)) {
            result = ClRefreshResult::Transient;
            goto done;
        }
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.addHeader("Accept", "application/json");
        if (clMag.length() > 0) http.addHeader("mag-identifier", clMag);

        const String sentRefresh = clRefresh;
        s_lastRefreshPostMs = millis();
        s_refreshInFlight = true;
        esp_rom_printf("[carelink] refresh post\n");
        const int code = http.POST(body);
        String payload;
        if (code == HTTP_CODE_OK) {
            WiFiClient *stream = http.getStreamPtr();
            const int contentLen = http.getSize();
            // id_token + access + refresh bežne >12 kB – truncate kazil JWT
            constexpr int kMaxPayload = 24576;
            payload.reserve(6144);
            const uint32_t deadline = millis() + 12000;
            while (stream && (int)payload.length() < kMaxPayload && (millis() < deadline) &&
                   (contentLen < 0 || (int)payload.length() < contentLen)) {
                int avail = stream->available();
                if (avail <= 0) {
                    if (!http.connected()) break;
                    delay(5);
                    continue;
                }
                while (avail-- > 0 && (int)payload.length() < kMaxPayload) {
                    const int c = stream->read();
                    if (c < 0) break;
                    payload += (char)c;
                }
            }
        } else if (code > 0) {
            payload = http.getString();
        }
        http.end();
        esp_rom_printf("[carelink] refresh HTTP %d len=%u\n", code, (unsigned)payload.length());

        if (code <= 0) {
            // Bez HTTP odpovede server token skor neotocil. Nezamykaj natrvalo,
            // dalsi pokus az ked je WiFi hore a pauza prejde.
            Serial.printf("[carelink] refresh net err %d\n", code);
            esp_rom_printf("[carelink] refresh net %d\n", code);
            result = ClRefreshResult::Transient;
            goto done;
        }
        if (code == 400 || code == 401 || code == 403) {
            // Natrvalo zamknúť len pri explicitnom Auth0 odmietnutí (invalid_grant…).
            // Prázdne telo / HTML / filter / captive = Transient – inak rano
            // „CareLink odhlásený“ na celé hodiny bez skutočného logoutu.
            const bool html = carelinkBodyIsHtml(payload);
            const bool oauth = carelinkOAuthRejected(payload);
            if (!oauth) {
                esp_rom_printf("[carelink] refresh HTTP %d bez zamku html=%d len=%u\n",
                               code, (int)html, (unsigned)payload.length());
                result = ClRefreshResult::Transient;
                goto done;
            }
            carelinkMarkRefreshSent(slot, sentRefresh);
            Serial.printf("[carelink] refresh HTTP %d (token uz neplati)\n", code);
            esp_rom_printf("[carelink] refresh invalid HTTP %d\n", code);
            result = ClRefreshResult::Invalid;
            goto done;
        }
        if (code != HTTP_CODE_OK) {
            // 5xx token skor neotocil. Nezamykaj, dalsi cyklus to skusi znova.
            Serial.printf("[carelink] refresh HTTP %d\n", code);
            esp_rom_printf("[carelink] refresh HTTP %d retry\n", code);
            result = ClRefreshResult::Transient;
            goto done;
        }
        JsonDocument doc;
        String accessStr;
        String refreshStr;
        if (!deserializeJson(doc, payload)) {
            accessStr = doc["access_token"] | "";
            refreshStr = doc["refresh_token"] | "";
        }
        if (accessStr.length() < 20) carelinkJsonField(payload, "access_token", accessStr);
        if (refreshStr.length() <= 10) carelinkJsonField(payload, "refresh_token", refreshStr);
        if (accessStr.length() < 20) {
            // HTTP 200 bez access: zamkni stary refresh LEN ak telo obsahuje refresh_token
            // (Auth0 ho skor otocil). Inak Transient – truncate/filter.
            carelinkAdoptJournal(slot);
            carelinkLoadTokensFromNvs(slot);
            if (clAccess.length() >= 20 && !carelinkRefreshAlreadySent(slot, clRefresh)) {
                carelinkClearRefreshGuard(slot);
                s_lastRefreshOkMs = millis();
                esp_rom_printf("[carelink] refresh json fail, journal OK\n");
                result = ClRefreshResult::Ok;
                goto done;
            }
            if (payload.indexOf("refresh_token") >= 0) {
                carelinkMarkRefreshSent(slot, sentRefresh);
                Serial.println("[carelink] refresh JSON zlyhal, token znova neposlem");
                esp_rom_printf("[carelink] refresh json invalid (rotated?)\n");
                result = ClRefreshResult::Held;
            } else {
                esp_rom_printf("[carelink] refresh JSON truncate, bez zamku\n");
                result = ClRefreshResult::Transient;
            }
            goto done;
        }
        if (refreshStr.length() > 10) {
            if (carelinkCommitNewRefresh(slot, accessStr, refreshStr)) {
                Serial.println("[carelink] token obnoveny");
                result = ClRefreshResult::Ok;
            } else {
                carelinkMarkRefreshSent(slot, sentRefresh);
                esp_rom_printf("[carelink] novy refresh len v RAM\n");
                s_lastRefreshOkMs = millis();
                result = ClRefreshResult::Ok;
            }
        } else {
            // Server neposlal novy refresh. Stary nechaj a nezamykaj ho.
            carelinkRefreshStored(slot);
            esp_rom_printf("[carelink] no new refresh, keep old\n");
            s_lastRefreshOkMs = millis();
            Serial.println("[carelink] token obnoveny");
            result = ClRefreshResult::Ok;
        }
    }

done:
    s_refreshInFlight = false;
    carelinkTokensUnlock();
    return result;
}

static String carelinkTrendToDirection(const char *trend) {
    String t = trend ? trend : "";
    t.toUpperCase();
    if (t.indexOf("TRIPLE") >= 0 && t.indexOf("UP") >= 0) return "DoubleUp";
    if (t.indexOf("DOUBLE") >= 0 && t.indexOf("UP") >= 0) return "DoubleUp";
    if (t.indexOf("TRIPLE") >= 0 && t.indexOf("DOWN") >= 0) return "DoubleDown";
    if (t.indexOf("DOUBLE") >= 0 && t.indexOf("DOWN") >= 0) return "DoubleDown";
    if (t.indexOf("FORTY") >= 0 && t.indexOf("UP") >= 0) return "FortyFiveUp";
    if (t.indexOf("FORTY") >= 0 && t.indexOf("DOWN") >= 0) return "FortyFiveDown";
    if (t == "UP") return "SingleUp";
    if (t == "DOWN") return "SingleDown";
    if (t == "FLAT" || t == "NONE" || t.length() == 0) return "Flat";
    return "Flat";
}

static long parseCarelinkTime(JsonVariant last) {
    const char *dt = last["datetime"] | "";
    if (strlen(dt) == 0) dt = last["dateTime"] | "";
    if (strlen(dt) > 0) {
        int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
        if (sscanf(dt, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) >= 6) {
            long ts = (long)timegmUtc(y, mo, d, h, mi, se);
            const char *tz = strchr(dt, 'T');
            if (tz) {
                const char *off = strpbrk(tz + 1, "+-");
                if (off && strlen(off) >= 6 && off[3] == ':') {
                    int oh = atoi(off + 1);
                    int om = atoi(off + 4);
                    long delta = (oh * 60L + om) * 60L;
                    if (*off == '+') ts -= delta;
                    else ts += delta;
                }
            }
            return ts;
        }
    }
    double ms = last["timestamp"] | 0.0;
    if (ms > 1000000000000.0) return (long)(ms / 1000.0);
    if (ms > 1000000000.0) return (long)ms;
    time_t now = time(nullptr);
    return (now > 1700000000) ? (long)now : 0;
}

static bool readGlucose(bool &authFail) {
    diaDiagStep(dia_active_slot, DiaDiagStep::Read);
    authFail = false;
    String url = clBaseCumulus + "/display/message";
    JsonDocument body;
    body["role"] = "patient";
    body["appVersion"] = CARELINK_APP_VERSION;

    JsonDocument me;
    int code = 0;
    if (!clGetJson(clBaseCare + "/users/me", me, code)) {
        authFail = (code == 401 || code == 403) && !s_lastHttpHtml;
        esp_rom_printf("[carelink] /users/me fail HTTP %d\n", code);
        return false;
    }
    if (clUsername.length() == 0) {
        const char *u = me["username"] | "";
        if (strlen(u) == 0) u = me["accountId"] | "";
        if (strlen(u) > 0) clUsername = u;
    }
    body["username"] = clUsername;

    const char *role = me["role"] | "";
    String roleStr = role;
    roleStr.toUpperCase();
    Serial.printf("[carelink] me role=%s user=%s slot=%d\n", role, clUsername.c_str(), dia_active_slot);
    if (roleStr.indexOf("CARE_PARTNER") >= 0) {
        JsonDocument patients;
        if (!clGetJson(clBaseCare + "/links/patients", patients, code)) {
            authFail = (code == 401 || code == 403) && !s_lastHttpHtml;
            esp_rom_printf("[carelink] /patients fail HTTP %d\n", code);
            return false;
        }
        String patientUser;
        String softMatch;
        // Pref zo slotu (nie len globál) – B musí držať vlastného pacienta
        String pref = carelink_patient_pref;
        if (dia_active_slot >= 0 && dia_active_slot < DIA_PROFILE_COUNT) {
            String slotPref = profiles[dia_active_slot].carelink_patient_pref;
            slotPref.trim();
            if (slotPref.length()) pref = slotPref;
        }
        pref.trim();
        String prefLower = pref;
        prefLower.toLowerCase();
        String list;
        String firstAny;
        String firstOther; // dual A+B CareLink, B bez pref: iný než A v tomto cykle
        const bool dualClSameAccount =
            dia_active_slot == 1 && profiles[0].dsSource.equalsIgnoreCase("guardian");
        for (JsonObject p : patients.as<JsonArray>()) {
            String u = p["username"] | "";
            String fn = p["firstName"] | "";
            String ln = p["lastName"] | "";
            String display = fn;
            if (ln.length()) {
                if (display.length()) display += " ";
                display += ln;
            }
            if (display.length() == 0) display = u;
            if (u.length()) {
                // Celý zoznam vždy – break pri pref skôr skrátil list na 1 meno v menu
                list += u;
                list += '\t';
                list += display;
                list += '\n';
                if (firstAny.length() == 0) firstAny = u;
                if (dualClSameAccount && firstOther.length() == 0 &&
                    s_lastFetchedPatientId.length() > 0 &&
                    !u.equalsIgnoreCase(s_lastFetchedPatientId)) {
                    firstOther = u;
                }
            }
            if (pref.length() == 0 || u.length() == 0 || patientUser.length() > 0) continue;
            if (u.equalsIgnoreCase(pref) || display.equalsIgnoreCase(pref)) {
                patientUser = u; // exact – bez break, dozoznam ostáva kompletný
                continue;
            }
            String blob = u + " " + fn + " " + ln;
            blob.toLowerCase();
            if (softMatch.length() == 0 && blob.indexOf(prefLower) >= 0) softMatch = u;
        }
        if (patientUser.length() == 0) patientUser = softMatch;
        if (patientUser.length() == 0 && pref.length() == 0) {
            if (dualClSameAccount && firstOther.length() > 0) patientUser = firstOther;
            else patientUser = firstAny;
        }
        // Prvý výber bez pref → ulož, aby B nestriedal (A=NS + B=CL)
        if (pref.length() == 0 && patientUser.length() > 0 &&
            dia_active_slot >= 0 && dia_active_slot < DIA_PROFILE_COUNT) {
            profiles[dia_active_slot].carelink_patient_pref = patientUser;
            carelink_patient_pref = patientUser;
            char key[12];
            snprintf(key, sizeof(key), "p%d_cpf", dia_active_slot);
            preferences.putString(key, patientUser);
            if (dia_active_slot == 0) preferences.putString("cl_pref", patientUser);
        }
        if (list.length() > 0) {
            carelink_patient_list = list;
            // Zoznam len na Guardian sloty – Dexcom/Libre A nesmie „zdediť“ CareLink pacientov
            for (int s = 0; s < DIA_PROFILE_COUNT; s++) {
                if (!profiles[s].dsSource.equalsIgnoreCase("guardian")) continue;
                profiles[s].carelink_patient_list = list;
                char key[12];
                snprintf(key, sizeof(key), "p%d_cpl", s);
                String prev = preferences.isKey(key) ? preferences.getString(key, "") : "";
                if (prev != list) preferences.putString(key, list);
            }
            if (dia_active_slot == 0) {
                String prev = preferences.isKey("cl_pats") ? preferences.getString("cl_pats", "") : "";
                if (prev != list) {
                    preferences.putString("cl_pats", list);
                    Serial.printf("[carelink] pacienti ulozene (%u B)\n", (unsigned)list.length());
                }
            }
        }
        if (patientUser.length() == 0) {
            Serial.println("[carelink] care partner bez pacienta (skontroluj preferenciu)");
            esp_rom_printf("[carelink] no patient match pref='%s'\n",
                           carelink_patient_pref.c_str());
            markFetchError(FetchError::Config);
            return true; // chyba už zafrontovaná – nie „Sieť“
        }
        Serial.printf("[carelink] patientId=%s pref=%s\n", patientUser.c_str(), carelink_patient_pref.c_str());
        esp_rom_printf("[carelink] patient=%s\n", patientUser.c_str());
        body["role"] = "carepartner";
        body["patientId"] = patientUser;
        s_lastFetchedPatientId = patientUser;
    }

    String bodyStr;
    serializeJson(body, bodyStr);

    // 350 KB skôr ako JsonDocument a natrvalo. Inak prvý slot (Emily) v rozbitej
    // PSRAM alokáciu nedostane, ostane na cache, a druhý slot už prejde.
    static char *bodyBuf = nullptr;
    static constexpr size_t kBodyCap = 350 * 1024;
    if (!bodyBuf) {
        bodyBuf = (char *)heap_caps_malloc(kBodyCap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!bodyBuf) {
            vTaskDelay(pdMS_TO_TICKS(300));
            bodyBuf = (char *)heap_caps_malloc(kBodyCap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
    }
    if (!bodyBuf) {
        esp_rom_printf("[carelink] no PSRAM for body\n");
        return false;
    }

    // lastSG + sgs.sg (filter drží doc malý; parse ide cez DiaYieldBuf kvôli WDT)
    JsonDocument filter(&s_clPsramAlloc);
    filter["patientData"]["lastSG"] = true;
    filter["patientData"]["lastSGTrend"] = true;
    filter["patientData"]["sgs"][0]["sg"] = true;
    filter["lastSG"] = true;
    filter["lastSGTrend"] = true;
    filter["sgs"][0]["sg"] = true;

    DeserializationError err = DeserializationError::Ok;
    JsonDocument doc(&s_clPsramAlloc);
    bool got = false;
    for (int attempt = 0; attempt < 2 && !got; attempt++) {
        if (attempt > 0) {
            esp_rom_printf("[carelink] data retry %d\n", attempt);
            vTaskDelay(pdMS_TO_TICKS(600));
        }
        WiFiClientSecure client;
        configureTlsClient(client, true);
        HTTPClient http;
        http.setTimeout(20000);
        http.setReuse(false);
        if (!http.begin(client, url)) {
            esp_rom_printf("[carelink] data begin fail\n");
            continue;
        }
        addClHeaders(http, true);
        code = http.POST(bodyStr);
        if (code == 401 || code == 403) {
            const bool html = carelinkBodyIsHtml(carelinkReadSnippet(http));
            http.end();
            if (html) {
                esp_rom_printf("[carelink] data HTTP %d html\n", code);
                return false;
            }
            authFail = true;
            esp_rom_printf("[carelink] data HTTP %d (auth)\n", code);
            return false;
        }
        // 206 = Upgrade application (chýba / starý appVersion)
        if (code == 206) {
            Serial.println("[carelink] data HTTP 206 – upgrade appVersion");
            esp_rom_printf("[carelink] data HTTP 206\n");
            http.end();
            markFetchError(FetchError::Config);
            return true;
        }
        if (code != HTTP_CODE_OK) {
            Serial.printf("[carelink] data HTTP %d\n", code);
            esp_rom_printf("[carelink] data HTTP %d\n", code);
            http.end();
            continue;
        }
        WiFiClient &stream = http.getStream();
        size_t len = 0;
        const uint32_t t0 = millis();
        while (len < kBodyCap - 1 && (millis() - t0) < 25000UL) {
            int av = stream.available();
            if (av <= 0) {
                if (!http.connected()) break;
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }
            while (av-- > 0 && len < kBodyCap - 1) {
                int ch = stream.read();
                if (ch < 0) break;
                bodyBuf[len++] = (char)ch;
                if ((len & 0x3FF) == 0) vTaskDelay(1);
            }
        }
        bodyBuf[len] = '\0';
        http.end();
        esp_rom_printf("[carelink] body %u B\n", (unsigned)len);
        if (len < 32) continue;
        doc.clear();
        {
            DiaYieldBuf ybuf(bodyBuf, len);
            err = deserializeJson(doc, ybuf, DeserializationOption::Filter(filter));
        }
        vTaskDelay(pdMS_TO_TICKS(20));
        if (!err) {
            got = true;
            break;
        }
        Serial.printf("[carelink] JSON data: %s\n", err.c_str());
        esp_rom_printf("[carelink] JSON %s\n", err.c_str());
        if (err != DeserializationError::IncompleteInput &&
            err != DeserializationError::NoMemory) {
            break;
        }
    }
    if (!got) return false;

    JsonObject pd = doc["patientData"].as<JsonObject>();
    if (pd.isNull()) pd = doc.as<JsonObject>();
    JsonVariant last = pd["lastSG"];
    float sg = 0;
    if (last.is<JsonObject>()) sg = last["sg"] | 0.0f;
    else sg = last | 0.0f;
    if (sg <= 0.0f) {
        Serial.println("[carelink] lastSG prazdne");
        markFetchError(FetchError::Empty);
        return true;
    }

    float mmol = (sg > 30.0f) ? mgdl_to_mmol(sg) : sg;
    const char *trend = pd["lastSGTrend"] | "";
    long ts = last.is<JsonObject>() ? parseCarelinkTime(last) : 0;
    if (ts <= 0) {
        time_t now = time(nullptr);
        ts = (now > 1700000000) ? (long)now : 0;
    }

    // Sparkline z patientData.sgs (oldest→newest); fallback = jeden bod
    JsonArray sgs = pd["sgs"].as<JsonArray>();
    if (sgs.isNull()) sgs = doc["sgs"].as<JsonArray>();
    if (!sgs.isNull() && sgs.size() > 0) {
        const size_t n = sgs.size();
        size_t start = (n > (size_t)DIA_HISTORY_MAX) ? (n - (size_t)DIA_HISTORY_MAX) : 0;
        float histNf[DIA_HISTORY_MAX];
        int histN = 0;
        for (int i = (int)n - 1; i >= (int)start && histN < DIA_HISTORY_MAX; i--) {
            float v = sgs[i]["sg"] | 0.0f;
            if (v <= 0.0f) continue;
            histNf[histN++] = (v > 30.0f) ? mgdl_to_mmol(v) : v;
            if ((histN & 7) == 0) vTaskDelay(1);
        }
        if (histN > 0) {
            glucoseHistorySetNewestFirst(histNf, histN);
            Serial.printf("[carelink] sgs=%u hist=%d\n", (unsigned)n, histN);
            esp_rom_printf("[carelink] hist=%d\n", histN);
        } else {
            glucoseHistoryPush(mmol);
        }
    } else {
        glucoseHistoryPush(mmol);
    }

    Serial.printf("[carelink] sg=%.0f mmol=%.1f trend=%s\n", sg, mmol, trend);
    applyGlucoseReading(mmol, getTrendSymbol(carelinkTrendToDirection(trend)), ts);
    return true;
}

bool carelinkFetchPatientList(int slot, String &listOut, String &errOut) {
    listOut = "";
    errOut = "";
    carelinkBindSlot(slot);
    if (!carelinkTokensReady()) {
        errOut = "Najprv ulož CareLink tokeny.";
        return false;
    }
    long exp = 0;
    if (!decodeAccessClaims(exp)) {
        errOut = "Access token nie je platný JWT.";
        return false;
    }
    if (!discoverEndpoints()) {
        errOut = "Discovery / endpointy zlyhali.";
        return false;
    }
    JsonDocument me;
    int code = 0;
    if (!clGetJson(clBaseCare + "/users/me", me, code) && (code == 401 || code == 403)) {
        ClRefreshResult rr = refreshAccessToken();
        if (rr == ClRefreshResult::Ok) {
            decodeAccessClaims(exp);
            code = 0;
            me.clear();
            clGetJson(clBaseCare + "/users/me", me, code);
        } else if (rr == ClRefreshResult::Invalid || rr == ClRefreshResult::Held) {
            errOut = "Obnova tokenu zlyhala. Prihlás sa znova.";
            return false;
        }
    }
    if (code != HTTP_CODE_OK) {
        errOut = "users/me HTTP " + String(code);
        return false;
    }
    const char *role = me["role"] | "";
    String roleStr = role;
    roleStr.toUpperCase();
    if (roleStr.indexOf("CARE_PARTNER") < 0) {
        errOut = "Účet nie je Care Partner (role=" + String(role) + ").";
        return false;
    }

    JsonDocument patients;
    if (!clGetJson(clBaseCare + "/links/patients", patients, code)) {
        errOut = "links/patients HTTP " + String(code);
        return false;
    }
    if (!patients.is<JsonArray>()) {
        errOut = "Odpoveď pacientov nie je zoznam.";
        return false;
    }
    for (JsonObject p : patients.as<JsonArray>()) {
        String u = p["username"] | "";
        String fn = p["firstName"] | "";
        String ln = p["lastName"] | "";
        String display = fn;
        if (ln.length()) {
            if (display.length()) display += " ";
            display += ln;
        }
        if (display.length() == 0) display = u;
        if (u.length() == 0) continue;
        listOut += u;
        listOut += '\t';
        listOut += display;
        listOut += '\n';
    }
    if (listOut.length() == 0) {
        errOut = "Care Partner nemá žiadneho prepojeného pacienta.";
        return false;
    }
    carelink_patient_list = listOut;
    auto saveListSlot = [&](int s) {
        if (s < 0 || s >= DIA_PROFILE_COUNT) return;
        // Len Guardian – inak by sa CareLink zoznam ohnal do Dexcom/Libre slotu
        if (!profiles[s].dsSource.equalsIgnoreCase("guardian") && s != slot) return;
        profiles[s].carelink_patient_list = listOut;
        char key[12];
        snprintf(key, sizeof(key), "p%d_cpl", s);
        preferences.putString(key, listOut);
    };
    saveListSlot(slot);
    // 1 konto: zdieľaný zoznam len na druhý slot ak je tiež Guardian
    if (slot == 0 && !carelinkSlotHasOwnTokens(1) && profiles[1].dsSource.equalsIgnoreCase("guardian"))
        saveListSlot(1);
    if (slot == 1 && !carelinkSlotHasOwnTokens(1) && profiles[0].dsSource.equalsIgnoreCase("guardian"))
        saveListSlot(0);
    if (slot == 0) preferences.putString("cl_pats", listOut);
    return true;
}

/** Access JWT ešte použiteľný (NTP OK, >60 s do exp). */
static bool carelinkAccessUsable(long exp) {
    const time_t now = time(nullptr);
    if (now < 1600000000L || exp <= 0) return false;
    return (exp - (long)now) > 60L;
}

void fetchGuardian() {
    // Dual CareLink: A nastaví s_lastFetchedPatientId, B bez pref vezme „iného“.
    // A=Nightscout/Libre: fetchGuardian(0) nebeží → ID sa nevyčistí a B strieda 2 pacientov.
    if (dia_active_slot == 0) {
        s_lastFetchedPatientId = "";
    } else if (dia_active_slot == 1 &&
               !profiles[0].dsSource.equalsIgnoreCase("guardian")) {
        s_lastFetchedPatientId = "";
    }
    esp_rom_printf("[carelink] fetch slot=%d heap=%u psram=%u\n",
                   dia_active_slot, (unsigned)ESP.getFreeHeap(),
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    carelinkBindSlot(dia_active_slot);
    if (!carelinkTokensReady()) {
        esp_rom_printf("[carelink] Config: no tokens\n");
        markFetchError(FetchError::Config);
        return;
    }
    // Najprv discovery (+ NVS cache token URL) – refresh pred tým padal na prázdne clTokenUrl → falošný Auth
    if (!discoverEndpoints()) {
        esp_rom_printf("[carelink] Network: discover fail\n");
        markFetchError(FetchError::Network);
        return;
    }
    long exp = 0;
    if (!decodeAccessClaims(exp)) {
        const int slot = (clBoundSlot <= 0) ? 0 : clBoundSlot;
        carelinkAdoptJournal(slot);
        carelinkLoadTokensFromNvs(slot);
        if (!decodeAccessClaims(exp)) {
            // Poškodený access JWT – skús refresh, nie hneď Auth
            esp_rom_printf("[carelink] bad JWT → refresh\n");
            ClRefreshResult rr0 = refreshAccessToken();
            if (rr0 == ClRefreshResult::Ok && decodeAccessClaims(exp)) {
                // pokračuj nižšie
            } else if (rr0 == ClRefreshResult::Invalid || rr0 == ClRefreshResult::Held) {
                esp_rom_printf("[carelink] Auth: bad JWT + refresh dead\n");
                markFetchError(FetchError::Auth);
                return;
            } else {
                esp_rom_printf("[carelink] Network: bad JWT refresh pending\n");
                markFetchError(FetchError::Network);
                return;
            }
        }
    }
    // Proaktívna obnova pred expiráciou access tokenu (Auth0 ~1 h)
    {
        const time_t now = time(nullptr);
        if (exp > 0 && now > 1600000000L && (exp - (long)now) < 300L) {
            ClRefreshResult pre = refreshAccessToken();
            if (pre == ClRefreshResult::Ok) decodeAccessClaims(exp);
            else if ((pre == ClRefreshResult::Invalid || pre == ClRefreshResult::Held) &&
                     !carelinkAccessUsable(exp)) {
                // Refresh mŕtvy a access už neplatí → treba /carelink
                esp_rom_printf("[carelink] Auth: proactive refresh dead\n");
                markFetchError(FetchError::Auth);
                return;
            }
            // Held/Invalid ale access ešte žije → pokračuj bez Auth
            else if (pre == ClRefreshResult::Invalid || pre == ClRefreshResult::Held) {
                esp_rom_printf("[carelink] proactive held, access este OK\n");
            } else if (pre == ClRefreshResult::Transient && !carelinkAccessUsable(exp)) {
                esp_rom_printf("[carelink] Network: proactive refresh pending\n");
                markFetchError(FetchError::Network);
                return;
            }
        }
    }

    bool authFail = false;
    if (readGlucose(authFail)) {
        esp_rom_printf("[carelink] OK heap=%u\n", (unsigned)ESP.getFreeHeap());
        return;
    }
    if (!authFail) {
        esp_rom_printf("[carelink] Network: read fail\n");
        markFetchError(FetchError::Network);
        return;
    }
    // 401 v inej izbe býva filter alebo ešte nehotová cesta von, nie odhlásenie.
    // Kým cloud neodpovie, refresh token sa neposiela a nezamyká.
    carelinkForgetEndpoints();
    if (!discoverEndpoints()) {
        esp_rom_printf("[carelink] 401 bez cloudu\n");
        markFetchError(FetchError::Network);
        return;
    }
    ClRefreshResult rr = refreshAccessToken();
    if (rr == ClRefreshResult::Ok) {
        decodeAccessClaims(exp);
        authFail = false;
        delay(150);
        if (readGlucose(authFail)) {
            esp_rom_printf("[carelink] OK after refresh heap=%u\n", (unsigned)ESP.getFreeHeap());
            return;
        }
        // Refresh „Ok“ ale API 401: 45s skip by zablokoval okamžitý retry – uvoľni a skús raz
        if (authFail) {
            s_lastRefreshOkMs = 0;
            esp_rom_printf("[carelink] 401 after refresh → retry once\n");
            rr = refreshAccessToken();
            if (rr == ClRefreshResult::Ok) {
                decodeAccessClaims(exp);
                authFail = false;
                delay(150);
                if (readGlucose(authFail)) {
                    esp_rom_printf("[carelink] OK after 2nd refresh heap=%u\n",
                                   (unsigned)ESP.getFreeHeap());
                    return;
                }
            }
        }
        esp_rom_printf("[carelink] fail auth=%d after refresh\n", (int)authFail);
        markFetchError(FetchError::Network);
        return;
    }
    if (rr == ClRefreshResult::Invalid) {
        esp_rom_printf("[carelink] Auth: refresh invalid\n");
        markFetchError(FetchError::Auth);
        return;
    }
    // Transient: sieť – skúšame znova. Held: refresh bol spotrebovaný, bez nového
    // tokenu v journali už nePOSTujeme → Auth (treba /carelink), nie slepý retry.
    if (rr == ClRefreshResult::Held) {
        esp_rom_printf("[carelink] Auth: refresh held (re-login)\n");
        markFetchError(FetchError::Auth);
        return;
    }
    esp_rom_printf("[carelink] Network: refresh transient\n");
    markFetchError(FetchError::Network);
}
