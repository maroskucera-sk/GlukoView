#pragma once

#include <Arduino.h>

class IPAddress {
public:
    IPAddress() : text_("0.0.0.0") {}
    explicit IPAddress(const char *text) : text_(text ? text : "") {}
    String toString() const { return String(text_.c_str()); }

private:
    std::string text_;
};

#ifndef WL_CONNECTED
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
#define WL_IDLE_STATUS 0
#endif

class WiFiClass {
public:
    int status();
    IPAddress localIP();
    int RSSI();
};

extern WiFiClass WiFi;
