#pragma once

/** In-memory stand-in for NVS defaults used by the screenshot scenes. */
void hostPrepareDefaults();

/** Next time the language picker paints, save this PNG and choose Slovenčina. */
void hostArmLanguageCapture(const char *pngPath);
void hostEndLanguageCapture();

/** Fixed wall clock for the clock face, so shots do not depend on the minute. */
void hostSetClock(int hour, int minute);

bool hostClickText(const char *text);
void hostSavePng(const char *path);
