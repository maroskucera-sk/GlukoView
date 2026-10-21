// Screenshot host. Compiles the real LVGL UI (src/dia_ui.cpp).
// Round board (DIA_BOARD_WS_1_28) writes docs/img/ws128/*.png.
// Wide board (DIA_BOARD_WS_3_5) writes docs/img/ws35/*.png and adds split + sound.
//
// Scenes:
//   lang, splash, boot-connect, boot-setup,
//   face-ok, face-low, face-high, face-critical, face-stale, face-clock, face-chart, face-source,
//   menu-system, menu-system-2, menu-limits, menu-look, menu-bright,
//   edit-low, info, confirm-erase, pin, ota
// Wide only: face-split, face-split-chart, menu-sound

#include "host_api.h"

#include "dia_config.h"
#include "dia_glucose.h"
#include "dia_i18n.h"
#include "dia_ui.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <string>

extern lv_obj_t *ui_settings_panel;
extern lv_obj_t *ui_edit_panel;
extern lv_obj_t *ui_info_panel;
extern lv_obj_t *ui_confirm_panel;
extern lv_obj_t *ota_panel;
extern lv_obj_t *ota_label;
extern lv_obj_t *ota_bar;

#if defined(DIA_BOARD_WS_3_5)
static std::string g_out = "docs/img/ws35";
#else
static std::string g_out = "docs/img/ws128";
#endif

static void mkdirP(const std::string &path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); i++) {
        cur.push_back(path[i]);
        if (path[i] == '/' || path[i] == '\\') _mkdir(cur.c_str());
    }
    _mkdir(path.c_str());
}

static void shot(const char *name) {
    diaUiTick(30);
    std::string path = g_out + "/" + name + ".png";
    hostSavePng(path.c_str());
    printf("wrote %s\n", path.c_str());
    fflush(stdout);
}

static void hideOverlays() {
    if (ui_settings_panel) lv_obj_add_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    if (ui_edit_panel) lv_obj_add_flag(ui_edit_panel, LV_OBJ_FLAG_HIDDEN);
    if (ui_info_panel) lv_obj_add_flag(ui_info_panel, LV_OBJ_FLAG_HIDDEN);
    if (ui_confirm_panel) lv_obj_add_flag(ui_confirm_panel, LV_OBJ_FLAG_HIDDEN);
    if (ota_panel) lv_obj_add_flag(ota_panel, LV_OBJ_FLAG_HIDDEN);
}

static void showFace(int style, float mmol, const char *trend, float delta, int ageMin, bool source) {
    hideOverlays();
    restoreMainScreenChrome();
    watchface_style = style;
    show_source = source;
    apply_watchface();

    float hist[12] = {5.1f, 5.4f, 5.8f, 6.4f, 7.1f, 6.6f, 6.0f, 5.5f, 5.2f, 5.8f, 6.3f, mmol};
    glucoseHistorySetNewestFirst(hist, 12);

    long ts = (long)time(nullptr) - ageMin * 60 - 20;
    applyGlucoseReading(mmol, String(trend), ts, true, delta);
    if (style == 1) updateClockDisplay();
    diaUiUpdateArcColors(300);
    diaUiTick(40);
}

static void openMenu(int col, int scrollY) {
    hideOverlays();
    lv_obj_clear_flag(ui_settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_settings_panel);
    lv_obj_set_tile_id(ui_settings_panel, (uint32_t)col, 0, LV_ANIM_OFF);
    diaUiTick(40);
    lv_obj_t *tile = lv_tileview_get_tile_act(ui_settings_panel);
    if (tile) lv_obj_scroll_to_y(tile, scrollY, LV_ANIM_OFF);
    diaUiTick(40);
}

int main(int argc, char **argv) {
    if (argc > 1 && argv[1][0]) g_out = argv[1];
    mkdirP(g_out);
    hostPrepareDefaults();
#if defined(DIA_BOARD_WS_3_5)
    profiles[0].name = "Anna";
    profiles[1].name = "Adam";
    profiles[1].dsSource = "libre";
#endif
    hostSetClock(9, 41);

    std::string langPath = g_out + "/lang.png";
    hostArmLanguageCapture(langPath.c_str());
    diaUiInitHardware();
    diaUiInitLvgl();
    build_ui();
    if (!diaUiRunLanguageSetup()) {
        fprintf(stderr, "language setup did not run\n");
        return 1;
    }
    hostEndLanguageCapture();
    lv_deinit();

    diaUiInitHardware();
    diaUiInitLvgl();
    build_ui();

    diaUiTick(150);
    shot("splash");
    diaUiTick(2500);
    diaUiTick(40);

    showBootStatus("WiFi", S(Str::BootConnecting));
    shot("boot-connect");
    showBootStatus("WiFi", S(Str::BootSetupHint));
    shot("boot-setup");

    showFace(0, 6.4f, "\xE2\x86\x97", 0.3f, 3, false);
    shot("face-ok");
    showFace(0, 3.1f, "\xE2\x86\x98", -0.6f, 2, false);
    shot("face-low");
    showFace(0, 11.8f, "\xE2\x86\x97", 0.8f, 4, false);
    shot("face-high");
    showFace(0, 16.4f, "\xE2\x86\x91", 1.5f, 1, false);
    shot("face-critical");
    showFace(0, 6.2f, "\xE2\x86\x92", 0.0f, 42, false);
    shot("face-stale");
    showFace(1, 6.4f, "\xE2\x86\x97", 0.3f, 3, false);
    shot("face-clock");
    showFace(2, 6.4f, "\xE2\x86\x97", 0.3f, 3, false);
    shot("face-chart");
    showFace(0, 6.4f, "\xE2\x86\x97", 0.3f, 3, true);
    shot("face-source");

    show_source = false;
    watchface_style = 0;

    openMenu(0, 0);
    shot("menu-system");
    openMenu(0, 210);
    shot("menu-system-2");
    openMenu(1, 0);
    shot("menu-limits");
    openMenu(2, 0);
    shot("menu-look");
    openMenu(3, 0);
    shot("menu-bright");

    openMenu(1, 0);
    char lowBuf[64];
    formatTargetLabel(S(Str::Low), targetLow, lowBuf, sizeof(lowBuf));
    if (hostClickText(lowBuf)) diaUiTick(80);
    shot("edit-low");

    openMenu(0, 0);
    if (hostClickText(S(Str::Info))) diaUiTick(80);
    shot("info");

    openMenu(0, 180);
    if (hostClickText(S(Str::ForgetWifi))) diaUiTick(80);
    shot("confirm-erase");

    openMenu(0, 180);
    if (hostClickText(S(Str::PinOff))) diaUiTick(80);
    shot("pin");
    hostClickText(S(Str::Back));
    diaUiTick(30);

    hideOverlays();
    lv_obj_clear_flag(ota_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ota_panel);
    char otaBuf[64];
    snprintf(otaBuf, sizeof(otaBuf), S(Str::OtaPct), 42);
    lv_label_set_text(ota_label, otaBuf);
    lv_bar_set_value(ota_bar, 42, LV_ANIM_OFF);
    shot("ota");

#if defined(DIA_BOARD_WS_3_5)
    {
        float histB[12] = {8.2f, 8.8f, 9.4f, 10.1f, 11.0f, 11.6f,
                           12.2f, 11.4f, 10.8f, 11.2f, 11.9f, 12.4f};
        dia_active_slot = 1;
        glucoseHistorySetNewestFirst(histB, 12);
        long tsB = (long)time(nullptr) - 4 * 60 - 20;
        glucoseStoreReadingSlot(1, 12.4f, String("\xE2\x86\x91"), tsB, true, 0.5f);
        dia_active_slot = 0;

        show_source = true;
        openMenu(0, 0);
        if (!hostClickText(S(Str::ModeBtn1))) {
            fprintf(stderr, "mode button missing\n");
        }
        diaUiTick(80);
        hideOverlays();
        diaUiUpdateArcColors(300);
        diaUiTick(40);
        shot("face-split");

        watchface_style = 2;
        diaUiApplyDisplayMode();
        diaUiTick(40);
        shot("face-split-chart");

        openMenu(1, 0);
        if (!hostClickText(S(Str::SoundBtn))) {
            fprintf(stderr, "sound button missing\n");
        }
        diaUiTick(40);
        shot("menu-sound");
    }
#endif

    printf("done\n");
    return 0;
}
