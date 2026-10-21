#pragma once

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>

class String {
public:
    String() = default;
    String(const char *c) : s_(c ? c : "") {}
    String(const String &) = default;
    String &operator=(const String &) = default;
    String &operator=(const char *c) {
        s_ = c ? c : "";
        return *this;
    }
    const char *c_str() const { return s_.c_str(); }
    size_t length() const { return s_.size(); }
    bool operator==(const String &o) const { return s_ == o.s_; }
    bool operator==(const char *c) const { return s_ == (c ? c : ""); }
    bool operator!=(const String &o) const { return !(*this == o); }
    bool operator!=(const char *c) const { return !(*this == c); }
    String &operator+=(const char *c) {
        if (c) s_ += c;
        return *this;
    }
    String &operator+=(const String &o) {
        s_ += o.s_;
        return *this;
    }

private:
    std::string s_;
};

class SerialClass {
public:
    void println(const char *msg) {
        if (msg) puts(msg);
    }
    void println(const String &msg) { puts(msg.c_str()); }
    void printf(const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
};

extern SerialClass Serial;

class EspClass {
public:
    void restart();
};

extern EspClass ESP;

unsigned long millis();
void delay(unsigned long ms);
float temperatureRead();
bool getLocalTime(struct tm *info, uint32_t ms = 5000);
void vTaskDelay(uint32_t ticks);

#ifndef HIGH
#define HIGH 1
#define LOW 0
#endif
