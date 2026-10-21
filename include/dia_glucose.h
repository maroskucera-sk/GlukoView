#pragma once

#include <Arduino.h>
#include "dia_types.h"

extern bool is_critical;
extern bool data_is_stale;
extern bool has_valid_reading;
extern bool has_delta;
extern uint32_t current_arc_color;
extern float last_mmol_value;
extern float last_delta_mmol;
extern String last_trend_symbol;
extern FetchError last_fetch_error;
extern long last_reading_timestamp;

float mmol_to_mgdl(float mmol);
float mgdl_to_mmol(float mgdl);
void formatGlucose(float mmol, char *buf, size_t n);
void formatTargetLabel(const char *prefix, float mmol, char *buf, size_t n);

String getTrendSymbol(String direction);
String getLibreTrendSymbol(int arrow);

uint32_t calculate_dynamic_color(float mmol);
void refresh_critical_and_color();

void saveGlucoseCache();
void loadGlucoseCache();

/** História pre sparkline: index 0 = najstarší, count-1 = najnovší (mmol/L). */
extern float glucose_history[DIA_HISTORY_MAX];
extern int glucose_history_count;

void glucoseHistoryClear();
/** API typicky vracia newest-first – uloží chronologicky oldest→newest. */
void glucoseHistorySetNewestFirst(const float *mmolNewestFirst, int count);
void glucoseHistoryPush(float mmol);

const char *fetchErrorText(FetchError err);

/**
 * Aktualizuje stav po novom meraní (bez UI).
 * hasSourceDelta=true → delta zo API (2 merania); inak výpočet z predchádzajúcej hodnoty zariadenia.
 */
void glucoseStoreReading(float mmol, const String &trendSymbol, long unix_ts,
                         bool hasSourceDelta = false, float sourceDeltaMmol = 0.0f);

/** Per-slot stav (dual profile). Globals vyššie = mirror slotu 0 / aktívneho. */
struct DiaSlotReading {
    bool is_critical = false;
    bool data_is_stale = false;
    bool has_valid_reading = false;
    bool has_delta = false;
    uint32_t arc_color = 0x00FF00;
    float mmol = 0;
    float delta_mmol = 0;
    String trend = "-";
    FetchError err = FetchError::None;
    long timestamp = 0;
    float history[DIA_HISTORY_MAX] = {0};
    int history_count = 0;
};
extern DiaSlotReading slotReadings[2];
void glucoseSyncGlobalsFromSlot(int slot);
void glucoseStoreReadingSlot(int slot, float mmol, const String &trendSymbol, long unix_ts,
                             bool hasSourceDelta = false, float sourceDeltaMmol = 0.0f);
/** Prepočíta is_critical/arc_color podľa profiles[].target* (po zmene limitov). */
void glucoseRecalcSlotVisuals();
