#include "dia_ota.h"
#include "dia_board.h"
#include "dia_i18n.h"
#include "dia_config.h"
#include "dia_fetch.h"
#include "dia_ota_pubkey.h"
#include "dia_tls.h"
#include "dia_ui.h"
#include "dia_version.h"

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <time.h>

unsigned long last_ota_check = 0;
const unsigned long otaCheckInterval = 86400000UL; // 24 h medzi kontrolami
const unsigned long otaFirstCheckDelay = 120000UL; // prvá kontrola ~2 min po štarte
bool ota_first_check_done = false;

#if defined(DIA_BOARD_WS_1_28)
static const char *OTA_BOARD_ID = "ws128";
static const char kOtaBoardTag[] = "GVBOARD:ws128";
#else
static const char *OTA_BOARD_ID = "ws35";
static const char kOtaBoardTag[] = "GVBOARD:ws35";
#endif
static const char *OTA_VERSION_HTTPS = "https://glukoview.com/update/"
#if defined(DIA_BOARD_WS_1_28)
    "ws128"
#else
    "ws35"
#endif
    "/version.txt";
static const char *OTA_FIRMWARE_HTTPS = "https://glukoview.com/update/"
#if defined(DIA_BOARD_WS_1_28)
    "ws128"
#else
    "ws35"
#endif
    "/firmware.bin";
static const char *OTA_SIG_HTTPS = "https://glukoview.com/update/"
#if defined(DIA_BOARD_WS_1_28)
    "ws128"
#else
    "ws35"
#endif
    "/firmware.sig";
static const char *s_otaFail = "";

// ota_expect = číslo z version.txt tesne pred reštartom do nového binárneho súboru
// ota_block  = to isté číslo, ak po reštarte firmvér v sebe stále nemá vyššiu verziu
static const char *OTA_KEY_EXPECT = "ota_expect";
static const char *OTA_KEY_BLOCK = "ota_block";

void diaOtaNoteBoot() {
    if (!preferences.isKey(OTA_KEY_EXPECT)) return;
    int expect = preferences.getInt(OTA_KEY_EXPECT, 0);
    preferences.remove(OTA_KEY_EXPECT);
    if (expect > FIRMWARE_VERSION) {
        preferences.putInt(OTA_KEY_BLOCK, expect);
        Serial.printf("[OTA] subor verzie %d nenechal vyssiu verziu (bezi %d) – automatiku zastavujem, kym version.txt nestupne\n",
                      expect, FIRMWARE_VERSION);
    }
}

static int ota_shown_pct = -1;

static void paintOtaNow() {
    lv_obj_invalidate(ota_panel);
    lv_refr_now(NULL);
}

void update_progress_cb(int current, int total) {
    int percent = 0;
    if (total > 0) percent = (current * 100) / total;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (percent == ota_shown_pct) return;
    ota_shown_pct = percent;
    lv_bar_set_value(ota_bar, percent, LV_ANIM_OFF);
    char buf[40];
    snprintf(buf, sizeof(buf), S(Str::OtaPct), percent);
    lv_label_set_text(ota_label, buf);
    paintOtaNow();
}

static void showOtaPanel(const char *msg) {
    ota_shown_pct = -1;
    lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_info_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ota_panel);
    lv_obj_set_style_text_color(ota_label, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_label_set_text(ota_label, msg);
    lv_bar_set_value(ota_bar, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(ota_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ota_panel, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 4; i++) {
        lv_timer_handler();
        delay(15);
    }
    paintOtaNow();
}

static void hideOtaPanelAfterDelay(int frames) {
    for (int i = 0; i < frames; i++) {
        lv_timer_handler();
        delay(10);
    }
    lv_obj_add_flag(ota_panel, LV_OBJ_FLAG_HIDDEN);
}

static void showOtaError(const char *msg) {
    lv_label_set_text(ota_label, msg);
    lv_obj_set_style_text_color(ota_label, lv_color_hex(0xFF0000), LV_PART_MAIN);
    hideOtaPanelAfterDelay(300);
}

/** "2.1" → 201, staré "3" → 300. Číslo >= 100 berie ako už zakódované. */
static int parseOtaVersion(String raw) {
    raw.trim();
    int cut = raw.indexOf('\r');
    if (cut < 0) cut = raw.indexOf('\n');
    if (cut >= 0) raw = raw.substring(0, cut);
    raw.trim();

    int dot = raw.indexOf('.');
    int major = 0;
    int minor = 0;
    if (dot >= 0) {
        major = raw.substring(0, dot).toInt();
        minor = raw.substring(dot + 1).toInt();
    } else {
        int v = raw.toInt();
        if (v >= 100) return v;
        major = v;
    }
    if (major < 0) major = 0;
    if (minor < 0) minor = 0;
    if (minor > 99) minor = 99;
    return major * 100 + minor;
}

/** Stiahne číslo verzie len cez overené HTTPS. HTTP záloha nie je. */
static bool fetchLatestVersion(int &outVer, uint32_t timeoutMs = 15000) {
    outVer = 0;
    WiFiClientSecure secure;
    configureTlsClient(secure, false);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(timeoutMs);
    if (!http.begin(secure, OTA_VERSION_HTTPS)) {
        Serial.println("[OTA] HTTPS version begin failed");
        return false;
    }
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        outVer = parseOtaVersion(http.getString());
        http.end();
        Serial.printf("[OTA] HTTPS %s version=%d (local=%d.%d)\n",
                      OTA_BOARD_ID, outVer, FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR);
        return outVer > 0;
    }
    Serial.printf("[OTA] HTTPS version HTTP %d\n", code);
    http.end();
    return false;
}

int diaOtaServerVersion() {
    if (WiFi.status() != WL_CONNECTED) return 0;
    int v = 0;
    if (!fetchLatestVersion(v, 3000)) return 0;
    return v;
}

static void otaEnsureTime() {
    if (time(nullptr) > 1700000000) return;
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    for (int i = 0; i < 40; i++) {
        delay(250);
        if (time(nullptr) > 1700000000) return;
    }
    Serial.println("[OTA] cas este nie je nastaveny, certifikat sa nemusi podarit overit");
}

static bool otaVerifySig(const uint8_t hash[32], const uint8_t *sig, size_t sigLen) {
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    int rc = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)DIA_OTA_PUBKEY,
                                         sizeof(DIA_OTA_PUBKEY));
    if (rc != 0) {
        Serial.printf("[OTA] verejny kluc %d\n", rc);
        mbedtls_pk_free(&pk);
        return false;
    }
    rc = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, sig, sigLen);
    mbedtls_pk_free(&pk);
    if (rc != 0) Serial.printf("[OTA] podpis nesedi %d\n", rc);
    return rc == 0;
}

static bool otaReadSig(uint8_t *sig, int &sigLen) {
    sigLen = 0;
    WiFiClientSecure secure;
    configureTlsClient(secure, false);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000);
    if (!http.begin(secure, OTA_SIG_HTTPS)) return false;
    int code = http.GET();
    int len = http.getSize();
    if (code != HTTP_CODE_OK || len < 64 || len > 96) {
        Serial.printf("[OTA] podpis HTTP %d len=%d\n", code, len);
        http.end();
        return false;
    }
    WiFiClient *stream = http.getStreamPtr();
    uint32_t t0 = millis();
    while (sigLen < len && millis() - t0 < 15000) {
        int n = stream->read(sig + sigLen, len - sigLen);
        if (n <= 0) {
            delay(10);
            continue;
        }
        sigLen += n;
        t0 = millis();
    }
    http.end();
    return sigLen == len;
}

static void otaNoteTag(uint8_t b, int &matched, bool &saw) {
    if (saw) return;
    const int n = (int)sizeof(kOtaBoardTag) - 1;
    if (b == (uint8_t)kOtaBoardTag[matched]) {
        matched++;
        if (matched == n) saw = true;
        return;
    }
    matched = (b == (uint8_t)kOtaBoardTag[0]) ? 1 : 0;
}

static t_httpUpdate_return runFirmwareUpdate() {
    s_otaFail = "";
    otaEnsureTime();

    uint8_t sig[96];
    int sigLen = 0;
    if (!otaReadSig(sig, sigLen)) {
        s_otaFail = "sig";
        return HTTP_UPDATE_FAILED;
    }

    WiFiClientSecure secure;
    configureTlsClient(secure, false);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(20000);
    if (!http.begin(secure, OTA_FIRMWARE_HTTPS)) {
        s_otaFail = "begin";
        return HTTP_UPDATE_FAILED;
    }
    int code = http.GET();
    int len = http.getSize();
    if (code != HTTP_CODE_OK || len < 100000 || len > 4 * 1024 * 1024) {
        Serial.printf("[OTA] firmware HTTP %d len=%d\n", code, len);
        http.end();
        s_otaFail = "bin";
        return HTTP_UPDATE_FAILED;
    }
    if (!Update.begin((size_t)len)) {
        Serial.printf("[OTA] Update.begin zlyhal: %s\n", Update.errorString());
        http.end();
        s_otaFail = "flash";
        return HTTP_UPDATE_FAILED;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha, 0);
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[1024];
    size_t written = 0;
    int matched = 0;
    bool sawTag = false;
    uint32_t lastByte = millis();
    bool ok = true;
    while (written < (size_t)len) {
        int avail = stream->available();
        if (avail <= 0) {
            if (millis() - lastByte > 20000) {
                ok = false;
                s_otaFail = "timeout";
                break;
            }
            delay(10);
            continue;
        }
        int want = avail;
        if (want > (int)sizeof(buf)) want = (int)sizeof(buf);
        if (want > (int)((size_t)len - written)) want = (int)((size_t)len - written);
        int n = stream->read(buf, want);
        if (n <= 0) {
            delay(10);
            continue;
        }
        lastByte = millis();
        mbedtls_sha256_update_ret(&sha, buf, n);
        for (int i = 0; i < n; i++) otaNoteTag(buf[i], matched, sawTag);
        if (Update.write(buf, n) != (size_t)n) {
            ok = false;
            s_otaFail = "write";
            break;
        }
        written += (size_t)n;
        update_progress_cb((int)written, len);
        delay(1);
    }
    uint8_t hash[32];
    mbedtls_sha256_finish_ret(&sha, hash);
    mbedtls_sha256_free(&sha);
    http.end();

    if (!ok || written != (size_t)len || !sawTag || !otaVerifySig(hash, sig, (size_t)sigLen)) {
        if (ok && written == (size_t)len && !sawTag) s_otaFail = "board";
        else if (ok && written == (size_t)len) s_otaFail = "sig";
        Serial.printf("[OTA] odmietnuté (%s, tag=%d, %u/%d)\n",
                      s_otaFail, (int)sawTag, (unsigned)written, len);
        Update.abort();
        return HTTP_UPDATE_FAILED;
    }
    if (!Update.end(true)) {
        Serial.printf("[OTA] Update.end: %s\n", Update.errorString());
        s_otaFail = "end";
        return HTTP_UPDATE_FAILED;
    }
    Serial.println("[OTA] podpis sedí, reštart");
    delay(200);
    ESP.restart();
    return HTTP_UPDATE_OK;
}

void check_firmware_update(bool manual) {
    if (WiFi.status() != WL_CONNECTED) {
        if (manual) {
            showInfoMessageLayout(S(Str::WifiOff));
            lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_info_panel, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (manual) showOtaPanel(S(Str::OtaCheck));

    int latest_ver = 0;
    if (!fetchLatestVersion(latest_ver)) {
        if (manual) {
            if (ota_panel && !lv_obj_has_flag(ota_panel, LV_OBJ_FLAG_HIDDEN)) {
                showOtaError(S(Str::ServerDown));
            } else {
                showInfoMessageLayout(S(Str::ServerDown));
                lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_info_panel, LV_OBJ_FLAG_HIDDEN);
            }
        }
        return;
    }

    if (latest_ver <= FIRMWARE_VERSION) {
        Serial.println("[OTA] už aktuálne");
        if (manual) {
            lv_obj_add_flag(ota_panel, LV_OBJ_FLAG_HIDDEN);
            showInfoMessageLayout(S(Str::UpToDate));
            lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_info_panel, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    int blocked = preferences.isKey(OTA_KEY_BLOCK) ? preferences.getInt(OTA_KEY_BLOCK, 0) : 0;
    if (!manual && latest_ver <= blocked) {
        Serial.printf("[OTA] verzia %d uz bola nainstalovana a firmver ostal %d – preskakujem\n",
                      latest_ver, FIRMWARE_VERSION);
        return;
    }

    Serial.printf("[OTA] nová verzia %d > %d – sťahujem\n", latest_ver, FIRMWARE_VERSION);
    showOtaPanel(S(Str::OtaFound));
    // Reštart uprostred obnovy CareLink tokenu by nový token stratil.
    // Kým sťahovanie glykémie nedobehne, update sa odloží.
    esp_rom_printf("[OTA] cakam na fetch\n");
    if (!diaFetchQuiesce(40000)) {
        diaFetchResume();
        esp_rom_printf("[OTA] fetch busy, skip\n");
        if (manual) showOtaError(S(Str::OtaFail));
        return;
    }
    preferences.putInt(OTA_KEY_EXPECT, latest_ver);

    t_httpUpdate_return ret = runFirmwareUpdate();
    if (ret != HTTP_UPDATE_OK) {
        preferences.remove(OTA_KEY_EXPECT);
        diaFetchResume();
    }

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] FAILED: %s\n", s_otaFail);
            showOtaError(S(Str::OtaFail));
            break;
        case HTTP_UPDATE_NO_UPDATES:
            lv_obj_add_flag(ota_panel, LV_OBJ_FLAG_HIDDEN);
            if (manual) {
                showInfoMessageLayout(S(Str::UpToDate));
                lv_obj_clear_flag(ui_info_panel, LV_OBJ_FLAG_HIDDEN);
            }
            break;
        case HTTP_UPDATE_OK:
            // reštart (rebootOnUpdate)
            break;
    }
}
