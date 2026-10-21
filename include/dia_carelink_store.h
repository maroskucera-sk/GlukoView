#pragma once

#include <Arduino.h>

/** Chunkovaný zápis CareLink tokenov do NVS (kla/clr / c1a/c1r). */
bool carelinkNvsPutChunks(const char *prefix, const String &val, bool seal);
String carelinkNvsGetChunks(const char *prefix, bool *plainOut, bool *failOut);
void carelinkNvsClearPrefix(const char *prefix, int chunkCount);
void carelinkNvsLogStats(const char *why);
void carelinkNvsFreeJournals(int chunkCount);
