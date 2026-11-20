#pragma once
#include "Arduino.h"
#define WL_CONNECTED 3
#define WIFI_STA 1
struct IPAddress { String toString() const { return "192.168.0.42"; } };
struct WiFiStub {
    int simStatus = WL_CONNECTED;
    int status() { return simStatus; }
    String SSID() { return "Casa_5G"; }
    int RSSI() { return -54; }
    IPAddress localIP() { return IPAddress(); }
    void disconnect() {}
    void begin(const char*, const char*) {}
};
extern WiFiStub WiFi;
