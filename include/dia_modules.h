#pragma once

// Dia Monitor – rozdelenie kódu
//
// include/dia_board.h    – HAL displej/touch (WS 1.28 / 3.5B)
// include/dia_version.h  – FIRMWARE_VERSION (build_flags)
// include/dia_tls.h      – configureTlsClient()
// include/dia_types.h    – FetchError, konštanty
// include/dia_config.h   – NVS / credentials / limity
// include/dia_glucose.h  – stav glykémie, cache, farby, formátovanie
// include/dia_fetch.h    – Nightscout / Libre / Dexcom / Guardian
// include/dia_wifi.h     – WiFiManager portál, zálohy, reconnect
// include/dia_ui.h       – LVGL UI (bez pinov)
// include/dia_ota.h      – OTA aktualizácia
// include/dia_i18n.h     – SK/EN texty
// include/dia_carelink.h – CareLink tokeny / Guardian
// include/dia_audio.h    – critical chime + snooze (3.5B ES8311)
//
// src/board/board_ws_1_28.cpp  – GC9A01 + CST816S
// src/board/board_ws_3_5.cpp   – AXS15231B QSPI + touch
// src/main.cpp                 – setup() / loop()

#include "dia_board.h"
#include "dia_version.h"
#include "dia_tls.h"
#include "dia_types.h"
#include "dia_config.h"
#include "dia_glucose.h"
#include "dia_fetch.h"
#include "dia_wifi.h"
#include "dia_ui.h"
#include "dia_ota.h"
#include "dia_audio.h"
