#include "dia_secret.h"
#include "dia_config.h"

#include <esp_mac.h>
#include <esp_random.h>
#include <esp_rom_sys.h>
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>
#include <string.h>

static bool s_began = false;
static bool s_ready = false;
static bool s_sawPlain = false;
static uint8_t s_key[16];

static char s_failKey[16][16];
static int s_failN = 0;

static bool deriveKey() {
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) return false;
    static const char kLabel[] = "GlukoView.nvs.v1";
    uint8_t material[sizeof(kLabel) - 1 + 6];
    memcpy(material, kLabel, sizeof(kLabel) - 1);
    memcpy(material + sizeof(kLabel) - 1, mac, 6);
    uint8_t hash[32];
    if (mbedtls_sha256_ret(material, sizeof(material), hash, 0) != 0) return false;
    memcpy(s_key, hash, sizeof(s_key));
    return true;
}

bool diaSecretIsSealed(const String &stored) {
    return stored.startsWith("gv1.");
}

static void markFailed(const char *key) {
    if (!key || s_failN >= 16) return;
    for (int i = 0; i < s_failN; i++) {
        if (strcmp(s_failKey[i], key) == 0) return;
    }
    strncpy(s_failKey[s_failN], key, sizeof(s_failKey[0]) - 1);
    s_failKey[s_failN][sizeof(s_failKey[0]) - 1] = '\0';
    s_failN++;
}

static bool keyFailed(const char *key) {
    if (!key) return false;
    for (int i = 0; i < s_failN; i++) {
        if (strcmp(s_failKey[i], key) == 0) return true;
    }
    return false;
}

static String sealWithKey(const String &plain) {
    if (plain.length() == 0 || plain.length() > 6000) return "";
    const size_t n = plain.length();
    const size_t rawLen = 12 + n + 16;
    uint8_t *raw = (uint8_t *)malloc(rawLen);
    if (!raw) return "";
    uint8_t *nonce = raw;
    uint8_t *cipher = raw + 12;
    uint8_t *tag = raw + 12 + n;
    esp_fill_random(nonce, 12);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    String out;
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_key, 128);
    if (rc == 0) {
        rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, n, nonce, 12,
                                       (const unsigned char *)"gv1", 3,
                                       (const unsigned char *)plain.c_str(), cipher, 16, tag);
    }
    mbedtls_gcm_free(&gcm);
    if (rc != 0) {
        free(raw);
        return "";
    }

    const size_t b64Need = 4 * ((rawLen + 2) / 3) + 1;
    if (b64Need > 9000) {
        free(raw);
        return "";
    }
    uint8_t *b64 = (uint8_t *)malloc(b64Need);
    if (!b64) {
        free(raw);
        return "";
    }
    size_t written = 0;
    rc = mbedtls_base64_encode(b64, b64Need, &written, raw, rawLen);
    free(raw);
    if (rc != 0 || written == 0) {
        free(b64);
        return "";
    }
    b64[written] = 0;
    out.reserve(4 + written);
    out = "gv1.";
    out += (const char *)b64;
    free(b64);
    return out;
}

String diaSecretSeal(const String &plain) {
    if (plain.length() == 0) return "";
    if (!s_ready) return plain;
    String sealed = sealWithKey(plain);
    if (!diaSecretIsSealed(sealed)) return "";
    return sealed;
}

String diaSecretOpen(const String &stored, bool *failed) {
    if (failed) *failed = false;
    if (stored.length() == 0) return "";
    if (!diaSecretIsSealed(stored)) return stored;
    if (!s_ready) {
        if (failed) *failed = true;
        return "";
    }

    const char *b64 = stored.c_str() + 4;
    const size_t b64Len = stored.length() - 4;
    if (b64Len < 16 || (b64Len % 4) != 0) {
        if (failed) *failed = true;
        return "";
    }
    const size_t rawCap = (b64Len / 4) * 3;
    if (rawCap < 12 + 16 || rawCap > 7000) {
        if (failed) *failed = true;
        return "";
    }
    uint8_t *raw = (uint8_t *)malloc(rawCap);
    if (!raw) {
        if (failed) *failed = true;
        return "";
    }
    size_t rawLen = 0;
    int rc = mbedtls_base64_decode(raw, rawCap, &rawLen, (const unsigned char *)b64, b64Len);
    if (rc != 0 || rawLen < 12 + 16) {
        free(raw);
        if (failed) *failed = true;
        return "";
    }
    const size_t n = rawLen - 12 - 16;
    uint8_t *plain = (uint8_t *)malloc(n + 1);
    if (!plain) {
        free(raw);
        if (failed) *failed = true;
        return "";
    }
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_key, 128);
    if (rc == 0) {
        rc = mbedtls_gcm_auth_decrypt(&gcm, n, raw, 12, (const unsigned char *)"gv1", 3,
                                      raw + 12 + n, 16, raw + 12, plain);
    }
    mbedtls_gcm_free(&gcm);
    free(raw);
    if (rc != 0) {
        free(plain);
        if (failed) *failed = true;
        return "";
    }
    plain[n] = 0;
    String out = (const char *)plain;
    free(plain);
    return out;
}

bool diaSecretBegin() {
    if (s_began) return s_ready;
    s_began = true;
    if (!deriveKey()) {
        esp_rom_printf("[secret] key fail\n");
        return false;
    }
    s_ready = true;
    String sealed = diaSecretSeal("glukoview-selftest");
    bool failed = false;
    String opened = diaSecretOpen(sealed, &failed);
    bool tamperCaught = false;
    if (!failed && opened == "glukoview-selftest" && sealed.length() > 8) {
        String bad = sealed;
        const int at = bad.length() - 1;
        bad.setCharAt(at, bad.charAt(at) == 'A' ? 'B' : 'A');
        bool badFail = false;
        diaSecretOpen(bad, &badFail);
        tamperCaught = badFail;
    }
    s_ready = !failed && opened == "glukoview-selftest" && tamperCaught;
    if (!s_ready) esp_rom_printf("[secret] selftest FAIL\n");
    else esp_rom_printf("[secret] selftest OK\n");
    return s_ready;
}

bool diaSecretActive() { return s_ready; }

bool diaSecretTakePlaintext() {
    const bool v = s_sawPlain;
    s_sawPlain = false;
    return v;
}

String diaNvsGetSecret(const char *key) {
    if (!key || !preferences.isKey(key)) return "";
    String raw = preferences.getString(key, "");
    if (raw.length() == 0) return "";
    if (s_ready && !diaSecretIsSealed(raw)) s_sawPlain = true;
    bool failed = false;
    String plain = diaSecretOpen(raw, &failed);
    if (failed) markFailed(key);
    return plain;
}

void diaNvsPutSecret(const char *key, const String &plain) {
    if (!key) return;
    if (plain.length() == 0 && keyFailed(key)) return;
    if (!s_ready) {
        preferences.putString(key, plain);
        return;
    }
    if (plain.length() == 0) {
        preferences.putString(key, "");
        return;
    }
    String sealed = diaSecretSeal(plain);
    if (!diaSecretIsSealed(sealed)) {
        esp_rom_printf("[secret] seal fail\n");
        return;
    }
    preferences.putString(key, sealed);
}
