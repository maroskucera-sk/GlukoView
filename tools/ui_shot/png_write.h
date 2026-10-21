#pragma once

#include <stdint.h>

/** RGB565 framebuffer to PNG. Round mask makes the corners transparent. */
bool pngWriteRgb565(const char *path, const uint16_t *px, int w, int h, bool roundMask);
