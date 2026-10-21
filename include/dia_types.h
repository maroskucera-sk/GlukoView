#pragma once

#include <stdint.h>

enum class FetchError : uint8_t {
    None = 0,
    Wifi,
    Auth,
    Network,
    Empty,
    Config
};

#define STALE_AGE_MINUTES 15
#define DIA_HISTORY_MAX 36

constexpr float MGDL_FACTOR = 18.0182f;
