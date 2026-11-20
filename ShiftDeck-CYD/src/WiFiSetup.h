/*-------------------------------------------------------------------------------------------------
** WiFiSetup.h - Captive portal for WiFi configuration
** AP with password, DNS hijack, web config page
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

class WiFiSetup {
public:
    static WiFiSetup& getInstance();

    void startCaptivePortal();
    void stopCaptivePortal();
    void handleLoop();

    String getAPPassword();
    String getAPSSID() { return AP_SSID; }

private:
    WiFiSetup() {}

    DNSServer _dnsServer;
    WebServer _server;
    bool _portalRunning = false;
    // Fixed SSID and password for the captive-portal AP. The password
    // is a deterministic 10-character string (no special symbols so
    // it survives shell-quoting on every platform) and lives in flash
    // so it's the same on every device.
    static constexpr const char *AP_SSID = "Shiftdeck-Spotify";
    static constexpr const char *AP_PASSWORD = "SftDckSp0t";

    void handleRoot();
    void handleSave();
    static String scanNetworks();
    static const char* HTML_HEAD;
    static const char* HTML_CSS;
    static const char* HTML_FOOT;
    static const char* HTML_SAVED;
};
