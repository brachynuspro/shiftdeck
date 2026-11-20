/*-------------------------------------------------------------------------------------------------
** Vault.h - Credential & settings management via JSON on LittleFS
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>

class Vault {
public:
    static Vault& getInstance();

    void initialize();

    String getSpotifyClientID();
    String getSpotifyClientSecret();
    void setSpotifyCredentials(const String& clientId, const String& clientSecret);
    String getSpotifyRefreshToken();
    void setSpotifyRefreshToken(const String& token);

    String getAppsScriptUrl();
    void setAppsScriptUrl(const String& url);

    String getWiFiSSID();
    String getWiFiPassword();
    void setWiFiCredentials(const String& ssid, const String& pass);

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
    String _appsScriptUrl;
    String _wifiSSID;
    String _wifiPassword;
    int _brightness = 80;
    String _timezone = "UTC-3";

    bool _touchCalibrated = false;
    int16_t _touchRawX1 = 0;
    int16_t _touchRawY1 = 0;
    int16_t _touchRawX2 = 0;
    int16_t _touchRawY2 = 0;

    void load();
};
