#include "dia_ui_status.h"
#include "dia_fetch.h"
#include "dia_glucose.h"
#include "dia_i18n.h"

#include <Arduino.h>

static uint32_t s_uiReadyMs = 0;

void diaUiMarkReady() {
    if (s_uiReadyMs == 0) s_uiReadyMs = millis();
}

bool diaUiShowClLoggedOut(FetchError err) {
    if (err != FetchError::Auth) return false;
    if (diaFetchBusy()) return false;
    if (s_uiReadyMs == 0) return false;
    // Boot grace + krátka pauza aj v behu (refresh môže ešte dobehnúť)
    if ((millis() - s_uiReadyMs) < 75000UL) return false;
    return true;
}

const char *diaUiAuthStatusText(FetchError err, const String &src) {
    if (err == FetchError::Auth && src == "guardian") {
        if (diaUiShowClLoggedOut(err)) return S(Str::ClLoggedOut);
        return S(Str::Connecting);
    }
    return fetchErrorText(err);
}
