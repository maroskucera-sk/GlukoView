#pragma once

#include <Arduino.h>

class Preferences {
public:
    bool begin(const char *name, bool readOnly = false);
    void end();
    bool clear();
    bool isKey(const char *key);

    size_t putInt(const char *key, int32_t value);
    size_t putUChar(const char *key, uint8_t value);
    size_t putLong(const char *key, int32_t value);
    size_t putFloat(const char *key, float value);
    size_t putBool(const char *key, bool value);
    size_t putString(const char *key, const char *value);
    size_t putString(const char *key, const String &value);

    int32_t getInt(const char *key, int32_t defaultValue = 0);
    uint8_t getUChar(const char *key, uint8_t defaultValue = 0);
    int32_t getLong(const char *key, int32_t defaultValue = 0);
    float getFloat(const char *key, float defaultValue = 0);
    bool getBool(const char *key, bool defaultValue = false);
    String getString(const char *key, String defaultValue = String());
};

extern Preferences preferences;
