#include "dia_config.h"
#include "dia_board.h"
#include "dia_carelink.h"
#include "dia_fetch.h"
#include "dia_i18n.h"
#include "dia_secret.h"

#include <esp_system.h>

static const char *diaResetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_UNKNOWN: return "UNKNOWN";
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXT";
        case ESP_RST_SW: return "SW";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "SLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "OTHER";
    }
}

DiaProfile profiles[DIA_PROFILE_COUNT];
uint8_t display_mode = 1;
int dia_active_slot = 0;

String dsSource = "nightscout";
String nsUrl = "";
String nsToken = "";
String dsUser = "";
String dsPwd = "";
String dsRegion = "eu";
String libreUser = "";
String librePwd = "";
String libreRegion = "eu";
String dexUser = "";
String dexPwd = "";
String dexRegion = "ous";

String libre_token = "";
String libre_account_hash = "";
String libre_patient_id = "";
String libre_patient_pref = "";
String libre_patient_list = "";
String carelink_patient_pref = "";
String carelink_patient_list = "";

String dexcom_session_id = "";
String dexcom_account_id = "";

String backup_ssid1 = "";
String backup_pwd1 = "";
String backup_ssid2 = "";
String backup_pwd2 = "";
String backup_ssid3 = "";
String backup_pwd3 = "";
String backup_ssid4 = "";
String backup_pwd4 = "";

float targetLow = 3.9;
float targetHigh = 10.0;
float targetCrit = 15.0;
int day_brightness = 255;
int night_brightness = 25;
int day_start_hour = 7;
int night_start_hour = 22;
bool use_mgdl = false;
bool show_delta = true;
bool show_source = false;
bool show_pump = true;
bool show_glow = true;
uint8_t orient_mode = 0; // 0=USB vpravo, 1=USB vľavo
bool tls_insecure = false;
bool auto_update = true;
int watchface_style = 0;
uint8_t single_slot = 0;
bool sound_on = false;
int sound_volume = 2;
int snooze_min = 30;

Preferences preferences;

bool forceFetch = true;
bool shouldSaveConfig = false;
bool wifi_was_connected = false;

bool pending_pref_save = false;
int pending_pref_mode = 0;
int pending_pref_val = 0;

static void profileDefaults(DiaProfile &p) {
    p.name = "";
    p.dsSource = "off";
    p.nsUrl = "";
    p.nsToken = "";
    p.dsUser = "";
    p.dsPwd = "";
    p.dsRegion = "eu";
    p.libreUser = "";
    p.librePwd = "";
    p.libreRegion = "eu";
    p.dexUser = "";
    p.dexPwd = "";
    p.dexRegion = "ous";
    p.libre_token = "";
    p.libre_account_hash = "";
    p.libre_patient_id = "";
    p.libre_patient_pref = "";
    p.libre_patient_list = "";
    p.carelink_patient_pref = "";
    p.carelink_patient_list = "";
    p.dexcom_session_id = "";
    p.dexcom_account_id = "";
    p.targetLow = 3.9f;
    p.targetHigh = 10.0f;
    p.targetCrit = 15.0f;
}

void diaConfigPushSlot(int slot) {
    if (slot < 0 || slot >= DIA_PROFILE_COUNT) slot = 0;
    // Dual fetch: neprepísať credentials/URL slotu, ktorý práve beží na fetch tasku
    if (diaFetchDeferUi() && slot != diaFetchCurrentSlot()) {
        return;
    }
    dia_active_slot = slot;
    const DiaProfile &p = profiles[slot];
    dsSource = p.dsSource;
    nsUrl = p.nsUrl;
    nsToken = p.nsToken;
    dsUser = p.dsUser;
    dsPwd = p.dsPwd;
    dsRegion = p.dsRegion;
    libreUser = p.libreUser;
    librePwd = p.librePwd;
    libreRegion = p.libreRegion;
    dexUser = p.dexUser;
    dexPwd = p.dexPwd;
    dexRegion = p.dexRegion;
    libre_token = p.libre_token;
    libre_account_hash = p.libre_account_hash;
    libre_patient_id = p.libre_patient_id;
    libre_patient_pref = p.libre_patient_pref;
    libre_patient_list = p.libre_patient_list;
    carelink_patient_pref = p.carelink_patient_pref;
    carelink_patient_list = p.carelink_patient_list;
    dexcom_session_id = p.dexcom_session_id;
    dexcom_account_id = p.dexcom_account_id;
    targetLow = p.targetLow;
    targetHigh = p.targetHigh;
    targetCrit = p.targetCrit;
}

void diaConfigPullSessionsSlot(int slot) {
    if (slot < 0 || slot >= DIA_PROFILE_COUNT) return;
    DiaProfile &p = profiles[slot];
    // Len to, čo fetch mení – NIE dsSource/URL (tie by sa pri race pokazili na cudzí slot)
    p.libre_token = libre_token;
    p.libre_account_hash = libre_account_hash;
    p.libre_patient_id = libre_patient_id;
    p.libre_patient_pref = libre_patient_pref;
    p.libre_patient_list = libre_patient_list;
    // Pref pacienta je voľba v portáli. Globál sa pri slote B prepne a zápis
    // späť by v RAM vymenil dieťa A, kým reštart nenačíta NVS.
    p.carelink_patient_list = carelink_patient_list;
    p.libreRegion = libreRegion;
    p.dexcom_session_id = dexcom_session_id;
    p.dexcom_account_id = dexcom_account_id;
}

void diaConfigPullSlot(int slot) {
    if (slot < 0 || slot >= DIA_PROFILE_COUNT) return;
    DiaProfile &p = profiles[slot];
    p.dsSource = dsSource;
    p.nsUrl = nsUrl;
    p.nsToken = nsToken;
    p.dsUser = dsUser;
    p.dsPwd = dsPwd;
    p.dsRegion = dsRegion;
    p.libreUser = libreUser;
    p.librePwd = librePwd;
    p.libreRegion = libreRegion;
    p.dexUser = dexUser;
    p.dexPwd = dexPwd;
    p.dexRegion = dexRegion;
    p.libre_token = libre_token;
    p.libre_account_hash = libre_account_hash;
    p.libre_patient_id = libre_patient_id;
    p.libre_patient_pref = libre_patient_pref;
    p.libre_patient_list = libre_patient_list;
    p.carelink_patient_pref = carelink_patient_pref;
    p.carelink_patient_list = carelink_patient_list;
    p.dexcom_session_id = dexcom_session_id;
    p.dexcom_account_id = dexcom_account_id;
    p.targetLow = targetLow;
    p.targetHigh = targetHigh;
    p.targetCrit = targetCrit;
}

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
    for (int i = 0; i < DIA_PROFILE_COUNT; i++) {
        if (diaProfileActive(i)) n++;
    }
    return n;
}

bool diaAnyNightscout() {
    for (int i = 0; i < DIA_PROFILE_COUNT; i++) {
        if (!diaProfileActive(i)) continue;
        const String &s = profiles[i].dsSource;
        if (s == "nightscout" || s == "ns") return true;
    }
    return false;
}

static String k(int slot, const char *suf) {
    return String("p") + String(slot) + "_" + suf;
}

static void loadProfileFromNvs(int slot) {
    DiaProfile &p = profiles[slot];
    profileDefaults(p);
    String pref = k(slot, "");
    // Ak existuje p0_src, načítaj prefixované; inak legacy len pre slot 0
    if (preferences.isKey(k(slot, "src").c_str()) || preferences.isKey(k(slot, "name").c_str())) {
        p.name = preferences.getString(k(slot, "name").c_str(), "");
        p.dsSource = preferences.getString(k(slot, "src").c_str(), "off");
        p.nsUrl = preferences.getString(k(slot, "nsu").c_str(), "");
        p.nsToken = diaNvsGetSecret(k(slot, "nst").c_str());
        p.dsUser = preferences.getString(k(slot, "dsu").c_str(), "");
        p.dsPwd = diaNvsGetSecret(k(slot, "dsp").c_str());
        p.dsRegion = preferences.getString(k(slot, "dsr").c_str(), "eu");
        p.libreUser = preferences.getString(k(slot, "llu").c_str(), "");
        p.librePwd = diaNvsGetSecret(k(slot, "llp").c_str());
        p.libreRegion = preferences.getString(k(slot, "llr").c_str(), "eu");
        p.dexUser = preferences.getString(k(slot, "dxu").c_str(), "");
        p.dexPwd = diaNvsGetSecret(k(slot, "dxp").c_str());
        p.dexRegion = preferences.getString(k(slot, "dxr").c_str(), "ous");
        p.libre_patient_pref = preferences.isKey(k(slot, "lpf").c_str())
                                   ? preferences.getString(k(slot, "lpf").c_str(), "")
                                   : "";
        p.libre_patient_list = preferences.isKey(k(slot, "lpl").c_str())
                                   ? preferences.getString(k(slot, "lpl").c_str(), "")
                                   : "";
        p.carelink_patient_pref = preferences.isKey(k(slot, "cpf").c_str())
                                      ? preferences.getString(k(slot, "cpf").c_str(), "")
                                      : "";
        p.carelink_patient_list = preferences.isKey(k(slot, "cpl").c_str())
                                      ? preferences.getString(k(slot, "cpl").c_str(), "")
                                      : "";
        // Migrácia: starý Libre pref použitý pre Guardian
        if (p.carelink_patient_pref.length() == 0 && p.dsSource == "guardian" && p.libre_patient_pref.length() > 0) {
            p.carelink_patient_pref = p.libre_patient_pref;
        }
        p.targetLow = preferences.getFloat(k(slot, "tlo").c_str(), 3.9f);
        p.targetHigh = preferences.getFloat(k(slot, "thi").c_str(), 10.0f);
        p.targetCrit = preferences.getFloat(k(slot, "tcr").c_str(), 15.0f);
    } else if (slot == 0) {
        // Legacy single-user keys → profile 0
        p.name = preferences.getString("p0_name", "");
        p.dsSource = preferences.getString("ds_source", "off");
        p.nsUrl = preferences.getString("ns_url", "");
        p.nsToken = diaNvsGetSecret("ns_token");
        p.dsUser = preferences.getString("ds_user", "");
        p.dsPwd = diaNvsGetSecret("ds_pwd");
        p.dsRegion = preferences.isKey("ds_region") ? preferences.getString("ds_region", "eu") : "eu";
        p.libreUser = preferences.isKey("llu_user") ? preferences.getString("llu_user", "") : p.dsUser;
        p.librePwd = preferences.isKey("llu_pwd") ? diaNvsGetSecret("llu_pwd") : p.dsPwd;
        p.libreRegion = preferences.isKey("llu_reg") ? preferences.getString("llu_reg", "eu") : p.dsRegion;
        if (p.libreRegion.length() == 0) p.libreRegion = "eu";
        p.dexUser = preferences.isKey("dx_user") ? preferences.getString("dx_user", "") : p.dsUser;
        p.dexPwd = preferences.isKey("dx_pwd") ? diaNvsGetSecret("dx_pwd") : p.dsPwd;
        String dexRegFallback = p.dsRegion;
        if (dexRegFallback == "eu" || dexRegFallback == "eu2" || dexRegFallback == "global")
            dexRegFallback = "ous";
        p.dexRegion = preferences.isKey("dx_reg") ? preferences.getString("dx_reg", "ous") : dexRegFallback;
        if (p.dexRegion.length() == 0) p.dexRegion = "ous";
        p.libre_patient_pref = preferences.isKey("libre_pref") ? preferences.getString("libre_pref", "") : "";
        p.libre_patient_list = preferences.isKey("llu_pats") ? preferences.getString("llu_pats", "") : "";
        p.carelink_patient_pref = preferences.isKey("cl_pref") ? preferences.getString("cl_pref", "") : "";
        p.carelink_patient_list = preferences.isKey("cl_pats") ? preferences.getString("cl_pats", "") : "";
        if (p.carelink_patient_pref.length() == 0 && p.dsSource == "guardian" && p.libre_patient_pref.length() > 0) {
            p.carelink_patient_pref = p.libre_patient_pref;
        }
        p.targetLow = preferences.getFloat("target_low", 3.9f);
        p.targetHigh = preferences.getFloat("target_high", 10.0f);
        p.targetCrit = preferences.getFloat("target_crit", 15.0f);
    }
    if (p.libreRegion.length() == 0) p.libreRegion = "eu";
    if (p.dexRegion.length() == 0) p.dexRegion = "ous";
}

void diaConfigSaveProfiles() {
    preferences.putUChar("disp_mode", display_mode);
    for (int slot = 0; slot < DIA_PROFILE_COUNT; slot++) {
        const DiaProfile &p = profiles[slot];
        preferences.putString(k(slot, "name").c_str(), p.name);
        preferences.putString(k(slot, "src").c_str(), p.dsSource);
        preferences.putString(k(slot, "nsu").c_str(), p.nsUrl);
        diaNvsPutSecret(k(slot, "nst").c_str(), p.nsToken);
        preferences.putString(k(slot, "dsu").c_str(), p.dsUser);
        diaNvsPutSecret(k(slot, "dsp").c_str(), p.dsPwd);
        preferences.putString(k(slot, "dsr").c_str(), p.dsRegion);
        preferences.putString(k(slot, "llu").c_str(), p.libreUser);
        diaNvsPutSecret(k(slot, "llp").c_str(), p.librePwd);
        preferences.putString(k(slot, "llr").c_str(), p.libreRegion);
        preferences.putString(k(slot, "dxu").c_str(), p.dexUser);
        diaNvsPutSecret(k(slot, "dxp").c_str(), p.dexPwd);
        preferences.putString(k(slot, "dxr").c_str(), p.dexRegion);
        preferences.putString(k(slot, "lpf").c_str(), p.libre_patient_pref);
        preferences.putString(k(slot, "lpl").c_str(), p.libre_patient_list);
        preferences.putString(k(slot, "cpf").c_str(), p.carelink_patient_pref);
        preferences.putString(k(slot, "cpl").c_str(), p.carelink_patient_list);
        preferences.putFloat(k(slot, "tlo").c_str(), p.targetLow);
        preferences.putFloat(k(slot, "thi").c_str(), p.targetHigh);
        preferences.putFloat(k(slot, "tcr").c_str(), p.targetCrit);
    }
    // Legacy mirror profilu 0
    preferences.putString("ds_source", profiles[0].dsSource);
    preferences.putString("ns_url", profiles[0].nsUrl);
    diaNvsPutSecret("ns_token", profiles[0].nsToken);
    preferences.putString("ds_user", profiles[0].dsUser);
    diaNvsPutSecret("ds_pwd", profiles[0].dsPwd);
    preferences.putString("ds_region", profiles[0].dsRegion);
    preferences.putString("llu_user", profiles[0].libreUser);
    diaNvsPutSecret("llu_pwd", profiles[0].librePwd);
    preferences.putString("llu_reg", profiles[0].libreRegion);
    preferences.putString("libre_pref", profiles[0].libre_patient_pref);
    preferences.putString("llu_pats", profiles[0].libre_patient_list);
    preferences.putString("cl_pref", profiles[0].carelink_patient_pref);
    preferences.putString("cl_pats", profiles[0].carelink_patient_list);
    preferences.putString("dx_user", profiles[0].dexUser);
    diaNvsPutSecret("dx_pwd", profiles[0].dexPwd);
    preferences.putString("dx_reg", profiles[0].dexRegion);
    preferences.putFloat("target_low", profiles[0].targetLow);
    preferences.putFloat("target_high", profiles[0].targetHigh);
    preferences.putFloat("target_crit", profiles[0].targetCrit);
}

void diaConfigBegin() {
    preferences.begin("dia-config", false);
}

void diaConfigLoad() {
    diaSecretBegin();
    for (int i = 0; i < DIA_PROFILE_COUNT; i++) loadProfileFromNvs(i);

    display_mode = preferences.getUChar("disp_mode", 1);
    if (display_mode < 1) display_mode = 1;
    if (display_mode > 2) display_mode = 2;
#if !defined(DIA_BOARD_WS_3_5)
    display_mode = 1; // round: vždy jeden
#endif

    backup_ssid1 = preferences.isKey("ssid1") ? preferences.getString("ssid1", "") : "";
    backup_pwd1 = diaNvsGetSecret("pwd1");
    backup_ssid2 = preferences.isKey("ssid2") ? preferences.getString("ssid2", "") : "";
    backup_pwd2 = diaNvsGetSecret("pwd2");
    backup_ssid3 = preferences.isKey("ssid3") ? preferences.getString("ssid3", "") : "";
    backup_pwd3 = diaNvsGetSecret("pwd3");
    backup_ssid4 = preferences.isKey("ssid4") ? preferences.getString("ssid4", "") : "";
    backup_pwd4 = diaNvsGetSecret("pwd4");
    String wifiPwd = diaNvsGetSecret("wifi_pwd");
    if (diaSecretTakePlaintext()) {
        diaConfigSaveProfiles();
        if (backup_pwd1.length()) diaNvsPutSecret("pwd1", backup_pwd1);
        if (backup_pwd2.length()) diaNvsPutSecret("pwd2", backup_pwd2);
        if (backup_pwd3.length()) diaNvsPutSecret("pwd3", backup_pwd3);
        if (backup_pwd4.length()) diaNvsPutSecret("pwd4", backup_pwd4);
        if (wifiPwd.length() > 0) diaNvsPutSecret("wifi_pwd", wifiPwd);
        esp_rom_printf("[secret] stare hesla zasifrovane\n");
    }

    single_slot = preferences.getUChar("face_slot", 0) == 1 ? 1 : 0;
    show_pump = preferences.isKey("show_pump") ? preferences.getBool("show_pump", true) : true;
    watchface_style = preferences.getInt("wf_style", 0);
    if (watchface_style < 0 || watchface_style > 3) watchface_style = 0;
    if (watchface_style == 3 && (!show_pump || !diaAnyNightscout())) watchface_style = 0;
    day_brightness = preferences.getInt("day_br", 255);
    night_brightness = preferences.getInt("night_br", 25);
    day_start_hour = preferences.getInt("day_h", 7);
    night_start_hour = preferences.getInt("night_h", 22);
    auto_update = preferences.getBool("auto_upd", true);
    use_mgdl = preferences.getBool("use_mgdl", false);
    show_delta = preferences.isKey("show_delta") ? preferences.getBool("show_delta", true) : true;
    show_source = preferences.isKey("show_src") ? preferences.getBool("show_src", false) : false;
    show_glow = preferences.isKey("show_glow") ? preferences.getBool("show_glow", true) : true;
    {
        uint8_t raw = preferences.isKey("orient") ? preferences.getUChar("orient", 0) : 0;
        if (!preferences.isKey("orient_v2")) {
            // Staré: 0=Auto, 1=USB vpravo, 2=USB vľavo
            orient_mode = (raw == 2) ? 1 : 0;
            preferences.putUChar("orient", orient_mode);
            preferences.putBool("orient_v2", true);
        } else {
            orient_mode = (raw > 0) ? 1 : 0;
        }
    }
#if !defined(DIA_BOARD_WS_3_5)
    orient_mode = 0;
#endif
    diaBoardSetOrientMode(orient_mode);
    tls_insecure = preferences.isKey("tls_insec") ? preferences.getBool("tls_insec", false) : false;
    sound_on = preferences.isKey("sound_on") ? preferences.getBool("sound_on", false) : false;
    sound_volume = preferences.isKey("sound_vol") ? (int)preferences.getUChar("sound_vol", 2) : 2;
    if (sound_volume < 0 || sound_volume > 3) sound_volume = 2;
    snooze_min = preferences.getInt("snooze_min", 30);
    if (snooze_min != 15 && snooze_min != 30 && snooze_min != 60) snooze_min = 30;

    carelinkLoad();
    diaI18nLoad();
    diaConfigPushSlot(diaShownSlot());
    // USB-Serial-JTAG na 3.5B nevidí Serial.* – dump cez esp_rom_printf
    const uint8_t prevRst = preferences.isKey("last_rst") ? preferences.getUChar("last_rst", 0xFF) : 0xFF;
    const uint8_t badRst = preferences.isKey("bad_rst") ? preferences.getUChar("bad_rst", 0xFF) : 0xFF;
    const unsigned badN = preferences.isKey("bad_rst_n") ? (unsigned)preferences.getUInt("bad_rst_n", 0) : 0;
    const esp_reset_reason_t rr = esp_reset_reason();
    preferences.putUChar("last_rst", (uint8_t)rr);
    // Sticky: PANIC/WDT/BROWNOUT prežije aj ďalší flash/SW reset
    if (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT ||
        rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT) {
        preferences.putUChar("bad_rst", (uint8_t)rr);
        preferences.putUInt("bad_rst_n", (uint32_t)(badN + 1));
    }
    esp_rom_printf("[boot] reset=%s (%d) prev=%s bad=%s x%u A=%s B=%s heap=%u\n",
                   diaResetReasonStr(rr), (int)rr,
                   prevRst == 0xFF ? "none" : diaResetReasonStr((esp_reset_reason_t)prevRst),
                   badRst == 0xFF ? "none" : diaResetReasonStr((esp_reset_reason_t)badRst),
                   badN,
                   profiles[0].dsSource.c_str(),
                   profiles[1].dsSource.c_str(),
                   (unsigned)ESP.getFreeHeap());
}

void diaConfigClearSessionsSlot(int slot) {
    if (slot < 0 || slot >= DIA_PROFILE_COUNT) return;
    profiles[slot].libre_token = "";
    profiles[slot].libre_account_hash = "";
    profiles[slot].libre_patient_id = "";
    profiles[slot].dexcom_session_id = "";
    profiles[slot].dexcom_account_id = "";
}

void diaConfigClearSessions() {
    for (int i = 0; i < DIA_PROFILE_COUNT; i++) diaConfigClearSessionsSlot(i);
    libre_token = "";
    libre_account_hash = "";
    libre_patient_id = "";
    dexcom_session_id = "";
    dexcom_account_id = "";
}
