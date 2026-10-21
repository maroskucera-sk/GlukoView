#pragma once

#include <Arduino.h>

/** True, keď je v NVS uložený 4-miestny PIN portálu. */
bool diaPinIsSet();

/** Uloží PIN (presne 4 číslice). Starú reláciu v prehliadači zruší. */
bool diaPinSet(const char *pin);

/** Zmaže len PIN. WiFi, účty a limity ostanú. */
void diaPinClear();

/** Overí PIN. Pri úspechu vydá nový kľúč relácie. */
bool diaPinCheck(const char *pin);

/** True krátko po sérii zlých pokusov. */
bool diaPinLockedOut();

/** Aktuálny kľúč relácie, prázdny kým nikto PIN nezadal. */
const char *diaPinToken();

/** Cookie hlavička obsahuje platný kľúč. Bez PINu vždy true. */
bool diaPinCookieOk(const String &cookieHeader);
