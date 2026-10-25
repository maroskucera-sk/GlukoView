#include <Arduino.h>
#include <WiFi.h>

#include "dia_audio.h"
#include "dia_carelink.h"
#include "dia_config.h"
#include "dia_diag.h"
#include "dia_fetch.h"
#include "dia_glucose.h"
#include "dia_i18n.h"
#include "dia_legal.h"
#include "dia_modules.h"
#include "dia_ota.h"
#include "dia_ui.h"
#include "dia_wifi.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    diaConfigBegin();
    diaConfigLoad();
    diaDiagBoot();
    diaOtaNoteBoot();

    if (preferences.isKey("cache_mmol")) {
        loadGlucoseCache();
        int cacheSlot = preferences.getUChar("cache_slot", 0);
        if (cacheSlot < 0 || cacheSlot > 1) cacheSlot = 0;
        slotReadings[cacheSlot].mmol = last_mmol_value;
        slotReadings[cacheSlot].timestamp = last_reading_timestamp;
        slotReadings[cacheSlot].trend = last_trend_symbol;
        slotReadings[cacheSlot].delta_mmol = last_delta_mmol;
        slotReadings[cacheSlot].has_delta = has_delta;
        slotReadings[cacheSlot].has_valid_reading = has_valid_reading;
        slotReadings[cacheSlot].is_critical = is_critical;
        slotReadings[cacheSlot].arc_color = current_arc_color;
        glucoseSyncGlobalsFromSlot(diaShownSlot());
    }

    diaUiInitHardware(); // BL off, čierna
    diaUiInitLvgl();
    build_ui();
    // Logo + startup zvuk spolu; potom tvrdo vypnúť I2S/MCLK a až potom WiFi.
    diaUiFlushHandlers(diaBoardPreferFullRefresh() ? 3 : 12, diaBoardPreferFullRefresh() ? 40 : 10);
    diaUiRevealDisplay();
    diaAudioInit();
    diaAudioQuiesceForWifi();
    delay(300);

    pinMode(BOOT_BTN, INPUT_PULLUP);
    if (digitalRead(BOOT_BTN) == LOW) {
        delay(5000);
        if (digitalRead(BOOT_BTN) == LOW) {
            wm.resetSettings();
            preferences.clear();
            ESP.restart();
        }
    }

    if (diaI18nNeedsSetup()) {
        diaUiRunLanguageSetup();
    }
    // Logo ostane 2,2 s ako predtým. Veta o liečbe ide až po ňom.
    diaUiHoldSplash();
    // Až po jazyku. Kým nie je súhlas, fetch ani CareLink sa nespúšťajú.
    diaUiRunLegalConsent();

    showBootStatus("WiFi", S(Str::BootConnecting));
    diaUiFlushHandlers(4, 5);
    Serial.printf("[boot] freeheap before WiFi: %u\n", (unsigned)ESP.getFreeHeap());
    diaWifiEnsureConnected();

    diaFetchInit();
    forceFetch = true;
}

void loop() {
    diaWifiPollBootLegal();
    diaWifiProcess();
    diaWifiSavePortalConfigIfNeeded();
    diaUiProcessPendingPrefSave();

    // Krátky stisk BOOT → settings (dlhý 5 s pri boote = factory reset)
    {
        static bool bootWasDown = false;
        static uint32_t bootDownAt = 0;
        bool down = digitalRead(BOOT_BTN) == LOW;
        if (down && !bootWasDown) {
            bootDownAt = millis();
        } else if (!down && bootWasDown) {
            uint32_t held = millis() - bootDownAt;
            if (held >= 50 && held < 2000) {
                diaUiOpenSettings();
            }
        }
        bootWasDown = down;
    }

    static uint32_t lastTickMillis = millis();
    uint32_t currentMillis = millis();
    lv_tick_inc(currentMillis - lastTickMillis);
    lastTickMillis = currentMillis;
    lv_timer_handler();

    diaWifiMaintain(currentMillis);
    {
        static uint32_t lastFlashSafe = 0;
        if (currentMillis - lastFlashSafe > 2000) {
            lastFlashSafe = currentMillis;
            if (!diaFetchBusy() && !carelinkRefreshInFlight()) esp_rom_printf("[flash] safe\n");
        }
    }
    diaUiUpdateArcColors(currentMillis);
    diaAudioTick(currentMillis);

    diaFetchPoll();
    diaFetchWatchdog(currentMillis);
    // Kým nie je zdroj, nechaj IP návod – Sync/Config by ho prepísali
    if (!diaWifiNeedsSetupHint() && !diaFetchBusy() &&
        (forceFetch || (currentMillis - lastUpdate > updateInterval))) {
        forceFetch = false;
        lastUpdate = currentMillis;
        diaFetchRequest();
    } else if (diaWifiNeedsSetupHint()) {
        forceFetch = false;
    }

    {
        static bool ota_night_hold = false;
        unsigned long otaDue = ota_first_check_done ? otaCheckInterval : otaFirstCheckDelay;
        if (ota_night_hold) otaDue = otaNightRetry;
        if (auto_update && (currentMillis - last_ota_check > otaDue)) {
            last_ota_check = currentMillis;
            if (!diaOtaInDayWindow()) {
                ota_night_hold = true;
                esp_rom_printf("[OTA] mimo denného okna, bez sťahovania\n");
            } else {
                ota_night_hold = false;
                ota_first_check_done = true;
                if (!is_critical && last_mmol_value > 0.0 && !diaFetchBusy()) {
                    check_firmware_update(false);
                }
            }
        }
    }

    static uint32_t lastAgeUpdate = 0;
    if (currentMillis - lastAgeUpdate > 10000) {
        lastAgeUpdate = currentMillis;
        updateBrightness();
        if (display_mode >= 2) {
            diaUiUpdateSlotFace(0);
            diaUiUpdateSlotFace(1);
        } else {
            const int shown = diaShownSlot();
            diaConfigPushSlot(shown);
            glucoseSyncGlobalsFromSlot(shown);
            updateAgeDisplay();
        }
    }

    static uint32_t lastTimeUpdate = 0;
    if (currentMillis - lastTimeUpdate > 1000) {
        lastTimeUpdate = currentMillis;
        updateClockDisplay();
    }

    delay(5);
}
