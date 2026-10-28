#include "dia_ui.h"
#include "dia_diag.h"
#include "dia_ui_status.h"
#include "dia_pin.h"
#include "dia_i18n.h"
#include "dia_legal.h"
#include "dia_config.h"
#include "dia_board.h"
#include "dia_audio.h"
#include "dia_fetch.h"
#include "dia_glucose.h"
#include "dia_ota.h"
#include "dia_version.h"
#include "dia_wifi.h"

#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <math.h>
#include <time.h>

LV_FONT_DECLARE(font_sk_12);
LV_FONT_DECLARE(font_sk_16);
LV_FONT_DECLARE(font_sk_20);
LV_FONT_DECLARE(font_sk_24);
LV_FONT_DECLARE(font_sk_28);
LV_FONT_DECLARE(font_sk_32);
LV_FONT_DECLARE(font_sk_48);
LV_FONT_DECLARE(font_sk_72);
LV_FONT_DECLARE(font_sk_88);
LV_FONT_DECLARE(font_pump_56);
LV_IMG_DECLARE(logo_boot);

static const uint32_t screenWidth = DIA_SCREEN_W;
static const uint32_t screenHeight = DIA_SCREEN_H;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;

lv_obj_t *arc_bg;
lv_obj_t *label_time;
lv_obj_t *label_glycemia;
lv_obj_t *label_trend;
lv_obj_t *label_delta;
lv_obj_t *label_age;
lv_obj_t *label_status;
lv_obj_t *label_source = nullptr;
lv_obj_t *chart_spark = NULL;
lv_chart_series_t *chart_spark_ser = NULL;
static lv_color_t *disp_draw_buf2 = nullptr;

// Slot 1 (druhé dieťa) – landscape split
static lv_obj_t *label_name0 = nullptr;
static lv_obj_t *label_name1 = nullptr;
static lv_obj_t *label_glycemia1 = nullptr;
static lv_obj_t *label_trend1 = nullptr;
static lv_obj_t *label_delta1 = nullptr;
static lv_obj_t *label_age1 = nullptr;
static lv_obj_t *label_source1 = nullptr;
static lv_obj_t *label_pump = nullptr;
/** Pump ciferník (NS): Sach / IOB kruhy + stredný BG kruh. */
static lv_obj_t *arc_pump_cob = nullptr;
static lv_obj_t *arc_pump_iob = nullptr;
static lv_obj_t *arc_pump_bg = nullptr;
static lv_obj_t *label_pump_cob = nullptr;
static lv_obj_t *label_pump_iob = nullptr;
/** Hodnoty v satelitoch (väčší font než nápis Sach/IOB). */
static lv_obj_t *label_pump_cob_val = nullptr;
static lv_obj_t *label_pump_iob_val = nullptr;
static lv_obj_t *split_line = nullptr;
static lv_obj_t *panel_frame0 = nullptr;
static lv_obj_t *panel_frame1 = nullptr;
static lv_obj_t *chart_spark1 = nullptr;
static lv_chart_series_t *chart_spark_ser1 = nullptr;
static lv_obj_t *lbl_btn_mode = nullptr;

/** Kvadratický úbytok: pri farbe jemnejšie, v strede skoro nič. */
static lv_opa_t diaUiInwardOpa(int i, int n, int peak) {
    if (n <= 0 || i >= n) return 0;
    const int rem = n - i;
    int opa = (peak * rem * rem) / (n * n);
    if (opa < 3) return 0;
    if (opa > LV_OPA_COVER) return LV_OPA_COVER;
    return (lv_opa_t)opa;
}

#if defined(DIA_BOARD_WS_3_5)
/** Medzera medzi panelmi (bez stredného stĺpca s časom). */
static constexpr int kSplitMidW = 10;
static constexpr int kSplitFrameGap = 6;
static constexpr int kSplitFrameBorder = 8;
static constexpr int kSplitFrameRadius = 14;
static constexpr int kSplitNameTop = 12;
/** Rezerva pod menom (aj 2 riadky dlhého mena), aby nezasahovalo do hodnoty. */
static constexpr int kSplitNameClear = 56;

/** Jemný prechod od farebného rámu dovnútra, ako výplň pod grafom. */
static void diaUiPanelInwardFade(lv_event_t *e) {
    if (!show_glow) return;
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    if (!obj || !ctx) return;
    lv_color_t col = lv_obj_get_style_border_color(obj, LV_PART_MAIN);
    if (col.full == lv_color_hex(0x222222).full) return;
    lv_area_t box;
    lv_obj_get_coords(obj, &box);
    const int depth = 36;
    const int step = 1;
    const int band = 1;
    const int n = depth / step;
    for (int i = 0; i < n; i++) {
        const int inset = kSplitFrameBorder + i * step;
        lv_area_t a;
        a.x1 = box.x1 + inset;
        a.y1 = box.y1 + inset;
        a.x2 = box.x2 - inset;
        a.y2 = box.y2 - inset;
        if (a.x2 <= a.x1 + 12 || a.y2 <= a.y1 + 12) break;
        lv_opa_t opa = diaUiInwardOpa(i, n, 132);
        if (opa == 0) break;
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_opa = LV_OPA_TRANSP;
        dsc.border_color = col;
        dsc.border_width = band;
        dsc.border_opa = opa;
        int rad = kSplitFrameRadius - inset;
        dsc.radius = rad > 0 ? rad : 0;
        lv_draw_rect(ctx, &dsc, &a);
    }
}

static void diaUiStylePanelFrame(lv_obj_t *f) {
    if (!f) return;
    lv_obj_set_style_bg_opa(f, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(f, kSplitFrameBorder, LV_PART_MAIN);
    lv_obj_set_style_border_color(f, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_border_opa(f, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(f, kSplitFrameRadius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(f, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(f, 0, LV_PART_MAIN);
    lv_obj_clear_flag(f, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(f, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(f, diaUiPanelInwardFade, LV_EVENT_DRAW_MAIN_END, NULL);
}

static void diaUiSetPanelFrameColor(lv_obj_t *f, uint32_t color) {
    if (f) lv_obj_set_style_border_color(f, lv_color_hex(color), LV_PART_MAIN);
}
#endif

lv_obj_t *ui_settings_panel;
lv_obj_t *ui_edit_panel;
lv_obj_t *ui_info_panel;
lv_obj_t *ui_confirm_panel;

lv_obj_t *ota_panel;
lv_obj_t *ota_label;
lv_obj_t *ota_bar;

lv_obj_t *lbl_btn_day;
lv_obj_t *lbl_btn_night;
lv_obj_t *lbl_btn_low;
lv_obj_t *lbl_btn_high;
lv_obj_t *lbl_btn_crit;
lv_obj_t *lbl_btn_day_h;
lv_obj_t *lbl_btn_night_h;
lv_obj_t *lbl_btn_autoupdate;
lv_obj_t *lbl_btn_units;
lv_obj_t *lbl_btn_delta;
lv_obj_t *lbl_btn_source = nullptr;
lv_obj_t *lbl_btn_pump = nullptr;
lv_obj_t *lbl_btn_glow = nullptr;
lv_obj_t *lbl_btn_orient = nullptr;
lv_obj_t *lbl_btn_sound = nullptr;
static lv_obj_t *ui_sound_panel = nullptr;
static lv_obj_t *lbl_btn_sound_open = nullptr;
static lv_obj_t *lbl_look_title = nullptr;
static lv_obj_t *lbl_sound_title = nullptr;
static lv_obj_t *lbl_vol = nullptr;
static lv_obj_t *lbl_btn_vol_down = nullptr;
static lv_obj_t *lbl_btn_vol_up = nullptr;
static lv_obj_t *lbl_back_look = nullptr;
static lv_obj_t *lbl_back_sound = nullptr;
static lv_obj_t *lbl_btn_child = nullptr;
lv_obj_t *lbl_btn_snooze = nullptr;
lv_obj_t *lbl_btn_snooze_now = nullptr;
lv_obj_t *lbl_btn_lang = nullptr;

static lv_obj_t *lbl_t1_title = nullptr;
static lv_obj_t *lbl_t2_title = nullptr;
static lv_obj_t *lbl_t3_title = nullptr;
static lv_obj_t *lbl_btn_info = nullptr;
static lv_obj_t *lbl_btn_refresh = nullptr;
static lv_obj_t *lbl_btn_update = nullptr;
static lv_obj_t *lbl_btn_forget = nullptr;
static lv_obj_t *lbl_back1 = nullptr;
static lv_obj_t *lbl_back2 = nullptr;
static lv_obj_t *lbl_back3 = nullptr;
static lv_obj_t *lbl_info_title = nullptr;
static lv_obj_t *lbl_info_close = nullptr;
static lv_obj_t *lbl_confirm_title = nullptr;
static lv_obj_t *lbl_yes = nullptr;
static lv_obj_t *lbl_no = nullptr;
static lv_obj_t *lbl_save = nullptr;
static lv_obj_t *lbl_btn_pin = nullptr;
static lv_obj_t *lbl_btn_pin_clear = nullptr;
static lv_obj_t *ui_pin_panel = nullptr;
static lv_obj_t *lbl_pin_title = nullptr;
static lv_obj_t *lbl_pin_digits = nullptr;
static lv_obj_t *lbl_pin_back = nullptr;
static char s_pinEntry[5];
static char s_pinFirst[5];
static int s_pinLen = 0;
static int s_pinStage = 0;
static int s_confirmKind = 0;

lv_obj_t *lbl_edit_title;
lv_obj_t *lbl_edit_val;
lv_obj_t *slider_edit;
static int current_edit_mode = 0;

lv_obj_t *lbl_info_keys;
lv_obj_t *lbl_info_text;
static lv_obj_t *lbl_info_status = nullptr;
static lv_obj_t *lbl_info_legal = nullptr;
static lv_obj_t *ui_info_scroll = nullptr;

unsigned long lastUpdate = 0;
// 60 s – dual Libre+NS; 180 s pôsobilo ako „zamrznuté“ voči appke
const unsigned long updateInterval = 60000;

static int16_t last_x = 0;
static int16_t last_y = 0;
static bool was_long_pressed = false;
static bool gesture_just_handled = false;
static unsigned long s_lastManualFetchMs = 0;
static constexpr unsigned long kManualFetchCooldownMs = 20000;
// Počas WiFi/portálu: žiadny dual layout / zdroje / mená (ako 1.28)
static bool s_bootUiLock = true;
/** Scrollovací AP návod (dlhý text na kruhu / 3.5"). */
static lv_obj_t *ui_boot_scroll = nullptr;
static lv_obj_t *lbl_boot_legal = nullptr;
static char s_bootShownTitle[64];
static char s_bootShownDetail[512];
static uint32_t s_bootLegalFrom = 0;
static constexpr uint32_t kBootLegalHoldMs = 5000;

static bool diaUiFaceOverlaysOpen() {
    if (ui_settings_panel && !lv_obj_has_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN)) return true;
    if (ui_edit_panel && !lv_obj_has_flag(ui_edit_panel, LV_OBJ_FLAG_HIDDEN)) return true;
    if (ui_info_panel && !lv_obj_has_flag(ui_info_panel, LV_OBJ_FLAG_HIDDEN)) return true;
    if (ui_sound_panel && !lv_obj_has_flag(ui_sound_panel, LV_OBJ_FLAG_HIDDEN)) return true;
    if (ota_panel && !lv_obj_has_flag(ota_panel, LV_OBJ_FLAG_HIDDEN)) return true;
    return false;
}

static void diaUiRaiseOverlays() {
    if (ui_settings_panel) lv_obj_move_foreground(ui_settings_panel);
    if (ui_edit_panel) lv_obj_move_foreground(ui_edit_panel);
    if (ui_info_panel) lv_obj_move_foreground(ui_info_panel);
    if (ui_sound_panel) lv_obj_move_foreground(ui_sound_panel);
    if (ui_pin_panel) lv_obj_move_foreground(ui_pin_panel);
    if (ui_confirm_panel) lv_obj_move_foreground(ui_confirm_panel);
    if (ota_panel) lv_obj_move_foreground(ota_panel);
}

static void diaUiFacePassThroughTouches() {
    // Labely/grafy inak „zožerú“ tap – long-press/dvojklik ide len na lv_scr_act()
    lv_obj_t *objs[] = {
        label_time, label_glycemia, label_trend, label_delta, label_age, label_status,
        label_source, label_pump, label_name0, label_name1, label_glycemia1, label_trend1, label_delta1,
        label_age1, label_source1, arc_bg, chart_spark, chart_spark1, split_line,
        panel_frame0, panel_frame1,
    };
    for (lv_obj_t *o : objs) {
        if (o) lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void diaUiTriggerFetch(bool respectCooldown) {
    // Aj počas bežiaceho Guardian fetchu – fronta cez forceFetch (main loop)
    const unsigned long now = millis();
    if (respectCooldown && s_lastManualFetchMs != 0 &&
        (now - s_lastManualFetchMs) < kManualFetchCooldownMs) {
        Serial.println("[ui] manual fetch cooldown");
        return;
    }
    s_lastManualFetchMs = now;
    forceFetch = true;
    Serial.printf("[ui] manual fetch queued busy=%d\n", (int)diaFetchBusy());
    if (label_age) {
        lv_label_set_text(label_age, S(Str::Sync));
        lv_obj_set_style_text_color(label_age, lv_color_hex(0x88AACC), LV_PART_MAIN);
    }
#if defined(DIA_BOARD_WS_3_5)
    if (label_age1) {
        lv_label_set_text(label_age1, S(Str::Sync));
        lv_obj_set_style_text_color(label_age1, lv_color_hex(0x88AACC), LV_PART_MAIN);
    }
#endif
}

static void diaUiRequestManualFetch() {
    if (s_bootUiLock || diaUiFaceOverlaysOpen()) return;
    diaUiTriggerFetch(true);
}

static void clearMainOverlays();
static void diaUiHideSplash();
static void showInfoValuesLayout();
static lv_obj_t *create_menu_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb, int user_data,
                                 uint32_t color, int btn_w = 0);
static void diaUiMakeScrollSnappy(lv_obj_t *obj);
static int faceY(int y240);
static int faceX(int x240);
static void diaUiApplySplitSourceLabels();
/** Sparkline: withGrid=true na dual 3.5"; false na kruhu (bez mriežky). */
static void diaUiStyleSpark(lv_obj_t *chart, bool withGrid);

void diaUiTick(uint32_t deltaMs) {
    lv_tick_inc(deltaMs);
    lv_timer_handler();
}

void diaUiFlushHandlers(int frames, int delayMs) {
    for (int i = 0; i < frames; i++) {
        lv_tick_inc(delayMs);
        lv_timer_handler();
        delay(delayMs);
    }
}

static void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    if (diaBoardIsLandscape() && diaBoardPreferFullRefresh()) {
        // AXS15231B: len full frame; LVGL buffer = landscape FB → single-pass present
        if (lv_disp_flush_is_last(disp_drv)) {
            diaBoardPresentLand((const uint16_t *)&color_p->full);
        }
    } else {
        diaBoardFlush(area->x1, area->y1, area->x2, area->y2, (const uint16_t *)&color_p->full);
        if (lv_disp_flush_is_last(disp_drv)) {
            diaBoardFlushPresent();
        }
    }
    lv_disp_flush_ready(disp_drv);
#if defined(DIA_BOARD_WS_3_5)
    // Medzi pásmi nech WDT/I2C dýcha – sync SPI inak drží loop desiatky ms
    yield();
#endif
}

static void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    (void)indev_driver;
    int16_t x = 0, y = 0;
    if (diaBoardTouchRead(&x, &y)) {
        data->state = LV_INDEV_STATE_PR;
        last_x = x;
        last_y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    data->point.x = last_x;
    data->point.y = last_y;
}

void diaUiInitHardware() {
    diaBoardInit(); // BL zostáva 0 – zapne sa až po logu (diaUiRevealDisplay)
    esp_rom_printf("[ui] board=%s %ux%u face=%d\n",
                   diaBoardName(), (unsigned)screenWidth, (unsigned)screenHeight, DIA_FACE_SIZE);
}

void diaUiRevealDisplay() {
    diaBoardSetBrightness((uint8_t)day_brightness);
    esp_rom_printf("[ui] BL on br=%d\n", day_brightness);
}

void diaUiInitLvgl() {
    lv_init();

    const bool full = diaBoardPreferFullRefresh();
    const bool land = diaBoardIsLandscape();
    uint32_t buf_px;
    if (full) {
        buf_px = screenWidth * screenHeight;
    } else {
        // ST7796: kratšie pásy v internom RAM = menej lag pri kliku (PSRAM SPI blokuje loop)
        buf_px = screenWidth * (land ? 40 : 40);
    }

    size_t nbytes = sizeof(lv_color_t) * buf_px;
    // Full frame (AXS) → PSRAM. Partial (ST7796) → internal/DMA kvôli rýchlosti touch UI.
    uint32_t caps = MALLOC_CAP_8BIT |
                    (full ? MALLOC_CAP_SPIRAM
                          : (land ? (MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) : MALLOC_CAP_INTERNAL));

    disp_draw_buf = (lv_color_t *)heap_caps_malloc(nbytes, caps);
    if (!disp_draw_buf) disp_draw_buf = (lv_color_t *)heap_caps_malloc(nbytes, MALLOC_CAP_8BIT);
    disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(nbytes, caps);
    if (!disp_draw_buf2) disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(nbytes, MALLOC_CAP_8BIT);

    if (!disp_draw_buf) {
        esp_rom_printf("[ui] LVGL draw buf alloc FAILED\n");
    } else {
        esp_rom_printf("[ui] LVGL buf px=%u full=%d buf2=%d free_int=%u free_psram=%u\n",
                       (unsigned)buf_px, (int)full, (int)(disp_draw_buf2 != nullptr),
                       (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                       (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, disp_draw_buf2, buf_px);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    if (full) disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
#if defined(DIA_BOARD_WS_3_5)
    // Kompromis: dosť na tap (neukradnúť do swipe), málo na krátky flick stránky
    indev_drv.scroll_limit = 18;
    indev_drv.scroll_throw = 10;
#else
    indev_drv.scroll_limit = 15;
#endif
    indev_drv.long_press_time = 400;
    lv_indev_drv_register(&indev_drv);
}

/** Vertikálny scroll v dlaždici – bez bounce/zotrvačnosti (full-refresh by sekal). */
static void diaUiMakeScrollSnappy(lv_obj_t *obj) {
    if (!obj) return;
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_style_anim_time(obj, 0, 0);
}

/** Horizontálny tileview – krátky flick stačí na ďalšiu stránku. */
static void diaUiMakeTileviewSwipeFriendly(lv_obj_t *obj) {
    if (!obj) return;
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_style_anim_time(obj, 120, 0);
#else
    lv_obj_set_style_anim_time(obj, 160, 0);
#endif
}

/** Sivíť len Auth/Config; Wifi/Network/Empty pri platných dátach nesiví (blip). */
static bool diaUiHardVisualErr(FetchError err, bool hasValid) {
    if (err == FetchError::Auth || err == FetchError::Config) return true;
    if ((err == FetchError::Wifi || err == FetchError::Empty) && !hasValid) return true;
    return false;
}

/** Číslo ostáva. Server len neodpovedal, účet nie je odhlásený.
 *  Nestaré „Čakám na server“ pri čerstvom čísle / pri Empty (senzor) – mätie. */
static bool diaUiWaitingWithNumber(int slot, FetchError err, bool hasNumber) {
    if (!hasNumber) return false;
    if (err == FetchError::Auth || err == FetchError::Config || err == FetchError::Empty) return false;
    if (!diaFetchSlotWaiting(slot)) return false;
    if (slot < 0 || slot > 1) return diaFetchSlotWaiting(0);
    const DiaSlotReading &s = slotReadings[slot];
    if (s.data_is_stale) return false; // radšej „Staré“
    time_t now = time(nullptr);
    if (now > 100000 && s.timestamp > 0) {
        const int ageMin = (int)((now - s.timestamp) / 60);
        if (ageMin < 5) return false; // krátky blip pri čerstvých dátach – ticho
    }
    return true;
}

static int faceY(int y240) {
#if defined(DIA_BOARD_WS_3_5)
    return (y240 * DIA_FACE_SIZE) / 240;
#else
    return y240;
#endif
}

static int faceX(int x240) {
#if defined(DIA_BOARD_WS_3_5)
    return (x240 * DIA_FACE_SIZE) / 240;
#else
    return x240;
#endif
}

static void diaUiStyleSpark(lv_obj_t *chart, bool withGrid) {
    if (!chart) return;
    if (withGrid) {
        lv_chart_set_div_line_count(chart, 3, 4);
        lv_obj_set_style_bg_opa(chart, LV_OPA_10, LV_PART_MAIN);
        lv_obj_set_style_bg_color(chart, lv_color_hex(0x222222), LV_PART_MAIN);
        lv_obj_set_style_border_width(chart, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(chart, lv_color_hex(0x444444), LV_PART_MAIN);
        lv_obj_set_style_pad_all(chart, 4, LV_PART_MAIN);
        lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);
        lv_obj_set_style_line_color(chart, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
        lv_obj_set_style_line_opa(chart, LV_OPA_60, LV_PART_MAIN);
    } else {
        // Kruh: čistý sparkline bez mriežky/rámu – šetrí miesto
        lv_chart_set_div_line_count(chart, 0, 0);
        lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(chart, 0, LV_PART_MAIN);
        lv_obj_set_style_line_opa(chart, LV_OPA_TRANSP, LV_PART_MAIN);
    }
    lv_obj_set_style_radius(chart, withGrid ? 4 : 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
}

void updateBrightness() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        diaBoardSetBrightness((uint8_t)day_brightness);
        return;
    }
    int hour = timeinfo.tm_hour;
    bool isDay = false;

    if (day_start_hour < night_start_hour) {
        isDay = (hour >= day_start_hour && hour < night_start_hour);
    } else {
        isDay = (hour >= day_start_hour || hour < night_start_hour);
    }

    if (isDay) diaBoardSetBrightness((uint8_t)day_brightness);
    else diaBoardSetBrightness((uint8_t)night_brightness);
}

void updateClockDisplay() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char time_str[10];
        snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        lv_label_set_text(label_time, time_str);
    } else {
        lv_label_set_text(label_time, "--:--");
    }
    updateStaleAndErrorVisuals();
}

static const char *sourceFaceNameOf(const String &src) {
    if (src == "libre") return "Libre";
    if (src == "dexcom") return "Dexcom";
    if (src == "guardian") return "Guardian";
    if (src == "nightscout" || src == "ns") return "Nightscout";
    return "";
}

static const char *sourceFaceName() {
    return sourceFaceNameOf(dsSource);
}

static bool diaUiIsSplitMode() {
#if defined(DIA_BOARD_WS_3_5)
    // Režim 2 = twin UI aj keď B ešte nemá zdroj (panel ukáže …)
    return display_mode >= 2;
#else
    return false;
#endif
}

/** Meno dieťaťa v ľavom okraji, mimo farebného kruhu (kruh začína na x ≈ 90). */
static void diaUiPlaceSingleName() {
#if defined(DIA_BOARD_WS_3_5)
    if (!label_name0) return;
    // Pri otvorenom menu/info/OTA nesmie ísť ponad tlačidlá (move_foreground)
    if (diaUiFaceOverlaysOpen()) {
        lv_obj_add_flag(label_name0, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (watchface_style == 3) {
        lv_obj_add_flag(label_name0, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const bool show = !diaUiIsSplitMode() && diaProfileConfigured(0) && diaProfileConfigured(1);
    if (!show) {
        lv_obj_add_flag(label_name0, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const char *nm = profiles[diaShownSlot()].name.c_str();
    if (!nm || !nm[0]) nm = diaShownSlot() == 0 ? "A" : "B";
    lv_label_set_text(label_name0, nm);
    lv_obj_set_width(label_name0, 70);
    lv_label_set_long_mode(label_name0, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label_name0, &font_sk_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_name0, lv_color_hex(0x9fd4f5), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_name0, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(label_name0, -2, LV_PART_MAIN);
    lv_obj_align(label_name0, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_clear_flag(label_name0, LV_OBJ_FLAG_HIDDEN);
    // Bez move_foreground – inak meno po refreshi vylezie ponad menu
#else
    (void)0;
#endif
}

/** Dual: 0 = čísla, 1 = grafy, 2 = pumpa (len NS + show_pump). */
static int diaUiSplitPage() {
#if defined(DIA_BOARD_WS_3_5)
    if (watchface_style == 2) return 1;
    if (watchface_style == 3 && show_pump && diaAnyNightscout()) return 2;
    return 0;
#else
    return 0;
#endif
}

static void updateSourceLabel() {
    if (!label_source) return;
    if (!show_source) {
        lv_label_set_text(label_source, "");
        lv_obj_add_flag(label_source, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(label_source, sourceFaceName());
    lv_obj_clear_flag(label_source, LV_OBJ_FLAG_HIDDEN);
}

static void place_top(lv_obj_t *obj, int x, int y) {
    if (!obj) return;
    // Na 3.5" posunieme ciferník do stredu výšky; na kruhu OFFSET=0
    lv_obj_align(obj, LV_ALIGN_TOP_MID, x, y + DIA_FACE_OFFSET_Y);
}

/** Single ciferník: stránky 0..2 (+ 3 = pumpa len pri NS a show_pump). */
static int diaUiFacePageCount() {
    return (show_pump && diaAnyNightscout()) ? 4 : 3;
}

static bool diaUiProfileIsNs(int slot) {
    if (slot < 0 || slot > 1) return false;
    const String &s = profiles[slot].dsSource;
    return s == "nightscout" || s == "ns";
}

static void diaUiHidePumpExtras() {
    if (arc_pump_cob) lv_obj_add_flag(arc_pump_cob, LV_OBJ_FLAG_HIDDEN);
    if (arc_pump_iob) lv_obj_add_flag(arc_pump_iob, LV_OBJ_FLAG_HIDDEN);
    if (arc_pump_bg) lv_obj_add_flag(arc_pump_bg, LV_OBJ_FLAG_HIDDEN);
    if (label_pump_cob) lv_obj_add_flag(label_pump_cob, LV_OBJ_FLAG_HIDDEN);
    if (label_pump_iob) lv_obj_add_flag(label_pump_iob, LV_OBJ_FLAG_HIDDEN);
    if (label_pump_cob_val) lv_obj_add_flag(label_pump_cob_val, LV_OBJ_FLAG_HIDDEN);
    if (label_pump_iob_val) lv_obj_add_flag(label_pump_iob_val, LV_OBJ_FLAG_HIDDEN);
    if (label_pump) lv_obj_add_flag(label_pump, LV_OBJ_FLAG_HIDDEN);
}

/** Plná šírka farebného oblúka (žiara ju používa aj keď je vizuálny prstenec tenší). */
static int diaUiArcWidthFull() {
    return (DIA_FACE_SIZE >= 280) ? 18 : 15;
}

static void diaUiRestoreMainArcStyle() {
    if (!arc_bg) return;
    lv_obj_set_size(arc_bg, DIA_FACE_SIZE, DIA_FACE_SIZE);
    const int arc_w = diaUiArcWidthFull();
    lv_obj_set_style_arc_width(arc_bg, arc_w, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_bg, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc_bg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_bg, arc_w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc_bg, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_center(arc_bg);
    lv_obj_clear_flag(arc_bg, LV_OBJ_FLAG_HIDDEN);
}

/** Pump face: farebný prstenec o polovicu užší; žiara ostáva na plnej šírke. */
static void diaUiApplyPumpArcStyle() {
    diaUiRestoreMainArcStyle();
    if (!arc_bg) return;
    const int thin = diaUiArcWidthFull() / 2;
    lv_obj_set_style_arc_width(arc_bg, thin, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_bg, thin, LV_PART_INDICATOR);
}

/** Pozícia v súradniciach 240×240 face (ľavý horný roh objektu). */
static void place_face_lt(lv_obj_t *obj, int x240, int y240) {
    if (!obj) return;
    const int ox = ((int)screenWidth - DIA_FACE_SIZE) / 2;
    const int oy = DIA_FACE_OFFSET_Y;
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, ox + faceX(x240), oy + faceY(y240));
}

static void place_face_mid(lv_obj_t *obj, int cx240, int cy240) {
    if (!obj) return;
    lv_obj_update_layout(obj);
    const int ox = ((int)screenWidth - DIA_FACE_SIZE) / 2;
    const int oy = DIA_FACE_OFFSET_Y;
    const int w = lv_obj_get_width(obj);
    const int h = lv_obj_get_height(obj);
    // align TOP_LEFT – set_pos by nechal predchádzajúci TOP_MID z place_top
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, ox + faceX(cx240) - w / 2, oy + faceY(cy240) - h / 2);
}

static void diaUiRefreshPumpBtn() {
    if (!lbl_btn_pump) return;
    const bool ns = diaAnyNightscout();
    if (ns) {
        lv_obj_clear_flag(lbl_btn_pump, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_btn_pump, S(show_pump ? Str::PumpOn : Str::PumpOff));
    } else {
        lv_obj_add_flag(lbl_btn_pump, LV_OBJ_FLAG_HIDDEN);
    }
}

/** Satelitné kruhy: Sach↖ IOB↙ Delta↘ – glykémia ostáva vždy (aj bez pump JSON). */
static void diaUiColorPumpSatellites(uint32_t color) {
    lv_obj_t *arcs[] = {arc_pump_cob, arc_pump_iob, arc_pump_bg};
    for (lv_obj_t *a : arcs) {
        if (!a) continue;
        lv_obj_set_style_arc_color(a, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_arc_color(a, lv_color_hex(color), LV_PART_INDICATOR);
    }
}

/** Stred objektu v absolútnych px obrazovky (satelity mimo face kruhu). */
static void place_screen_mid(lv_obj_t *obj, int cx, int cy) {
    if (!obj) return;
    lv_obj_update_layout(obj);
    const int w = lv_obj_get_width(obj);
    const int h = lv_obj_get_height(obj);
    lv_obj_align(obj, LV_ALIGN_TOP_LEFT, cx - w / 2, cy - h / 2);
}

/** Kruhový pump face (240): vľavo Sach → gly(+šípka vnútri) → IOB s medzerami;
 *  vpravo väčší čas (vycentrovaný) → graf nižšie → vek. */
static void diaUiPlacePumpFaceRound() {
    // Medzera medzi satelitmi; Sach/gly/IOB viac dovnútra (nie na obvod)
    const int gap = 8;
    const int cobS = 48, glyS = 72, iobS = 48;
    const int cobCx = 70, cobCy = 52;
    const int glyCx = 56; // trochu vľavo
    const int glyCy = cobCy + cobS / 2 + gap + glyS / 2;
    const int iobCx = 82;
    const int iobCy = glyCy + glyS / 2 + gap + iobS / 2;
    const int rightCx = 158;

    if (arc_pump_cob) {
        lv_obj_set_size(arc_pump_cob, cobS, cobS);
        place_face_mid(arc_pump_cob, cobCx, cobCy);
        lv_obj_clear_flag(arc_pump_cob, LV_OBJ_FLAG_HIDDEN);
    }
    if (arc_pump_bg) {
        lv_obj_set_size(arc_pump_bg, glyS, glyS);
        place_face_mid(arc_pump_bg, glyCx, glyCy);
        lv_obj_clear_flag(arc_pump_bg, LV_OBJ_FLAG_HIDDEN);
    }
    if (arc_pump_iob) {
        lv_obj_set_size(arc_pump_iob, iobS, iobS);
        place_face_mid(arc_pump_iob, iobCx, iobCy);
        lv_obj_clear_flag(arc_pump_iob, LV_OBJ_FLAG_HIDDEN);
    }

    // Nápis (12) hore, hodnota v kruhu (Sach menšia aby sa zmestila; IOB 20)
    if (label_pump_cob) {
        lv_obj_set_width(label_pump_cob, cobS - 8);
        lv_obj_set_style_text_font(label_pump_cob, &font_sk_12, LV_PART_MAIN);
        lv_obj_set_style_text_align(label_pump_cob, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(label_pump_cob, "Sach.");
        place_face_mid(label_pump_cob, cobCx, cobCy - 7);
        lv_obj_clear_flag(label_pump_cob, LV_OBJ_FLAG_HIDDEN);
    }
    if (label_pump_cob_val) {
        lv_obj_set_width(label_pump_cob_val, cobS - 8);
        lv_obj_set_style_text_font(label_pump_cob_val, &font_sk_16, LV_PART_MAIN);
        lv_obj_set_style_text_align(label_pump_cob_val, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(label_pump_cob_val, LV_LABEL_LONG_CLIP);
        place_face_mid(label_pump_cob_val, cobCx, cobCy + 7);
        lv_obj_clear_flag(label_pump_cob_val, LV_OBJ_FLAG_HIDDEN);
    }
    if (label_pump_iob) {
        lv_obj_set_width(label_pump_iob, iobS - 8);
        lv_obj_set_style_text_font(label_pump_iob, &font_sk_12, LV_PART_MAIN);
        lv_obj_set_style_text_align(label_pump_iob, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(label_pump_iob, "IOB");
        place_face_mid(label_pump_iob, iobCx, iobCy - 7);
        lv_obj_clear_flag(label_pump_iob, LV_OBJ_FLAG_HIDDEN);
    }
    if (label_pump_iob_val) {
        lv_obj_set_width(label_pump_iob_val, iobS - 6);
        lv_obj_set_style_text_font(label_pump_iob_val, &font_sk_20, LV_PART_MAIN);
        lv_obj_set_style_text_align(label_pump_iob_val, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(label_pump_iob_val, LV_LABEL_LONG_CLIP);
        place_face_mid(label_pump_iob_val, iobCx, iobCy + 7);
        lv_obj_clear_flag(label_pump_iob_val, LV_OBJ_FLAG_HIDDEN);
    }

    // Gly + trend vyššie v kruhu, väčší font gly
    lv_obj_set_style_text_font(label_trend, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_glycemia, &font_sk_28, LV_PART_MAIN);
    lv_obj_set_width(label_glycemia, glyS - 12);
    lv_obj_set_style_text_align(label_glycemia, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label_glycemia, LV_LABEL_LONG_CLIP);
    place_face_mid(label_trend, glyCx, glyCy - 16);
    lv_obj_clear_flag(label_trend, LV_OBJ_FLAG_HIDDEN);
    place_face_mid(label_glycemia, glyCx, glyCy + 6);
    lv_obj_clear_flag(label_glycemia, LV_OBJ_FLAG_HIDDEN);

    if (label_delta) lv_obj_add_flag(label_delta, LV_OBJ_FLAG_HIDDEN);

    // Čas / graf / vek – posunuté nižšie ako jeden blok
    lv_obj_set_style_text_font(label_time, &font_sk_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    place_face_mid(label_time, rightCx, 95); // ~2 mm nižšie; graf/vek bez zmeny
    lv_obj_clear_flag(label_time, LV_OBJ_FLAG_HIDDEN);

    if (chart_spark) {
        diaUiStyleSpark(chart_spark, false);
        lv_obj_set_size(chart_spark, 84, 46);
        place_face_mid(chart_spark, rightCx, 148);
        lv_obj_clear_flag(chart_spark, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_style_text_font(label_age, &font_sk_16, LV_PART_MAIN);
    place_face_mid(label_age, rightCx, 196);
    lv_obj_clear_flag(label_age, LV_OBJ_FLAG_HIDDEN);

    if (arc_pump_bg) lv_obj_move_foreground(arc_pump_bg);
    if (label_trend) lv_obj_move_foreground(label_trend);
    if (label_glycemia) lv_obj_move_foreground(label_glycemia);
    if (label_pump_cob) lv_obj_move_foreground(label_pump_cob);
    if (label_pump_cob_val) lv_obj_move_foreground(label_pump_cob_val);
    if (label_pump_iob) lv_obj_move_foreground(label_pump_iob);
    if (label_pump_iob_val) lv_obj_move_foreground(label_pump_iob_val);
    if (arc_pump_cob) lv_obj_move_foreground(arc_pump_cob);
    if (label_time) lv_obj_move_foreground(label_time);

    place_top(label_status, 0, 2);
    if (label_source) lv_obj_add_flag(label_source, LV_OBJ_FLAG_HIDDEN);
    if (label_pump) lv_obj_add_flag(label_pump, LV_OBJ_FLAG_HIDDEN);
}

/** Široký pump face: stred čas/gly/šípka/graf/vek; satelity Sach↖ IOB↙ Delta↘ mimo kruhu. */
static void diaUiPlacePumpFaceWide() {
    const int sw = (int)screenWidth;
    const int sh = (int)screenHeight;
    const int satS = 80;
    const int labW = 70;
    const int cobCx = satS / 2 + 6;
    const int cobCy = satS / 2 + 8;
    const int iobCx = satS / 2 + 6;
    const int iobCy = sh - satS / 2 - 8;
    const int dltCx = sw - satS / 2 - 6;
    const int dltCy = sh - satS / 2 - 8;

    auto placeSat = [&](lv_obj_t *arc, lv_obj_t *lab, int cx, int cy, bool on) {
        if (!on) {
            if (arc) lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
            if (lab) lv_obj_add_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (arc) {
            lv_obj_set_size(arc, satS, satS);
            place_screen_mid(arc, cx, cy);
            lv_obj_clear_flag(arc, LV_OBJ_FLAG_HIDDEN);
        }
        if (lab) {
            lv_obj_set_width(lab, labW);
            lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_line_space(lab, -2, LV_PART_MAIN);
            lv_label_set_long_mode(lab, LV_LABEL_LONG_WRAP);
            place_screen_mid(lab, cx, cy);
            lv_obj_clear_flag(lab, LV_OBJ_FLAG_HIDDEN);
        }
    };

    placeSat(arc_pump_cob, label_pump_cob, cobCx, cobCy, true);
    placeSat(arc_pump_iob, label_pump_iob, iobCx, iobCy, true);
    placeSat(arc_pump_bg, label_delta, dltCx, dltCy, show_delta);
    if (label_pump_cob_val) lv_obj_add_flag(label_pump_cob_val, LV_OBJ_FLAG_HIDDEN);
    if (label_pump_iob_val) lv_obj_add_flag(label_pump_iob_val, LV_OBJ_FLAG_HIDDEN);
    if (show_delta && label_delta) {
        lv_obj_set_style_text_font(label_delta, &font_sk_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(label_delta, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    }

    const int arcW = (DIA_FACE_SIZE >= 280) ? 18 : 15;
    const int innerTop = arcW + 22;
    const int timeH = 48;
    const int glyH = 72;
    const int ageH = 22;
    const int gapTimeGly = 6;
    const int gapGlyTrend = 6;
    const int gapChartAge = 6;

    int y = innerTop;
    place_top(label_time, 0, y);
    lv_obj_clear_flag(label_time, LV_OBJ_FLAG_HIDDEN);

    y += timeH + gapTimeGly;
    place_top(label_glycemia, 0, y);
    lv_obj_clear_flag(label_glycemia, LV_OBJ_FLAG_HIDDEN);

    y += glyH + gapGlyTrend;
    place_top(label_trend, 0, y);
    lv_obj_clear_flag(label_trend, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(label_trend);
    const int trendBottomFace =
        lv_obj_get_y(label_trend) + lv_obj_get_height(label_trend) - DIA_FACE_OFFSET_Y;

    const int bottomPad = 30;
    const int ageY = DIA_FACE_SIZE - arcW - bottomPad - ageH;
    const int chartBot = ageY - gapChartAge;
    int chartH = 160;
    int chartY = chartBot - chartH;
    const int minChartY = trendBottomFace + 6;
    if (chartY < minChartY) {
        chartY = minChartY;
        chartH = chartBot - chartY;
        if (chartH < 20) chartH = 20;
    }

    if (chart_spark) {
        diaUiStyleSpark(chart_spark, true);
        lv_obj_set_size(chart_spark, 140, chartH);
        place_top(chart_spark, 0, chartY);
        lv_obj_clear_flag(chart_spark, LV_OBJ_FLAG_HIDDEN);
    }

    place_top(label_age, 0, ageY);
    lv_obj_clear_flag(label_age, LV_OBJ_FLAG_HIDDEN);

    place_top(label_status, 0, 2);
    if (label_source) lv_obj_add_flag(label_source, LV_OBJ_FLAG_HIDDEN);
    if (label_pump) lv_obj_add_flag(label_pump, LV_OBJ_FLAG_HIDDEN);
}

static void diaUiPlacePumpFace() {
    if (!label_glycemia) return;
    if (diaBoardIsRound()) diaUiPlacePumpFaceRound();
    else diaUiPlacePumpFaceWide();
    // Pump prvky idú foreground – menu/dialógy musia ostať navrchu
    if (diaUiFaceOverlaysOpen()) diaUiRaiseOverlays();
}

static void diaUiRefreshPumpLabel() {
    if (s_bootUiLock || diaUiIsSplitMode() || watchface_style != 3 || !show_pump ||
        !diaAnyNightscout()) {
        diaUiHidePumpExtras();
        return;
    }
    if (!arc_pump_cob || !label_pump_cob || !label_pump_iob) return;

    const int slot = diaShownSlot();
    const DiaPumpStatus &p = slotPump[slot];
    char cobVal[24], iobVal[24];
    if (p.valid && p.hasCob)
        snprintf(cobVal, sizeof(cobVal), "%.1fg", (double)p.cob);
    else
        snprintf(cobVal, sizeof(cobVal), "-");
    if (p.valid && p.hasIob)
        snprintf(iobVal, sizeof(iobVal), "%.2f", (double)p.iob);
    else
        snprintf(iobVal, sizeof(iobVal), "-");

    if (diaBoardIsRound() && label_pump_cob_val && label_pump_iob_val) {
        lv_label_set_text(label_pump_cob, "Sach.");
        lv_label_set_text(label_pump_iob, "IOB");
        lv_label_set_text(label_pump_cob_val, cobVal);
        lv_label_set_text(label_pump_iob_val, iobVal);
    } else {
        char cob[40], iob[40];
        snprintf(cob, sizeof(cob), "Sach.\n%s", cobVal);
        snprintf(iob, sizeof(iob), "IOB\n%s", iobVal);
        lv_label_set_text(label_pump_cob, cob);
        lv_label_set_text(label_pump_iob, iob);
    }
    diaUiPlacePumpFace();
    diaUiColorPumpSatellites(current_arc_color ? current_arc_color : 0xC41E3Au);
}

void apply_watchface() {
    if (s_bootUiLock) return;
    if (watchface_style < 0 || watchface_style > 3) watchface_style = 0;
    if (watchface_style == 3 && (!show_pump || !diaAnyNightscout())) watchface_style = 0;

#if defined(DIA_BOARD_WS_3_5)
    if (diaUiIsSplitMode()) {
        diaUiHidePumpExtras();
        diaUiApplyDisplayMode();
        return;
    }
#endif

    if (watchface_style != 3) diaUiHidePumpExtras();
    if (watchface_style != 3) diaUiRestoreMainArcStyle();

    if (chart_spark) lv_obj_add_flag(chart_spark, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(label_age, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_delta, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_source, &font_sk_16, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_status, &font_sk_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_glycemia, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(label_trend, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(label_age, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(label_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(label_delta, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(label_glycemia, 0, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(label_age, 0, LV_PART_MAIN);
    lv_obj_set_width(label_glycemia, LV_SIZE_CONTENT);
    lv_obj_set_width(label_age, LV_SIZE_CONTENT);
    lv_obj_set_width(label_delta, LV_SIZE_CONTENT);
    lv_obj_set_width(label_source, LV_SIZE_CONTENT);
    lv_obj_set_width(label_status, 110);
    lv_label_set_long_mode(label_glycemia, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(label_age, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(label_delta, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_line_space(label_delta, 0, LV_PART_MAIN);
    lv_label_set_long_mode(label_status, LV_LABEL_LONG_CLIP);

    // Šípka + delta v jednom riadku (delta opticky na stred šípky).
    auto placeArrowDelta = [&](int arrowY) {
        const int ax = show_delta ? faceX(-22) : 0;
        place_top(label_trend, ax, arrowY);
        if (show_delta) {
            place_top(label_delta, ax + faceX(50), arrowY + faceY(12));
            lv_obj_clear_flag(label_delta, LV_OBJ_FLAG_HIDDEN);
        }
    };

    if (watchface_style == 0) {
        // Hero: číslo nižšie (nie pri hornej hrane), zdroj nad farebným kruhom.
        lv_arc_set_bg_angles(arc_bg, 0, 360);
        lv_arc_set_angles(arc_bg, 0, 360);
        lv_arc_set_rotation(arc_bg, 270);
        lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(label_glycemia, &font_sk_72, LV_PART_MAIN);
        lv_obj_set_style_text_font(label_trend, &font_sk_48, LV_PART_MAIN);

        place_top(label_status, 0, faceY(16));
        // Glykémia + tesne pod ňou šípka (medzera ~polovica oproti predtým)
        place_top(label_glycemia, 0, faceY(68));
        placeArrowDelta(faceY(128)); // bolo 148 – uvoľní miesto na vek
        place_top(label_age, 0, faceY(168));
        // Zdroj stále nad farebným oblúkom (~15 px)
        place_top(label_source, 0, faceY(190));
    } else if (watchface_style == 1) {
        // Časový ciferník – viac vzduchu medzi riadkami.
        lv_arc_set_bg_angles(arc_bg, 45, 315);
        lv_arc_set_angles(arc_bg, 45, 315);
        lv_arc_set_rotation(arc_bg, 90);
        lv_obj_clear_flag(label_time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(label_time, &font_sk_48, LV_PART_MAIN);
        lv_obj_set_style_text_font(label_glycemia, &font_sk_48, LV_PART_MAIN);
        lv_obj_set_style_text_font(label_trend, &font_sk_48, LV_PART_MAIN);

        place_top(label_status, 0, faceY(10));
        place_top(label_source, 0, faceY(28));
        place_top(label_time, 0, faceY(52));
        place_top(label_glycemia, 0, faceY(108));
        placeArrowDelta(faceY(162));
        place_top(label_age, 0, faceY(210));
    } else if (watchface_style == 2) {
        // Graf: od oblúka → zdroj → medzera → gly → medzera → šípka+delta → medzera → vek → medzera → graf
        lv_arc_set_bg_angles(arc_bg, 0, 360);
        lv_arc_set_angles(arc_bg, 0, 360);
        lv_arc_set_rotation(arc_bg, 270);
        lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(label_glycemia, &font_sk_48, LV_PART_MAIN);
        lv_obj_set_style_text_font(label_trend, &font_sk_48, LV_PART_MAIN);

        // Status hore pri okraji (ak je), inak prázdny – neprekáža layoutu
        place_top(label_status, 0, faceY(10));
        place_top(label_source, 0, faceY(26));   // medzera od farebného kruhu
        place_top(label_glycemia, 0, faceY(64)); // väčšia medzera pod názvom senzora
        placeArrowDelta(faceY(116));             // menšia medzera pod glykémiou (48 px font)
        place_top(label_age, 0, faceY(154));     // medzera pod šípkou – bez prekrytia

        if (chart_spark) {
            diaUiStyleSpark(chart_spark, false);
            lv_obj_set_size(chart_spark, faceX(150), faceY(38));
            lv_obj_clear_flag(chart_spark, LV_OBJ_FLAG_HIDDEN);
            place_top(chart_spark, 0, faceY(176));
            refresh_sparkline();
        }
    } else if (watchface_style == 3) {
        // Pump: kruh = Sach/gly/IOB vľavo + čas/graf/vek vpravo; široký = stredný stĺpec + satelity
        const bool round = diaBoardIsRound();
        diaUiApplyPumpArcStyle(); // užší farebný prstenec; žiara ostáva plná
        lv_arc_set_bg_angles(arc_bg, 0, 360);
        lv_arc_set_angles(arc_bg, 0, 360);
        lv_arc_set_rotation(arc_bg, 270);

        lv_obj_clear_flag(label_time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(label_time, &font_sk_48, LV_PART_MAIN);
        lv_obj_set_style_text_color(label_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(label_glycemia, round ? &font_sk_28 : &font_sk_72, LV_PART_MAIN);
        lv_obj_set_style_text_font(label_trend, round ? &font_sk_20 : &font_sk_48, LV_PART_MAIN);
        lv_obj_set_style_text_font(label_age, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
        lv_obj_set_style_text_font(label_delta, &font_sk_16, LV_PART_MAIN);
        if (label_pump_cob)
            lv_obj_set_style_text_font(label_pump_cob, &font_sk_16, LV_PART_MAIN);
        if (label_pump_iob)
            lv_obj_set_style_text_font(label_pump_iob, &font_sk_16, LV_PART_MAIN);

        diaUiPlacePumpFace();
        if (chart_spark) refresh_sparkline();
        updateClockDisplay();
    }

    updateSourceLabel();
    if (watchface_style == 3 && label_source)
        lv_obj_add_flag(label_source, LV_OBJ_FLAG_HIDDEN);
    updateDeltaLabel();
    diaUiRefreshPumpLabel();
    if (diaUiFaceOverlaysOpen()) diaUiRaiseOverlays();
}

static void clearMainOverlays() {
    if (arc_bg) lv_obj_add_flag(arc_bg, LV_OBJ_FLAG_HIDDEN);
    if (chart_spark) lv_obj_add_flag(chart_spark, LV_OBJ_FLAG_HIDDEN);
    if (chart_spark1) lv_obj_add_flag(chart_spark1, LV_OBJ_FLAG_HIDDEN);
    if (panel_frame0) lv_obj_add_flag(panel_frame0, LV_OBJ_FLAG_HIDDEN);
    if (panel_frame1) lv_obj_add_flag(panel_frame1, LV_OBJ_FLAG_HIDDEN);
    if (label_time) {
        lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_time, "");
    }
    if (label_trend) {
        lv_obj_add_flag(label_trend, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_trend, "");
    }
    if (label_delta) {
        lv_obj_add_flag(label_delta, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_delta, "");
    }
    if (label_status) {
        lv_obj_add_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_status, "");
    }
    if (label_source) {
        lv_obj_add_flag(label_source, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_source, "");
    }
    if (label_pump) lv_obj_add_flag(label_pump, LV_OBJ_FLAG_HIDDEN);
    diaUiHidePumpExtras();
    // 3.5B dual: počas WiFi/setup žiadna cache / zdroj v pozadí
    if (label_source1) {
        lv_obj_add_flag(label_source1, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_source1, "");
    }
    if (split_line) lv_obj_add_flag(split_line, LV_OBJ_FLAG_HIDDEN);
    if (label_glycemia1) {
        lv_obj_add_flag(label_glycemia1, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_glycemia1, "");
    }
    if (label_trend1) {
        lv_obj_add_flag(label_trend1, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_trend1, "");
    }
    if (label_delta1) {
        lv_obj_add_flag(label_delta1, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_delta1, "");
    }
    if (label_age1) {
        lv_obj_add_flag(label_age1, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_age1, "");
    }
    if (label_name0) {
        lv_obj_add_flag(label_name0, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_name0, "");
    }
    if (label_name1) {
        lv_obj_add_flag(label_name1, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_name1, "");
    }
}

static bool bootStatusSame(const char *title, const char *detail) {
    const char *t = title ? title : "";
    const char *d = detail ? detail : "";
    return strncmp(s_bootShownTitle, t, sizeof(s_bootShownTitle) - 1) == 0 &&
           strncmp(s_bootShownDetail, d, sizeof(s_bootShownDetail) - 1) == 0;
}

static void bootStatusRemember(const char *title, const char *detail) {
    const char *t = title ? title : "";
    const char *d = detail ? detail : "";
    strncpy(s_bootShownTitle, t, sizeof(s_bootShownTitle) - 1);
    s_bootShownTitle[sizeof(s_bootShownTitle) - 1] = '\0';
    strncpy(s_bootShownDetail, d, sizeof(s_bootShownDetail) - 1);
    s_bootShownDetail[sizeof(s_bootShownDetail) - 1] = '\0';
}

void showBootStatus(const char *title, const char *detail) {
    s_bootUiLock = true;
    // AP callback volá to isté ešte raz uprostred štartu rádia. Druhé kreslenie
    // zhodí posun na začiatok a na chvíľu zamrzne dotyk.
    if (ui_boot_scroll && bootStatusSame(title, detail)) return;
    bootStatusRemember(title, detail);
    // Logo inak zostane nad textom: počas blokujúceho WiFi nebeží loop → timer 2.2 s sa nespustí
    diaUiHideSplash();
    clearMainOverlays();

    // Skryť zvyšok ciferníka – žiadne prekrytie s cache glykémiou / trendom / slot B
    if (label_trend) {
        lv_obj_add_flag(label_trend, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_trend, "");
    }
    if (label_delta) {
        lv_obj_add_flag(label_delta, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_delta, "");
    }
    if (label_source) {
        lv_obj_add_flag(label_source, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_source, "");
    }
    if (label_source1) {
        lv_obj_add_flag(label_source1, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_source1, "");
    }
    if (label_glycemia1) {
        lv_obj_add_flag(label_glycemia1, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_glycemia1, "");
    }
    if (label_name0) {
        lv_obj_add_flag(label_name0, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_name0, "");
    }
    if (label_name1) {
        lv_obj_add_flag(label_name1, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(label_name1, "");
    }
    if (panel_frame0) lv_obj_add_flag(panel_frame0, LV_OBJ_FLAG_HIDDEN);
    if (panel_frame1) lv_obj_add_flag(panel_frame1, LV_OBJ_FLAG_HIDDEN);
    if (split_line) lv_obj_add_flag(split_line, LV_OBJ_FLAG_HIDDEN);
    if (arc_bg) lv_obj_add_flag(arc_bg, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(label_glycemia, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(label_age, LV_OBJ_FLAG_HIDDEN);

    const bool hasTitle = title && title[0];
    if (hasTitle) {
        lv_obj_clear_flag(label_glycemia, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(label_glycemia, &font_sk_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(label_glycemia, lv_color_hex(0x00AAFF), LV_PART_MAIN);
        lv_obj_set_width(label_glycemia, DIA_SCREEN_W >= 400 ? 400 : 180);
        lv_label_set_long_mode(label_glycemia, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(label_glycemia, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_line_space(label_glycemia, 4, LV_PART_MAIN);
        lv_label_set_text(label_glycemia, title);
        lv_obj_set_parent(label_glycemia, lv_scr_act());
        lv_obj_align(label_glycemia, LV_ALIGN_TOP_MID, 0, diaBoardIsRound() ? 12 : 28);
    } else {
        lv_label_set_text(label_glycemia, "");
        lv_obj_add_flag(label_glycemia, LV_OBJ_FLAG_HIDDEN);
    }

    // Scrollovací box pre dlhý AP návod (na kruhu inak odrezaný)
    if (!ui_boot_scroll) {
        ui_boot_scroll = lv_obj_create(lv_scr_act());
        lv_obj_set_style_bg_opa(ui_boot_scroll, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(ui_boot_scroll, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(ui_boot_scroll, 2, LV_PART_MAIN);
        lv_obj_set_scroll_dir(ui_boot_scroll, LV_DIR_VER);
        lv_obj_add_flag(ui_boot_scroll, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(ui_boot_scroll, LV_SCROLLBAR_MODE_OFF);
        // Dojazd by na tomto displeji dobiehal aj počas štartu WiFi a pôsobil ako zaseknutie.
        diaUiMakeScrollSnappy(ui_boot_scroll);
        // Inak dlhé podržanie skončí v návode a menu na obrazovke sa neotvorí.
        lv_obj_add_flag(ui_boot_scroll, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
    lv_obj_clear_flag(ui_boot_scroll, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_parent(label_age, ui_boot_scroll);
    lv_obj_set_style_text_font(label_age, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_age, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_height(label_age, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label_age, LV_LABEL_LONG_WRAP);
    // Dôležité: custom font má malý line_height – bez line_space sa riadky prekrývajú (p/j/g)
    lv_obj_set_style_text_line_space(label_age, 6, LV_PART_MAIN);
    lv_label_set_text(label_age, detail);
    // Viacriadkový návod (AP aj „otvor IP a nastav zdroj“) nemá vetu o pomôcke.
    // Číslované kroky ostávajú vľavo. Krátke „Pripájam…“ / „Načítavam…“ sú v strede.
    int breaks = 0;
    if (detail) {
        for (const char *p = detail; *p; ++p) {
            if (*p == '\n') breaks++;
        }
    }
    const bool guide = breaks >= 2;
    // Číslovaný AP návod začína „1.“. Samotný počet riadkov by IP pod textom posunul doľava.
    const bool steps = detail && detail[0] >= '0' && detail[0] <= '9' && detail[1] == '.';
    lv_obj_set_width(label_age, diaBoardIsRound() ? (guide ? 176 : 190) : (guide ? 400 : 420));
    lv_obj_set_style_text_align(label_age, steps ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_age, steps ? LV_ALIGN_TOP_LEFT : LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(label_age, LV_OBJ_FLAG_CLICKABLE);
    // Návod po súhlase aj po WiFi, kým chýba senzor. Veta by zakryla ďalší krok.
    const bool round = diaBoardIsRound();
    if (guide) {
        if (round) {
            lv_obj_set_size(ui_boot_scroll, 184, hasTitle ? 156 : 188);
            lv_obj_align(ui_boot_scroll, LV_ALIGN_TOP_MID, 0, hasTitle ? 48 : 26);
        } else {
            lv_obj_set_size(ui_boot_scroll, 440, hasTitle ? 210 : 250);
            lv_obj_align(ui_boot_scroll, LV_ALIGN_TOP_MID, 0, hasTitle ? 78 : 28);
        }
    }
    if (!lbl_boot_legal) {
        lbl_boot_legal = lv_label_create(ui_boot_scroll);
        lv_obj_set_style_text_font(lbl_boot_legal, &font_sk_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_boot_legal, lv_color_hex(0xE6EEF6), LV_PART_MAIN);
        lv_label_set_long_mode(lbl_boot_legal, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(lbl_boot_legal, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_line_space(lbl_boot_legal, 2, LV_PART_MAIN);
        lv_obj_set_height(lbl_boot_legal, LV_SIZE_CONTENT);
        lv_obj_clear_flag(lbl_boot_legal, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_parent(lbl_boot_legal, guide ? lv_scr_act() : ui_boot_scroll);
    if (guide) {
        lv_obj_add_flag(lbl_boot_legal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_pad_bottom(ui_boot_scroll, 2, LV_PART_MAIN);
    } else {
        // Veta a „Skenujem siete...“ sú v jednom posuve. Medzera ich oddelí,
        // a keď sa na kruh nezmestia, text sa dá posunúť, nič sa neodreže.
        const int boxW = round ? (hasTitle ? 176 : 180) : 440;
        const int gap = round ? 22 : 28;
        lv_obj_set_width(label_age, boxW - 8);
        lv_obj_set_width(lbl_boot_legal, boxW - 8);
        lv_label_set_text(lbl_boot_legal, S(Str::LegalBoot));
        lv_obj_align(lbl_boot_legal, LV_ALIGN_TOP_MID, 0, 4);
        lv_obj_clear_flag(lbl_boot_legal, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(lbl_boot_legal);
        lv_obj_align_to(label_age, lbl_boot_legal, LV_ALIGN_OUT_BOTTOM_MID, 0, gap);
        lv_obj_set_style_pad_bottom(ui_boot_scroll, 14, LV_PART_MAIN);
        if (round) {
            lv_obj_set_size(ui_boot_scroll, boxW, hasTitle ? 168 : 190);
            lv_obj_align(ui_boot_scroll, LV_ALIGN_TOP_MID, 0, hasTitle ? 52 : 26);
        } else {
            lv_obj_set_size(ui_boot_scroll, boxW, hasTitle ? 210 : 248);
            lv_obj_align(ui_boot_scroll, LV_ALIGN_TOP_MID, 0, hasTitle ? 72 : 32);
        }
    }
    lv_obj_scroll_to_y(ui_boot_scroll, 0, LV_ANIM_OFF);
    if (s_bootLegalFrom == 0) s_bootLegalFrom = millis();

    lv_obj_invalidate(lv_scr_act());
    // Jedno prekreslenie stačí. Viac snímok tu len drží dotyk, kým sa spúšťa WiFi.
    diaUiFlushHandlers(diaBoardPreferFullRefresh() ? 2 : 3, diaBoardPreferFullRefresh() ? 20 : 5);
}

bool diaUiBootLegalHolding() {
    if (s_bootLegalFrom == 0) return false;
    return (uint32_t)(millis() - s_bootLegalFrom) < kBootLegalHoldMs;
}

void restoreMainScreenChrome() {
    s_bootUiLock = false;
    s_bootShownTitle[0] = '\0';
    s_bootShownDetail[0] = '\0';
    diaUiMarkReady();
    if (lbl_boot_legal) lv_obj_add_flag(lbl_boot_legal, LV_OBJ_FLAG_HIDDEN);
    if (ui_boot_scroll) {
        lv_obj_add_flag(ui_boot_scroll, LV_OBJ_FLAG_HIDDEN);
        if (label_age) lv_obj_set_parent(label_age, lv_scr_act());
    }
    lv_obj_set_width(label_glycemia, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label_glycemia, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(label_glycemia, &font_sk_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_glycemia, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_align(label_glycemia, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(label_glycemia, 0, LV_PART_MAIN);

    lv_obj_set_width(label_age, LV_SIZE_CONTENT);
    lv_obj_set_height(label_age, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label_age, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(label_age, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_age, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_line_space(label_age, 0, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_age, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_set_style_text_font(label_trend, &font_sk_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_trend, lv_color_hex(0xAAAAAA), LV_PART_MAIN);

    lv_obj_clear_flag(label_glycemia, LV_OBJ_FLAG_HIDDEN);
    // Číslo nesmie byť klikateľné. Inak dlhé podržanie na hodnote neprejde na obrazovku a menu sa neotvorí.
    diaUiFacePassThroughTouches();
    lv_obj_clear_flag(label_trend, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_age, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(arc_bg, LV_OBJ_FLAG_HIDDEN);
    updateDeltaLabel();

    // Vždy zmaž boot texty – inak „Pripojené!/Načítavam...“ ostane cez glykémiu
    // (updateAgeDisplay pred NTP často skorý return a text neprepíše)
    lv_label_set_text(label_status, "");
    lv_label_set_text(label_age, S(Str::Sync));
    if (!has_valid_reading) {
        lv_label_set_text(label_glycemia, "...");
        lv_label_set_text(label_trend, "");
        lv_label_set_text(label_delta, "");
    }
    // Boot presunie číslo na vrch. Panely musia ostať nad ním, inak hodnota svieti cez text.
    diaUiRaiseOverlays();
}

static void main_screen_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_GESTURE) {
        if (s_bootUiLock || diaUiFaceOverlaysOpen()) return;
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
            if (diaUiIsSplitMode()) {
                // Dual: čísla (0) ↔ grafy (2) ↔ pumpa (3) ak NS+pump
                const bool pumpOk = show_pump && diaAnyNightscout();
                if (pumpOk) {
                    if (dir == LV_DIR_LEFT) {
                        if (watchface_style == 0) watchface_style = 2;
                        else if (watchface_style == 2) watchface_style = 3;
                        else watchface_style = 0;
                    } else {
                        if (watchface_style == 0) watchface_style = 3;
                        else if (watchface_style == 3) watchface_style = 2;
                        else watchface_style = 0;
                    }
                } else {
                    watchface_style = (watchface_style == 2) ? 0 : 2;
                }
            } else {
                // Single: 0 číslo, 1 čas, 2 graf, (+3 pumpa len pri NS + show_pump)
                const int n = diaUiFacePageCount();
                if (dir == LV_DIR_LEFT) watchface_style = (watchface_style + 1) % n;
                else watchface_style = (watchface_style + n - 1) % n;
            }
            preferences.putInt("wf_style", watchface_style);
            apply_watchface();
            updateDeltaLabel();
            lv_obj_invalidate(lv_scr_act());
            was_long_pressed = false;
            gesture_just_handled = true;
        } else if (dir == LV_DIR_TOP || dir == LV_DIR_BOTTOM) {
            Serial.println("[ui] swipe refresh");
            diaUiRequestManualFetch();
            was_long_pressed = false;
            gesture_just_handled = true;
        }
    } else if (code == LV_EVENT_LONG_PRESSED) {
        was_long_pressed = true;
    } else if (code == LV_EVENT_RELEASED) {
        if (gesture_just_handled) {
            gesture_just_handled = false;
            was_long_pressed = false;
            return;
        }
        if (was_long_pressed) {
            diaUiOpenSettings();
            was_long_pressed = false;
        } else if (!diaUiFaceOverlaysOpen()) {
            if (sound_on && diaBoardHasAudio() && diaAudioAnyCritical()) {
                diaAudioSnooze();
                updateStaleAndErrorVisuals();
            }
        }
    }
}

void diaUiOpenSettings() {
    if (!ui_settings_panel) return;
    if (ui_sound_panel) lv_obj_add_flag(ui_sound_panel, LV_OBJ_FLAG_HIDDEN);
    if (label_name0) lv_obj_add_flag(label_name0, LV_OBJ_FLAG_HIDDEN);
    if (label_name1) lv_obj_add_flag(label_name1, LV_OBJ_FLAG_HIDDEN);
    diaUiRefreshPumpBtn();
    lv_obj_clear_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_settings_panel);
}

static void delayed_close_settings_task(lv_timer_t *timer) {
    lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    if (ui_sound_panel) lv_obj_add_flag(ui_sound_panel, LV_OBJ_FLAG_HIDDEN);
    // Počas AP/boot návodu nechať WiFi SETUP text – neobnovovať ciferník
    if (!s_bootUiLock) {
        updateBrightness();
        diaUiPlaceSingleName();
    }
    lv_timer_del(timer);
}

static void close_settings_cb(lv_event_t *e) {
    (void)e;
    // AP wait loop volá lv_timer_handler zriedkavo medzi WiFi – skryť hneď
    if (s_bootUiLock) {
        lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
        if (ui_sound_panel) lv_obj_add_flag(ui_sound_panel, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_timer_create(delayed_close_settings_task, 50, NULL);
}

static void diaUiPlaceLegalInfo() {
    if (!lbl_info_status || !lbl_info_legal) return;
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(lbl_info_status, 420);
    lv_obj_align(lbl_info_status, LV_ALIGN_TOP_MID, 0, 118);
    lv_obj_set_width(lbl_info_legal, 420);
#else
    lv_obj_set_width(lbl_info_status, 196);
    lv_obj_align(lbl_info_status, LV_ALIGN_TOP_MID, 0, 96);
    lv_obj_set_width(lbl_info_legal, 196);
#endif
    lv_obj_align_to(lbl_info_legal, lbl_info_status, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
}

static void diaUiSetLegalInfoText() {
    if (!lbl_info_status || !lbl_info_legal) return;
    const bool ok = diaLegalAccepted();
    lv_label_set_text(lbl_info_status, S(ok ? Str::LegalInfoOk : Str::LegalInfoNo));
    lv_obj_set_style_text_color(lbl_info_status, lv_color_hex(ok ? 0x3DDC97 : 0xFFAA44), LV_PART_MAIN);
    if (ok) {
        static char shown[768];
        snprintf(shown, sizeof(shown), "%s\n\n%s", S(Str::LegalInfoStored), S(Str::LegalIndep));
        lv_label_set_text(lbl_info_legal, shown);
    } else {
        lv_label_set_text(lbl_info_legal, S(Str::LegalInfoMissing));
    }
    diaUiPlaceLegalInfo();
}

static void showInfoValuesLayout() {
    if (ui_info_scroll) {
        lv_obj_clear_flag(ui_info_scroll, LV_OBJ_FLAG_HIDDEN);
        if (lbl_info_text) lv_obj_set_parent(lbl_info_text, ui_info_scroll);
    }
    if (lbl_info_keys) lv_obj_clear_flag(lbl_info_keys, LV_OBJ_FLAG_HIDDEN);
    if (lbl_info_status) lv_obj_clear_flag(lbl_info_status, LV_OBJ_FLAG_HIDDEN);
    if (lbl_info_legal) lv_obj_clear_flag(lbl_info_legal, LV_OBJ_FLAG_HIDDEN);
    diaUiSetLegalInfoText();
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(lbl_info_text, 180);
    lv_obj_align(lbl_info_text, LV_ALIGN_TOP_MID, 90, 0);
#else
    lv_obj_set_width(lbl_info_text, 100);
    lv_obj_align(lbl_info_text, LV_ALIGN_TOP_MID, 48, 0);
#endif
    lv_label_set_long_mode(lbl_info_text, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(lbl_info_text, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_line_space(lbl_info_text, 2, LV_PART_MAIN);
}

void showInfoMessageLayout(const char *msg) {
    if (ui_info_scroll) lv_obj_add_flag(ui_info_scroll, LV_OBJ_FLAG_HIDDEN);
    if (lbl_info_text) lv_obj_set_parent(lbl_info_text, ui_info_panel);
    if (lbl_info_keys) lv_obj_add_flag(lbl_info_keys, LV_OBJ_FLAG_HIDDEN);
    if (lbl_info_status) lv_obj_add_flag(lbl_info_status, LV_OBJ_FLAG_HIDDEN);
    if (lbl_info_legal) lv_obj_add_flag(lbl_info_legal, LV_OBJ_FLAG_HIDDEN);
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(lbl_info_text, 400);
#else
    lv_obj_set_width(lbl_info_text, 150);
#endif
    lv_label_set_long_mode(lbl_info_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl_info_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(lbl_info_text, 2, LV_PART_MAIN);
    lv_label_set_text(lbl_info_text, msg);
    lv_obj_align(lbl_info_text, LV_ALIGN_CENTER, 0, -12);
    if (ui_info_panel) lv_obj_move_foreground(ui_info_panel);
}

static void delayed_open_info_task(lv_timer_t *timer) {
    char ver[24];
    const int srv = diaOtaKnownVersion();
    if (srv > FIRMWARE_VERSION) {
        snprintf(ver, sizeof(ver), "%d.%d >%d.%d",
                 FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, srv / 100, srv % 100);
    } else {
        snprintf(ver, sizeof(ver), "%d.%d", FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR);
    }
    char info_buf[128];
    snprintf(info_buf, sizeof(info_buf),
             "%s\n%d dBm\n%.1f °C\n%s",
             WiFi.localIP().toString().c_str(),
             WiFi.RSSI(),
             temperatureRead(),
             ver);

    showInfoValuesLayout();
    lv_label_set_text(lbl_info_text, info_buf);
    diaUiSetLegalInfoText();
    if (ui_info_scroll) lv_obj_scroll_to_y(ui_info_scroll, 0, LV_ANIM_OFF);
    lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_info_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_info_panel);
    lv_timer_del(timer);
}

static void open_info_cb(lv_event_t *e) {
    (void)e;
    lv_timer_create(delayed_open_info_task, 50, NULL);
}

static void delayed_close_info_task(lv_timer_t *timer) {
    lv_obj_add_flag(ui_info_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    showInfoMessageLayout(S(Str::Loading));
    lv_timer_del(timer);
}

static void close_info_cb(lv_event_t *e) {
    (void)e;
    lv_timer_create(delayed_close_info_task, 50, NULL);
}

static void delayed_manual_update_task(lv_timer_t *timer) {
    check_firmware_update(true);
    lv_timer_del(timer);
}

static void manual_update_cb(lv_event_t *e) {
    (void)e;
    lv_timer_create(delayed_manual_update_task, 50, NULL);
}

static void toggle_autoupdate_cb(lv_event_t *e) {
    (void)e;
    auto_update = !auto_update;
    preferences.putBool("auto_upd", auto_update);
    lv_label_set_text(lbl_btn_autoupdate, S(auto_update ? Str::AutoOn : Str::AutoOff));
}

static void diaUiRefreshChildBtn() {
#if defined(DIA_BOARD_WS_3_5)
    if (!lbl_btn_child) return;
    lv_obj_t *btn = lv_obj_get_parent(lbl_btn_child);
    const bool both = diaProfileConfigured(0) && diaProfileConfigured(1);
    const bool show = display_mode < 2 && both;
    if (!btn) return;
    if (!show) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);
    const char *nm = profiles[diaShownSlot()].name.c_str();
    if (!nm || !nm[0]) nm = diaShownSlot() == 0 ? "A" : "B";
    char buf[48];
    snprintf(buf, sizeof(buf), S(Str::ShowChild), nm);
    lv_label_set_text(lbl_btn_child, buf);
#else
    (void)0;
#endif
}

static void diaUiFocusShownSlot() {
    const int shown = diaShownSlot();
    diaConfigPushSlot(shown);
    glucoseSyncGlobalsFromSlot(shown);
}

static void toggle_child_cb(lv_event_t *e) {
    (void)e;
#if defined(DIA_BOARD_WS_3_5)
    if (!(diaProfileConfigured(0) && diaProfileConfigured(1))) return;
    single_slot = diaShownSlot() == 0 ? 1 : 0;
    preferences.putUChar("face_slot", single_slot);
    diaUiFocusShownSlot();
    diaUiApplyDisplayMode();
    diaUiRefreshChildBtn();
    forceFetch = true;
#endif
}

static void toggle_display_mode_cb(lv_event_t *e) {
    (void)e;
#if defined(DIA_BOARD_WS_3_5)
    display_mode = (display_mode >= 2) ? 1 : 2;
    preferences.putUChar("disp_mode", display_mode);
    diaConfigSaveProfiles();
    if (lbl_btn_mode) {
        lv_label_set_text(lbl_btn_mode, S(display_mode >= 2 ? Str::ModeBtn2 : Str::ModeBtn1));
    }
    diaUiFocusShownSlot();
    diaUiApplyDisplayMode();
    diaUiRefreshChildBtn();
    // Počas IP setupu nespúšťať fetch (prepísal by Sync)
    if (!s_bootUiLock) forceFetch = true;
#else
    (void)e;
#endif
}

static void toggle_units_cb(lv_event_t *e) {
    (void)e;
    use_mgdl = !use_mgdl;
    preferences.putBool("use_mgdl", use_mgdl);
    update_target_btn_labels();
    refresh_glucose_label();
    updateDeltaLabel();
    lv_obj_invalidate(lv_scr_act());
}

static void toggle_delta_cb(lv_event_t *e) {
    (void)e;
    show_delta = !show_delta;
    preferences.putBool("show_delta", show_delta);
    if (lbl_btn_delta) {
        lv_label_set_text(lbl_btn_delta, S(show_delta ? Str::DeltaOn : Str::DeltaOff));
    }
    if (diaUiIsSplitMode()) {
        diaUiApplyDisplayMode();
    } else {
        apply_watchface();
    }
}

static void toggle_sound_cb(lv_event_t *e) {
    (void)e;
    if (!diaBoardHasAudio()) return;
    sound_on = !sound_on;
    preferences.putBool("sound_on", sound_on);
    if (!sound_on) {
        diaAudioClearSnooze();
    } else {
        // Okamžitý testový chime – overenie HW bez čakania na critical
        diaAudioResetChimeTimer();
        diaAudioPlayChime();
    }
    if (lbl_btn_sound) {
        lv_label_set_text(lbl_btn_sound, S(sound_on ? Str::SoundOn : Str::SoundOff));
    }
    updateStaleAndErrorVisuals();
}

static void cycle_snooze_cb(lv_event_t *e) {
    (void)e;
    if (snooze_min == 15) snooze_min = 30;
    else if (snooze_min == 30) snooze_min = 60;
    else snooze_min = 15;
    preferences.putInt("snooze_min", snooze_min);
    if (lbl_btn_snooze) {
        char buf[32];
        snprintf(buf, sizeof(buf), S(Str::SnoozeFmt), snooze_min);
        lv_label_set_text(lbl_btn_snooze, buf);
    }
}

static void snooze_now_cb(lv_event_t *e) {
    (void)e;
    diaAudioSnooze();
    updateStaleAndErrorVisuals();
}

static void toggle_source_cb(lv_event_t *e) {
    (void)e;
    show_source = !show_source;
    preferences.putBool("show_src", show_source);
    if (lbl_btn_source) {
        lv_label_set_text(lbl_btn_source, S(show_source ? Str::SourceOn : Str::SourceOff));
    }
    if (diaUiIsSplitMode()) {
        diaUiApplyDisplayMode();
    } else {
        apply_watchface();
    }
}

static void toggle_pump_cb(lv_event_t *e) {
    (void)e;
    show_pump = !show_pump;
    preferences.putBool("show_pump", show_pump);
    diaUiRefreshPumpBtn();
    if (!show_pump) {
        diaPumpClear(0);
        diaPumpClear(1);
        if (watchface_style == 3) {
            watchface_style = 0;
            preferences.putInt("wf_style", watchface_style);
        }
        diaUiHidePumpExtras();
    } else if (diaAnyNightscout()) {
        // Single aj dual: hneď pump stránka (swipe 3.)
        watchface_style = 3;
        preferences.putInt("wf_style", watchface_style);
        forceFetch = true; // IOB/COB – glykémia sa nemaže
    }
    if (diaUiIsSplitMode()) {
        diaUiApplyDisplayMode();
    } else {
        apply_watchface();
    }
    // Zapnutie pumpy nesmie prekryť otvorené menu
    if (diaUiFaceOverlaysOpen()) diaUiRaiseOverlays();
}

static void toggle_glow_cb(lv_event_t *e) {
    (void)e;
    show_glow = !show_glow;
    preferences.putBool("show_glow", show_glow);
    if (lbl_btn_glow) lv_label_set_text(lbl_btn_glow, S(show_glow ? Str::GlowOn : Str::GlowOff));
    lv_obj_invalidate(lv_scr_act());
}

static Str diaUiOrientStr() {
    return orient_mode ? Str::OrientUsbL : Str::OrientUsbR;
}

static void diaUiRefreshOrientBtn() {
    if (lbl_btn_orient) lv_label_set_text(lbl_btn_orient, S(diaUiOrientStr()));
}

static void toggle_orient_cb(lv_event_t *e) {
    (void)e;
    if (!diaBoardSupportsOrient()) return;
    orient_mode = orient_mode ? 0 : 1;
    preferences.putUChar("orient", orient_mode);
    preferences.putBool("orient_v2", true);
    diaBoardSetOrientMode(orient_mode);
    diaUiRefreshOrientBtn();
    lv_obj_invalidate(lv_scr_act());
}

static const Str kVolNames[] = {Str::VolQuiet, Str::VolLow, Str::VolMid, Str::VolHigh};

static void refreshVolLabel() {
    int v = sound_volume;
    if (v < 0) v = 0;
    if (v > 3) v = 3;
    if (lbl_vol) lv_label_set_text(lbl_vol, S(kVolNames[v]));
}

static void open_sound_cb(lv_event_t *e) {
    (void)e;
    if (!ui_sound_panel) return;
    refreshVolLabel();
    lv_obj_clear_flag(ui_sound_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_sound_panel);
}

static void close_sound_cb(lv_event_t *e) {
    (void)e;
    if (ui_sound_panel) lv_obj_add_flag(ui_sound_panel, LV_OBJ_FLAG_HIDDEN);
}

static void vol_step_cb(lv_event_t *e) {
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    int v = sound_volume + dir;
    if (v < 0) v = 0;
    if (v > 3) v = 3;
    if (v == sound_volume) return;
    sound_volume = v;
    preferences.putUChar("sound_vol", (uint8_t)sound_volume);
    refreshVolLabel();
    if (sound_on && diaBoardHasAudio()) diaAudioPlayAck();
}

static void diaUiRefreshPinBtns() {
    if (lbl_btn_pin) lv_label_set_text(lbl_btn_pin, S(diaPinIsSet() ? Str::PinOn : Str::PinOff));
    if (lbl_btn_pin_clear) {
        lv_obj_t *btn = lv_obj_get_parent(lbl_btn_pin_clear);
        if (diaPinIsSet()) lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static void pin_show_status(const char *text, bool visible) {
    if (!lbl_pin_title) return;
    lv_label_set_text(lbl_pin_title, text);
    lv_obj_set_style_text_color(lbl_pin_title, lv_color_hex(0xFFCC66), LV_PART_MAIN);
    if (visible) lv_obj_clear_flag(lbl_pin_title, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(lbl_pin_title, LV_OBJ_FLAG_HIDDEN);
}

static void pin_show_digits() {
    char vis[12];
    int n = 0;
    for (int i = 0; i < 4; i++) {
        vis[n++] = (i < s_pinLen) ? s_pinEntry[i] : '_';
        if (i < 3) vis[n++] = ' ';
    }
    vis[n] = '\0';
    if (lbl_pin_digits) lv_label_set_text(lbl_pin_digits, vis);
}

static void pin_reset_entry() {
    s_pinLen = 0;
    s_pinEntry[0] = '\0';
    pin_show_digits();
}

static void pin_close() {
    memset(s_pinEntry, 0, sizeof(s_pinEntry));
    memset(s_pinFirst, 0, sizeof(s_pinFirst));
    s_pinLen = 0;
    s_pinStage = 0;
    if (ui_pin_panel) lv_obj_add_flag(ui_pin_panel, LV_OBJ_FLAG_HIDDEN);
    if (ui_settings_panel) lv_obj_clear_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
}

static void pin_commit() {
    if (s_pinLen != 4) return;
    if (s_pinStage == 0) {
        memcpy(s_pinFirst, s_pinEntry, 5);
        s_pinStage = 1;
        pin_reset_entry();
        if (lbl_pin_title) pin_show_status(S(Str::PinAgain), true);
        return;
    }
    if (memcmp(s_pinFirst, s_pinEntry, 4) != 0) {
        s_pinStage = 0;
        memset(s_pinFirst, 0, sizeof(s_pinFirst));
        pin_reset_entry();
        if (lbl_pin_title) {
            pin_show_status(S(Str::PinMismatch), true);
            lv_obj_set_style_text_color(lbl_pin_title, lv_color_hex(0xFF5555), LV_PART_MAIN);
        }
        return;
    }
    diaPinSet(s_pinEntry);
    pin_close();
    diaUiRefreshPinBtns();
}

static void pin_key_cb(lv_event_t *e) {
    lv_obj_t *mx = lv_event_get_target(e);
    const uint16_t id = lv_btnmatrix_get_selected_btn(mx);
    if (id == LV_BTNMATRIX_BTN_NONE) return;
    const char *txt = lv_btnmatrix_get_btn_text(mx, id);
    if (!txt || !txt[0]) return;
    if (strcmp(txt, "←") == 0) {
        if (s_pinLen > 0) {
            s_pinLen--;
            s_pinEntry[s_pinLen] = '\0';
            pin_show_digits();
        }
        return;
    }
    if (txt[0] == 'O' && txt[1] == 'K') {
        pin_commit();
        return;
    }
    if (txt[0] < '0' || txt[0] > '9' || s_pinLen >= 4) return;
    if (s_pinStage == 0 && s_pinLen == 0 && !diaBoardIsRound())
        pin_show_status(S(Str::PinEnter), true);
    s_pinEntry[s_pinLen++] = txt[0];
    s_pinEntry[s_pinLen] = '\0';
    pin_show_digits();
    if (s_pinLen == 4) pin_commit();
}

static void delayed_open_pin_task(lv_timer_t *timer) {
    s_pinStage = 0;
    memset(s_pinFirst, 0, sizeof(s_pinFirst));
    pin_reset_entry();
    pin_show_status(S(Str::PinEnter), !diaBoardIsRound());
    lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pin_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_pin_panel);
    lv_timer_del(timer);
}

static void open_pin_cb(lv_event_t *e) {
    (void)e;
    lv_timer_create(delayed_open_pin_task, 50, NULL);
}

static void close_pin_cb(lv_event_t *e) {
    (void)e;
    pin_close();
}

static void delayed_open_confirm_task(lv_timer_t *timer) {
    lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_confirm_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_confirm_panel);
    lv_timer_del(timer);
}

static void open_confirm_cb(lv_event_t *e) {
    (void)e;
    s_confirmKind = 0;
    if (lbl_confirm_title) {
        lv_label_set_text(lbl_confirm_title, S(Str::ConfirmErase));
        lv_obj_set_style_text_color(lbl_confirm_title, lv_color_hex(0xFF5555), LV_PART_MAIN);
    }
    if (lbl_yes) lv_label_set_text(lbl_yes, S(Str::YesErase));
    lv_timer_create(delayed_open_confirm_task, 50, NULL);
}

static void open_pin_clear_cb(lv_event_t *e) {
    (void)e;
    s_confirmKind = 1;
    if (lbl_confirm_title) {
        lv_label_set_text(lbl_confirm_title, S(Str::PinClearAsk));
        lv_obj_set_style_text_color(lbl_confirm_title, lv_color_hex(0xFFCC66), LV_PART_MAIN);
    }
    if (lbl_yes) lv_label_set_text(lbl_yes, S(Str::PinClear));
    lv_timer_create(delayed_open_confirm_task, 50, NULL);
}

static void delayed_close_confirm_task(lv_timer_t *timer) {
    lv_obj_add_flag(ui_confirm_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_timer_del(timer);
}

static void close_confirm_cb(lv_event_t *e) {
    (void)e;
    lv_timer_create(delayed_close_confirm_task, 50, NULL);
}

static void reset_wifi_confirm_cb(lv_event_t *e) {
    (void)e;
    if (s_confirmKind == 1) {
        diaPinClear();
        diaUiRefreshPinBtns();
        lv_timer_create(delayed_close_confirm_task, 50, NULL);
        return;
    }
    wm.resetSettings();
    preferences.clear();
    ESP.restart();
}

static void update_edit_label() {
    int val = lv_slider_get_value(slider_edit);
    char buf[32];
    if (current_edit_mode == 1 || current_edit_mode == 2) {
        snprintf(buf, sizeof(buf), "%d", val);
    } else if (current_edit_mode == 6 || current_edit_mode == 7) {
        snprintf(buf, sizeof(buf), "%02d:00", val);
    } else if (use_mgdl) {
        snprintf(buf, sizeof(buf), "%d", val);
    } else {
        snprintf(buf, sizeof(buf), "%.1f", val / 10.0);
    }
    lv_label_set_text(lbl_edit_val, buf);
}

static void slider_live_cb(lv_event_t *e) {
    (void)e;
    update_edit_label();
    int val = lv_slider_get_value(slider_edit);
    if (current_edit_mode == 1 || current_edit_mode == 2) diaBoardSetBrightness((uint8_t)val);
}

static void delayed_open_edit_task(lv_timer_t *timer) {
    int mode = *(int *)timer->user_data;
    current_edit_mode = mode;

    if (mode == 1) {
        lv_label_set_text(lbl_edit_title, S(Str::EditDay));
        lv_slider_set_range(slider_edit, 10, 255);
        lv_slider_set_value(slider_edit, day_brightness, LV_ANIM_OFF);
    } else if (mode == 2) {
        lv_label_set_text(lbl_edit_title, S(Str::EditNight));
        lv_slider_set_range(slider_edit, 5, 255);
        lv_slider_set_value(slider_edit, night_brightness, LV_ANIM_OFF);
    } else if (mode == 3) {
        lv_label_set_text(lbl_edit_title, S(use_mgdl ? Str::EditLowMg : Str::EditLow));
        if (use_mgdl) {
            int hi = (int)lroundf(mmol_to_mgdl(targetHigh)) - 1;
            lv_slider_set_range(slider_edit, 54, hi > 55 ? hi : 55);
            lv_slider_set_value(slider_edit, (int)lroundf(mmol_to_mgdl(targetLow)), LV_ANIM_OFF);
        } else {
            lv_slider_set_range(slider_edit, 30, (int)(targetHigh * 10) - 1);
            lv_slider_set_value(slider_edit, (int)(targetLow * 10), LV_ANIM_OFF);
        }
    } else if (mode == 4) {
        lv_label_set_text(lbl_edit_title, S(use_mgdl ? Str::EditHighMg : Str::EditHigh));
        if (use_mgdl) {
            int lo = (int)lroundf(mmol_to_mgdl(targetLow)) + 1;
            int hi = (int)lroundf(mmol_to_mgdl(targetCrit)) - 1;
            lv_slider_set_range(slider_edit, lo, hi > lo ? hi : lo + 1);
            lv_slider_set_value(slider_edit, (int)lroundf(mmol_to_mgdl(targetHigh)), LV_ANIM_OFF);
        } else {
            lv_slider_set_range(slider_edit, (int)(targetLow * 10) + 1, (int)(targetCrit * 10) - 1);
            lv_slider_set_value(slider_edit, (int)(targetHigh * 10), LV_ANIM_OFF);
        }
    } else if (mode == 5) {
        lv_label_set_text(lbl_edit_title, S(use_mgdl ? Str::EditCritMg : Str::EditCrit));
        if (use_mgdl) {
            int lo = (int)lroundf(mmol_to_mgdl(targetHigh)) + 1;
            lv_slider_set_range(slider_edit, lo, 450);
            lv_slider_set_value(slider_edit, (int)lroundf(mmol_to_mgdl(targetCrit)), LV_ANIM_OFF);
        } else {
            lv_slider_set_range(slider_edit, (int)(targetHigh * 10) + 1, 250);
            lv_slider_set_value(slider_edit, (int)(targetCrit * 10), LV_ANIM_OFF);
        }
    } else if (mode == 6) {
        lv_label_set_text(lbl_edit_title, S(Str::EditDayStart));
        lv_slider_set_range(slider_edit, 0, 23);
        lv_slider_set_value(slider_edit, day_start_hour, LV_ANIM_OFF);
    } else if (mode == 7) {
        lv_label_set_text(lbl_edit_title, S(Str::EditNightStart));
        lv_slider_set_range(slider_edit, 0, 23);
        lv_slider_set_value(slider_edit, night_start_hour, LV_ANIM_OFF);
    }

    update_edit_label();
    lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_edit_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_edit_panel);

    free(timer->user_data);
    lv_timer_del(timer);
}

static void btn_edit_cb(lv_event_t *e) {
    int *mode = (int *)malloc(sizeof(int));
    *mode = (int)(intptr_t)lv_event_get_user_data(e);
    lv_timer_create(delayed_open_edit_task, 50, mode);
}

static void save_edit_cb(lv_event_t *e) {
    (void)e;
    pending_pref_val = lv_slider_get_value(slider_edit);
    pending_pref_mode = current_edit_mode;
    pending_pref_save = true;

    lv_obj_add_flag(ui_edit_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    updateBrightness();
}

static void sync_btn_cb(lv_event_t *e) {
    (void)e;
    diaUiTriggerFetch(false); // z menu bez cooldownu
    lv_timer_create(delayed_close_settings_task, 50, NULL);
}

static lv_obj_t *create_menu_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb, int user_data,
                                 uint32_t color, int btn_w) {
    const int w = (btn_w > 0) ? btn_w : DIA_MENU_BTN_W;
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, DIA_MENU_BTN_H);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_left(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_right(btn, 6, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

    lv_obj_t *label = lv_label_create(btn);
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_style_text_font(label, &font_sk_20, LV_PART_MAIN);
#else
    lv_obj_set_style_text_font(label, &font_sk_16, LV_PART_MAIN);
#endif
    lv_obj_set_width(label, w - 12);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)user_data);
    return label;
}

void update_target_btn_labels() {
    char buf[64];
    formatTargetLabel(S(Str::Low), targetLow, buf, sizeof(buf));
    if (lbl_btn_low) lv_label_set_text(lbl_btn_low, buf);
    formatTargetLabel(S(Str::High), targetHigh, buf, sizeof(buf));
    if (lbl_btn_high) lv_label_set_text(lbl_btn_high, buf);
    formatTargetLabel(S(Str::Crit), targetCrit, buf, sizeof(buf));
    if (lbl_btn_crit) lv_label_set_text(lbl_btn_crit, buf);
    if (lbl_btn_units) {
        lv_label_set_text(lbl_btn_units, S(use_mgdl ? Str::UnitsMg : Str::UnitsMmol));
    }
}

void refresh_glucose_label() {
    if (last_mmol_value <= 0.0 || !label_glycemia) return;
    char glycemia_str[10];
    formatGlucose(last_mmol_value, glycemia_str, sizeof(glycemia_str));
    lv_label_set_text(label_glycemia, glycemia_str);
}

static uint32_t diaDeltaColor(float mmol) {
    if (mmol > 0.05f) return 0xFFAA00;
    if (mmol < -0.05f) return 0x55AAFF;
    return 0xAAAAAA;
}

void updateDeltaLabel() {
    if (!label_delta) return;

    if (!show_delta) {
        lv_label_set_text(label_delta, "");
        lv_obj_add_flag(label_delta, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(label_delta, LV_OBJ_FLAG_HIDDEN);

    // Široký pump: Delta v satelite; kruhový pump Delta nemá
    const bool pumpFaceWide =
        (watchface_style == 3 && show_pump && !diaUiIsSplitMode() && !diaBoardIsRound());

    char val[16];
    if (!has_valid_reading || !has_delta) {
        snprintf(val, sizeof(val), "-");
    } else if (use_mgdl) {
        int d = (int)lroundf(mmol_to_mgdl(last_delta_mmol));
        if (d == 0) snprintf(val, sizeof(val), "±0");
        else snprintf(val, sizeof(val), "%+d", d);
    } else {
        if (fabsf(last_delta_mmol) < 0.05f) snprintf(val, sizeof(val), "±0.0");
        else snprintf(val, sizeof(val), "%+.1f", last_delta_mmol);
    }

    if (pumpFaceWide) {
        char buf[24];
        snprintf(buf, sizeof(buf), "Delta\n%s", val);
        lv_label_set_text(label_delta, buf);
        lv_obj_set_style_text_color(label_delta, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    } else {
        if (!has_valid_reading || !has_delta) {
            lv_label_set_text(label_delta, "");
            return;
        }
        lv_label_set_text(label_delta, val);
        lv_obj_set_style_text_color(label_delta, lv_color_hex(diaDeltaColor(last_delta_mmol)),
                                    LV_PART_MAIN);
    }
}

void updateStaleAndErrorVisuals() {
    if (s_bootUiLock) return;
    if (label_status) {
        const int snoozeRem = diaAudioSnoozeRemainMin(millis());
        if (diaUiIsSplitMode()) {
            // Dual: per-slot „Staré“ je pri zdroji; hore len snooze / spoločná chyba
            if (snoozeRem > 0) {
                char sb[24];
                snprintf(sb, sizeof(sb), S(Str::SnoozeActiveFmt), snoozeRem);
                lv_label_set_text(label_status, sb);
                lv_obj_set_style_text_color(label_status, lv_color_hex(0x88AACC), LV_PART_MAIN);
                lv_obj_align(label_status, LV_ALIGN_TOP_MID, 0, 4);
                lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_label_set_text(label_status, "");
                lv_obj_add_flag(label_status, LV_OBJ_FLAG_HIDDEN);
            }
        } else if (diaUiWaitingWithNumber(diaShownSlot(), last_fetch_error,
                                          has_valid_reading && last_mmol_value > 0.0f)) {
            lv_obj_set_width(label_status, 220);
            lv_label_set_text(label_status, S(Str::WaitServer));
            lv_obj_set_style_text_color(label_status, lv_color_hex(0x88AACC), LV_PART_MAIN);
            lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        } else if ((diaFetchShowConnecting() &&
                    last_fetch_error != FetchError::Auth &&
                    last_fetch_error != FetchError::Config) ||
                   (last_fetch_error == FetchError::Auth && dsSource == "guardian" &&
                    !diaUiShowClLoggedOut(last_fetch_error))) {
            // Po flashi/boote: Auth ešte nie je isté → „Pripájam…“, nie „odhlásený“
            lv_obj_set_width(label_status, 110);
            lv_label_set_text(label_status, S(Str::Connecting));
            lv_obj_set_style_text_color(label_status, lv_color_hex(0x88AACC), LV_PART_MAIN);
            lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        } else if (diaUiHardVisualErr(last_fetch_error, has_valid_reading)) {
            lv_obj_set_width(label_status, 110);
            lv_label_set_text(label_status, diaUiAuthStatusText(last_fetch_error, dsSource));
            lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF8800), LV_PART_MAIN);
            lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        } else if (snoozeRem > 0) {
            char sb[24];
            snprintf(sb, sizeof(sb), S(Str::SnoozeActiveFmt), snoozeRem);
            lv_label_set_text(label_status, sb);
            lv_obj_set_style_text_color(label_status, lv_color_hex(0x88AACC), LV_PART_MAIN);
            lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        } else if (data_is_stale) {
            lv_label_set_text(label_status, S(Str::Stale));
            lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF4444), LV_PART_MAIN);
            lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(label_status, "");
        }
    }

    // Single: číslo biele/sivé (stale / istý auth…; Network blip / boot-grace Auth nesiví)
    // Pump face: farbu rozsahu rieši diaUiUpdateArcColors (okrem stale/err → sivá)
    if (!diaUiIsSplitMode() && has_valid_reading && label_glycemia) {
        const bool hardAuth = last_fetch_error == FetchError::Auth &&
                              (dsSource != "guardian" || diaUiShowClLoggedOut(last_fetch_error));
        if (data_is_stale ||
            (diaUiHardVisualErr(last_fetch_error, has_valid_reading) &&
             (last_fetch_error != FetchError::Auth || hardAuth))) {
            lv_obj_set_style_text_color(label_glycemia, lv_color_hex(0x888888), LV_PART_MAIN);
        } else if (!(watchface_style == 3 && show_pump)) {
            lv_obj_set_style_text_color(label_glycemia, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        }
    }
}

void showCachedReadingOnUi() {
    if (!has_valid_reading || last_mmol_value <= 0.0f) return;
    char glycemia_str[10];
    formatGlucose(last_mmol_value, glycemia_str, sizeof(glycemia_str));
    lv_label_set_text(label_glycemia, glycemia_str);
    lv_label_set_text(label_trend, last_trend_symbol.c_str());
    updateDeltaLabel();
    updateAgeDisplay();
    updateStaleAndErrorVisuals();
    refresh_sparkline();
}

static void refreshSparklineObj(lv_obj_t *chart, lv_chart_series_t *ser,
                                const float *hist, int count, uint32_t color) {
    if (!chart || !ser) return;
    lv_chart_set_series_color(chart, ser, lv_color_hex(color));

    if (!hist || count < 2) {
        for (uint16_t i = 0; i < DIA_HISTORY_MAX; i++) {
            lv_chart_set_value_by_id(chart, ser, i, LV_CHART_POINT_NONE);
        }
        lv_chart_refresh(chart);
        return;
    }

    float ymin = hist[0];
    float ymax = hist[0];
    for (int i = 1; i < count; i++) {
        if (hist[i] < ymin) ymin = hist[i];
        if (hist[i] > ymax) ymax = hist[i];
    }
    float span = ymax - ymin;
    if (span < 1.0f) {
        float mid = (ymin + ymax) * 0.5f;
        ymin = mid - 0.5f;
        ymax = mid + 0.5f;
    } else {
        float pad = span * 0.12f;
        ymin -= pad;
        ymax += pad;
    }
    if (ymin < 0.5f) ymin = 0.5f;

    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y,
                       (lv_coord_t)lroundf(ymin * 10.0f),
                       (lv_coord_t)lroundf(ymax * 10.0f));

    for (uint16_t i = 0; i < DIA_HISTORY_MAX; i++) {
        if ((int)i < count) {
            lv_chart_set_value_by_id(chart, ser, i, (lv_coord_t)lroundf(hist[i] * 10.0f));
        } else {
            lv_chart_set_value_by_id(chart, ser, i, LV_CHART_POINT_NONE);
        }
    }
    lv_chart_refresh(chart);
}

void refresh_sparkline() {
    refreshSparklineObj(chart_spark, chart_spark_ser, glucose_history, glucose_history_count,
                        current_arc_color);
}

static void refreshSplitSparklines() {
#if defined(DIA_BOARD_WS_3_5)
    refreshSparklineObj(chart_spark, chart_spark_ser,
                        slotReadings[0].history, slotReadings[0].history_count,
                        slotReadings[0].arc_color);
    refreshSparklineObj(chart_spark1, chart_spark_ser1,
                        slotReadings[1].history, slotReadings[1].history_count,
                        slotReadings[1].arc_color);
#endif
}

void updateAgeDisplay() {
    // Počas IP návodu nesmie Sync prepísať label_age / label_glycemia
    if (s_bootUiLock) return;
    if (last_reading_timestamp == 0) {
        data_is_stale = false;
        if (label_age) {
            // Connecting zostáva len v status – age = Sync / Auth (nie 3× Pripájam)
            if (last_fetch_error == FetchError::Auth || last_fetch_error == FetchError::Config)
                lv_label_set_text(label_age, diaUiAuthStatusText(last_fetch_error, dsSource));
            else if (last_fetch_error == FetchError::Network || last_fetch_error == FetchError::Wifi)
                lv_label_set_text(label_age,
                                  diaFetchShowConnecting() ? S(Str::Sync) : S(Str::ErrNet));
            else
                lv_label_set_text(label_age, S(Str::Sync));
        }
        updateStaleAndErrorVisuals();
        return;
    }
    time_t now;
    time(&now);
    // NTP ešte nie je – nechaj Sync..., neponechávaj boot text
    if (now < 100000) {
        if (label_age) lv_label_set_text(label_age, S(Str::Sync));
        return;
    }

    int age_mins = (now - last_reading_timestamp) / 60;
    if (age_mins < 0) age_mins = 0;

    char age_str[16];
    snprintf(age_str, sizeof(age_str), "%d min", age_mins);
    lv_label_set_text(label_age, age_str);

    data_is_stale = (age_mins > STALE_AGE_MINUTES);
    if (data_is_stale) lv_obj_set_style_text_color(label_age, lv_color_hex(0xFF0000), LV_PART_MAIN);
    else lv_obj_set_style_text_color(label_age, lv_color_hex(0x888888), LV_PART_MAIN);

    updateStaleAndErrorVisuals();
    lv_obj_invalidate(lv_scr_act());
}

void refresh_ui_colors() {
    refresh_critical_and_color();
    if (chart_spark && chart_spark_ser) {
        lv_chart_set_series_color(chart_spark, chart_spark_ser, lv_color_hex(current_arc_color));
    }
    if (last_mmol_value > 0.0) {
        lv_obj_invalidate(lv_scr_act());
    }
}

void applyGlucoseReading(float mmol, const String &trendSymbol, long unix_ts,
                         bool hasSourceDelta, float sourceDeltaMmol) {
    if (diaFetchDeferUi()) {
        diaFetchEnqueueSuccess(mmol, trendSymbol.c_str(), unix_ts, hasSourceDelta, sourceDeltaMmol);
        return;
    }
    if (s_bootUiLock) return;

    int slot = diaFetchCurrentSlot();
    glucoseStoreReadingSlot(slot, mmol, trendSymbol, unix_ts, hasSourceDelta, sourceDeltaMmol);
    diaUiUpdateSlotFace(slot);
    lv_obj_invalidate(lv_scr_act());
}

void markFetchError(FetchError err) {
    if (diaFetchDeferUi()) {
        diaFetchEnqueueError(err);
        return;
    }
    diaDiagResult(diaFetchCurrentSlot(), (int)err);

    int slot = diaFetchCurrentSlot();
    if (slot < 0 || slot > 1) slot = 0;
    // Blip: neprepisuj farebné čerstvé dáta (aj krátky WiFi výpadok)
    if ((err == FetchError::Network || err == FetchError::Empty || err == FetchError::Wifi) &&
        slotReadings[slot].has_valid_reading &&
        !slotReadings[slot].data_is_stale) {
        Serial.printf("[fetch] slot %d blip err=%d (sync) – drzim posledne data\n", slot, (int)err);
        lastUpdate = millis() - updateInterval + 15000;
        return;
    }
    slotReadings[slot].err = err;
    last_fetch_error = err;
    if (s_bootUiLock) return;
    diaUiUpdateSlotFace(slot);
    updateStaleAndErrorVisuals();
    lv_obj_invalidate(lv_scr_act());
    lastUpdate = millis() - updateInterval + 15000;
}

static void diaUiApplySplitSourceLabels() {
    auto setSrc = [](lv_obj_t *lab, int slot) {
        if (!lab) return;
        const DiaSlotReading &s = slotReadings[slot];
        if (s.err == FetchError::Auth && profiles[slot].dsSource == "guardian") {
            lv_label_set_text(lab, diaUiAuthStatusText(s.err, profiles[slot].dsSource));
            lv_obj_set_style_text_color(lab,
                lv_color_hex(diaUiShowClLoggedOut(s.err) ? 0xFF4444u : 0x88AACCu), LV_PART_MAIN);
            lv_obj_clear_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        // Staré má prednosť pred „Čakám na server“ (senzor offline ≠ server dole)
        if (s.has_valid_reading && s.mmol > 0.0f && s.data_is_stale) {
            char staleBuf[32];
            snprintf(staleBuf, sizeof(staleBuf), "! %s", S(Str::Stale));
            lv_label_set_text(lab, staleBuf);
            lv_obj_set_style_text_color(lab, lv_color_hex(0xFF4444), LV_PART_MAIN);
            lv_obj_clear_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        // Empty + ešte platné číslo: bez nových dát zo senzora (nie Auth)
        if (s.err == FetchError::Empty && s.has_valid_reading && s.mmol > 0.0f) {
            lv_label_set_text(lab, S(Str::ErrEmpty));
            lv_obj_set_style_text_color(lab, lv_color_hex(0xFF8800), LV_PART_MAIN);
            lv_obj_clear_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (diaUiWaitingWithNumber(slot, s.err, s.has_valid_reading && s.mmol > 0.0f)) {
            lv_label_set_text(lab, S(Str::WaitServer));
            lv_obj_set_style_text_color(lab, lv_color_hex(0x88AACC), LV_PART_MAIN);
            lv_obj_clear_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        // Len tento slot (nie globálny wait druhého dieťaťa)
        if (diaFetchSlotWaiting(slot) &&
            (s.err == FetchError::Network || s.err == FetchError::Wifi) &&
            (!s.has_valid_reading || s.mmol <= 0.0f)) {
            lv_label_set_text(lab, S(Str::Connecting));
            lv_obj_set_style_text_color(lab, lv_color_hex(0x88AACC), LV_PART_MAIN);
            lv_obj_clear_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (s.err != FetchError::None && (!s.has_valid_reading || s.mmol <= 0.0f)) {
            lv_label_set_text(lab, diaUiAuthStatusText(s.err, profiles[slot].dsSource));
            lv_obj_set_style_text_color(lab, lv_color_hex(0xFF8800), LV_PART_MAIN);
            lv_obj_clear_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        // Dual pump: 1) IOB/COB  2) bazál inak rezervoár  — vek je samostatne pod tým
        if (diaUiIsSplitMode() && watchface_style == 3 && show_pump && diaUiProfileIsNs(slot)) {
            char pumpBuf[96];
            const DiaPumpStatus &p = slotPump[slot];
            // Len ASCII – font_sk_* nemá „—“ (inak □)
            if (p.valid && p.hasIob && p.hasCob)
                snprintf(pumpBuf, sizeof(pumpBuf), "IOB %.2f   COB %.0fg", (double)p.iob, (double)p.cob);
            else if (p.valid && p.hasIob)
                snprintf(pumpBuf, sizeof(pumpBuf), "IOB %.2f   COB -", (double)p.iob);
            else if (p.valid && p.hasCob)
                snprintf(pumpBuf, sizeof(pumpBuf), "IOB -   COB %.0fg", (double)p.cob);
            else
                snprintf(pumpBuf, sizeof(pumpBuf), "IOB -   COB -");
            size_t n = strlen(pumpBuf);
            if (p.valid && p.hasBasal)
                snprintf(pumpBuf + n, sizeof(pumpBuf) - n, "\n%.2f U/h", (double)p.basal);
            else if (p.valid && p.hasReservoir)
                snprintf(pumpBuf + n, sizeof(pumpBuf) - n, "\nrez. %.0f U", (double)p.reservoir);
            else
                snprintf(pumpBuf + n, sizeof(pumpBuf) - n, "\n-");
            lv_label_set_text(lab, pumpBuf);
            lv_obj_set_width(lab, 200);
            lv_label_set_long_mode(lab, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_line_space(lab, 2, LV_PART_MAIN);
            lv_obj_set_style_text_font(lab, &font_sk_20, LV_PART_MAIN);
            lv_obj_set_style_text_color(lab, lv_color_hex(0xE6EEF6), LV_PART_MAIN);
            lv_obj_clear_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (!show_source) {
            lv_label_set_text(lab, "");
            lv_obj_add_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (!diaProfileActive(slot)) {
            lv_label_set_text(lab, "off");
            lv_obj_set_style_text_color(lab, lv_color_hex(0x888888), LV_PART_MAIN);
            lv_obj_clear_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        const char *n = sourceFaceNameOf(profiles[slot].dsSource);
        if (!n || !n[0]) {
            lv_label_set_text(lab, "");
            lv_obj_add_flag(lab, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        lv_label_set_text(lab, n);
        lv_obj_set_style_text_color(lab, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_clear_flag(lab, LV_OBJ_FLAG_HIDDEN);
    };
    setSrc(label_source, 0);
    setSrc(label_source1, 1);
}

void diaUiUpdateSlotFace(int slot) {
    if (s_bootUiLock) return;
    if (slot < 0 || slot > 1) return;
    const DiaSlotReading &s = slotReadings[slot];
    char glycemia_str[16];
    const bool split = diaUiIsSplitMode();

    auto fill = [&](lv_obj_t *g, lv_obj_t *tr, lv_obj_t *dlt, lv_obj_t *age, lv_obj_t *nm) {
        if (nm) {
            const char *n = profiles[slot].name.c_str();
            lv_label_set_text(nm, (n && n[0]) ? n : (slot == 0 ? "A" : "B"));
            // Po fetchi nesmie ostať HIDDEN z bootu/overlay – inak mená až po zmene ciferníka
            if (split && !diaUiFaceOverlaysOpen()) {
                lv_obj_clear_flag(nm, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (!s.has_valid_reading || s.mmol <= 0.0f) {
            if (g) {
                // Počas „Pripájam…“ drž „...“ – Connecting len v status (inak 3× ten istý text)
                const bool connecting = diaFetchShowConnecting() &&
                                        (s.err == FetchError::Network || s.err == FetchError::Wifi);
                if (connecting || s.err == FetchError::None)
                    lv_label_set_text(g, "...");
                else
                    lv_label_set_text(g, fetchErrorText(s.err));
                lv_obj_set_style_text_font(g, &font_sk_20, LV_PART_MAIN);
            }
            if (tr) lv_label_set_text(tr, "");
            if (dlt) {
                lv_label_set_text(dlt, "");
                if (split) lv_obj_add_flag(dlt, LV_OBJ_FLAG_HIDDEN);
            }
            if (age) lv_label_set_text(age, S(Str::Sync));
            return;
        }
        formatGlucose(s.mmol, glycemia_str, sizeof(glycemia_str));
        bool stale = false;
        if (age && s.timestamp > 0) {
            time_t now = time(nullptr);
            // Rovnaká NTP ochrana ako single – pred sync neoznačovať stale
            if (now < 100000) {
                lv_label_set_text(age, S(Str::Sync));
                stale = false;
                slotReadings[slot].data_is_stale = false;
            } else {
                long ageMin = (now > s.timestamp) ? (now - s.timestamp) / 60 : 0;
                char ab[32];
                snprintf(ab, sizeof(ab), "%ld min", ageMin);
                lv_label_set_text(age, ab);
                stale = ageMin > STALE_AGE_MINUTES;
                slotReadings[slot].data_is_stale = stale;
                lv_obj_set_style_text_color(age, lv_color_hex(stale ? 0xFF0000u : 0x888888u),
                                            LV_PART_MAIN);
            }
        } else {
            stale = s.data_is_stale;
        }
        // Wifi/Network blip pri platných dátach zruš. Empty pri starých nechaj (senzor).
        {
            FetchError e = slotReadings[slot].err;
            if (e == FetchError::Wifi || e == FetchError::Network ||
                (e == FetchError::Empty && !stale)) {
                slotReadings[slot].err = FetchError::None;
                if (slot == diaShownSlot()) last_fetch_error = FetchError::None;
            }
        }
        // Auth/Config / Empty-bez-dát; Wifi blip pri platných dátach nesiví
        const bool hardErr = diaUiHardVisualErr(slotReadings[slot].err,
                                                slotReadings[slot].has_valid_reading &&
                                                    slotReadings[slot].mmol > 0.0f);
        const bool bad = stale || hardErr;
        if (g) {
            lv_label_set_text(g, glycemia_str);
            if (split) {
#if defined(DIA_BOARD_WS_3_5)
                // Po "..." (font 20) musíme vrátiť veľký font – layout sa nemusí znova spustiť
                const int sp = diaUiSplitPage();
                if (sp == 2) {
                    lv_obj_set_style_text_font(g, &font_sk_48, LV_PART_MAIN);
                    if (tr) lv_obj_set_style_text_font(tr, &font_sk_48, LV_PART_MAIN);
                } else if (sp == 1) {
                    lv_obj_set_style_text_font(g, &font_sk_72, LV_PART_MAIN);
                    if (tr) lv_obj_set_style_text_font(tr, &font_sk_48, LV_PART_MAIN);
                } else {
                    lv_obj_set_style_text_font(g, &font_sk_88, LV_PART_MAIN);
                    if (tr) lv_obj_set_style_text_font(tr, &font_sk_72, LV_PART_MAIN);
                }
#endif
                lv_obj_set_style_text_color(g, lv_color_hex(bad ? 0x888888u : s.arc_color),
                                            LV_PART_MAIN);
            }
        }
        if (tr) lv_label_set_text(tr, s.trend.c_str());
        if (dlt) {
            if (show_delta && s.has_delta) {
                char db[24];
                if (use_mgdl) snprintf(db, sizeof(db), "%+.0f", mmol_to_mgdl(s.delta_mmol));
                else snprintf(db, sizeof(db), "%+.1f", s.delta_mmol);
                lv_label_set_text(dlt, db);
                lv_obj_set_style_text_color(dlt, lv_color_hex(diaDeltaColor(s.delta_mmol)), LV_PART_MAIN);
                lv_obj_clear_flag(dlt, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_label_set_text(dlt, "");
                if (split) lv_obj_add_flag(dlt, LV_OBJ_FLAG_HIDDEN);
            }
        }
    };

    if (!split && slot == diaShownSlot()) {
        fill(label_glycemia, label_trend, label_delta, label_age, label_name0);
        diaConfigPushSlot(slot);
        glucoseSyncGlobalsFromSlot(slot);
        updateDeltaLabel();
        updateAgeDisplay();
        refresh_sparkline();
        if (arc_bg) {
            lv_obj_set_style_arc_color(arc_bg, lv_color_hex(slotReadings[slot].arc_color),
                                      LV_PART_INDICATOR);
        }
        if (watchface_style == 3 && show_pump)
            diaUiColorPumpSatellites(slotReadings[slot].arc_color);
        updateSourceLabel();
        diaUiPlaceSingleName();
        diaUiRefreshPumpLabel();
        if (watchface_style == 3 && show_pump) {
            diaUiPlacePumpFace();
            lv_obj_clear_flag(label_glycemia, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (split && slot == 0) {
        fill(label_glycemia, label_trend, label_delta, label_age, label_name0);
#if defined(DIA_BOARD_WS_3_5)
        diaUiApplySplitSourceLabels();
        if (diaUiSplitPage() == 1) refreshSplitSparklines();
#else
        diaUiApplySplitSourceLabels();
#endif
    } else if (split) {
        fill(label_glycemia1, label_trend1, label_delta1, label_age1, label_name1);
        if (split) {
#if defined(DIA_BOARD_WS_3_5)
            diaUiApplySplitSourceLabels();
            if (diaUiSplitPage() == 1) refreshSplitSparklines();
#else
            diaUiApplySplitSourceLabels();
#endif
        }
    }
}

void diaUiApplyDisplayMode() {
#if defined(DIA_BOARD_WS_3_5)
    if (lbl_btn_mode) {
        lv_label_set_text(lbl_btn_mode, S(display_mode >= 2 ? Str::ModeBtn2 : Str::ModeBtn1));
    }
    if (s_bootUiLock) {
        clearMainOverlays();
        if (arc_bg) lv_obj_add_flag(arc_bg, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const bool split = diaUiIsSplitMode();
    if (split_line) {
        if (split) lv_obj_clear_flag(split_line, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(split_line, LV_OBJ_FLAG_HIDDEN);
    }
    if (!label_glycemia1) {
        if (lbl_btn_mode) {
            lv_label_set_text(lbl_btn_mode, S(display_mode >= 2 ? Str::ModeBtn2 : Str::ModeBtn1));
        }
        return;
    }

    if (split) {
        // Dual: 0 = čísla, 2 = grafy, 3 = pumpa (ak NS+show_pump)
        if (watchface_style == 1) watchface_style = 0;
        if (watchface_style == 3 && !(show_pump && diaAnyNightscout())) watchface_style = 0;
        if (watchface_style != 0 && watchface_style != 2 && watchface_style != 3) watchface_style = 0;
        diaUiHidePumpExtras(); // kruhové Sach/IOB len v single
        const int page = diaUiSplitPage(); // 0 čísla, 1 grafy, 2 pumpa

        if (arc_bg) lv_obj_add_flag(arc_bg, LV_OBJ_FLAG_HIDDEN);
        if (label_status) {
            lv_obj_add_flag(label_status, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(label_status, "");
        }
        if (label_time) lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);
        if (split_line) lv_obj_add_flag(split_line, LV_OBJ_FLAG_HIDDEN);

        const int fw = (DIA_SCREEN_W - kSplitMidW) / 2 - kSplitFrameGap;
        const int fh = DIA_SCREEN_H - 2 * kSplitFrameGap;
        const int pad = kSplitFrameBorder + 10;
        const int leftCx = kSplitFrameGap + fw / 2;
        const int rightCx = DIA_SCREEN_W - kSplitFrameGap - fw / 2;
        const int leftOff = leftCx - (int)DIA_SCREEN_W / 2;
        const int rightOff = rightCx - (int)DIA_SCREEN_W / 2;
        const int nameY = kSplitFrameGap + kSplitNameTop;

        if (panel_frame0) {
            lv_obj_set_size(panel_frame0, fw, fh);
            lv_obj_align(panel_frame0, LV_ALIGN_LEFT_MID, kSplitFrameGap, 0);
            lv_obj_clear_flag(panel_frame0, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_background(panel_frame0);
        }
        if (panel_frame1) {
            lv_obj_set_size(panel_frame1, fw, fh);
            lv_obj_align(panel_frame1, LV_ALIGN_RIGHT_MID, -kSplitFrameGap, 0);
            lv_obj_clear_flag(panel_frame1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_background(panel_frame1);
        }

        lv_obj_clear_flag(label_glycemia1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(label_trend1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(label_age1, LV_OBJ_FLAG_HIDDEN);
        if (label_name0) lv_obj_clear_flag(label_name0, LV_OBJ_FLAG_HIDDEN);
        if (label_name1) lv_obj_clear_flag(label_name1, LV_OBJ_FLAG_HIDDEN);
        if (label_glycemia) lv_obj_clear_flag(label_glycemia, LV_OBJ_FLAG_HIDDEN);
        if (label_trend) lv_obj_clear_flag(label_trend, LV_OBJ_FLAG_HIDDEN);
        if (label_age) lv_obj_clear_flag(label_age, LV_OBJ_FLAG_HIDDEN);

        auto styleSplitName = [&](lv_obj_t *nm, int colOff) {
            if (!nm) return;
            lv_obj_set_width(nm, fw - 2 * pad);
            lv_label_set_long_mode(nm, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_align(nm, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(nm, lv_color_hex(0x1fa3ec), LV_PART_MAIN);
            lv_obj_set_style_text_font(nm, &font_sk_24, LV_PART_MAIN);
            lv_obj_set_style_text_line_space(nm, -2, LV_PART_MAIN);
            lv_obj_align(nm, LV_ALIGN_TOP_MID, colOff, nameY);
        };
        styleSplitName(label_name0, leftOff);
        styleSplitName(label_name1, rightOff);

        // Bez grafu: meno → medzera → hodnota → šípka(+delta) → vek → zdroj
        auto placeSplitColNoChart = [&](int colOff, lv_obj_t *g, lv_obj_t *tr, lv_obj_t *dlt, lv_obj_t *age,
                                        lv_obj_t *src) {
            const int glyY = nameY + kSplitNameClear;
            const int arrowY = glyY + 96;
            const int ageY = arrowY + 56;
            const int srcY = ageY + 32;
            lv_obj_set_style_text_font(g, &font_sk_88, LV_PART_MAIN);
            lv_obj_set_style_text_font(tr, &font_sk_72, LV_PART_MAIN);
            lv_obj_set_style_text_font(dlt, &font_sk_20, LV_PART_MAIN);
            lv_obj_set_style_text_font(age, &font_sk_20, LV_PART_MAIN);
            if (src) lv_obj_set_style_text_font(src, &font_sk_16, LV_PART_MAIN);

            lv_obj_align(g, LV_ALIGN_TOP_MID, colOff, glyY);
            const int ax = show_delta ? -22 : 0;
            lv_obj_align(tr, LV_ALIGN_TOP_MID, colOff + ax, arrowY);
            if (dlt) {
                if (show_delta) {
                    lv_obj_align(dlt, LV_ALIGN_TOP_MID, colOff + ax + 52, arrowY + 14);
                    lv_obj_clear_flag(dlt, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(dlt, LV_OBJ_FLAG_HIDDEN);
                }
            }
            lv_obj_align(age, LV_ALIGN_TOP_MID, colOff, ageY);
            if (src) lv_obj_align(src, LV_ALIGN_TOP_MID, colOff, srcY);
        };

        // S grafom: vek+zdroj ukotvené OD GRAFU nahor (BOTTOM) – nikdy nelezú do chart boxu
        auto placeSplitColWithChart = [&](int colOff, lv_obj_t *g, lv_obj_t *tr, lv_obj_t *dlt, lv_obj_t *age,
                                          lv_obj_t *src, lv_obj_t *chart, bool left) {
            const int chartH = 48;
            const int chartBottom = -(kSplitFrameGap + pad); // typ. -24
            const int gapSrcChart = 16;
            const int gapAgeSrc = 12;
            const int gapArrowAge = 14;
            const int srcLineH = 18;
            const int ageLineH = 22;
            const int arrowH = 48;
            // BOTTOM_MID: záporné = nahor; spodok labelu na screenH+offset
            const int srcBottom = chartBottom - chartH - gapSrcChart;
            const int ageBottom = srcBottom - srcLineH - gapAgeSrc;
            // TOP y: horný okraj veku − medzera − výška šípky
            const int ageTopY = DIA_SCREEN_H + ageBottom - ageLineH;
            const int arrowY = ageTopY - gapArrowAge - arrowH;
            const int glyY = nameY + 40;

            lv_obj_set_style_text_font(g, &font_sk_72, LV_PART_MAIN);
            lv_obj_set_style_text_font(tr, &font_sk_48, LV_PART_MAIN);
            lv_obj_set_style_text_font(dlt, &font_sk_20, LV_PART_MAIN);
            lv_obj_set_style_text_font(age, &font_sk_20, LV_PART_MAIN);
            if (src) lv_obj_set_style_text_font(src, &font_sk_16, LV_PART_MAIN);

            lv_obj_align(g, LV_ALIGN_TOP_MID, colOff, glyY);
            const int ax = show_delta ? -22 : 0;
            lv_obj_align(tr, LV_ALIGN_TOP_MID, colOff + ax, arrowY);
            if (dlt) {
                if (show_delta) {
                    lv_obj_align(dlt, LV_ALIGN_TOP_MID, colOff + ax + 52, arrowY + 14);
                    lv_obj_clear_flag(dlt, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(dlt, LV_OBJ_FLAG_HIDDEN);
                }
            }
            lv_obj_align(age, LV_ALIGN_BOTTOM_MID, colOff, ageBottom);
            if (src) lv_obj_align(src, LV_ALIGN_BOTTOM_MID, colOff, srcBottom);

            if (chart) {
                diaUiStyleSpark(chart, true);
                lv_obj_set_size(chart, fw - 2 * pad, chartH);
                if (left) {
                    lv_obj_align(chart, LV_ALIGN_BOTTOM_LEFT, kSplitFrameGap + pad, chartBottom);
                } else {
                    lv_obj_align(chart, LV_ALIGN_BOTTOM_RIGHT, -(kSplitFrameGap + pad), chartBottom);
                }
                lv_obj_clear_flag(chart, LV_OBJ_FLAG_HIDDEN);
            }
        };

        if (page == 0) {
            if (chart_spark) lv_obj_add_flag(chart_spark, LV_OBJ_FLAG_HIDDEN);
            if (chart_spark1) lv_obj_add_flag(chart_spark1, LV_OBJ_FLAG_HIDDEN);
            if (label_time) lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);
            placeSplitColNoChart(leftOff, label_glycemia, label_trend, label_delta, label_age, label_source);
            placeSplitColNoChart(rightOff, label_glycemia1, label_trend1, label_delta1, label_age1,
                                 label_source1);
        } else if (page == 1) {
            if (label_time) lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);
            placeSplitColWithChart(leftOff, label_glycemia, label_trend, label_delta, label_age, label_source,
                                   chart_spark, true);
            placeSplitColWithChart(rightOff, label_glycemia1, label_trend1, label_delta1, label_age1,
                                   label_source1, chart_spark1, false);
            refreshSplitSparklines();
        } else {
            // Pump dual: meno → menšia gly → medzera → trend+delta → IOB  COB → bazál/vek
            if (chart_spark) lv_obj_add_flag(chart_spark, LV_OBJ_FLAG_HIDDEN);
            if (chart_spark1) lv_obj_add_flag(chart_spark1, LV_OBJ_FLAG_HIDDEN);
            if (label_time) lv_obj_add_flag(label_time, LV_OBJ_FLAG_HIDDEN);

            auto placeSplitColPump = [&](int slot, int colOff, lv_obj_t *g, lv_obj_t *tr, lv_obj_t *dlt,
                                         lv_obj_t *age, lv_obj_t *src) {
                const bool nsPump = diaUiProfileIsNs(slot);
                const int glyY = nameY + kSplitNameClear;
                const int arrowY = glyY + 52;
                const int iobY = arrowY + 52;
                // src = 2 riadky (IOB/COB + bazál/rez) → medzera → vek
                const int ageY = iobY + 60;

                lv_obj_set_style_text_font(g, &font_sk_48, LV_PART_MAIN);
                lv_obj_set_style_text_font(tr, &font_sk_48, LV_PART_MAIN);
                lv_obj_set_style_text_font(dlt, &font_sk_20, LV_PART_MAIN);
                lv_obj_set_style_text_font(age, &font_sk_20, LV_PART_MAIN);
                if (src) lv_obj_set_style_text_font(src, &font_sk_20, LV_PART_MAIN);

                lv_obj_align(g, LV_ALIGN_TOP_MID, colOff, glyY);
                const int ax = show_delta ? -22 : 0;
                lv_obj_align(tr, LV_ALIGN_TOP_MID, colOff + ax, arrowY);
                if (dlt) {
                    if (show_delta) {
                        lv_obj_align(dlt, LV_ALIGN_TOP_MID, colOff + ax + 52, arrowY + 14);
                        lv_obj_clear_flag(dlt, LV_OBJ_FLAG_HIDDEN);
                    } else {
                        lv_obj_add_flag(dlt, LV_OBJ_FLAG_HIDDEN);
                    }
                }
                if (nsPump && src) {
                    lv_obj_set_width(src, fw - 2 * pad);
                    lv_obj_align(src, LV_ALIGN_TOP_MID, colOff, iobY);
                    lv_obj_clear_flag(src, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_align(age, LV_ALIGN_TOP_MID, colOff, ageY);
                } else {
                    if (src) lv_obj_add_flag(src, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_align(age, LV_ALIGN_TOP_MID, colOff, iobY);
                }
                lv_obj_clear_flag(age, LV_OBJ_FLAG_HIDDEN);
            };
            placeSplitColPump(0, leftOff, label_glycemia, label_trend, label_delta, label_age,
                              label_source);
            placeSplitColPump(1, rightOff, label_glycemia1, label_trend1, label_delta1, label_age1,
                              label_source1);
        }
    } else {
        lv_obj_add_flag(label_glycemia1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label_trend1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label_delta1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label_age1, LV_OBJ_FLAG_HIDDEN);
        if (label_name1) lv_obj_add_flag(label_name1, LV_OBJ_FLAG_HIDDEN);
        if (label_name0) lv_obj_add_flag(label_name0, LV_OBJ_FLAG_HIDDEN);
        if (label_source1) lv_obj_add_flag(label_source1, LV_OBJ_FLAG_HIDDEN);
        if (panel_frame0) lv_obj_add_flag(panel_frame0, LV_OBJ_FLAG_HIDDEN);
        if (panel_frame1) lv_obj_add_flag(panel_frame1, LV_OBJ_FLAG_HIDDEN);
        if (chart_spark1) lv_obj_add_flag(chart_spark1, LV_OBJ_FLAG_HIDDEN);
        if (arc_bg) lv_obj_clear_flag(arc_bg, LV_OBJ_FLAG_HIDDEN);
        if (label_status) lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        apply_watchface();
        diaUiPlaceSingleName();
    }
    if (lbl_btn_mode) {
        lv_label_set_text(lbl_btn_mode, S(display_mode >= 2 ? Str::ModeBtn2 : Str::ModeBtn1));
    }
    if (split) {
        diaUiUpdateSlotFace(0);
        diaUiUpdateSlotFace(1);
    } else {
        diaUiUpdateSlotFace(diaShownSlot());
    }
#else
    (void)0;
#endif
}

void diaUiProcessPendingPrefSave() {
    if (!pending_pref_save) return;
    pending_pref_save = false;
    int val = pending_pref_val;
    char buf[64];

    if (pending_pref_mode == 1) {
        day_brightness = val;
        preferences.putInt("day_br", val);
        snprintf(buf, sizeof(buf), S(Str::DayBrFmt), val);
        lv_label_set_text(lbl_btn_day, buf);
        updateBrightness();
    } else if (pending_pref_mode == 2) {
        night_brightness = val;
        preferences.putInt("night_br", val);
        snprintf(buf, sizeof(buf), S(Str::NightBrFmt), val);
        lv_label_set_text(lbl_btn_night, buf);
        updateBrightness();
    } else if (pending_pref_mode == 3) {
        targetLow = use_mgdl ? mgdl_to_mmol((float)val) : (val / 10.0f);
        // Menu limity = spoločné pre A aj B (portál môže mať per-user, menu zjednotí)
        profiles[0].targetLow = targetLow;
        profiles[1].targetLow = targetLow;
        preferences.putFloat("target_low", targetLow);
        diaConfigSaveProfiles();
        formatTargetLabel(S(Str::Low), targetLow, buf, sizeof(buf));
        lv_label_set_text(lbl_btn_low, buf);
        glucoseRecalcSlotVisuals();
        diaAudioResetChimeTimer();
        refresh_ui_colors();
        diaUiUpdateSlotFace(0);
        if (display_mode >= 2) diaUiUpdateSlotFace(1);
    } else if (pending_pref_mode == 4) {
        targetHigh = use_mgdl ? mgdl_to_mmol((float)val) : (val / 10.0f);
        profiles[0].targetHigh = targetHigh;
        profiles[1].targetHigh = targetHigh;
        preferences.putFloat("target_high", targetHigh);
        diaConfigSaveProfiles();
        formatTargetLabel(S(Str::High), targetHigh, buf, sizeof(buf));
        lv_label_set_text(lbl_btn_high, buf);
        glucoseRecalcSlotVisuals();
        refresh_ui_colors();
        diaUiUpdateSlotFace(0);
        if (display_mode >= 2) diaUiUpdateSlotFace(1);
    } else if (pending_pref_mode == 5) {
        targetCrit = use_mgdl ? mgdl_to_mmol((float)val) : (val / 10.0f);
        profiles[0].targetCrit = targetCrit;
        profiles[1].targetCrit = targetCrit;
        preferences.putFloat("target_crit", targetCrit);
        diaConfigSaveProfiles();
        formatTargetLabel(S(Str::Crit), targetCrit, buf, sizeof(buf));
        lv_label_set_text(lbl_btn_crit, buf);
        glucoseRecalcSlotVisuals();
        diaAudioResetChimeTimer();
        refresh_ui_colors();
        diaUiUpdateSlotFace(0);
        if (display_mode >= 2) diaUiUpdateSlotFace(1);
    } else if (pending_pref_mode == 6) {
        day_start_hour = val;
        preferences.putInt("day_h", val);
        snprintf(buf, sizeof(buf), S(Str::DayFromFmt), val);
        lv_label_set_text(lbl_btn_day_h, buf);
        updateBrightness();
    } else if (pending_pref_mode == 7) {
        night_start_hour = val;
        preferences.putInt("night_h", val);
        snprintf(buf, sizeof(buf), S(Str::NightFromFmt), val);
        lv_label_set_text(lbl_btn_night_h, buf);
        updateBrightness();
    }
}

void diaUiUpdateArcColors(uint32_t currentMillis) {
    // Plynulý pulse bez skoku: fáza z ms, bez time() v hot path
    const float pulse = 0.5f + 0.5f * sinf((float)currentMillis * (float)(2.0 * M_PI / 1200.0));
#if defined(DIA_BOARD_WS_3_5)
    if (diaUiIsSplitMode()) {
        lv_obj_t *frames[2] = {panel_frame0, panel_frame1};
        lv_obj_t *glyLabs[2] = {label_glycemia, label_glycemia1};
        for (int i = 0; i < 2; i++) {
            lv_obj_t *f = frames[i];
            if (!f) continue;
            const DiaSlotReading &s = slotReadings[i];
            const bool hardErr = diaUiHardVisualErr(s.err, s.has_valid_reading && s.mmol > 0.0f);
            const bool stale = s.data_is_stale || hardErr;
            // Staré / auth majú prednosť pred critical pulse (Network blip nesiví)
            if (stale) {
                diaUiSetPanelFrameColor(f, 0x555555u);
                if (glyLabs[i] && s.has_valid_reading)
                    lv_obj_set_style_text_color(glyLabs[i], lv_color_hex(0x888888), LV_PART_MAIN);
            } else if (s.is_critical && s.has_valid_reading && s.mmol > 0.0f) {
                uint8_t r = (s.arc_color >> 16) & 0xFF;
                uint8_t g = (s.arc_color >> 8) & 0xFF;
                uint8_t b = s.arc_color & 0xFF;
                uint8_t dark_r = r / 4, dark_g = g / 4, dark_b = b / 4;
                const uint8_t out_r = (uint8_t)(dark_r + pulse * (r - dark_r));
                const uint8_t out_g = (uint8_t)(dark_g + pulse * (g - dark_g));
                const uint8_t out_b = (uint8_t)(dark_b + pulse * (b - dark_b));
                lv_obj_set_style_border_color(f, lv_color_make(out_r, out_g, out_b), LV_PART_MAIN);
                if (glyLabs[i]) {
                    lv_obj_set_style_text_color(glyLabs[i], lv_color_make(out_r, out_g, out_b),
                                                LV_PART_MAIN);
                }
            } else if (s.has_valid_reading && s.mmol > 0.0f) {
                diaUiSetPanelFrameColor(f, s.arc_color);
                if (glyLabs[i])
                    lv_obj_set_style_text_color(glyLabs[i], lv_color_hex(s.arc_color), LV_PART_MAIN);
            } else {
                diaUiSetPanelFrameColor(f, 0x222222u);
            }
        }
        return;
    }
#endif
    // Single: stale / ťažká chyba > critical (Network blip necháva farbu)
    const bool pumpFace = (watchface_style == 3 && show_pump);
    auto paintArc = [&](uint32_t c) {
        lv_obj_set_style_arc_color(arc_bg, lv_color_hex(c), LV_PART_INDICATOR);
        if (pumpFace) diaUiColorPumpSatellites(c);
    };
    auto paintArcRgb = [&](uint8_t out_r, uint8_t out_g, uint8_t out_b) {
        lv_obj_set_style_arc_color(arc_bg, lv_color_make(out_r, out_g, out_b), LV_PART_INDICATOR);
        if (pumpFace) {
            const uint32_t c = ((uint32_t)out_r << 16) | ((uint32_t)out_g << 8) | out_b;
            diaUiColorPumpSatellites(c);
        }
        // Pump face: glykémia v farbe rozsahu (ako mockup) + blink pri critical
        if (pumpFace && label_glycemia && has_valid_reading && !data_is_stale)
            lv_obj_set_style_text_color(label_glycemia, lv_color_make(out_r, out_g, out_b),
                                        LV_PART_MAIN);
    };

    if (data_is_stale || diaUiHardVisualErr(last_fetch_error, has_valid_reading)) {
        paintArc(0x555555u);
    } else if (is_critical) {
        uint8_t r = (current_arc_color >> 16) & 0xFF;
        uint8_t g = (current_arc_color >> 8) & 0xFF;
        uint8_t b = current_arc_color & 0xFF;
        uint8_t dark_r = r / 4, dark_g = g / 4, dark_b = b / 4;
        paintArcRgb((uint8_t)(dark_r + pulse * (r - dark_r)),
                    (uint8_t)(dark_g + pulse * (g - dark_g)),
                    (uint8_t)(dark_b + pulse * (b - dark_b)));
    } else {
        paintArc(current_arc_color);
        if (pumpFace && label_glycemia && has_valid_reading)
            lv_obj_set_style_text_color(label_glycemia, lv_color_hex(current_arc_color),
                                        LV_PART_MAIN);
    }
}

static lv_obj_t *splash_obj = nullptr;
static uint32_t s_splashShownAt = 0;
static constexpr uint32_t kSplashHoldMs = 2200;

static void diaUiHideSplash() {
    if (!splash_obj) return;
    lv_obj_del(splash_obj);
    splash_obj = nullptr;
    s_splashShownAt = 0;
}

void diaUiHoldSplash() {
    if (!splash_obj || s_splashShownAt == 0) return;
    // Len čas. lv_tick by spustil 2,2 s timer a logo by zmizlo skôr.
    while ((uint32_t)(millis() - s_splashShownAt) < kSplashHoldMs) delay(20);
}

static void splash_hide_cb(lv_timer_t *timer) {
    diaUiHideSplash();
    lv_timer_del(timer);
}

static void diaUiShowSplash() {
    splash_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(splash_obj, DIA_SCREEN_W, DIA_SCREEN_H);
    lv_obj_center(splash_obj);
    lv_obj_set_style_radius(splash_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(splash_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(splash_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(splash_obj, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(splash_obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(splash_obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *img = lv_img_create(splash_obj);
    lv_img_set_src(img, &logo_boot);
#if DIA_SPLASH_ZOOM != 256
    lv_img_set_zoom(img, DIA_SPLASH_ZOOM);
    lv_img_set_antialias(img, true);
#endif
    lv_obj_center(img);

    lv_obj_t *title = lv_label_create(splash_obj);
    char ver[16];
    snprintf(ver, sizeof(ver), "Ver. %d.%d", FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR);
    lv_label_set_text(title, ver);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1fa3ec), LV_PART_MAIN);
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_style_text_font(title, &font_sk_20, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_BOTTOM_MID, 0, -28);
#else
    lv_obj_set_style_text_font(title, &font_sk_16, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_BOTTOM_MID, 0, -16);
#endif

    s_splashShownAt = millis();
    lv_timer_create(splash_hide_cb, kSplashHoldMs, NULL);
}

static void diaUiApplyLanguage();

static void toggle_lang_cb(lv_event_t *e) {
    (void)e;
    diaI18nCycle();
    diaUiApplyLanguage();
}

/** Jemný prechod od farebného oblúka. Maska je hotová raz, snímok ju len prefarbí. */
struct DiaGlowMask {
    uint8_t *px;
    int w;
    int h;
    int cx;
    int cy;
    int rad0;
    int radIn;
    uint16_t a0;
    uint16_t a1;
    int outer;
    int ring;
};

static DiaGlowMask s_glow;

static bool diaUiGlowSame(const DiaGlowMask &g, int w, int h, int cx, int cy, int outer, int ring,
                           uint16_t a0, uint16_t a1) {
    return g.px && g.w == w && g.h == h && g.cx == cx && g.cy == cy && g.outer == outer &&
           g.ring == ring && g.a0 == a0 && g.a1 == a1;
}

static void diaUiGlowBuild(int w, int h, int cx, int cy, int outer, int ring, uint16_t a0, uint16_t a1) {
    if (diaUiGlowSame(s_glow, w, h, cx, cy, outer, ring, a0, a1)) return;
    const size_t n = (size_t)w * (size_t)h;
    if (!s_glow.px || s_glow.w != w || s_glow.h != h) {
        if (s_glow.px) heap_caps_free(s_glow.px);
        s_glow.px = (uint8_t *)heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_glow.px) s_glow.px = (uint8_t *)heap_caps_malloc(n, MALLOC_CAP_8BIT);
        s_glow.w = w;
        s_glow.h = h;
    }
    if (!s_glow.px) return;

    const bool roundFace = outer < 150;
    const int bandW = roundFace ? 2 : 3;
    const int step = roundFace ? 1 : 2;
    const int bands = roundFace ? 24 : 16;
    const int peak = roundFace ? 108 : 142;
    const int rad0 = outer - ring;
    int reach = (bands - 1) * step + bandW;
    int radIn = rad0 - reach;
    if (radIn < 0) radIn = 0;

    const int span0 = (int)a1 - (int)a0;
    const bool full = span0 >= 360 || span0 <= -360;
    int span = span0;
    while (span < 0) span += 360;
    while (span >= 360) span -= 360;
    const int startDeg = (int)(a0 % 360);

    memset(s_glow.px, 0, n);
    const uint32_t rad0sq = (uint32_t)rad0 * (uint32_t)rad0;
    const uint32_t radInsq = (uint32_t)radIn * (uint32_t)radIn;

    for (int y = 0; y < h; y++) {
        const int dy = y - cy;
        if (dy > rad0 || dy < -rad0) continue;
        if ((y & 15) == 0) vTaskDelay(1);
        uint8_t *row = s_glow.px + (size_t)y * (size_t)w;
        for (int x = 0; x < w; x++) {
            const int dx = x - cx;
            if (dx > rad0 || dx < -rad0) continue;
            const uint32_t d2 = (uint32_t)(dx * dx + dy * dy);
            if (d2 > rad0sq || d2 <= radInsq) continue;
            if (!full && (dx != 0 || dy != 0)) {
                int rel = (int)lv_atan2(dy, dx) - startDeg;
                if (rel < 0) rel += 360;
                if (rel > span) continue;
            }
            int acc = 0;
            int R = rad0;
            for (int i = 0; i < bands && R > 0; i++) {
                int Rin = R - bandW;
                if (Rin < 0) Rin = 0;
                const uint32_t o2 = (uint32_t)R * (uint32_t)R;
                const uint32_t i2 = (uint32_t)Rin * (uint32_t)Rin;
                if (d2 <= o2 && d2 > i2) {
                    const int opa = diaUiInwardOpa(i, bands, peak);
                    if (opa) acc = acc + opa - (acc * opa) / 255;
                }
                R -= step;
            }
            if (acc > 255) acc = 255;
            row[x] = (uint8_t)acc;
        }
    }

    s_glow.cx = cx;
    s_glow.cy = cy;
    s_glow.rad0 = rad0;
    s_glow.radIn = radIn;
    s_glow.a0 = a0;
    s_glow.a1 = a1;
    s_glow.outer = outer;
    s_glow.ring = ring;
}

static void diaUiGlowBlit(lv_draw_ctx_t *ctx, const lv_area_t *box, lv_color_t col) {
    if (!s_glow.px || !ctx->buf || !ctx->buf_area || !ctx->clip_area) return;
    const lv_area_t *clip = ctx->clip_area;
    const lv_area_t *ba = ctx->buf_area;
    lv_coord_t x1 = LV_MAX(clip->x1, box->x1);
    lv_coord_t y1 = LV_MAX(clip->y1, box->y1);
    lv_coord_t x2 = LV_MIN(clip->x2, box->x2);
    lv_coord_t y2 = LV_MIN(clip->y2, box->y2);
    if (x2 < x1 || y2 < y1) return;

    lv_color_t *buf = (lv_color_t *)ctx->buf;
    const int bufW = lv_area_get_width(ba);
    const int cx = box->x1 + s_glow.cx;
    const int cy = box->y1 + s_glow.cy;
    const uint32_t rad0sq = (uint32_t)s_glow.rad0 * (uint32_t)s_glow.rad0;
    const uint32_t radInsq = (uint32_t)s_glow.radIn * (uint32_t)s_glow.radIn;

    for (int y = y1; y <= y2; y++) {
        const int dy = y - cy;
        const int dy2 = dy * dy;
        if ((uint32_t)dy2 > rad0sq) continue;
        lv_sqrt_res_t q;
        lv_sqrt(rad0sq - (uint32_t)dy2, &q, 0x8000);
        int dxOut = q.i;
        int dxIn = 0;
        if ((uint32_t)dy2 < radInsq) {
            lv_sqrt(radInsq - (uint32_t)dy2, &q, 0x8000);
            dxIn = q.i;
        }
        const int my = y - box->y1;
        if (my < 0 || my >= s_glow.h) continue;
        const uint8_t *row = s_glow.px + (size_t)my * (size_t)s_glow.w;
        const int segs[2][2] = {{cx - dxOut, cx - dxIn}, {cx + (dxIn == 0 ? 1 : dxIn), cx + dxOut}};
        for (int s = 0; s < 2; s++) {
            int xa = segs[s][0];
            int xb = segs[s][1];
            if (xa < x1) xa = x1;
            if (xb > x2) xb = x2;
            if (xb < xa) continue;
            lv_color_t *dst = buf + (y - ba->y1) * bufW + (xa - ba->x1);
            for (int x = xa; x <= xb; x++, dst++) {
                const int mx = x - box->x1;
                if ((unsigned)mx >= (unsigned)s_glow.w) continue;
                const uint8_t a = row[mx];
                if (a) *dst = lv_color_mix(col, *dst, a);
            }
        }
    }
}

static void diaUiArcInwardFade(lv_event_t *e) {
    if (!show_glow) return;
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    if (!obj || !ctx) return;
    lv_arc_t *arc = (lv_arc_t *)obj;
    const uint16_t a0 = (uint16_t)(arc->indic_angle_start + arc->rotation);
    const uint16_t a1 = (uint16_t)(arc->indic_angle_end + arc->rotation);
    if (a0 == a1) return;

    lv_coord_t left = lv_obj_get_style_pad_left(obj, LV_PART_MAIN);
    lv_coord_t right = lv_obj_get_style_pad_right(obj, LV_PART_MAIN);
    lv_coord_t top = lv_obj_get_style_pad_top(obj, LV_PART_MAIN);
    lv_coord_t bottom = lv_obj_get_style_pad_bottom(obj, LV_PART_MAIN);
    lv_coord_t outer = (LV_MIN(lv_obj_get_width(obj) - left - right,
                               lv_obj_get_height(obj) - top - bottom)) / 2;
    // Štart žiary = vnútorný okraj aktuálneho (aj tenšieho) prstenca; bands/peak v GlowBuild ostávajú
    lv_coord_t ring = lv_obj_get_style_arc_width(obj, LV_PART_INDICATOR);
    if (outer <= ring + 4) return;

    const int w = lv_obj_get_width(obj);
    const int h = lv_obj_get_height(obj);
    const int cx = outer + left;
    const int cy = outer + top;
    diaUiGlowBuild(w, h, cx, cy, outer, ring, a0, a1);
    diaUiGlowBlit(ctx, &obj->coords, lv_obj_get_style_arc_color(obj, LV_PART_INDICATOR));
}

void build_ui() {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);

    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(lv_scr_act(), main_screen_event_cb, LV_EVENT_ALL, NULL);

    arc_bg = lv_arc_create(lv_scr_act());
    lv_obj_set_size(arc_bg, DIA_FACE_SIZE, DIA_FACE_SIZE);
    const int arc_w = (DIA_FACE_SIZE >= 280) ? 18 : 15;
    lv_obj_set_style_arc_color(arc_bg, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_bg, arc_w, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_bg, lv_color_hex(0x0055FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_bg, arc_w, LV_PART_INDICATOR);

    lv_obj_remove_style(arc_bg, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(arc_bg);
    lv_obj_add_event_cb(arc_bg, diaUiArcInwardFade, LV_EVENT_DRAW_MAIN_END, NULL);

    label_time = lv_label_create(lv_scr_act());
    lv_label_set_text(label_time, "--:--");
    lv_obj_set_style_text_color(label_time, lv_color_hex(0x888888), LV_PART_MAIN);

    label_glycemia = lv_label_create(lv_scr_act());
    lv_label_set_text(label_glycemia, "WiFi");
    lv_obj_set_style_text_color(label_glycemia, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_glycemia, &font_sk_48, LV_PART_MAIN);

    label_trend = lv_label_create(lv_scr_act());
    lv_label_set_text(label_trend, "...");
    lv_obj_set_style_text_color(label_trend, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_trend, &font_sk_48, LV_PART_MAIN);

    label_delta = lv_label_create(lv_scr_act());
    lv_label_set_text(label_delta, "");
    lv_obj_set_style_text_color(label_delta, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_delta, &font_sk_20, LV_PART_MAIN);

    label_age = lv_label_create(lv_scr_act());
    lv_label_set_text(label_age, "");
    lv_obj_set_style_text_color(label_age, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_age, &font_sk_20, LV_PART_MAIN);

    label_status = lv_label_create(lv_scr_act());
    lv_label_set_text(label_status, "");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF8800), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_status, &font_sk_20, LV_PART_MAIN);

    label_source = lv_label_create(lv_scr_act());
    lv_label_set_text(label_source, "");
    lv_obj_set_style_text_color(label_source, lv_color_hex(0x88AACC), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_source, &font_sk_16, LV_PART_MAIN);
    lv_obj_add_flag(label_source, LV_OBJ_FLAG_HIDDEN);

    label_pump = lv_label_create(lv_scr_act());
    lv_label_set_text(label_pump, "");
    lv_obj_set_width(label_pump, faceX(168));
    lv_obj_set_style_text_color(label_pump, lv_color_hex(0xE6EEF6), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_pump, &font_sk_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_pump, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(label_pump, 2, LV_PART_MAIN);
    lv_label_set_long_mode(label_pump, LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(label_pump, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_pump, LV_OBJ_FLAG_CLICKABLE);

    auto makePumpArc = [](lv_coord_t sz) -> lv_obj_t * {
        lv_obj_t *a = lv_arc_create(lv_scr_act());
        lv_obj_set_size(a, sz, sz);
        lv_arc_set_bg_angles(a, 0, 360);
        lv_arc_set_angles(a, 0, 360);
        lv_arc_set_rotation(a, 270);
        lv_obj_set_style_arc_width(a, 2, LV_PART_MAIN);
        lv_obj_set_style_arc_color(a, lv_color_hex(0xC41E3A), LV_PART_MAIN);
        lv_obj_set_style_arc_width(a, 2, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(a, lv_color_hex(0xC41E3A), LV_PART_INDICATOR);
        lv_obj_remove_style(a, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(a, LV_OBJ_FLAG_HIDDEN);
        return a;
    };
    arc_pump_cob = makePumpArc(faceX(64));
    arc_pump_iob = makePumpArc(faceX(64));
    arc_pump_bg = makePumpArc(faceX(86));

    auto makePumpLab = []() -> lv_obj_t * {
        lv_obj_t *l = lv_label_create(lv_scr_act());
        lv_label_set_text(l, "");
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(l, &font_sk_16, LV_PART_MAIN);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_line_space(l, -2, LV_PART_MAIN);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(l, faceX(56));
        lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
        return l;
    };
    label_pump_cob = makePumpLab();
    label_pump_iob = makePumpLab();
    label_pump_cob_val = makePumpLab();
    label_pump_iob_val = makePumpLab();
    if (label_pump_cob_val)
        lv_obj_set_style_text_font(label_pump_cob_val, &font_sk_20, LV_PART_MAIN);
    if (label_pump_iob_val)
        lv_obj_set_style_text_font(label_pump_iob_val, &font_sk_20, LV_PART_MAIN);

    auto makeSpark = [](lv_obj_t **outChart, lv_chart_series_t **outSer) {
        *outChart = lv_chart_create(lv_scr_act());
        lv_obj_set_size(*outChart, 168, 58);
        lv_chart_set_type(*outChart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(*outChart, DIA_HISTORY_MAX);
        lv_chart_set_range(*outChart, LV_CHART_AXIS_PRIMARY_Y, 30, 120); // 3.0–12.0 mmol ×10
        diaUiStyleSpark(*outChart, true); // default; kruh prepíše pri apply_watchface
        lv_obj_clear_flag(*outChart, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(*outChart, LV_OBJ_FLAG_HIDDEN);
        // Gradient výplň pod čiarou (LVGL 8 draw mask)
        lv_obj_add_event_cb(*outChart, [](lv_event_t *e) {
            lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
            if (!dsc || dsc->part != LV_PART_ITEMS || !dsc->p1 || !dsc->p2) return;
            lv_obj_t *obj = lv_event_get_target(e);

            lv_draw_mask_line_param_t line_mask_param;
            lv_draw_mask_line_points_init(&line_mask_param, dsc->p1->x, dsc->p1->y, dsc->p2->x, dsc->p2->y,
                                          LV_DRAW_MASK_LINE_SIDE_BOTTOM);
            int16_t line_mask_id = lv_draw_mask_add(&line_mask_param, NULL);

            lv_coord_t h = lv_obj_get_height(obj);
            lv_draw_mask_fade_param_t fade_mask_param;
            lv_draw_mask_fade_init(&fade_mask_param, &obj->coords, LV_OPA_50, obj->coords.y1 + h / 6,
                                   LV_OPA_TRANSP, obj->coords.y2);
            int16_t fade_mask_id = lv_draw_mask_add(&fade_mask_param, NULL);

            lv_draw_rect_dsc_t draw_rect_dsc;
            lv_draw_rect_dsc_init(&draw_rect_dsc);
            draw_rect_dsc.bg_opa = LV_OPA_40;
            draw_rect_dsc.bg_color = dsc->line_dsc ? dsc->line_dsc->color : lv_color_hex(0x00AAFF);

            lv_area_t a;
            a.x1 = dsc->p1->x;
            a.x2 = dsc->p2->x - 1;
            a.y1 = LV_MIN(dsc->p1->y, dsc->p2->y);
            a.y2 = obj->coords.y2;
            lv_draw_rect(dsc->draw_ctx, &draw_rect_dsc, &a);

            lv_draw_mask_free_param(&line_mask_param);
            lv_draw_mask_free_param(&fade_mask_param);
            lv_draw_mask_remove_id(line_mask_id);
            lv_draw_mask_remove_id(fade_mask_id);
        }, LV_EVENT_DRAW_PART_BEGIN, NULL);
        *outSer = lv_chart_add_series(*outChart, lv_color_hex(0x00AAFF), LV_CHART_AXIS_PRIMARY_Y);
    };
    makeSpark(&chart_spark, &chart_spark_ser);

#if defined(DIA_BOARD_WS_3_5)
    label_name0 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(label_name0, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_name0, lv_color_hex(0x88AACC), LV_PART_MAIN);
    lv_label_set_text(label_name0, "");
    lv_obj_add_flag(label_name0, LV_OBJ_FLAG_HIDDEN);

    label_name1 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(label_name1, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label_name1, lv_color_hex(0x88AACC), LV_PART_MAIN);
    lv_label_set_text(label_name1, "");
    lv_obj_add_flag(label_name1, LV_OBJ_FLAG_HIDDEN);

    label_glycemia1 = lv_label_create(lv_scr_act());
    lv_label_set_text(label_glycemia1, "...");
    lv_obj_set_style_text_color(label_glycemia1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_glycemia1, &font_sk_48, LV_PART_MAIN);
    lv_obj_add_flag(label_glycemia1, LV_OBJ_FLAG_HIDDEN);

    label_trend1 = lv_label_create(lv_scr_act());
    lv_label_set_text(label_trend1, "");
    lv_obj_set_style_text_color(label_trend1, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_trend1, &font_sk_48, LV_PART_MAIN);
    lv_obj_add_flag(label_trend1, LV_OBJ_FLAG_HIDDEN);

    label_delta1 = lv_label_create(lv_scr_act());
    lv_label_set_text(label_delta1, "");
    lv_obj_set_style_text_color(label_delta1, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_delta1, &font_sk_20, LV_PART_MAIN);
    lv_obj_add_flag(label_delta1, LV_OBJ_FLAG_HIDDEN);

    label_age1 = lv_label_create(lv_scr_act());
    lv_label_set_text(label_age1, "");
    lv_obj_set_style_text_color(label_age1, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_age1, &font_sk_20, LV_PART_MAIN);
    lv_obj_add_flag(label_age1, LV_OBJ_FLAG_HIDDEN);

    label_source1 = lv_label_create(lv_scr_act());
    lv_label_set_text(label_source1, "");
    lv_obj_set_style_text_color(label_source1, lv_color_hex(0x88AACC), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_source1, &font_sk_16, LV_PART_MAIN);
    lv_obj_add_flag(label_source1, LV_OBJ_FLAG_HIDDEN);

    split_line = lv_obj_create(lv_scr_act());
    lv_obj_set_size(split_line, 2, DIA_SCREEN_H - 20);
    lv_obj_set_style_bg_color(split_line, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_border_width(split_line, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(split_line, 0, LV_PART_MAIN);
    lv_obj_align(split_line, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(split_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(split_line, LV_OBJ_FLAG_HIDDEN);

    panel_frame0 = lv_obj_create(lv_scr_act());
    diaUiStylePanelFrame(panel_frame0);
    lv_obj_add_flag(panel_frame0, LV_OBJ_FLAG_HIDDEN);

    panel_frame1 = lv_obj_create(lv_scr_act());
    diaUiStylePanelFrame(panel_frame1);
    lv_obj_add_flag(panel_frame1, LV_OBJ_FLAG_HIDDEN);

    makeSpark(&chart_spark1, &chart_spark_ser1);
#endif

    apply_watchface();
    diaUiApplyDisplayMode();

    ui_settings_panel = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(ui_settings_panel, DIA_SCREEN_W, DIA_SCREEN_H);
    lv_obj_set_style_bg_color(ui_settings_panel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_settings_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    diaUiMakeTileviewSwipeFriendly(ui_settings_panel);

    char buf[64];

    auto setup_menu_tile = [](lv_obj_t *tile) {
#if defined(DIA_BOARD_WS_3_5)
        // Dva stĺpce – centrované (START nechávalo ~48 px dieru vpravo → „vyosené“)
        lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(tile, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_left(tile, 18, LV_PART_MAIN);
        lv_obj_set_style_pad_right(tile, 18, LV_PART_MAIN);
#else
        lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
#endif
        lv_obj_set_style_pad_top(tile, DIA_MENU_PAD_TOP, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(tile, 24, LV_PART_MAIN);
        lv_obj_set_style_pad_row(tile, DIA_MENU_PAD_ROW, LV_PART_MAIN);
        lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
        diaUiMakeScrollSnappy(tile);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    };

    lv_obj_t *tile1 = lv_tileview_add_tile(ui_settings_panel, 0, 0, LV_DIR_HOR);
    setup_menu_tile(tile1);

    lbl_t1_title = lv_label_create(tile1);
    lv_obj_set_style_text_font(lbl_t1_title, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_t1_title, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_label_set_text(lbl_t1_title, S(Str::SysTitle));
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(lbl_t1_title, DIA_MENU_BTN_W_FULL);
    lv_obj_set_style_text_align(lbl_t1_title, LV_TEXT_ALIGN_CENTER, 0);
#endif

    lbl_btn_info = create_menu_btn(tile1, S(Str::Info), open_info_cb, 0, 0x0055AA);
    lbl_btn_refresh = create_menu_btn(tile1, S(Str::Refresh), sync_btn_cb, 0, 0x008800);
#if defined(DIA_BOARD_WS_3_5)
    lbl_btn_mode = create_menu_btn(tile1, S(display_mode >= 2 ? Str::ModeBtn2 : Str::ModeBtn1),
                                   toggle_display_mode_cb, 0, 0x553300);
    {
        char childBuf[48];
        const char *nm = profiles[diaShownSlot()].name.c_str();
        if (!nm || !nm[0]) nm = diaShownSlot() == 0 ? "A" : "B";
        snprintf(childBuf, sizeof(childBuf), S(Str::ShowChild), nm);
        lbl_btn_child = create_menu_btn(tile1, childBuf, toggle_child_cb, 0, 0x1a3a55);
    }
    diaUiRefreshChildBtn();
#endif
    lbl_btn_autoupdate = create_menu_btn(tile1, S(auto_update ? Str::AutoOn : Str::AutoOff), toggle_autoupdate_cb, 0, 0x004400);
    lbl_btn_update = create_menu_btn(tile1, S(Str::Update), manual_update_cb, 0, 0x002288);
    lbl_btn_forget = create_menu_btn(tile1, S(Str::ForgetWifi), open_confirm_cb, 0, 0x880000);
    lbl_btn_pin = create_menu_btn(tile1, S(diaPinIsSet() ? Str::PinOn : Str::PinOff), open_pin_cb, 0, 0x224466);
    lbl_btn_pin_clear = create_menu_btn(tile1, S(Str::PinClear), open_pin_clear_cb, 0, 0x664422);
    if (!diaPinIsSet()) lv_obj_add_flag(lv_obj_get_parent(lbl_btn_pin_clear), LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *spacer = lv_label_create(tile1);
    lv_label_set_text(spacer, "");
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(spacer, DIA_MENU_BTN_W_FULL);
#endif
    lv_obj_set_height(spacer, 8);
    lbl_back1 = create_menu_btn(tile1, S(Str::Back), close_settings_cb, 0, 0x555555, DIA_MENU_BTN_W_FULL);

    lv_obj_t *tile2 = lv_tileview_add_tile(ui_settings_panel, 1, 0, LV_DIR_HOR);
    setup_menu_tile(tile2);

    lbl_t2_title = lv_label_create(tile2);
    lv_obj_set_style_text_font(lbl_t2_title, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_t2_title, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_label_set_text(lbl_t2_title, S(Str::LimitsTitle));
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(lbl_t2_title, DIA_MENU_BTN_W_FULL);
    lv_obj_set_style_text_align(lbl_t2_title, LV_TEXT_ALIGN_CENTER, 0);
#endif

    formatTargetLabel(S(Str::Low), targetLow, buf, sizeof(buf));
    lbl_btn_low = create_menu_btn(tile2, buf, btn_edit_cb, 3, 0x550000);
    formatTargetLabel(S(Str::High), targetHigh, buf, sizeof(buf));
    lbl_btn_high = create_menu_btn(tile2, buf, btn_edit_cb, 4, 0xAA5500);
    formatTargetLabel(S(Str::Crit), targetCrit, buf, sizeof(buf));
    lbl_btn_crit = create_menu_btn(tile2, buf, btn_edit_cb, 5, 0x550000);
#if defined(DIA_BOARD_WS_3_5)
    if (diaBoardHasAudio()) {
        lbl_btn_sound_open = create_menu_btn(tile2, S(Str::SoundBtn), open_sound_cb, 0, 0x334422);
        char snBuf[32];
        snprintf(snBuf, sizeof(snBuf), S(Str::SnoozeFmt), snooze_min);
        lbl_btn_snooze = create_menu_btn(tile2, snBuf, cycle_snooze_cb, 0, 0x334422);
        lbl_btn_snooze_now = create_menu_btn(tile2, S(Str::SnoozeNow), snooze_now_cb, 0, 0x445533);
    }
#endif

    lv_obj_t *spacer2 = lv_label_create(tile2);
    lv_label_set_text(spacer2, "");
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(spacer2, DIA_MENU_BTN_W_FULL);
#endif
    lv_obj_set_height(spacer2, 8);
    lbl_back2 = create_menu_btn(tile2, S(Str::Back), close_settings_cb, 0, 0x555555, DIA_MENU_BTN_W_FULL);

    lv_obj_t *tileLook = lv_tileview_add_tile(ui_settings_panel, 2, 0, LV_DIR_HOR);
    setup_menu_tile(tileLook);

    lbl_look_title = lv_label_create(tileLook);
    lv_obj_set_style_text_font(lbl_look_title, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_look_title, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_label_set_text(lbl_look_title, S(Str::LookTitle));
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(lbl_look_title, DIA_MENU_BTN_W_FULL);
    lv_obj_set_style_text_align(lbl_look_title, LV_TEXT_ALIGN_CENTER, 0);
#endif
    lbl_btn_units = create_menu_btn(tileLook, S(use_mgdl ? Str::UnitsMg : Str::UnitsMmol), toggle_units_cb, 0, 0x333366);
    lbl_btn_delta = create_menu_btn(tileLook, S(show_delta ? Str::DeltaOn : Str::DeltaOff), toggle_delta_cb, 0, 0x333366);
    lbl_btn_source = create_menu_btn(tileLook, S(show_source ? Str::SourceOn : Str::SourceOff), toggle_source_cb, 0, 0x333366);
    // Pumpa (NS IOB/COB) – 1 aj 2 kontá; skryté keď nie je Nightscout
    lbl_btn_pump = create_menu_btn(tileLook, S(show_pump ? Str::PumpOn : Str::PumpOff), toggle_pump_cb, 0, 0x333366);
    diaUiRefreshPumpBtn();
    lbl_btn_glow = create_menu_btn(tileLook, S(show_glow ? Str::GlowOn : Str::GlowOff), toggle_glow_cb, 0, 0x333366);
#if defined(DIA_BOARD_WS_3_5)
    lbl_btn_orient = create_menu_btn(tileLook, S(diaUiOrientStr()), toggle_orient_cb, 0, 0x333366);
#endif
    lbl_btn_lang = create_menu_btn(tileLook, diaI18nNativeName(diaI18nGet()), toggle_lang_cb, 0, 0x334455);
    {
        lv_obj_t *spacerL = lv_label_create(tileLook);
        lv_label_set_text(spacerL, "");
#if defined(DIA_BOARD_WS_3_5)
        lv_obj_set_width(spacerL, DIA_MENU_BTN_W_FULL);
#endif
        lv_obj_set_height(spacerL, 8);
    }
    lbl_back_look = create_menu_btn(tileLook, S(Str::Back), close_settings_cb, 0, 0x555555, DIA_MENU_BTN_W_FULL);

    lv_obj_t *tile3 = lv_tileview_add_tile(ui_settings_panel, 3, 0, LV_DIR_HOR);
    setup_menu_tile(tile3);

    lbl_t3_title = lv_label_create(tile3);
    lv_obj_set_style_text_font(lbl_t3_title, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_t3_title, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_label_set_text(lbl_t3_title, S(Str::BrightTitle));
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(lbl_t3_title, DIA_MENU_BTN_W_FULL);
    lv_obj_set_style_text_align(lbl_t3_title, LV_TEXT_ALIGN_CENTER, 0);
#endif

    snprintf(buf, sizeof(buf), S(Str::DayBrFmt), day_brightness);
    lbl_btn_day = create_menu_btn(tile3, buf, btn_edit_cb, 1, 0x333333);

    snprintf(buf, sizeof(buf), S(Str::DayFromFmt), day_start_hour);
    lbl_btn_day_h = create_menu_btn(tile3, buf, btn_edit_cb, 6, 0x0055AA);

    snprintf(buf, sizeof(buf), S(Str::NightBrFmt), night_brightness);
    lbl_btn_night = create_menu_btn(tile3, buf, btn_edit_cb, 2, 0x333333);

    snprintf(buf, sizeof(buf), S(Str::NightFromFmt), night_start_hour);
    lbl_btn_night_h = create_menu_btn(tile3, buf, btn_edit_cb, 7, 0x002266);

    lv_obj_t *spacer3 = lv_label_create(tile3);
    lv_label_set_text(spacer3, "");
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(spacer3, DIA_MENU_BTN_W_FULL);
#endif
    lv_obj_set_height(spacer3, 8);
    lbl_back3 = create_menu_btn(tile3, S(Str::Back), close_settings_cb, 0, 0x555555, DIA_MENU_BTN_W_FULL);

#if defined(DIA_BOARD_WS_3_5)
    auto make_sub_panel = [&](const char *title, lv_obj_t **titleOut) -> lv_obj_t * {
        lv_obj_t *panel = lv_obj_create(lv_scr_act());
        lv_obj_set_size(panel, DIA_SCREEN_W, DIA_SCREEN_H);
        lv_obj_set_pos(panel, 0, 0);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(panel, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
        setup_menu_tile(panel);
        lv_obj_t *lbl = lv_label_create(panel);
        lv_obj_set_style_text_font(lbl, &font_sk_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
        lv_label_set_text(lbl, title);
#if defined(DIA_BOARD_WS_3_5)
        lv_obj_set_width(lbl, DIA_MENU_BTN_W_FULL);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
#endif
        if (titleOut) *titleOut = lbl;
        return panel;
    };

    if (diaBoardHasAudio()) {
        ui_sound_panel = make_sub_panel(S(Str::SoundTitle), &lbl_sound_title);
        lbl_btn_sound = create_menu_btn(ui_sound_panel, S(sound_on ? Str::SoundOn : Str::SoundOff), toggle_sound_cb, 0, 0x334422);
        lbl_vol = lv_label_create(ui_sound_panel);
        lv_obj_set_style_text_font(lbl_vol, &font_sk_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_vol, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_width(lbl_vol, DIA_MENU_BTN_W_FULL);
        lv_obj_set_style_text_align(lbl_vol, LV_TEXT_ALIGN_CENTER, 0);
        refreshVolLabel();
        lbl_btn_vol_down = create_menu_btn(ui_sound_panel, S(Str::VolDown), vol_step_cb, -1, 0x334422);
        lbl_btn_vol_up = create_menu_btn(ui_sound_panel, S(Str::VolUp), vol_step_cb, 1, 0x334422);
        lv_obj_t *spacerS = lv_label_create(ui_sound_panel);
        lv_label_set_text(spacerS, "");
        lv_obj_set_width(spacerS, DIA_MENU_BTN_W_FULL);
        lv_obj_set_height(spacerS, 8);
        lbl_back_sound = create_menu_btn(ui_sound_panel, S(Str::Back), close_sound_cb, 0, 0x555555, DIA_MENU_BTN_W_FULL);
    }
#endif

    ui_edit_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ui_edit_panel, DIA_SCREEN_W, DIA_SCREEN_H);
    lv_obj_center(ui_edit_panel);
    lv_obj_set_style_bg_color(ui_edit_panel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_edit_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ui_edit_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_edit_panel, LV_OBJ_FLAG_HIDDEN);

    lbl_edit_title = lv_label_create(ui_edit_panel);
    lv_obj_set_style_text_font(lbl_edit_title, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_edit_title, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(lbl_edit_title, LV_ALIGN_TOP_MID, 0, DIA_SCREEN_H >= 300 ? 28 : 15);

    lbl_edit_val = lv_label_create(ui_edit_panel);
    lv_obj_set_style_text_font(lbl_edit_val, &font_sk_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_edit_val, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(lbl_edit_val, LV_ALIGN_CENTER, 0, -35);

    slider_edit = lv_slider_create(ui_edit_panel);
    lv_obj_set_size(slider_edit, DIA_SCREEN_W >= 400 ? 360 : 180, DIA_SCREEN_H >= 300 ? 22 : 15);
    lv_obj_set_style_pad_all(slider_edit, 12, LV_PART_KNOB);
    lv_obj_align(slider_edit, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_event_cb(slider_edit, slider_live_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *btn_save = lv_btn_create(ui_edit_panel);
    lv_obj_set_size(btn_save, DIA_SCREEN_W >= 400 ? 220 : 140, DIA_MENU_BTN_H);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x0055FF), LV_PART_MAIN);
    lbl_save = lv_label_create(btn_save);
    lv_obj_set_style_text_font(lbl_save, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_width(lbl_save, DIA_SCREEN_W >= 400 ? 200 : 120);
    lv_obj_set_style_text_align(lbl_save, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_save, S(Str::Save));
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save, save_edit_cb, LV_EVENT_SHORT_CLICKED, NULL);

    ui_info_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ui_info_panel, DIA_SCREEN_W, DIA_SCREEN_H);
    lv_obj_center(ui_info_panel);
    lv_obj_set_style_bg_color(ui_info_panel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_info_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_info_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui_info_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_info_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ui_info_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_info_panel, LV_OBJ_FLAG_HIDDEN);

    lbl_info_title = lv_label_create(ui_info_panel);
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_style_text_font(lbl_info_title, &font_sk_20, LV_PART_MAIN);
#else
    lv_obj_set_style_text_font(lbl_info_title, &font_sk_16, LV_PART_MAIN);
#endif
    lv_label_set_text(lbl_info_title, S(Str::Info));
    lv_obj_set_style_text_color(lbl_info_title, lv_color_hex(0x00AAFF), LV_PART_MAIN);
    lv_obj_align(lbl_info_title, LV_ALIGN_TOP_MID, 0, diaBoardIsRound() ? 16 : 24);

    ui_info_scroll = lv_obj_create(ui_info_panel);
    lv_obj_set_style_bg_opa(ui_info_scroll, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_info_scroll, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui_info_scroll, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_info_scroll, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(ui_info_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui_info_scroll, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(ui_info_scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui_info_scroll, LV_OBJ_FLAG_SCROLL_ELASTIC);
    if (diaBoardIsRound()) {
        lv_obj_set_size(ui_info_scroll, 210, 128);
        lv_obj_align(ui_info_scroll, LV_ALIGN_TOP_MID, 0, 38);
    } else {
        lv_obj_set_size(ui_info_scroll, 440, 168);
        lv_obj_align(ui_info_scroll, LV_ALIGN_TOP_MID, 0, 52);
    }

    lbl_info_keys = lv_label_create(ui_info_scroll);
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_style_text_font(lbl_info_keys, &font_sk_20, LV_PART_MAIN);
#else
    lv_obj_set_style_text_font(lbl_info_keys, &font_sk_16, LV_PART_MAIN);
#endif
    lv_obj_set_style_text_color(lbl_info_keys, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_line_space(lbl_info_keys, 2, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_info_keys, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lbl_info_keys, S(Str::InfoKeys));
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_align(lbl_info_keys, LV_ALIGN_TOP_MID, -110, 0);
#else
    lv_obj_align(lbl_info_keys, LV_ALIGN_TOP_MID, -52, 0);
#endif

    lbl_info_text = lv_label_create(ui_info_scroll);
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_style_text_font(lbl_info_text, &font_sk_20, LV_PART_MAIN);
#else
    lv_obj_set_style_text_font(lbl_info_text, &font_sk_16, LV_PART_MAIN);
#endif
    lv_obj_set_style_text_color(lbl_info_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_width(lbl_info_text, 180);
#else
    lv_obj_set_width(lbl_info_text, 100);
#endif
    lv_label_set_long_mode(lbl_info_text, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(lbl_info_text, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_line_space(lbl_info_text, 2, LV_PART_MAIN);
    lv_label_set_text(lbl_info_text, "...");
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_align(lbl_info_text, LV_ALIGN_TOP_MID, 90, 0);
#else
    lv_obj_align(lbl_info_text, LV_ALIGN_TOP_MID, 48, 0);
#endif

    lv_obj_t *btn_info_close = lv_btn_create(ui_info_panel);
    lv_obj_set_size(btn_info_close, DIA_SCREEN_W >= 400 ? 200 : 110, DIA_MENU_BTN_H);
    lv_obj_align(btn_info_close, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_bg_color(btn_info_close, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_info_close, 8, LV_PART_MAIN);
    lbl_info_close = lv_label_create(btn_info_close);
#if defined(DIA_BOARD_WS_3_5)
    lv_obj_set_style_text_font(lbl_info_close, &font_sk_20, LV_PART_MAIN);
#else
    lv_obj_set_style_text_font(lbl_info_close, &font_sk_16, LV_PART_MAIN);
#endif
    lv_label_set_text(lbl_info_close, S(Str::Back));
    lv_obj_center(lbl_info_close);
    lv_obj_add_event_cb(btn_info_close, close_info_cb, LV_EVENT_SHORT_CLICKED, NULL);

    lbl_info_status = lv_label_create(ui_info_scroll);
    lv_obj_set_style_text_font(lbl_info_status, &font_sk_20, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_info_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl_info_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(lbl_info_status, LV_OBJ_FLAG_CLICKABLE);

    lbl_info_legal = lv_label_create(ui_info_scroll);
    lv_obj_set_style_text_font(lbl_info_legal, &font_sk_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_info_legal, lv_color_hex(0xD6E4F0), LV_PART_MAIN);
    lv_label_set_long_mode(lbl_info_legal, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl_info_legal, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(lbl_info_legal, 3, LV_PART_MAIN);
    lv_obj_clear_flag(lbl_info_legal, LV_OBJ_FLAG_CLICKABLE);
    diaUiSetLegalInfoText();

    ui_confirm_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ui_confirm_panel, DIA_SCREEN_W, DIA_SCREEN_H);
    lv_obj_center(ui_confirm_panel);
    lv_obj_set_style_bg_color(ui_confirm_panel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_confirm_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ui_confirm_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_confirm_panel, LV_OBJ_FLAG_HIDDEN);

    lbl_confirm_title = lv_label_create(ui_confirm_panel);
    lv_obj_set_style_text_font(lbl_confirm_title, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_confirm_title, lv_color_hex(0xFF5555), LV_PART_MAIN);
    lv_obj_set_width(lbl_confirm_title, DIA_SCREEN_W >= 400 ? 400 : 180);
    lv_label_set_long_mode(lbl_confirm_title, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lbl_confirm_title, S(Str::ConfirmErase));
    lv_obj_set_style_text_align(lbl_confirm_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(lbl_confirm_title, diaBoardIsRound() ? 4 : 6, LV_PART_MAIN);
    // Kruh: text hore, tlačidlá dole — inak sa 2 riadky schovajú za Yes
    lv_obj_align(lbl_confirm_title, LV_ALIGN_TOP_MID, 0, diaBoardIsRound() ? 28 : 48);

    lv_obj_t *btn_confirm_yes = lv_btn_create(ui_confirm_panel);
    lv_obj_set_size(btn_confirm_yes, DIA_SCREEN_W >= 400 ? 240 : 150, DIA_MENU_BTN_H + 4);
    if (diaBoardIsRound())
        lv_obj_align(btn_confirm_yes, LV_ALIGN_BOTTOM_MID, 0, -78);
    else
        lv_obj_align(btn_confirm_yes, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(btn_confirm_yes, lv_color_hex(0xAA0000), LV_PART_MAIN);
    lbl_yes = lv_label_create(btn_confirm_yes);
    lv_obj_set_style_text_font(lbl_yes, &font_sk_20, LV_PART_MAIN);
    lv_label_set_text(lbl_yes, S(Str::YesErase));
    lv_obj_center(lbl_yes);
    lv_obj_add_event_cb(btn_confirm_yes, reset_wifi_confirm_cb, LV_EVENT_SHORT_CLICKED, NULL);

    lv_obj_t *btn_confirm_no = lv_btn_create(ui_confirm_panel);
    lv_obj_set_size(btn_confirm_no, DIA_SCREEN_W >= 400 ? 200 : 120, DIA_MENU_BTN_H + 4);
    lv_obj_align(btn_confirm_no, LV_ALIGN_BOTTOM_MID, 0, diaBoardIsRound() ? -22 : -24);
    lv_obj_set_style_bg_color(btn_confirm_no, lv_color_hex(0x444444), LV_PART_MAIN);
    lbl_no = lv_label_create(btn_confirm_no);
    lv_obj_set_style_text_font(lbl_no, &font_sk_20, LV_PART_MAIN);
    lv_label_set_text(lbl_no, S(Str::Back));
    lv_obj_center(lbl_no);
    lv_obj_add_event_cb(btn_confirm_no, close_confirm_cb, LV_EVENT_SHORT_CLICKED, NULL);

    ui_pin_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ui_pin_panel, DIA_SCREEN_W, DIA_SCREEN_H);
    lv_obj_center(ui_pin_panel);
    lv_obj_set_style_bg_color(ui_pin_panel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_pin_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ui_pin_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_pin_panel, LV_OBJ_FLAG_HIDDEN);
    if (!diaBoardIsRound()) {
        lv_obj_set_style_pad_row(ui_pin_panel, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_top(ui_pin_panel, 16, LV_PART_MAIN);
        lv_obj_set_flex_flow(ui_pin_panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(ui_pin_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    }

    lbl_pin_title = lv_label_create(ui_pin_panel);
    lv_obj_set_style_text_font(lbl_pin_title, diaBoardIsRound() ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_pin_title, lv_color_hex(0xFFCC66), LV_PART_MAIN);
    lv_obj_set_width(lbl_pin_title, diaBoardIsRound() ? 170 : (DIA_SCREEN_W - 16));
    lv_label_set_long_mode(lbl_pin_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(lbl_pin_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_pin_title, S(Str::PinEnter));

    lbl_pin_digits = lv_label_create(ui_pin_panel);
    lv_obj_set_style_text_font(lbl_pin_digits, diaBoardIsRound() ? &font_sk_24 : &font_sk_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_pin_digits, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_label_set_text(lbl_pin_digits, "_ _ _ _");

    static const char *kPinMap[] = {
        "1", "2", "3", "\n",
        "4", "5", "6", "\n",
        "7", "8", "9", "\n",
        "←", "0", "OK", ""
    };
    lv_obj_t *pin_mx = lv_btnmatrix_create(ui_pin_panel);
    lv_btnmatrix_set_map(pin_mx, kPinMap);
    lv_obj_set_size(pin_mx, diaBoardIsRound() ? 190 : 300, diaBoardIsRound() ? 128 : 200);
    lv_obj_set_style_bg_opa(pin_mx, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(pin_mx, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(pin_mx, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(pin_mx, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(pin_mx, 4, LV_PART_MAIN);
    lv_obj_set_style_text_font(pin_mx, diaBoardIsRound() ? &font_sk_20 : &font_sk_24, LV_PART_ITEMS);
    lv_btnmatrix_set_btn_ctrl_all(pin_mx, LV_BTNMATRIX_CTRL_CLICK_TRIG | LV_BTNMATRIX_CTRL_NO_REPEAT);
    lv_obj_add_event_cb(pin_mx, pin_key_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lbl_pin_back = create_menu_btn(ui_pin_panel, S(Str::Back), close_pin_cb, 0, 0x555555,
                                   diaBoardIsRound() ? 140 : 200);
    if (diaBoardIsRound()) {
        lv_obj_align(lbl_pin_digits, LV_ALIGN_TOP_MID, 0, 8);
        lv_obj_align(pin_mx, LV_ALIGN_TOP_MID, 0, 36);
        lv_obj_align(lv_obj_get_parent(lbl_pin_back), LV_ALIGN_TOP_MID, 0, 170);
        lv_obj_add_flag(lbl_pin_title, LV_OBJ_FLAG_HIDDEN);
    }

    ota_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ota_panel, DIA_SCREEN_W, DIA_SCREEN_H);
    lv_obj_center(ota_panel);
    lv_obj_set_style_bg_color(ota_panel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ota_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ota_panel, 0, LV_PART_MAIN);
    lv_obj_add_flag(ota_panel, LV_OBJ_FLAG_HIDDEN);

    ota_label = lv_label_create(ota_panel);
    lv_obj_set_width(ota_label, DIA_SCREEN_W >= 400 ? 400 : 200);
    lv_label_set_long_mode(ota_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(ota_label, &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ota_label, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_text_align(ota_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ota_label, S(Str::OtaZero));
    lv_obj_align(ota_label, LV_ALIGN_CENTER, 0, -40);

    ota_bar = lv_bar_create(ota_panel);
    lv_obj_set_size(ota_bar, DIA_SCREEN_W >= 400 ? 360 : 180, 22);
    lv_obj_align(ota_bar, LV_ALIGN_CENTER, 0, 28);
    lv_bar_set_range(ota_bar, 0, 100);
    lv_bar_set_value(ota_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ota_bar, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ota_bar, lv_color_hex(0x00FF00), LV_PART_INDICATOR);

    diaUiFacePassThroughTouches();
    if (!diaI18nNeedsSetup()) diaUiShowSplash();
}

static volatile int s_lang_pick = -1;
static volatile bool s_lang_ok = false;
static lv_obj_t *s_langBtns[8];
static lv_obj_t *s_langOkBtn = nullptr;
static int s_langBtnN = 0;

static void langPaintSelection() {
    for (int i = 0; i < s_langBtnN; i++) {
        if (!s_langBtns[i]) continue;
        lv_obj_set_style_bg_color(s_langBtns[i],
                                  lv_color_hex(i == s_lang_pick ? 0x1FA3EC : 0x1A3040), LV_PART_MAIN);
    }
    if (!s_langOkBtn) return;
    if (s_lang_pick >= 0) lv_obj_clear_state(s_langOkBtn, LV_STATE_DISABLED);
    else lv_obj_add_state(s_langOkBtn, LV_STATE_DISABLED);
}

static void lang_pick_cb(lv_event_t *e) {
    s_lang_pick = (int)(intptr_t)lv_event_get_user_data(e);
    langPaintSelection();
}

static void lang_ok_cb(lv_event_t *e) {
    (void)e;
    if (s_lang_pick < 0) return;
    if (s_langOkBtn && lv_obj_has_state(s_langOkBtn, LV_STATE_DISABLED)) return;
    s_lang_ok = true;
}

/** BOOT: 0=nič, 1=krátky stisk, 2=dlhý (≥700 ms). Na setup bez touchu. */
static int diaUiPollBootGesture() {
    static bool wasDown = false;
    static uint32_t downAt = 0;
    const bool down = digitalRead(BOOT_BTN) == LOW;
    int result = 0;
    if (down && !wasDown) {
        downAt = millis();
    } else if (!down && wasDown) {
        const uint32_t held = millis() - downAt;
        if (held >= 40 && held < 700) result = 1;
        else if (held >= 700) result = 2;
    }
    wasDown = down;
    return result;
}

bool diaUiRunLanguageSetup(bool force) {
    if (!force && !diaI18nNeedsSetup()) return false;
    s_lang_pick = (!diaI18nNeedsSetup()) ? (int)diaI18nGet() : -1;
    s_lang_ok = false;
    s_langOkBtn = nullptr;
    s_langBtnN = 0;
    s_bootUiLock = true;
    diaUiHideSplash();
    pinMode(BOOT_BTN, INPUT_PULLUP);

    const bool round = diaBoardIsRound();
    const bool needBoot = !diaBoardTouchOk();
    if (needBoot) esp_rom_printf("[ui] language: no touch – use BOOT\n");
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, screenWidth, screenHeight);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(overlay, 8, LV_PART_MAIN);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_move_foreground(overlay);

    lv_obj_t *title = lv_label_create(overlay);
    lv_label_set_text(title, needBoot ? "Language (BOOT)" : "Language / Jazyk");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, round ? 10 : 8);

    const int n = diaI18nCount();
    const int btnH = round ? 22 : 32;
    const int gap = round ? 2 : 4;
    const int startY = round ? 30 : 40;
    for (int i = 0; i < n && i < 8; i++) {
        lv_obj_t *btn = lv_btn_create(overlay);
        s_langBtns[i] = btn;
        s_langBtnN = i + 1;
        lv_obj_set_size(btn, round ? 168 : (screenWidth - 80), btnH);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A3040), LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, startY + i * (btnH + gap));
        lv_obj_add_event_cb(btn, lang_pick_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, diaI18nNativeNameAt(i));
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_center(lbl);
    }

    s_langOkBtn = lv_btn_create(overlay);
    lv_obj_set_size(s_langOkBtn, round ? 120 : 200, round ? 28 : 40);
    lv_obj_align(s_langOkBtn, LV_ALIGN_TOP_MID, 0, round ? 186 : 268);
    lv_obj_set_style_bg_color(s_langOkBtn, lv_color_hex(0x005588), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_langOkBtn, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_radius(s_langOkBtn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_langOkBtn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_langOkBtn, lang_ok_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_t *okLbl = lv_label_create(s_langOkBtn);
    lv_label_set_text(okLbl, "OK");
    lv_obj_set_style_text_font(okLbl, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(okLbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(okLbl);
    lv_obj_clear_flag(okLbl, LV_OBJ_FLAG_CLICKABLE);
    if (needBoot) {
        lv_obj_t *hint = lv_label_create(overlay);
        lv_label_set_text(hint, "BOOT short=next  hold=OK");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x1FA3EC), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, round ? -4 : -2);
        if (s_lang_pick < 0) s_lang_pick = 0;
    }
    langPaintSelection();

    uint32_t lastTick = millis();
    while (!s_lang_ok) {
        uint32_t now = millis();
        lv_tick_inc(now - lastTick);
        lastTick = now;
        lv_timer_handler();
        if (needBoot) {
            const int g = diaUiPollBootGesture();
            if (g == 1 && n > 0) {
                s_lang_pick = (s_lang_pick + 1) % n;
                langPaintSelection();
            } else if (g == 2 && s_lang_pick >= 0) {
                s_lang_ok = true;
            }
        }
        delay(5);
    }

    diaI18nSet((DiaLang)s_lang_pick);
    diaI18nMarkSetupDone();
    s_langOkBtn = nullptr;
    s_langBtnN = 0;
    lv_obj_del(overlay);
    s_bootUiLock = false;
    diaUiApplyLanguage();
    return true;
}

static lv_obj_t *s_legalGo = nullptr;
static lv_obj_t *s_legalBox = nullptr;
static bool s_legalChecked = false;
static volatile bool s_legalAcceptedClick = false;
static volatile bool s_legalWantLang = false;

static void legal_lang_cb(lv_event_t *e) {
    (void)e;
    s_legalWantLang = true;
}

static void legalApplyCheckVisual() {
    if (s_legalBox) {
        lv_obj_set_style_bg_opa(s_legalBox, s_legalChecked ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_legalBox, lv_color_hex(s_legalChecked ? 0x1FA3EC : 0x000000), LV_PART_MAIN);
    }
    if (!s_legalGo) return;
    if (s_legalChecked) lv_obj_clear_state(s_legalGo, LV_STATE_DISABLED);
    else lv_obj_add_state(s_legalGo, LV_STATE_DISABLED);
}

static void legal_toggle_cb(lv_event_t *e) {
    (void)e;
    s_legalChecked = !s_legalChecked;
    legalApplyCheckVisual();
}

static void legal_go_cb(lv_event_t *e) {
    (void)e;
    if (!s_legalChecked || !s_legalGo || lv_obj_has_state(s_legalGo, LV_STATE_DISABLED)) return;
    s_legalAcceptedClick = true;
}

bool diaUiRunLegalConsent() {
    if (diaLegalAccepted()) return true;
    s_legalChecked = false;
    s_legalAcceptedClick = false;
    s_legalWantLang = false;
    s_legalGo = nullptr;
    s_legalBox = nullptr;
    s_bootUiLock = true;
    diaUiHideSplash();
    pinMode(BOOT_BTN, INPUT_PULLUP);

    const bool round = diaBoardIsRound();
    const bool needBoot = !diaBoardTouchOk();
    if (needBoot) esp_rom_printf("[ui] legal: no touch – use BOOT\n");
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, screenWidth, screenHeight);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_move_foreground(overlay);

    lv_obj_t *title = lv_label_create(overlay);
    lv_label_set_text(title, S(Str::LegalTitle));
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
    lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, round ? 8 : 16);

    // Kruh: text je v posuvnom poli, aby sa dlhší preklad neorezal.
    lv_obj_t *sc = lv_obj_create(overlay);
    lv_obj_set_size(sc, round ? 196 : (screenWidth - 48), round ? 100 : 130);
    lv_obj_align(sc, LV_ALIGN_TOP_MID, 0, round ? 32 : 52);
    lv_obj_set_style_bg_opa(sc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(sc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sc, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(sc, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(sc, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(sc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(sc, LV_OBJ_FLAG_SCROLL_ELASTIC);

    lv_obj_t *body = lv_label_create(sc);
    lv_obj_set_width(body, round ? 186 : (screenWidth - 64));
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(body, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(body, lv_color_hex(0xDDE6EE), LV_PART_MAIN);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(body, 3, LV_PART_MAIN);
    lv_label_set_text(body, S(Str::LegalBody));
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *indep = lv_label_create(sc);
    lv_obj_set_width(indep, round ? 186 : (screenWidth - 64));
    lv_label_set_long_mode(indep, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(indep, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(indep, lv_color_hex(0xDDE6EE), LV_PART_MAIN);
    lv_obj_set_style_text_align(indep, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(indep, 3, LV_PART_MAIN);
    lv_label_set_text(indep, S(Str::LegalIndep));
    lv_obj_align_to(indep, body, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);
    lv_obj_clear_flag(indep, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *row = lv_btn_create(overlay);
    lv_obj_set_size(row, round ? 180 : 400, round ? 34 : 48);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, round ? 136 : 196);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1A3040), LV_PART_MAIN);
    lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(row, legal_toggle_cb, LV_EVENT_SHORT_CLICKED, NULL);

    s_legalBox = lv_obj_create(row);
    lv_obj_set_size(s_legalBox, 18, 18);
    lv_obj_align(s_legalBox, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_opa(s_legalBox, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_legalBox, lv_color_hex(0x88AACC), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_legalBox, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s_legalBox, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_legalBox, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_legalBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_legalBox, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *checkLbl = lv_label_create(row);
    lv_label_set_text(checkLbl, S(Str::LegalUnderstand));
    lv_obj_set_style_text_font(checkLbl, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(checkLbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(checkLbl, LV_ALIGN_LEFT_MID, 38, 0);
    lv_obj_clear_flag(checkLbl, LV_OBJ_FLAG_CLICKABLE);

    s_legalGo = lv_btn_create(overlay);
    lv_obj_set_size(s_legalGo, round ? 140 : 200, round ? 28 : 48);
    lv_obj_align(s_legalGo, LV_ALIGN_TOP_MID, round ? 0 : 110, round ? 204 : 252);
    lv_obj_set_style_bg_color(s_legalGo, lv_color_hex(0x005588), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_legalGo, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_radius(s_legalGo, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_legalGo, 0, LV_PART_MAIN);
    lv_obj_add_state(s_legalGo, LV_STATE_DISABLED);
    lv_obj_add_event_cb(s_legalGo, legal_go_cb, LV_EVENT_SHORT_CLICKED, NULL);

    lv_obj_t *goLbl = lv_label_create(s_legalGo);
    lv_label_set_text(goLbl, S(Str::LegalContinue));
    lv_obj_set_style_text_font(goLbl, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(goLbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(goLbl);
    lv_obj_clear_flag(goLbl, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *langBtn = lv_btn_create(overlay);
    lv_obj_set_size(langBtn, round ? 150 : 140, round ? 26 : 48);
    lv_obj_align(langBtn, LV_ALIGN_TOP_MID, round ? 0 : -150, round ? 174 : 252);
    lv_obj_set_style_bg_color(langBtn, lv_color_hex(0x243040), LV_PART_MAIN);
    lv_obj_set_style_radius(langBtn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(langBtn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(langBtn, legal_lang_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_t *langLbl = lv_label_create(langBtn);
    lv_label_set_text(langLbl, "Jazyk");
    lv_obj_set_style_text_font(langLbl, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(langLbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(langLbl);
    lv_obj_clear_flag(langLbl, LV_OBJ_FLAG_CLICKABLE);
    if (needBoot) {
        lv_obj_t *hint = lv_label_create(overlay);
        lv_label_set_text(hint, "BOOT short=check  hold=OK");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x1FA3EC), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, round ? &font_sk_16 : &font_sk_20, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    uint32_t lastTick = millis();
    for (;;) {
        while (!s_legalAcceptedClick && !s_legalWantLang) {
            uint32_t now = millis();
            lv_tick_inc(now - lastTick);
            lastTick = now;
            lv_timer_handler();
            if (needBoot) {
                const int g = diaUiPollBootGesture();
                if (g == 1) {
                    s_legalChecked = !s_legalChecked;
                    legalApplyCheckVisual();
                } else if (g == 2 && s_legalChecked) {
                    s_legalAcceptedClick = true;
                }
            }
            delay(5);
        }
        if (s_legalWantLang) {
            s_legalGo = nullptr;
            s_legalBox = nullptr;
            lv_obj_del(overlay);
            s_bootUiLock = false;
            return false;
        }
        if (diaLegalAccept()) break;
        s_legalAcceptedClick = false;
    }

    s_legalGo = nullptr;
    s_legalBox = nullptr;
    lv_obj_del(overlay);
    s_bootUiLock = false;
    return true;
}

static void diaUiApplyLanguage() {
    if (lbl_t1_title) lv_label_set_text(lbl_t1_title, S(Str::SysTitle));
    if (lbl_t2_title) lv_label_set_text(lbl_t2_title, S(Str::LimitsTitle));
    if (lbl_t3_title) lv_label_set_text(lbl_t3_title, S(Str::BrightTitle));
    if (lbl_btn_info) lv_label_set_text(lbl_btn_info, S(Str::Info));
    if (lbl_btn_refresh) lv_label_set_text(lbl_btn_refresh, S(Str::Refresh));
    if (lbl_btn_mode) lv_label_set_text(lbl_btn_mode, S(display_mode >= 2 ? Str::ModeBtn2 : Str::ModeBtn1));
    if (lbl_btn_update) lv_label_set_text(lbl_btn_update, S(Str::Update));
    if (lbl_btn_forget) lv_label_set_text(lbl_btn_forget, S(Str::ForgetWifi));
    diaUiRefreshPinBtns();
    if (lbl_pin_back) lv_label_set_text(lbl_pin_back, S(Str::Back));
    if (lbl_btn_lang) lv_label_set_text(lbl_btn_lang, diaI18nNativeName(diaI18nGet()));
    if (lbl_back1) lv_label_set_text(lbl_back1, S(Str::Back));
    if (lbl_back2) lv_label_set_text(lbl_back2, S(Str::Back));
    if (lbl_back3) lv_label_set_text(lbl_back3, S(Str::Back));
    if (lbl_btn_autoupdate) lv_label_set_text(lbl_btn_autoupdate, S(auto_update ? Str::AutoOn : Str::AutoOff));
    diaUiRefreshChildBtn();
    if (lbl_btn_delta) lv_label_set_text(lbl_btn_delta, S(show_delta ? Str::DeltaOn : Str::DeltaOff));
    if (lbl_btn_source) lv_label_set_text(lbl_btn_source, S(show_source ? Str::SourceOn : Str::SourceOff));
    diaUiRefreshPumpBtn();
    if (lbl_btn_glow) lv_label_set_text(lbl_btn_glow, S(show_glow ? Str::GlowOn : Str::GlowOff));
    diaUiRefreshOrientBtn();
    if (lbl_btn_units) lv_label_set_text(lbl_btn_units, S(use_mgdl ? Str::UnitsMg : Str::UnitsMmol));
    if (lbl_look_title) lv_label_set_text(lbl_look_title, S(Str::LookTitle));
    if (lbl_back_look) lv_label_set_text(lbl_back_look, S(Str::Back));
    if (lbl_btn_sound_open) lv_label_set_text(lbl_btn_sound_open, S(Str::SoundBtn));
    if (lbl_sound_title) lv_label_set_text(lbl_sound_title, S(Str::SoundTitle));
    if (lbl_back_sound) lv_label_set_text(lbl_back_sound, S(Str::Back));
    if (lbl_btn_sound) lv_label_set_text(lbl_btn_sound, S(sound_on ? Str::SoundOn : Str::SoundOff));
    if (lbl_btn_vol_down) lv_label_set_text(lbl_btn_vol_down, S(Str::VolDown));
    if (lbl_btn_vol_up) lv_label_set_text(lbl_btn_vol_up, S(Str::VolUp));
    refreshVolLabel();
    if (lbl_btn_snooze_now) lv_label_set_text(lbl_btn_snooze_now, S(Str::SnoozeNow));
    update_target_btn_labels();

    char buf[48];
    if (lbl_btn_snooze) {
        snprintf(buf, sizeof(buf), S(Str::SnoozeFmt), snooze_min);
        lv_label_set_text(lbl_btn_snooze, buf);
    }
    if (lbl_btn_day) {
        snprintf(buf, sizeof(buf), S(Str::DayBrFmt), day_brightness);
        lv_label_set_text(lbl_btn_day, buf);
    }
    if (lbl_btn_night) {
        snprintf(buf, sizeof(buf), S(Str::NightBrFmt), night_brightness);
        lv_label_set_text(lbl_btn_night, buf);
    }
    if (lbl_btn_day_h) {
        snprintf(buf, sizeof(buf), S(Str::DayFromFmt), day_start_hour);
        lv_label_set_text(lbl_btn_day_h, buf);
    }
    if (lbl_btn_night_h) {
        snprintf(buf, sizeof(buf), S(Str::NightFromFmt), night_start_hour);
        lv_label_set_text(lbl_btn_night_h, buf);
    }
    if (lbl_save) lv_label_set_text(lbl_save, S(Str::Save));
    if (lbl_info_title) lv_label_set_text(lbl_info_title, S(Str::Info));
    if (lbl_info_keys) lv_label_set_text(lbl_info_keys, S(Str::InfoKeys));
    if (lbl_info_close) lv_label_set_text(lbl_info_close, S(Str::Back));
    diaUiSetLegalInfoText();
    if (lbl_confirm_title) {
        lv_label_set_text(lbl_confirm_title, S(s_confirmKind ? Str::PinClearAsk : Str::ConfirmErase));
    }
    if (lbl_yes) lv_label_set_text(lbl_yes, S(s_confirmKind ? Str::PinClear : Str::YesErase));
    if (lbl_no) lv_label_set_text(lbl_no, S(Str::Back));

    if (lbl_edit_title && current_edit_mode != 0) {
        const char *title = nullptr;
        if (current_edit_mode == 1) title = S(Str::EditDay);
        else if (current_edit_mode == 2) title = S(Str::EditNight);
        else if (current_edit_mode == 3) title = S(use_mgdl ? Str::EditLowMg : Str::EditLow);
        else if (current_edit_mode == 4) title = S(use_mgdl ? Str::EditHighMg : Str::EditHigh);
        else if (current_edit_mode == 5) title = S(use_mgdl ? Str::EditCritMg : Str::EditCrit);
        else if (current_edit_mode == 6) title = S(Str::EditDayStart);
        else if (current_edit_mode == 7) title = S(Str::EditNightStart);
        if (title) lv_label_set_text(lbl_edit_title, title);
    }
}
