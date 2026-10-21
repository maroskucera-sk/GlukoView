#pragma once

#include <Arduino.h>
#include "dia_types.h"

/** Po boote / unlock UI – grace pred „CareLink odhlásený“. */
void diaUiMarkReady();

/** Istý Auth (nie sieť / grace) → červený „odhlásený“. */
bool diaUiShowClLoggedOut(FetchError err);

/** Text statusu pre Auth/guardian vs ostatné FetchError. */
const char *diaUiAuthStatusText(FetchError err, const String &src);
