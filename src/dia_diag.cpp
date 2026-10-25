#include "dia_diag.h"

#include "dia_board.h"
#include "dia_config.h"
#include "dia_fetch.h"
#include "dia_glucose.h"
#include "dia_version.h"

#include <WiFi.h>
#include <esp_system.h>
#include <time.h>

static const int kDiagN = 32;
static const char *kSnapKey = "diag_snap";
static const char *kLeadKey = "fetch_lead";

struct DiaDiagEv {
    uint32_t ms;
    int8_t slot;
    uint8_t step;
    int16_t code;
};

static DiaDiagEv s_ev[kDiagN];
static uint8_t s_i = 0;
static uint8_t s_count = 0;
static portMUX_TYPE s_diagMux = portMUX_INITIALIZER_UNLOCKED;

static const char *diagStepName(uint8_t step) {
    switch (static_cast<DiaDiagStep>(step)) {
    case DiaDiagStep::Boot: return "boot";
    case DiaDiagStep::Begin: return "slot";
    case DiaDiagStep::Login: return "login";
    case DiaDiagStep::Connections: return "connections";
    case DiaDiagStep::Graph: return "graph";
    case DiaDiagStep::Discover: return "discover";
    case DiaDiagStep::Refresh: return "refresh";
    case DiaDiagStep::Read: return "read";
    case DiaDiagStep::Result: return "result";
    case DiaDiagStep::Slow: return "slow";
    }
    return "?";
}

static const char *diagErrName(int code) {
    switch (static_cast<FetchError>(code)) {
    case FetchError::None: return "ok";
    case FetchError::Wifi: return "wifi";
    case FetchError::Auth: return "auth";
    case FetchError::Network: return "net";
    case FetchError::Empty: return "empty";
    case FetchError::Config: return "config";
    }
    return "?";
}

static const char *diagResetName(esp_reset_reason_t reason) {
    switch (reason) {
    case ESP_RST_POWERON: return "power";
    case ESP_RST_SW: return "restart";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "other";
    }
}

static void diagPush(int slot, DiaDiagStep step, int16_t code) {
    if (slot < -1) slot = -1;
    if (slot > 1) slot = 1;
    portENTER_CRITICAL(&s_diagMux);
    s_ev[s_i].ms = millis();
    s_ev[s_i].slot = (int8_t)slot;
    s_ev[s_i].step = (uint8_t)step;
    s_ev[s_i].code = code;
    s_i = (uint8_t)((s_i + 1) % kDiagN);
    if (s_count < kDiagN) s_count++;
    portEXIT_CRITICAL(&s_diagMux);
}

static void diagSaveSnap() {
    DiaDiagEv copy[8];
    int n = 0;
    portENTER_CRITICAL(&s_diagMux);
    int have = s_count < 8 ? s_count : 8;
    for (int k = have; k > 0; k--) {
        int idx = (int)s_i - k;
        while (idx < 0) idx += kDiagN;
        copy[n++] = s_ev[idx];
    }
    portEXIT_CRITICAL(&s_diagMux);

    String snap;
    snap.reserve(180);
    for (int i = 0; i < n; i++) {
        if (snap.length() > 140) break;
        if (snap.length()) snap += ';';
        snap += String((unsigned long)(copy[i].ms / 1000));
        snap += "s";
        snap += (copy[i].slot < 0) ? "-" : String((int)copy[i].slot);
        snap += " ";
        snap += diagStepName(copy[i].step);
        if (copy[i].step == (uint8_t)DiaDiagStep::Result) {
            snap += "=";
            snap += diagErrName(copy[i].code);
        } else if (copy[i].step == (uint8_t)DiaDiagStep::Slow) {
            snap += "=";
            snap += String((int)copy[i].code);
            snap += "s";
        }
    }
    preferences.putString(kSnapKey, snap);
}

void diaDiagBoot() {
    diagPush(-1, DiaDiagStep::Boot, (int16_t)esp_reset_reason());
}

void diaDiagStep(int slot, DiaDiagStep step) {
    diagPush(slot, step, 0);
}

void diaDiagResult(int slot, int errCode) {
    diagPush(slot, DiaDiagStep::Result, (int16_t)errCode);
}

void diaDiagSlow(int slot, uint32_t busyMs) {
    const int sec = (int)(busyMs / 1000UL);
    diagPush(slot, DiaDiagStep::Slow, (int16_t)(sec > 32000 ? 32000 : sec));
    int other = -1;
    for (int s = 0; s < DIA_PROFILE_COUNT; s++) {
        if (s != slot && diaProfileActive(s)) other = s;
    }
    if (other >= 0) preferences.putUChar(kLeadKey, (uint8_t)other);
    diagSaveSnap();
}

int diaDiagTakeLead() {
    if (!preferences.isKey(kLeadKey)) return -1;
    int lead = preferences.getUChar(kLeadKey, 0);
    preferences.remove(kLeadKey);
    if (lead != 0 && lead != 1) return -1;
    return lead;
}

void diaDiagFormat(String &out) {
    out.reserve(2200);
    out += "GlukoView ";
    out += diaBoardIsRound() ? "solo" : "duo";
    out += " ";
    out += String(FIRMWARE_VERSION_MAJOR);
    out += ".";
    out += String(FIRMWARE_VERSION_MINOR);
    out += " uptime_s=";
    out += String((unsigned long)(millis() / 1000UL));
    out += " reset=";
    out += diagResetName(esp_reset_reason());
    out += "\n";

    if (WiFi.status() == WL_CONNECTED) {
        out += "wifi=up rssi=";
        out += String(WiFi.RSSI());
        out += "\n";
    } else {
        out += "wifi=down\n";
    }
    out += "heap=";
    out += String((unsigned)ESP.getFreeHeap());
    out += " busy=";
    out += diaFetchBusy() ? "1" : "0";
    out += " slot=";
    out += String(diaFetchCurrentSlot());
    out += "\n";

    const time_t now = time(nullptr);
    for (int slot = 0; slot < DIA_PROFILE_COUNT; slot++) {
        if (diaBoardIsRound() && slot > 0) break;
        if (!diaProfileActive(slot) && profiles[slot].dsSource.length() == 0) continue;
        const DiaSlotReading &sr = slotReadings[slot];
        out += "slot";
        out += String(slot);
        out += " src=";
        out += profiles[slot].dsSource.length() ? profiles[slot].dsSource : String("-");
        out += " active=";
        out += diaProfileActive(slot) ? "1" : "0";
        out += " result=";
        out += diagErrName((int)sr.err);
        out += " has=";
        out += sr.has_valid_reading ? "1" : "0";
        if (sr.timestamp > 0 && now > 1600000000L) {
            long age = (long)now - sr.timestamp;
            if (age < 0) age = 0;
            out += " age_s=";
            out += String(age);
        }
        out += "\n";
    }

    if (preferences.isKey(kSnapKey)) {
        out += "saved=";
        out += preferences.getString(kSnapKey, "");
        out += "\n";
    }

    DiaDiagEv copy[kDiagN];
    int n = 0;
    portENTER_CRITICAL(&s_diagMux);
    n = s_count;
    for (int k = n; k > 0; k--) {
        int idx = (int)s_i - k;
        while (idx < 0) idx += kDiagN;
        copy[n - k] = s_ev[idx];
    }
    portEXIT_CRITICAL(&s_diagMux);

    for (int i = 0; i < n; i++) {
        out += "t=";
        out += String((unsigned long)(copy[i].ms / 1000UL));
        out += " slot=";
        out += (copy[i].slot < 0) ? "-" : String((int)copy[i].slot);
        out += " ";
        out += diagStepName(copy[i].step);
        if (copy[i].step == (uint8_t)DiaDiagStep::Result) {
            out += " ";
            out += diagErrName(copy[i].code);
        } else if (copy[i].step == (uint8_t)DiaDiagStep::Slow) {
            out += " ";
            out += String((int)copy[i].code);
            out += "s";
        } else if (copy[i].step == (uint8_t)DiaDiagStep::Boot) {
            out += " ";
            out += diagResetName((esp_reset_reason_t)copy[i].code);
        }
        out += "\n";
    }
}
