#pragma once

#include <WString.h>

/** Registrácia /carelink* ciest na WiFiManager WebServer. */
void diaPortalRegisterCarelinkRoutes();

/** Tichá právna veta na koniec stránky portálu. Nezasahuje do formulárov. */
void diaPortalAppendLegalNote(String &page);
