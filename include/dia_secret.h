#pragma once

#include <Arduino.h>

/** Šifrovanie tajomstiev v NVS. eFuse sa nezapáli. Kľúč je odvodený z MAC čipu. */
bool diaSecretBegin();
bool diaSecretActive();
bool diaSecretIsSealed(const String &stored);

String diaSecretSeal(const String &plain);
/** Nezašifrovaný starý text vráti tak, ako je. Zlý tag nastaví failed a vráti prázdny reťazec. */
String diaSecretOpen(const String &stored, bool *failed = nullptr);

String diaNvsGetSecret(const char *key);
void diaNvsPutSecret(const char *key, const String &plain);

/** true raz, ak get čítal ešte nezašifrovanú hodnotu. Potom sa príznak zmaže. */
bool diaSecretTakePlaintext();
