#include "dia_glucose.h"
#include "dia_config.h"
#include "dia_fetch.h"
#include "dia_i18n.h"
#include <math.h>
#include <string.h>
#include <time.h>

bool is_critical = false;
bool data_is_stale = false;
bool has_valid_reading = false;
bool has_delta = false;
uint32_t current_arc_color = 0x00FF00;
float last_mmol_value = 0.0;
float last_delta_mmol = 0.0;
String last_trend_symbol = "-";
FetchError last_fetch_error = FetchError::None;
long last_reading_timestamp = 0;

float glucose_history[DIA_HISTORY_MAX] = {0};
int glucose_history_count = 0;

DiaSlotReading slotReadings[2];

void glucoseSyncGlobalsFromSlot(int slot) {
    if (slot < 0 || slot > 1) slot = 0;
    const DiaSlotReading &s = slotReadings[slot];
    is_critical = s.is_critical;
    data_is_stale = s.data_is_stale;
    has_valid_reading = s.has_valid_reading;
    has_delta = s.has_delta;
    current_arc_color = s.arc_color;
    last_mmol_value = s.mmol;
    last_delta_mmol = s.delta_mmol;
    last_trend_symbol = s.trend;
    last_fetch_error = s.err;
    last_reading_timestamp = s.timestamp;
    glucose_history_count = s.history_count;
    memcpy(glucose_history, s.history, sizeof(glucose_history));
}

static void glucoseSyncSlotFromGlobals(int slot) {
    if (slot < 0 || slot > 1) return;
    DiaSlotReading &s = slotReadings[slot];
    s.is_critical = is_critical;
    s.data_is_stale = data_is_stale;
    s.has_valid_reading = has_valid_reading;
    s.has_delta = has_delta;
    s.arc_color = current_arc_color;
    s.mmol = last_mmol_value;
    s.delta_mmol = last_delta_mmol;
    s.trend = last_trend_symbol;
    s.err = last_fetch_error;
    s.timestamp = last_reading_timestamp;
    s.history_count = glucose_history_count;
    memcpy(s.history, glucose_history, sizeof(s.history));
}

void glucoseHistoryClear() {
    glucose_history_count = 0;
    memset(glucose_history, 0, sizeof(glucose_history));
}

void glucoseHistorySetNewestFirst(const float *mmolNewestFirst, int count) {
    if (!mmolNewestFirst || count <= 0) {
        glucoseHistoryClear();
        return;
    }
    if (count > DIA_HISTORY_MAX) count = DIA_HISTORY_MAX;
    // API: [0]=newest → buffer: [0]=oldest
    for (int i = 0; i < count; i++) {
        glucose_history[i] = mmolNewestFirst[count - 1 - i];
    }
    glucose_history_count = count;
}

void glucoseHistoryPush(float mmol) {
    if (mmol <= 0.0f) return;
    if (glucose_history_count < DIA_HISTORY_MAX) {
        glucose_history[glucose_history_count++] = mmol;
        return;
    }
    memmove(glucose_history, glucose_history + 1, (DIA_HISTORY_MAX - 1) * sizeof(float));
    glucose_history[DIA_HISTORY_MAX - 1] = mmol;
}

float mmol_to_mgdl(float mmol) { return mmol * MGDL_FACTOR; }
float mgdl_to_mmol(float mgdl) { return mgdl / MGDL_FACTOR; }

void formatGlucose(float mmol, char *buf, size_t n) {
    if (use_mgdl) snprintf(buf, n, "%d", (int)lroundf(mmol_to_mgdl(mmol)));
    else snprintf(buf, n, "%.1f", mmol);
}

void formatTargetLabel(const char *prefix, float mmol, char *buf, size_t n) {
    if (use_mgdl) snprintf(buf, n, "%s: %d", prefix, (int)lroundf(mmol_to_mgdl(mmol)));
    else snprintf(buf, n, "%s: %.1f", prefix, mmol);
}

String getTrendSymbol(String direction) {
    if (direction == "DoubleUp") return "↑↑";
    if (direction == "SingleUp") return "↑";
    if (direction == "FortyFiveUp") return "↗";
    if (direction == "Flat") return "→";
    if (direction == "FortyFiveDown") return "↘";
    if (direction == "SingleDown") return "↓";
    if (direction == "DoubleDown") return "↓↓";
    return "-";
}

String getLibreTrendSymbol(int arrow) {
    switch (arrow) {
        case 1: return "↓";
        case 2: return "↘";
        case 3: return "→";
        case 4: return "↗";
        case 5: return "↑";
        default: return "-";
    }
}

uint32_t calculate_dynamic_color(float mmol) {
    if (mmol <= targetLow) return 0xFF0000;   // hypo / pod cieľom
    if (mmol >= targetCrit) return 0xFF0000;  // kritická hyper
    if (mmol > targetHigh) return 0xFFA500;   // nad cieľom, ešte nie crit

    // V cieľovom pásme [targetLow, targetHigh]:
    // zelená od low po stred, potom gradient zelená → žltá smerom k high
    float mid = (targetLow + targetHigh) * 0.5f;
    if (mmol <= mid) return 0x00FF00;

    float range = targetHigh - mid;
    if (range < 0.01f) return 0x00FF00;

    float ratio = (mmol - mid) / range;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    uint8_t r = (uint8_t)(ratio * 255.0f);
    uint8_t g = (uint8_t)(255.0f - ratio * 90.0f);
    uint8_t b = 0;
    return (r << 16) | (g << 8) | b;
}

void refresh_critical_and_color() {
    if (last_mmol_value > 0.0) {
        is_critical = (last_mmol_value <= targetLow || last_mmol_value >= targetCrit);
        current_arc_color = calculate_dynamic_color(last_mmol_value);
    }
}

void saveGlucoseCache() {
    if (!has_valid_reading || last_mmol_value <= 0.0f) return;
    preferences.putFloat("cache_mmol", last_mmol_value);
    preferences.putLong("cache_ts", last_reading_timestamp);
    preferences.putString("cache_tr", last_trend_symbol);
    preferences.putFloat("cache_dlt", last_delta_mmol);
    preferences.putBool("cache_hd", has_delta);
}

void loadGlucoseCache() {
    float cached = preferences.getFloat("cache_mmol", 0.0f);
    if (cached <= 0.0f) return;
    last_mmol_value = cached;
    last_reading_timestamp = preferences.getLong("cache_ts", 0);
    last_trend_symbol = preferences.getString("cache_tr", "-");
    last_delta_mmol = preferences.getFloat("cache_dlt", 0.0f);
    has_delta = preferences.getBool("cache_hd", false);
    has_valid_reading = true;
    is_critical = (last_mmol_value <= targetLow || last_mmol_value >= targetCrit);
    current_arc_color = calculate_dynamic_color(last_mmol_value);
}

DiaPumpStatus slotPump[2];

void diaPumpClear(int slot) {
    if (slot < 0 || slot > 1) return;
    slotPump[slot] = DiaPumpStatus();
}

void diaPumpFormat(int slot, char *buf, size_t n, bool compact) {
    if (!buf || n == 0) return;
    buf[0] = '\0';
    if (slot < 0 || slot > 1 || !slotPump[slot].valid) return;
    const DiaPumpStatus &p = slotPump[slot];
    size_t used = 0;
    auto add = [&](const char *fmt, float a, const char *sep) {
        if (used + 4 >= n) return;
        if (sep && used > 0) {
            int k = snprintf(buf + used, n - used, "%s", sep);
            if (k < 0 || (size_t)k >= n - used) return;
            used += (size_t)k;
        }
        int k = snprintf(buf + used, n - used, fmt, a);
        if (k < 0 || (size_t)k >= n - used) {
            buf[used] = '\0';
            return;
        }
        used += (size_t)k;
    };
    if (compact) {
        if (p.hasIob) add("IOB %.1f", p.iob, nullptr);
        if (p.hasCob) add("COB %.0f", p.cob, "  ");
        if (p.hasBasal) add("%.2f U/h", p.basal, "  ");
        return;
    }
    if (p.hasIob) add("IOB %.1f U", p.iob, nullptr);
    if (p.hasCob) add("COB %.0f g", p.cob, "\n");
    if (p.hasBasal) add("%.2f U/h", p.basal, "\n");
    if (p.hasReservoir) add("%.0f U", p.reservoir, p.hasBasal ? " · " : "\n");
}

const char *fetchErrorText(FetchError err) {
    switch (err) {
        case FetchError::Wifi:
            return diaFetchShowConnecting() ? S(Str::Connecting) : S(Str::ErrWifi);
        case FetchError::Auth: return S(Str::ErrAuth);
        case FetchError::Network:
            return diaFetchShowConnecting() ? S(Str::Connecting) : S(Str::ErrNet);
        case FetchError::Empty: return S(Str::ErrEmpty);
        case FetchError::Config: return S(Str::ErrConfig);
        default: return "";
    }
}

void glucoseStoreReading(float mmol, const String &trendSymbol, long unix_ts,
                         bool hasSourceDelta, float sourceDeltaMmol) {
    glucoseStoreReadingSlot(dia_active_slot, mmol, trendSymbol, unix_ts,
                            hasSourceDelta, sourceDeltaMmol);
}

void glucoseStoreReadingSlot(int slot, float mmol, const String &trendSymbol, long unix_ts,
                             bool hasSourceDelta, float sourceDeltaMmol) {
    if (slot < 0 || slot > 1) slot = 0;
    DiaSlotReading &s = slotReadings[slot];
    // Limity z profilu – bez PushSlot (dual fetch by inak prepísal cudzie credentials)
    const float lo = profiles[slot].targetLow;
    const float hi = profiles[slot].targetHigh;
    const float cr = profiles[slot].targetCrit;

    if (hasSourceDelta) {
        s.delta_mmol = sourceDeltaMmol;
        s.has_delta = true;
    } else if (s.has_valid_reading && s.mmol > 0.0f) {
        if (unix_ts <= 0 || unix_ts != s.timestamp) {
            s.delta_mmol = mmol - s.mmol;
            s.has_delta = true;
        }
    }

    s.mmol = mmol;
    s.trend = trendSymbol;
    if (unix_ts > 0) {
        s.timestamp = unix_ts;
    } else {
        // API bez ts – aspoň „teraz“, inak vek zamrzne a dual ostane sivý
        time_t now = time(nullptr);
        if (now > 100000) s.timestamp = (long)now;
    }
    s.has_valid_reading = true;
    s.err = FetchError::None;
    s.data_is_stale = false;
    // Ak fetch práve naplnil globals históriu, preber ju
    if (glucose_history_count > 0 && dia_active_slot == slot) {
        s.history_count = glucose_history_count;
        memcpy(s.history, glucose_history, sizeof(s.history));
    }

    s.is_critical = (s.mmol <= lo || s.mmol >= cr);
    const float oL = targetLow, oH = targetHigh, oC = targetCrit;
    targetLow = lo;
    targetHigh = hi;
    targetCrit = cr;
    s.arc_color = calculate_dynamic_color(s.mmol);
    targetLow = oL;
    targetHigh = oH;
    targetCrit = oC;

    // Počas defer syncuj globals len pre aktívny fetch slot (A po duali obnoví fetch task)
    if (!diaFetchDeferUi() || slot == diaFetchCurrentSlot()) {
        glucoseSyncGlobalsFromSlot(slot);
    }
    if (slot == diaShownSlot() && s.has_valid_reading && s.mmol > 0.0f) {
        preferences.putFloat("cache_mmol", s.mmol);
        preferences.putLong("cache_ts", s.timestamp);
        preferences.putString("cache_tr", s.trend);
        preferences.putFloat("cache_dlt", s.delta_mmol);
        preferences.putBool("cache_hd", s.has_delta);
        preferences.putUChar("cache_slot", (uint8_t)slot);
    }
}

void glucoseRecalcSlotVisuals() {
    for (int i = 0; i < DIA_PROFILE_COUNT; i++) {
        DiaSlotReading &s = slotReadings[i];
        if (!s.has_valid_reading || s.mmol <= 0.0f) continue;
        const float lo = profiles[i].targetLow;
        const float hi = profiles[i].targetHigh;
        const float cr = profiles[i].targetCrit;
        // dočasne targets do globals pre calculate_dynamic_color
        targetLow = lo;
        targetHigh = hi;
        targetCrit = cr;
        s.is_critical = (s.mmol <= lo || s.mmol >= cr);
        s.arc_color = calculate_dynamic_color(s.mmol);
    }
    diaConfigPushSlot(diaShownSlot());
    glucoseSyncGlobalsFromSlot(diaShownSlot());
}
