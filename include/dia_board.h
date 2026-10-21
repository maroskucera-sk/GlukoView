#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Hardvérová vrstva – jeden kód, viac dosiek.
 * Build flag: -DDIA_BOARD_WS_1_28 alebo -DDIA_BOARD_WS_3_5
 */

#if defined(DIA_BOARD_WS_3_5)
/** LVGL logické rozmery (landscape). Panel natívne 320×480. */
#ifndef DIA_SCREEN_W
#define DIA_SCREEN_W 480
#endif
#ifndef DIA_SCREEN_H
#define DIA_SCREEN_H 320
#endif
#ifndef DIA_PANEL_W
#define DIA_PANEL_W 320
#endif
#ifndef DIA_PANEL_H
#define DIA_PANEL_H 480
#endif
/** Priemer ciferníka (fullscreen mode). */
#ifndef DIA_FACE_SIZE
#define DIA_FACE_SIZE 300
#endif
#ifndef DIA_FACE_OFFSET_Y
#define DIA_FACE_OFFSET_Y ((DIA_SCREEN_H - DIA_FACE_SIZE) / 2)
#endif
#ifndef DIA_BOOT_BTN
#define DIA_BOOT_BTN 0
#endif
/** Menu / dialógy (landscape 480×320). */
#ifndef DIA_MENU_BTN_W
#define DIA_MENU_BTN_W 210
#endif
#ifndef DIA_MENU_BTN_W_FULL
#define DIA_MENU_BTN_W_FULL 440
#endif
#ifndef DIA_MENU_BTN_H
#define DIA_MENU_BTN_H 48
#endif
#ifndef DIA_MENU_PAD_TOP
#define DIA_MENU_PAD_TOP 12
#endif
#ifndef DIA_MENU_PAD_ROW
#define DIA_MENU_PAD_ROW 8
#endif
/** LVGL zoom: 256 = 100 %. Logo 240 → ~320 px výšky. */
#ifndef DIA_SPLASH_ZOOM
#define DIA_SPLASH_ZOOM 340
#endif
#elif defined(DIA_BOARD_WS_1_28)
#ifndef DIA_SCREEN_W
#define DIA_SCREEN_W 240
#endif
#ifndef DIA_SCREEN_H
#define DIA_SCREEN_H 240
#endif
#ifndef DIA_PANEL_W
#define DIA_PANEL_W 240
#endif
#ifndef DIA_PANEL_H
#define DIA_PANEL_H 240
#endif
#ifndef DIA_FACE_SIZE
#define DIA_FACE_SIZE 240
#endif
#ifndef DIA_FACE_OFFSET_Y
#define DIA_FACE_OFFSET_Y 0
#endif
#ifndef DIA_BOOT_BTN
#define DIA_BOOT_BTN 0
#endif
#ifndef DIA_MENU_BTN_W
#define DIA_MENU_BTN_W 160
#endif
#ifndef DIA_MENU_BTN_W_FULL
#define DIA_MENU_BTN_W_FULL 160
#endif
#ifndef DIA_MENU_BTN_H
#define DIA_MENU_BTN_H 40
#endif
#ifndef DIA_MENU_PAD_TOP
#define DIA_MENU_PAD_TOP 36
#endif
#ifndef DIA_MENU_PAD_ROW
#define DIA_MENU_PAD_ROW 8
#endif
#ifndef DIA_SPLASH_ZOOM
#define DIA_SPLASH_ZOOM 256
#endif
#else
#error "Define DIA_BOARD_WS_1_28 or DIA_BOARD_WS_3_5 in build_flags"
#endif

#ifndef BOOT_BTN
#define BOOT_BTN DIA_BOOT_BTN
#endif

void diaBoardInit();
void diaBoardSetBrightness(uint8_t level); // 0–255
void diaBoardFlush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint16_t *pixels);
/** Po poslednom LVGL stripe framu – present na panel (3.5B soft-rotate). */
void diaBoardFlushPresent();
/** 3.5B: landscape FB → panel (single-pass rotate + full QSPI). */
void diaBoardPresentLand(const uint16_t *land);
/** true = prst dole; x/y v LVGL súradniciach (DIA_SCREEN_W/H). */
bool diaBoardTouchRead(int16_t *x, int16_t *y);
bool diaBoardIsRound();
const char *diaBoardName();
/** Preferovať LVGL full_refresh (1.28 nie; 3.5B áno – AXS partial window nefunguje). */
bool diaBoardPreferFullRefresh();
/** true = landscape soft-rotate (3.5B). */
bool diaBoardIsLandscape();
/** true = ES8311 + speaker (3.5B). */
bool diaBoardHasAudio();
/** PA enable cez TCA EXIO7 (NS4150B CTRL, active high). no-op bez audio. */
void diaBoardSpeakerAmp(bool enable);

/** Otočenie landscape (len 3.5B): 0=USB vpravo, 1=USB vľavo. */
bool diaBoardSupportsOrient();
uint8_t diaBoardGetOrientMode();
void diaBoardSetOrientMode(uint8_t mode);
/** Efektívny 180° flip voči predvolenému USB-vpravo. */
bool diaBoardIsFlipped();
