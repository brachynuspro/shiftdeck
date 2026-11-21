/*-------------------------------------------------------------------------------------------------
** Vault.h - Credential & settings management via JSON on LittleFS
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>
#include <vector>

struct WiFiNet { String ssid; String pass; };

class Vault {
public:
    static Vault& getInstance();

    void initialize();

    String getSpotifyClientID();
    String getSpotifyClientSecret();
    // Only setups from before PKCE (v1.3.2) have one; their refresh token still needs it.
    bool hasSpotifyClientSecret();
    void setSpotifyCredentials(const String& clientId, const String& clientSecret);
    String getSpotifyRefreshToken();
    void setSpotifyRefreshToken(const String& token);


    // Saved Wi-Fi networks, most recently used first (tried first when none is in range).
    static const int MAX_WIFI = 5;
    std::vector<WiFiNet> getWiFiNetworks();
    bool hasWiFi();
    void saveWiFiNetwork(const String& ssid, const String& pass);   // adds or replaces; a 6th drops the oldest
    void removeWiFiNetwork(const String& ssid);
    void markWiFiUsed(const String& ssid);

    // OTA bookkeeping: version being installed, and one that failed and was rolled back.
    String getPendingUpdate();
    void setPendingUpdate(const String& v);
    String getSkipVersion();
    void setSkipVersion(const String& v);

    int getBrightness();
    void setBrightness(int val);

    String getTimezone();

    bool isTouchCalibrated();
    void setTouchCalibration(int16_t rawX1, int16_t rawY1, int16_t rawX2, int16_t rawY2);
    void getTouchCalibration(int16_t &rawX1, int16_t &rawY1, int16_t &rawX2, int16_t &rawY2);

    void save();
    void factoryReset();

private:
    Vault() {}

    String _spotifyClientId;
    String _spotifyClientSecret;
    String _spotifyRefreshToken;
    std::vector<WiFiNet> _wifi;
    String _pendingUpdate;
    String _skipVersion;
    int _brightness = 80;
    String _timezone = "UTC-3";

    bool _touchCalibrated = false;
    int16_t _touchRawX1 = 0;
    int16_t _touchRawY1 = 0;
    int16_t _touchRawX2 = 0;
    int16_t _touchRawY2 = 0;

    void load();
};
