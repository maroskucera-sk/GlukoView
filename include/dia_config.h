#pragma once

#include <Arduino.h>
#include <Preferences.h>

#define DIA_PROFILE_COUNT 2

/** Per-dieťa / per-slot konfigurácia zdroja. */
struct DiaProfile {
    String name;       // "Janko"
    String dsSource;   // libre|nightscout|dexcom|guardian|off
    String nsUrl;
    String nsToken;
    String dsUser;
    String dsPwd;
    String dsRegion;
    String libreUser;
    String librePwd;
    String libreRegion;
    String dexUser;
    String dexPwd;
    String dexRegion;
    String libre_token;
    String libre_account_hash;
    String libre_patient_id;
    String libre_patient_pref;
    String libre_patient_list;
    /** CareLink / Guardian – oddelené od Libre (1 konto → 2 pacienti = rôzne pref, spoločné tokeny). */
    String carelink_patient_pref;
    String carelink_patient_list;
    String dexcom_session_id;
    String dexcom_account_id;
    float targetLow;
    float targetHigh;
    float targetCrit;
};

extern DiaProfile profiles[DIA_PROFILE_COUNT];
/** 1 = jeden fullscreen (profiles[0]), 2 = split pol-na-pol. */
extern uint8_t display_mode;

// Aktívny slot pre fetch (globals nižšie = mirror profiles[active] počas fetchu)
extern int dia_active_slot;

// Zdroj dát a credentials – sync s profiles[dia_active_slot] / legacy p0
extern String dsSource;
extern String nsUrl;
extern String nsToken;
extern String dsUser;
extern String dsPwd;
extern String dsRegion;

extern String libreUser;
extern String librePwd;
extern String libreRegion;
extern String dexUser;
extern String dexPwd;
extern String dexRegion;

extern String libre_token;
extern String libre_account_hash;
extern String libre_patient_id;
extern String libre_patient_pref;
extern String libre_patient_list;
extern String carelink_patient_pref;
extern String carelink_patient_list;

extern String dexcom_session_id;
extern String dexcom_account_id;

extern String backup_ssid1;
extern String backup_pwd1;
extern String backup_ssid2;
extern String backup_pwd2;
extern String backup_ssid3;
extern String backup_pwd3;
extern String backup_ssid4;
extern String backup_pwd4;

extern float targetLow;
extern float targetHigh;
extern float targetCrit;
extern int day_brightness;
extern int night_brightness;
extern int day_start_hour;
extern int night_start_hour;
extern bool use_mgdl;
extern bool show_delta;
extern bool show_source;
/** Nightscout pump údaje (IOB/COB/basal). Default ZAP ak je NS. */
extern bool show_pump;
extern bool show_glow;
/** 3.5B: 0=USB vpravo, 1=USB vľavo. Round ignoruje. */
extern uint8_t orient_mode;
extern bool tls_insecure;
extern bool auto_update;
extern int watchface_style;
/** Ktoré dieťa je na celú obrazovku, keď sú nakonfigurované obe (0 alebo 1). */
extern uint8_t single_slot;
/** Critical alert sound (3.5B). Default off. */
extern bool sound_on;
/** Hlasitosť 0 tichá … 3 vysoká. Stredná (2) je doterajšia úroveň alarmu. */
extern int sound_volume;
/** Snooze length minutes: 15 / 30 / 60. */
extern int snooze_min;

extern Preferences preferences;

extern bool forceFetch;
extern bool shouldSaveConfig;
extern bool wifi_was_connected;

extern bool pending_pref_save;
extern int pending_pref_mode;
extern int pending_pref_val;

void diaConfigBegin();
void diaConfigLoad();
void diaConfigClearSessions();
void diaConfigClearSessionsSlot(int slot);

/** Skopíruj profiles[slot] → globálne premenné (pre fetch). */
void diaConfigPushSlot(int slot);
/** Skopíruj VŠETKO z globals → profiles[slot] (pozor: nie počas cudzieho fetchu). */
void diaConfigPullSlot(int slot);
/** Len session tokeny z globals → profiles[slot] (po fetchi – bezpečné pri dual). */
void diaConfigPullSessionsSlot(int slot);
/** Ulož profiles + display_mode do NVS. */
void diaConfigSaveProfiles();

bool diaProfileConfigured(int slot);
/** Slot zobrazený v režime jedného dieťaťa. */
int diaShownSlot();
bool diaProfileActive(int slot);
int diaProfileActiveCount();
/** true = aspoň jeden aktívny profil má Nightscout (IOB/COB má zmysel). */
bool diaAnyNightscout();
