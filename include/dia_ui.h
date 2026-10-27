#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "dia_types.h"
#include "dia_board.h"

void diaUiInitHardware();
void diaUiInitLvgl();
void build_ui();
/** Zapne backlight až po vykreslení loga (bez boot šumu). */
void diaUiRevealDisplay();
/** Dobehne pôvodných 2,2 s loga. Potom môže ísť text o liečbe. */
void diaUiHoldSplash();

void showBootStatus(const char *title, const char *detail);
void restoreMainScreenChrome();
void showCachedReadingOnUi();
void apply_watchface();
void updateBrightness();
void updateClockDisplay();
void updateAgeDisplay();
void updateStaleAndErrorVisuals();
void updateDeltaLabel();
void update_target_btn_labels();
void refresh_glucose_label();
void refresh_ui_colors();
void refresh_sparkline();

void applyGlucoseReading(float mmol, const String &trendSymbol, long unix_ts,
                         bool hasSourceDelta = false, float sourceDeltaMmol = 0.0f);
void markFetchError(FetchError err);

void diaUiProcessPendingPrefSave();
void diaUiUpdateArcColors(uint32_t currentMillis);
void diaUiTick(uint32_t deltaMs);
void diaUiFlushHandlers(int frames, int delayMs);
/** Otvor settings (long-press / BOOT). */
void diaUiOpenSettings();
void diaUiApplyDisplayMode();
void diaUiUpdateSlotFace(int slot);
/** First-boot výber jazyka. force znova otvorí zoznam, aj keď jazyk už je uložený. */
bool diaUiRunLanguageSetup(bool force = false);
/** true = súhlas uložený. false = používateľ sa chce vrátiť na výber jazyka. */
bool diaUiRunLegalConsent();
/** true, kým od prvého boot textu neubehlo 5 s. Len drží ciferník, fetch nespomaľuje. */
bool diaUiBootLegalHolding();

// OTA panel (používa dia_ota)
extern lv_obj_t *ota_panel;
extern lv_obj_t *ota_label;
extern lv_obj_t *ota_bar;
extern lv_obj_t *ui_settings_panel;
extern lv_obj_t *ui_info_panel;

void showInfoMessageLayout(const char *msg);

extern unsigned long lastUpdate;
extern const unsigned long updateInterval;
