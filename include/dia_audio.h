#pragma once

#include <stdint.h>
#include <stdbool.h>

void diaAudioInit();
/** Po boot zvuku: stop I2S/MCLK aby WiFi na 3.5B videlo AP. */
void diaAudioQuiesceForWifi();
/** Periodický tick – chime pri critical, ak sound_on a nie snooze. */
void diaAudioTick(uint32_t nowMs);
/** Snooze na aktuálne snooze_min (z menu / tap). */
void diaAudioSnooze();
bool diaAudioIsSnoozed(uint32_t nowMs);
/** Zostávajúce minúty snooze (0 ak nie). */
int diaAudioSnoozeRemainMin(uint32_t nowMs);
void diaAudioClearSnooze();
/** Alarm WAV (high/low glykémia + strata spojenia). */
void diaAudioPlayChime();
/** Krátky ack beep po snooze. */
void diaAudioPlayAck();
/** True pri critical glykémii alebo strate spojenia (stale / WiFi). */
bool diaAudioAnyCritical();
/** Po zmene limitov – dovolí okamžitý chime (bez čakania 5 min). */
void diaAudioResetChimeTimer();
