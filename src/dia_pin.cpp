#include "dia_pin.h"
#include "dia_config.h"

#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <string.h>

static char s_token[33];
static bool s_hasToken = false;
static int s_fails = 0;
static uint32_t s_lockUntil = 0;

static String diaPinSalt() {
    String salt = preferences.isKey("pin_salt") ? preferences.getString("pin_salt", "") : "";
    if (salt.length() >= 16) return salt;
    char buf[17];
    for (int i = 0; i < 16; i++) buf[i] = "0123456789abcdef"[esp_random() & 15];
    buf[16] = '\0';
    salt = buf;
    preferences.putString("pin_salt", salt);
    return salt;
}

static String diaPinHash(const char *pin) {
    String msg = diaPinSalt();
    msg += pin;
    uint8_t out[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, (const uint8_t *)msg.c_str(), msg.length());
    mbedtls_sha256_finish_ret(&ctx, out);
    mbedtls_sha256_free(&ctx);
    char hex[65];
    for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", out[i]);
    hex[64] = '\0';
    return String(hex);
}

static void diaPinDropSession() {
    s_hasToken = false;
    s_token[0] = '\0';
    s_fails = 0;
    s_lockUntil = 0;
}

bool diaPinIsSet() {
    if (!preferences.isKey("pin_hash")) return false;
    return preferences.getString("pin_hash", "").length() == 64;
}

bool diaPinSet(const char *pin) {
    if (!pin || strlen(pin) != 4) return false;
    for (int i = 0; i < 4; i++) {
        if (pin[i] < '0' || pin[i] > '9') return false;
    }
    String hex = diaPinHash(pin);
    preferences.putString("pin_hash", hex);
    diaPinDropSession();
    return true;
}

void diaPinClear() {
    preferences.remove("pin_hash");
    preferences.remove("pin_salt");
    diaPinDropSession();
}

bool diaPinLockedOut() {
    return (int32_t)(s_lockUntil - millis()) > 0;
}

bool diaPinCheck(const char *pin) {
    if (!diaPinIsSet()) return true;
    if (diaPinLockedOut()) return false;
    if (!pin || strlen(pin) != 4) {
        s_fails++;
    } else {
        String got = diaPinHash(pin);
        String stored = preferences.getString("pin_hash", "");
        bool ok = got.length() == stored.length() &&
                  memcmp(got.c_str(), stored.c_str(), got.length()) == 0;
        if (ok) {
            s_fails = 0;
            for (int i = 0; i < 32; i++) s_token[i] = "0123456789abcdef"[esp_random() & 15];
            s_token[32] = '\0';
            s_hasToken = true;
            return true;
        }
        s_fails++;
    }
    if (s_fails >= 8) {
        s_lockUntil = millis() + 30000;
        s_fails = 0;
    }
    return false;
}

const char *diaPinToken() {
    return s_hasToken ? s_token : "";
}

bool diaPinCookieOk(const String &cookieHeader) {
    if (!diaPinIsSet()) return true;
    if (!s_hasToken || s_token[0] == '\0') return false;
    const int at = cookieHeader.indexOf("gvpin=");
    if (at < 0) return false;
    String v = cookieHeader.substring(at + 6);
    const int sc = v.indexOf(';');
    if (sc >= 0) v = v.substring(0, sc);
    v.trim();
    if (v.length() != 32) return false;
    return memcmp(v.c_str(), s_token, 32) == 0;
}
