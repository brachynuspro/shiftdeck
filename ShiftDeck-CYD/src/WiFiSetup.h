/*-------------------------------------------------------------------------------------------------
** WiFiSetup.h - Saved Wi-Fi networks: joining the best one, and the page that manages them
**
** The page lists the saved networks (edit / delete) and the ones in range (add).
** It is served two ways:
**   - captive portal: own access point + DNS hijack (first setup, or no saved network in range);
**     saving restarts the board so it joins the new network.
**   - on the current network (Settings > Wi-Fi > Adicionar): saving keeps the board online.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>

class WiFiSetup {
public:
    static WiFiSetup& getInstance();

    // Joins the strongest saved network in range; tick() runs while waiting.
    bool connectSaved(void (*tick)());
    bool savedNetworkInRange();

    void startCaptivePortal();
    void startWebPage();
    void stop();
    void handleLoop();
    // True once after a network was saved on the page (Settings then leaves the QR).
    bool takeSaved();

    String getAPPassword() { return AP_PASSWORD; }
    String getAPSSID() { return AP_SSID; }

private:
    WiFiSetup() {}

    // Fixed SSID and password for the captive-portal AP. The password
    // is a deterministic 10-character string (no special symbols so
    // it survives shell-quoting on every platform) and lives in flash
    // so it's the same on every device.
    static constexpr const char *AP_SSID = "Shiftdeck-Spotify";
    static constexpr const char *AP_PASSWORD = "SftDckSp0t";
};
