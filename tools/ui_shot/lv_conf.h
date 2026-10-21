/**
 * PC screenshot host. Same LVGL 8.3 defaults the firmware uses
 * (there is no lv_conf.h in the project; only a few -D font flags).
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (2U * 1024U * 1024U)

#define LV_TICK_CUSTOM 0
#define LV_DPI_DEF 130
#define LV_USE_LOG 0

#endif
