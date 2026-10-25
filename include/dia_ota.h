#pragma once

void diaOtaNoteBoot();
void check_firmware_update(bool manual);
/** Verzia na serveri pre túto dosku. 0 = nedostupná. Krátky časový limit. */
int diaOtaServerVersion();
/** Posledná známa verzia zo servera. 0 = kontrola ešte neprebehla. Bez siete. */
int diaOtaKnownVersion();
void update_progress_cb(int current, int total);

extern unsigned long last_ota_check;
/** Interval medzi kontrolami po prvej (24 h). */
extern const unsigned long otaCheckInterval;
/** Oneskorenie prvej auto-kontroly po štarte (~2 min). */
extern const unsigned long otaFirstCheckDelay;
/** Opakovaná kontrola, kým je noc alebo ešte nie je čas. */
extern const unsigned long otaNightRetry;
extern bool ota_first_check_done;
/** Deň podľa denného a nočného jasu. Bez nastaveného času je false. Ručná aktualizácia toto nepoužíva. */
bool diaOtaInDayWindow();
