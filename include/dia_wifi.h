#pragma once

#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFiMulti.h>

extern WiFiManager wm;
extern WiFiMulti wifiMulti;

extern WiFiManagerParameter *custom_source;
extern WiFiManagerParameter *custom_ns_url;
extern WiFiManagerParameter *custom_ns_token;
extern WiFiManagerParameter *custom_llu_user;
extern WiFiManagerParameter *custom_llu_pwd;
extern WiFiManagerParameter *custom_llu_region;
extern WiFiManagerParameter *custom_libre_patient;
extern WiFiManagerParameter *custom_dx_user;
extern WiFiManagerParameter *custom_dx_pwd;
extern WiFiManagerParameter *custom_dx_region;
extern WiFiManagerParameter *custom_units;
extern WiFiManagerParameter *custom_tls;
extern WiFiManagerParameter *custom_t_low;
extern WiFiManagerParameter *custom_t_high;
extern WiFiManagerParameter *custom_t_crit;

void saveConfigCallback();
void saveNetworkToBackup(String ssid, String pwd);
void autoSaveNetwork();
void refreshPortalSavedNetsHtml();
void diaWifiSetupManager();
void diaWifiAddParameters();

/** Skúsi všetky uložené siete (MRU first). */
bool diaWifiTryConnect();

/** Pripojenie alebo config portal. */
bool diaWifiEnsureConnected();

void diaWifiProcess();
/** Po rýchlom WiFi ešte chvíľu drží boot vetu, potom vráti ciferník. */
void diaWifiPollBootLegal();
void diaWifiSavePortalConfigIfNeeded();
void diaWifiMaintain(uint32_t currentMillis);
/** True = na displeji má ostať IP návod, fetch ešte nespúšťať. */
bool diaWifiNeedsSetupHint();
