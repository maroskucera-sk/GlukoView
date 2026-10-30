#pragma once

// Verzia 2.1 je vnútorne 201 (major * 100 + minor). version.txt môže byť "2.1".
#ifndef FIRMWARE_VERSION_MAJOR
#define FIRMWARE_VERSION_MAJOR 1
#endif
#ifndef FIRMWARE_VERSION_MINOR
#define FIRMWARE_VERSION_MINOR 1
#endif
#define FIRMWARE_VERSION ((FIRMWARE_VERSION_MAJOR) * 100 + (FIRMWARE_VERSION_MINOR))
