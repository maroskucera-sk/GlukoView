#pragma once
#include <Arduino.h>
#include <WString.h>

/** Base64 PNG – vygenerované z assets/favicon.png a assets/portal_logo.png */
extern const char DIA_PORTAL_FAVICON_B64[];
extern const char DIA_PORTAL_LOGO_B64[];

void diaPortalAppendFaviconLink(String &page);
void diaPortalAppendLogoHtml(String &page);
