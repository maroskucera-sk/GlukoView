#include "dia_carelink_store.h"
#include "dia_config.h"
#include "dia_secret.h"

#include <nvs.h>
#include <string.h>

void carelinkNvsLogStats(const char *why) {
    nvs_stats_t st{};
    if (nvs_get_stats(NULL, &st) != ESP_OK) return;
    esp_rom_printf("[carelink] NVS %s used=%u free=%u ns=%u\n", why ? why : "?",
                   (unsigned)st.used_entries, (unsigned)st.free_entries,
                   (unsigned)st.namespace_count);
}

void carelinkNvsClearPrefix(const char *prefix, int chunkCount) {
    for (int i = 0; i < chunkCount; i++) {
        char key[8];
        snprintf(key, sizeof(key), "%s%d", prefix, i);
        if (preferences.isKey(key)) preferences.remove(key);
    }
}

void carelinkNvsFreeJournals(int chunkCount) {
    for (int s = 0; s <= 1; s++) {
        for (int j = 0; j < chunkCount; j++) {
            char jk[8];
            snprintf(jk, sizeof(jk), "c%dq%d", s, j);
            if (preferences.isKey(jk)) preferences.remove(jk);
        }
        char lenKey[12];
        snprintf(lenKey, sizeof(lenKey), "c%dqlen", s);
        if (preferences.isKey(lenKey)) preferences.remove(lenKey);
    }
}

bool carelinkNvsPutChunks(const char *prefix, const String &val, bool seal) {
    String stored = val;
    if (val.length() > 0 && seal && diaSecretActive()) {
        String sealed = diaSecretSeal(val);
        if (diaSecretIsSealed(sealed)) stored = sealed;
        else {
            esp_rom_printf("[carelink] secret seal fail, ukladam plaintext\n");
            stored = val;
        }
    }
    constexpr int kChunks = 6;
    carelinkNvsClearPrefix(prefix, kChunks);
    for (int i = 0; i < kChunks; i++) {
        char key[8];
        snprintf(key, sizeof(key), "%s%d", prefix, i);
        int start = i * 1500;
        if (start >= (int)stored.length()) break;
        String part = stored.substring(start, start + 1500);
        size_t wrote = preferences.putString(key, part);
        if (wrote != part.length()) {
            carelinkNvsFreeJournals(kChunks);
            carelinkNvsLogStats("before-retry");
            wrote = preferences.putString(key, part);
            if (wrote != part.length()) {
                carelinkNvsLogStats("write-fail");
                esp_rom_printf("[carelink] NVS write fail %s need=%u\n", key, (unsigned)part.length());
                return false;
            }
        }
    }
    return true;
}

String carelinkNvsGetChunks(const char *prefix, bool *plainOut, bool *failOut) {
    if (plainOut) *plainOut = false;
    if (failOut) *failOut = false;
    String out;
    constexpr int kChunks = 6;
    for (int i = 0; i < kChunks; i++) {
        char key[8];
        snprintf(key, sizeof(key), "%s%d", prefix, i);
        if (!preferences.isKey(key)) break;
        String part = preferences.getString(key, "");
        if (part.length() == 0) break;
        out += part;
    }
    if (out.length() == 0) return out;
    if (diaSecretActive() && !diaSecretIsSealed(out)) {
        if (plainOut) *plainOut = true;
    }
    bool failed = false;
    String plain = diaSecretOpen(out, &failed);
    if (failed) {
        if (failOut) *failOut = true;
        return "";
    }
    return plain;
}
