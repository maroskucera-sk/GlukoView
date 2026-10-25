#pragma once

#include <stdint.h>

/**
 * Verzia znenia súhlasu. Zvýšenie znova vyžiada potvrdenie na displeji aj vo
 * portáli. Tokeny, WiFi a zdroje sa tým nemenia.
 */
/** 1 = len veta o pomôcke. 2 = k nej aj nezávislosť od Abbott, Dexcom a Medtronic. */
static constexpr uint8_t kDiaLegalVer = 2;

/** Súhlas na displeji je v NVS (legal_ok == kDiaLegalVer). */
bool diaLegalAccepted();
/** Zapíše súhlas. false len keď NVS zápis trikrát nesadne. */
bool diaLegalAccept();

/** Samostatný súhlas z webového portálu (legal_web). */
bool diaLegalWebAccepted();
bool diaLegalAcceptWeb();
