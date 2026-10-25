#pragma once

#include "dia_types.h"

void fetchGlucoseData();
void fetchLibre();
void fetchDexcom();
void fetchNightscout();

/** FreeRTOS fetch task – HTTP mimo UI slučku. */
void diaFetchInit();
bool diaFetchRequest();
bool diaFetchBusy();
/** Nový fetch sa nespustí. true = práve bežiaci fetch skončil. */
bool diaFetchQuiesce(uint32_t timeoutMs);
/** Znova povoľ sťahovanie. Po reštarte netreba. */
void diaFetchResume();
bool diaFetchDeferUi();
void diaFetchWatchdog(uint32_t nowMs);
void diaFetchEnqueueSuccess(float mmol, const char *trend, long unix_ts,
                            bool hasSourceDelta = false, float sourceDeltaMmol = 0.0f);
void diaFetchEnqueueError(FetchError err);
/** Aplikuj výsledok fetchu na UI (volať z loop / LVGL vlákna). */
void diaFetchPoll();
/** Slot práve fetchovaný (pre UI/enqueue). */
int diaFetchCurrentSlot();
/** Sieť sa ešte chytá. Token sa neskúša znova, na displeji je „Pripájam…“. */
bool diaFetchShowConnecting();
/** Slot čaká na server aj po rýchlych pokusoch. Číslo na displeji ostáva. */
bool diaFetchSlotWaiting(int slot);
/** Po uložení zdroja v portáli: zruš „Pripájam…“ a dovoľ hneď nový fetch. */
void diaFetchOnConfigChanged();
