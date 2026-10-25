#include "dia_legal.h"

#include <Arduino.h>
#include <Preferences.h>

extern Preferences preferences;

static bool diaLegalStore(const char *key) {
    for (int i = 0; i < 3; i++) {
        preferences.putUChar(key, kDiaLegalVer);
        if (preferences.getUChar(key, 0) == kDiaLegalVer) return true;
    }
    Serial.printf("[legal] NVS %s zlyhal\n", key);
    return false;
}

bool diaLegalAccepted() {
    return preferences.getUChar("legal_ok", 0) == kDiaLegalVer;
}

bool diaLegalAccept() {
    return diaLegalStore("legal_ok");
}

bool diaLegalWebAccepted() {
    return preferences.getUChar("legal_web", 0) == kDiaLegalVer;
}

bool diaLegalAcceptWeb() {
    return diaLegalStore("legal_web");
}
