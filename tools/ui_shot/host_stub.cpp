#include "host_api.h"
#include "png_write.h"

#include "dia_audio.h"
#include "dia_board.h"
#include "dia_config.h"
#include "dia_fetch.h"
#include "dia_ota.h"
#include "dia_pin.h"
#include "dia_wifi.h"

#include <WiFiManager.h>
#include <WiFiMulti.h>

#include <WiFi.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include <map>
#include <string.h>

static unsigned long g_ms = 200000;
static int g_clock_h = -1;
static int g_clock_m = -1;

SerialClass Serial;
WiFiClass WiFi;
Preferences preferences;
EspClass ESP;
WiFiManager wm;
WiFiMulti wifiMulti;

static std::map<std::string, int32_t> g_int;
static std::map<std::string, uint8_t> g_uchar;
static std::map<std::string, float> g_float;
static std::map<std::string, bool> g_bool;
static std::map<std::string, std::string> g_str;

static const int FB_W = DIA_SCREEN_W;
static const int FB_H = DIA_SCREEN_H;
static uint16_t g_fb[DIA_SCREEN_W * DIA_SCREEN_H];
static bool g_presented = false;

static bool g_lang_capture = false;
static bool g_lang_saved = false;
static int g_lang_frames = 0;
static char g_lang_path[512];

unsigned long millis() { return g_ms; }
void EspClass::restart() { fprintf(stderr, "ESP.restart ignored\n"); }
void vTaskDelay(uint32_t) {}

float temperatureRead() { return 43.2f; }

bool getLocalTime(struct tm *info, uint32_t) {
    if (!info) return false;
    time_t now = time(nullptr);
    struct tm *p = localtime(&now);
    if (!p) return false;
    *info = *p;
    if (g_clock_h >= 0) {
        info->tm_hour = g_clock_h;
        info->tm_min = g_clock_m;
        info->tm_sec = 0;
    }
    return true;
}

void hostSetClock(int hour, int minute) {
    g_clock_h = hour;
    g_clock_m = minute;
}

static bool hiddenChain(lv_obj_t *o) {
    for (lv_obj_t *p = o; p; p = lv_obj_get_parent(p)) {
        if (lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN)) return true;
    }
    return false;
}

static lv_obj_t *findLabel(lv_obj_t *o, const char *text, bool visibleOnly) {
    if (!o) return nullptr;
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *t = lv_label_get_text(o);
        if (t && strcmp(t, text) == 0 && (!visibleOnly || !hiddenChain(o))) return o;
    }
    uint32_t n = lv_obj_get_child_cnt(o);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *hit = findLabel(lv_obj_get_child(o, i), text, visibleOnly);
        if (hit) return hit;
    }
    return nullptr;
}

bool hostClickText(const char *text) {
    lv_obj_t *lbl = findLabel(lv_scr_act(), text, true);
    if (!lbl) {
        fprintf(stderr, "click miss: %s\n", text ? text : "");
        return false;
    }
    lv_obj_t *btn = lv_obj_get_parent(lbl);
    lv_event_send(btn, LV_EVENT_SHORT_CLICKED, nullptr);
    return true;
}

void *heap_caps_malloc(size_t size, uint32_t) { return malloc(size ? size : 1); }
void heap_caps_free(void *ptr) { free(ptr); }
size_t heap_caps_get_free_size(uint32_t) { return 8u * 1024u * 1024u; }

int WiFiClass::status() { return WL_CONNECTED; }
IPAddress WiFiClass::localIP() { return IPAddress("192.168.1.42"); }
int WiFiClass::RSSI() { return -58; }

bool Preferences::begin(const char *, bool) { return true; }
void Preferences::end() {}
bool Preferences::clear() {
    g_int.clear();
    g_uchar.clear();
    g_float.clear();
    g_bool.clear();
    g_str.clear();
    return true;
}
bool Preferences::isKey(const char *key) {
    if (!key) return false;
    return g_int.count(key) || g_uchar.count(key) || g_float.count(key) || g_bool.count(key) || g_str.count(key);
}
size_t Preferences::putInt(const char *key, int32_t value) {
    g_int[key] = value;
    return 4;
}
size_t Preferences::putUChar(const char *key, uint8_t value) {
    g_uchar[key] = value;
    return 1;
}
size_t Preferences::putLong(const char *key, int32_t value) { return putInt(key, value); }
size_t Preferences::putFloat(const char *key, float value) {
    g_float[key] = value;
    return 4;
}
size_t Preferences::putBool(const char *key, bool value) {
    g_bool[key] = value;
    return 1;
}
size_t Preferences::putString(const char *key, const char *value) {
    g_str[key] = value ? value : "";
    return g_str[key].size();
}
size_t Preferences::putString(const char *key, const String &value) { return putString(key, value.c_str()); }
int32_t Preferences::getInt(const char *key, int32_t defaultValue) {
    auto it = g_int.find(key);
    return it == g_int.end() ? defaultValue : it->second;
}
uint8_t Preferences::getUChar(const char *key, uint8_t defaultValue) {
    auto it = g_uchar.find(key);
    return it == g_uchar.end() ? defaultValue : it->second;
}
int32_t Preferences::getLong(const char *key, int32_t defaultValue) { return getInt(key, defaultValue); }
float Preferences::getFloat(const char *key, float defaultValue) {
    auto it = g_float.find(key);
    return it == g_float.end() ? defaultValue : it->second;
}
bool Preferences::getBool(const char *key, bool defaultValue) {
    auto it = g_bool.find(key);
    return it == g_bool.end() ? defaultValue : it->second;
}
String Preferences::getString(const char *key, String defaultValue) {
    auto it = g_str.find(key);
    if (it == g_str.end()) return defaultValue;
    return String(it->second.c_str());
}

void diaBoardInit() { memset(g_fb, 0, sizeof(g_fb)); }
void diaBoardSetBrightness(uint8_t) {}
void diaBoardFlush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint16_t *pixels) {
    if (!pixels || x2 < x1 || y2 < y1) return;
    const int stride = (int)(x2 - x1 + 1);
    for (int y = (int)y1; y <= (int)y2; y++) {
        const uint16_t *src = pixels + (y - (int)y1) * stride;
        if (y < 0 || y >= FB_H) continue;
        int xa = x1 < 0 ? 0 : (int)x1;
        int xb = x2 >= FB_W ? FB_W - 1 : (int)x2;
        if (xa > xb) continue;
        memcpy(&g_fb[y * FB_W + xa], src + (xa - (int)x1), (size_t)(xb - xa + 1) * sizeof(uint16_t));
    }
}
void diaBoardFlushPresent() { g_presented = true; }
void diaBoardPresentLand(const uint16_t *land) {
    if (!land) return;
    memcpy(g_fb, land, sizeof(g_fb));
    g_presented = true;
}
bool diaBoardTouchRead(int16_t *, int16_t *) { return false; }
#if defined(DIA_BOARD_WS_3_5)
bool diaBoardIsRound() { return false; }
const char *diaBoardName() { return "ws35"; }
bool diaBoardPreferFullRefresh() { return true; }
bool diaBoardIsLandscape() { return true; }
bool diaBoardHasAudio() { return true; }
#else
bool diaBoardIsRound() { return true; }
const char *diaBoardName() { return "ws128"; }
bool diaBoardPreferFullRefresh() { return false; }
bool diaBoardIsLandscape() { return false; }
bool diaBoardHasAudio() { return false; }
#endif
void diaBoardSpeakerAmp(bool) {}
bool diaBoardSupportsOrient() { return false; }
uint8_t diaBoardGetOrientMode() { return 0; }
void diaBoardSetOrientMode(uint8_t) {}
bool diaBoardIsFlipped() { return false; }

void hostSavePng(const char *path) {
    if (!pngWriteRgb565(path, g_fb, FB_W, FB_H, diaBoardIsRound())) {
        fprintf(stderr, "png failed: %s\n", path ? path : "");
    }
}

void hostArmLanguageCapture(const char *pngPath) {
    strncpy(g_lang_path, pngPath ? pngPath : "", sizeof(g_lang_path) - 1);
    g_lang_path[sizeof(g_lang_path) - 1] = 0;
    g_lang_capture = true;
    g_lang_saved = false;
    g_lang_frames = 0;
    g_presented = false;
}

void hostEndLanguageCapture() { g_lang_capture = false; }

void delay(unsigned long ms) {
    g_ms += ms;
    if (!g_lang_capture) return;
    lv_tick_inc((uint32_t)ms);
    g_lang_frames++;
    if (g_presented && !g_lang_saved) {
        hostSavePng(g_lang_path);
        printf("wrote %s\n", g_lang_path);
        fflush(stdout);
        g_lang_saved = true;
        lv_obj_t *lbl = findLabel(lv_scr_act(), "Slovenčina", true);
        if (!lbl) {
            fprintf(stderr, "language label missing\n");
            exit(1);
        }
        lv_event_send(lv_obj_get_parent(lbl), LV_EVENT_SHORT_CLICKED, nullptr);
        lv_obj_t *ok = findLabel(lv_scr_act(), "OK", true);
        if (ok) lv_event_send(lv_obj_get_parent(ok), LV_EVENT_SHORT_CLICKED, nullptr);
    }
    if (!g_lang_saved && g_lang_frames > 100) {
        fprintf(stderr, "language screen did not draw\n");
        exit(1);
    }
}

DiaProfile profiles[DIA_PROFILE_COUNT];
uint8_t display_mode = 1;
int dia_active_slot = 0;
String dsSource;
String nsUrl, nsToken, dsUser, dsPwd, dsRegion;
String libreUser, librePwd, libreRegion;
String dexUser, dexPwd, dexRegion;
String libre_token, libre_account_hash, libre_patient_id, libre_patient_pref, libre_patient_list;
String carelink_patient_pref, carelink_patient_list;
String dexcom_session_id, dexcom_account_id;
String backup_ssid1, backup_pwd1, backup_ssid2, backup_pwd2;
String backup_ssid3, backup_pwd3, backup_ssid4, backup_pwd4;
float targetLow = 3.9f;
float targetHigh = 10.0f;
float targetCrit = 15.0f;
int day_brightness = 255;
int night_brightness = 25;
int day_start_hour = 7;
int night_start_hour = 22;
bool use_mgdl = false;
bool show_delta = true;
bool show_source = false;
bool show_glow = true;
bool tls_insecure = false;
bool auto_update = true;
int watchface_style = 0;
uint8_t single_slot = 0;
bool sound_on = false;
int sound_volume = 2;
int snooze_min = 30;
bool forceFetch = false;
bool shouldSaveConfig = false;
bool wifi_was_connected = true;
bool pending_pref_save = false;
int pending_pref_mode = 0;
int pending_pref_val = 0;

void hostPrepareDefaults() {
    for (int i = 0; i < DIA_PROFILE_COUNT; i++) {
        profiles[i].dsSource = (i == 0) ? "nightscout" : "off";
        profiles[i].name = "";
        profiles[i].targetLow = 3.9f;
        profiles[i].targetHigh = 10.0f;
        profiles[i].targetCrit = 15.0f;
    }
    dsSource = "nightscout";
    targetLow = 3.9f;
    targetHigh = 10.0f;
    targetCrit = 15.0f;
    show_delta = true;
    show_source = false;
    show_glow = true;
    use_mgdl = false;
    watchface_style = 0;
    display_mode = 1;
    auto_update = true;
}

void diaConfigPushSlot(int slot) {
    if (slot < 0 || slot >= DIA_PROFILE_COUNT) slot = 0;
    if (diaFetchDeferUi() && slot != diaFetchCurrentSlot()) return;
    dia_active_slot = slot;
    const DiaProfile &p = profiles[slot];
    dsSource = p.dsSource;
    targetLow = p.targetLow;
    targetHigh = p.targetHigh;
    targetCrit = p.targetCrit;
}
void diaConfigPullSlot(int) {}
void diaConfigPullSessionsSlot(int) {}
void diaConfigSaveProfiles() {}
void diaConfigBegin() {}
void diaConfigLoad() {}
void diaConfigClearSessions() {}
void diaConfigClearSessionsSlot(int) {}

bool diaProfileConfigured(int slot) {
    if (slot < 0 || slot >= DIA_PROFILE_COUNT) return false;
    const String &src = profiles[slot].dsSource;
    return src.length() > 0 && src != "off";
}
int diaShownSlot() {
    const bool a = diaProfileConfigured(0);
    const bool b = diaProfileConfigured(1);
    if (a && b) return single_slot == 1 ? 1 : 0;
    if (b) return 1;
    return 0;
}
bool diaProfileActive(int slot) {
    if (!diaProfileConfigured(slot)) return false;
    if (display_mode >= 2) return true;
    return slot == diaShownSlot();
}
int diaProfileActiveCount() {
    int n = 0;
    for (int i = 0; i < DIA_PROFILE_COUNT; i++)
        if (diaProfileActive(i)) n++;
    return n;
}

bool diaFetchBusy() { return false; }
bool diaFetchSlotWaiting(int) { return false; }
bool diaFetchShowConnecting() { return false; }
void diaFetchOnConfigChanged() {}
bool diaFetchDeferUi() { return false; }
int diaFetchCurrentSlot() { return 0; }
void diaFetchEnqueueSuccess(float, const char *, long, bool, float) {}
void diaFetchEnqueueError(FetchError) {}
bool diaFetchRequest() { return false; }
void diaFetchInit() {}
bool diaFetchQuiesce(uint32_t) { return true; }
void diaFetchResume() {}
void diaFetchWatchdog(uint32_t) {}
void diaFetchPoll() {}

bool diaPinIsSet() { return false; }
bool diaPinSet(const char *) { return true; }
void diaPinClear() {}
bool diaPinCheck(const char *) { return false; }
bool diaPinLockedOut() { return false; }
const char *diaPinToken() { return ""; }
bool diaPinCookieOk(const String &) { return false; }

void diaAudioInit() {}
void diaAudioQuiesceForWifi() {}
void diaAudioTick(uint32_t) {}
void diaAudioSnooze() {}
bool diaAudioIsSnoozed(uint32_t) { return false; }
int diaAudioSnoozeRemainMin(uint32_t) { return 0; }
void diaAudioClearSnooze() {}
void diaAudioPlayChime() {}
void diaAudioPlayAck() {}
bool diaAudioAnyCritical() { return false; }
void diaAudioResetChimeTimer() {}

void diaOtaNoteBoot() {}
void check_firmware_update(bool) {}
int diaOtaServerVersion() { return 0; }
int diaOtaKnownVersion() { return 0; }
void diaDiagResult(int, int) {}
void update_progress_cb(int, int) {}

void saveConfigCallback() {}
void saveNetworkToBackup(String, String) {}
void autoSaveNetwork() {}
void refreshPortalSavedNetsHtml() {}
void diaWifiSetupManager() {}
void diaWifiAddParameters() {}
bool diaWifiTryConnect() { return true; }
bool diaWifiEnsureConnected() { return true; }
void diaWifiProcess() {}
void diaWifiSavePortalConfigIfNeeded() {}
void diaWifiMaintain(uint32_t) {}
