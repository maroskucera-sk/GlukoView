#pragma once

#include <Arduino.h>

/** Načítaj CareLink tokeny (slot 0 + legacy). */
void carelinkLoad();

/** Priprav tokeny pre fetch slotu (1 konto: B zdedí A; 2 kontá: vlastné tokeny B). */
void carelinkBindSlot(int slot);

/** True, ak aktuálne nabindované tokeny stačia. */
bool carelinkTokensReady();

/** True, ak má slot vlastné tokeny (nie zdedené z A). */
bool carelinkSlotOwnsTokens(int slot);

/** True, ak slot má vlastné tokeny alebo môže použiť A (fallback). */
bool carelinkTokensReadyForSlot(int slot);

/**
 * Ulož tokeny z logindata.json do daného slotu (0 alebo 1).
 * Pri chybe parsovania nič neprepíše.
 */
bool carelinkSaveFromJson(const String &json, int slot = 0);

/**
 * Priprav Auth0 authorize URL (Care Partner). region = "EU" alebo "US".
 * tokenSlot = pre ktorého používateľa (A/B) sa tokeny uložia.
 */
bool carelinkBeginBrowserLogin(const String &region, int tokenSlot, String &outAuthorizeUrl, String &errOut);

/**
 * Dokonči login: vlož callback URL (com.medtronic.carepartner:…?code=…),
 * samotný code=, alebo celý logindata.json. Uloží tokeny do slotu z begin.
 */
bool carelinkCompleteBrowserLogin(const String &callbackOrCodeOrJson, String &errOut);

/** True, ak beží rozpracované browser prihlásenie (čaká sa na kód). */
bool carelinkBrowserLoginPending();

/** Slot, do ktorého pôjde práve rozpracovaný login (0/1). */
int carelinkPendingTokenSlot();

/**
 * Stiahni zoznam sledovaných pacientov (Care Partner) pre slot.
 * Formát riadkov: username\\tMeno Priezvisko\\n
 */
bool carelinkFetchPatientList(int slot, String &listOut, String &errOut);

/** Stiahni poslednú glykémiu Guardian / 780G z CareLink. Beží vo fetch tasku. */
void fetchGuardian();

/** True, kým práve odchádza obnovenie refresh tokenu. V tom okamihu dosku nereštartovať. */
bool carelinkRefreshInFlight();

/** WiFi spadlo: zabudni objavené adresy serverov. Tokeny ostanú. */
void carelinkForgetEndpoints();
