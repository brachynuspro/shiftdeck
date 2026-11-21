/*-------------------------------------------------------------------------------------------------
** Vault.cpp - Settings via JSON on LittleFS
** ------------------------------------------------------------------------------------------------
*/
#include "Vault.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "logTags.h"
#include "SCLogger.h"
#include "iTunesProxy.h"   // FsLock

#define CONFIG_FILE "/config.json"
#define CONFIG_FILE_TMP "/config.json.tmp"

#define DEFAULT_SPOTIFY_CLIENT_ID "your_spotify_client_id"
#define DEFAULT_SPOTIFY_CLIENT_SECRET "your_spotify_client_secret"

Vault& Vault::getInstance() {
    static Vault instance;
    return instance;
}

void Vault::initialize() {
    _spotifyClientId = DEFAULT_SPOTIFY_CLIENT_ID;
    _spotifyClientSecret = DEFAULT_SPOTIFY_CLIENT_SECRET;
    load();
}

void Vault::load() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        spLogW(LOGTAG_GENERAL, "Vault::load - config.json does not exist");
        return;
    }

    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) {
        spLogE(LOGTAG_GENERAL, "Vault::load - failed to open config.json");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        spLogE(LOGTAG_GENERAL, "Vault::load - JSON parse error: %s", err.c_str());
        return;
    }

    _wifi.clear();
    for (JsonObject n : doc["wifi"].as<JsonArray>())
        if (_wifi.size() < MAX_WIFI && n["s"].as<String>().length()) _wifi.push_back({ n["s"].as<String>(), n["p"].as<String>() });
    // Up to v1.3 there was a single network
    if (_wifi.empty() && doc["wifi_ssid"].as<String>().length())
        _wifi.push_back({ doc["wifi_ssid"].as<String>(), doc["wifi_pass"].as<String>() });
    _pendingUpdate = doc["ota_pending"] | "";
    _skipVersion = doc["ota_skip"] | "";
    if (!doc["spotify_refresh_token"].isNull()) _spotifyRefreshToken = doc["spotify_refresh_token"].as<String>();
    if (!doc["spotify_client_id"].isNull()) _spotifyClientId = doc["spotify_client_id"].as<String>();
    if (!doc["spotify_client_secret"].isNull()) _spotifyClientSecret = doc["spotify_client_secret"].as<String>();
    if (!doc["brightness"].isNull()) _brightness = doc["brightness"].as<int>();
    if (!doc["timezone"].isNull()) _timezone = doc["timezone"].as<String>();
    if (!doc["touch_calibrated"].isNull()) _touchCalibrated = doc["touch_calibrated"].as<bool>();
    if (!doc["touch_raw_x1"].isNull()) _touchRawX1 = doc["touch_raw_x1"].as<int16_t>();
    if (!doc["touch_raw_y1"].isNull()) _touchRawY1 = doc["touch_raw_y1"].as<int16_t>();
    if (!doc["touch_raw_x2"].isNull()) _touchRawX2 = doc["touch_raw_x2"].as<int16_t>();
    if (!doc["touch_raw_y2"].isNull()) _touchRawY2 = doc["touch_raw_y2"].as<int16_t>();

    spLogI(LOGTAG_GENERAL, "Vault::load - refresh_token length=%d, client_id set=%d",
           _spotifyRefreshToken.length(), _spotifyClientId != DEFAULT_SPOTIFY_CLIENT_ID);
}

void Vault::save() {
    JsonDocument doc;
    JsonArray nets = doc["wifi"].to<JsonArray>();
    for (auto& n : _wifi) { JsonObject o = nets.add<JsonObject>(); o["s"] = n.ssid; o["p"] = n.pass; }
    // Older firmware reads only these: a downgrade keeps the most recent network.
    doc["wifi_ssid"] = _wifi.empty() ? String() : _wifi[0].ssid;
    doc["wifi_pass"] = _wifi.empty() ? String() : _wifi[0].pass;
    doc["ota_pending"] = _pendingUpdate;
    doc["ota_skip"] = _skipVersion;
    doc["spotify_refresh_token"] = _spotifyRefreshToken;
    doc["spotify_client_id"] = _spotifyClientId;
    doc["spotify_client_secret"] = _spotifyClientSecret;
    doc["brightness"] = _brightness;
    doc["timezone"] = _timezone;
    doc["touch_calibrated"] = _touchCalibrated;
    doc["touch_raw_x1"] = _touchRawX1;
    doc["touch_raw_y1"] = _touchRawY1;
    doc["touch_raw_x2"] = _touchRawX2;
    doc["touch_raw_y2"] = _touchRawY2;

    // Atomic write: write to a temp file, verify it parses back correctly,
    // then replace the real config file. This avoids corrupting config.json
    // if power is lost mid-write (a real risk in a car).
    // Also called from the player task (rotated refresh token): hold the LittleFS lock.
    FsLock fs(pdMS_TO_TICKS(5000));
    File f = LittleFS.open(CONFIG_FILE_TMP, "w");
    if (!f) {
        spLogE(LOGTAG_GENERAL, "Vault::save - failed to open %s for write", CONFIG_FILE_TMP);
        spLogE(LOGTAG_GENERAL, "Vault::save - ABORTED, refresh_token NOT persisted!");
        return;
    }
    size_t written = serializeJson(doc, f);
    f.close();

    if (written == 0) {
        spLogE(LOGTAG_GENERAL, "Vault::save - wrote 0 bytes, aborting (keeping old config)");
        spLogE(LOGTAG_GENERAL, "Vault::save - ABORTED, refresh_token NOT persisted!");
        LittleFS.remove(CONFIG_FILE_TMP);
        return;
    }

    // Verify the temp file is valid JSON before committing it
    File verify = LittleFS.open(CONFIG_FILE_TMP, "r");
    if (!verify) {
        spLogE(LOGTAG_GENERAL, "Vault::save - failed to reopen temp file for verification");
        spLogE(LOGTAG_GENERAL, "Vault::save - ABORTED, refresh_token NOT persisted!");
        LittleFS.remove(CONFIG_FILE_TMP);
        return;
    }
    JsonDocument verifyDoc;
    DeserializationError verifyErr = deserializeJson(verifyDoc, verify);
    verify.close();
    if (verifyErr || verifyDoc["wifi_ssid"].isNull()) {
        spLogE(LOGTAG_GENERAL, "Vault::save - verification failed (%s), keeping old config", verifyErr.c_str());
        spLogE(LOGTAG_GENERAL, "Vault::save - ABORTED, refresh_token NOT persisted!");
        LittleFS.remove(CONFIG_FILE_TMP);
        return;
    }

    if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
    }
    if (!LittleFS.rename(CONFIG_FILE_TMP, CONFIG_FILE)) {
        spLogE(LOGTAG_GENERAL, "Vault::save - rename failed, config may be lost!");
        spLogE(LOGTAG_GENERAL, "Vault::save - ABORTED, refresh_token NOT persisted!");
        return;
    }

    spLogI(LOGTAG_GENERAL, "Vault::save - committed atomically, refresh_token=%u bytes", (unsigned)_spotifyRefreshToken.length());
}

void Vault::factoryReset() {
    spLogW(LOGTAG_GENERAL, "Vault::factoryReset - erasing config.json");
    if (LittleFS.exists(CONFIG_FILE)) LittleFS.remove(CONFIG_FILE);
    if (LittleFS.exists(CONFIG_FILE_TMP)) LittleFS.remove(CONFIG_FILE_TMP);

    _wifi.clear();
    _pendingUpdate = "";
    _skipVersion = "";
    _spotifyRefreshToken = "";
    _spotifyClientId = DEFAULT_SPOTIFY_CLIENT_ID;
    _spotifyClientSecret = DEFAULT_SPOTIFY_CLIENT_SECRET;
    _brightness = 80;
    _timezone = "UTC-3";
    _touchCalibrated = false;
    _touchRawX1 = 0;
    _touchRawY1 = 0;
    _touchRawX2 = 0;
    _touchRawY2 = 0;
}

String Vault::getSpotifyClientID() { return _spotifyClientId; }
String Vault::getSpotifyClientSecret() { return _spotifyClientSecret; }
bool Vault::hasSpotifyClientSecret() { return _spotifyClientSecret.length() && !_spotifyClientSecret.startsWith("your_"); }
void Vault::setSpotifyCredentials(const String& clientId, const String& clientSecret) {
    _spotifyClientId = clientId;
    _spotifyClientSecret = clientSecret;
    save();
}
String Vault::getSpotifyRefreshToken() { return _spotifyRefreshToken; }
void Vault::setSpotifyRefreshToken(const String& token) {
    _spotifyRefreshToken = token;
    save();
}


std::vector<WiFiNet> Vault::getWiFiNetworks() { return _wifi; }
bool Vault::hasWiFi() { return !_wifi.empty(); }

void Vault::saveWiFiNetwork(const String& ssid, const String& pass) {
    for (size_t i = 0; i < _wifi.size(); i++)
        if (_wifi[i].ssid == ssid) { _wifi.erase(_wifi.begin() + i); break; }
    _wifi.insert(_wifi.begin(), { ssid, pass });
    if (_wifi.size() > MAX_WIFI) _wifi.pop_back();
    save();
}

void Vault::removeWiFiNetwork(const String& ssid) {
    for (size_t i = 0; i < _wifi.size(); i++)
        if (_wifi[i].ssid == ssid) { _wifi.erase(_wifi.begin() + i); save(); return; }
}

void Vault::markWiFiUsed(const String& ssid) {
    for (size_t i = 1; i < _wifi.size(); i++)
        if (_wifi[i].ssid == ssid) { saveWiFiNetwork(ssid, _wifi[i].pass); return; }
}

String Vault::getPendingUpdate() { return _pendingUpdate; }
void Vault::setPendingUpdate(const String& v) { if (v != _pendingUpdate) { _pendingUpdate = v; save(); } }
String Vault::getSkipVersion() { return _skipVersion; }
void Vault::setSkipVersion(const String& v) { if (v != _skipVersion) { _skipVersion = v; save(); } }

int Vault::getBrightness() { return _brightness; }
void Vault::setBrightness(int val) {
    _brightness = constrain(val, 0, 100);
    save();
}

String Vault::getTimezone() { return _timezone; }

bool Vault::isTouchCalibrated() { return _touchCalibrated; }

void Vault::setTouchCalibration(int16_t rawX1, int16_t rawY1, int16_t rawX2, int16_t rawY2) {
    _touchCalibrated = true;
    _touchRawX1 = rawX1;
    _touchRawY1 = rawY1;
    _touchRawX2 = rawX2;
    _touchRawY2 = rawY2;
    save();
}

void Vault::getTouchCalibration(int16_t &rawX1, int16_t &rawY1, int16_t &rawX2, int16_t &rawY2) {
    rawX1 = _touchRawX1;
    rawY1 = _touchRawY1;
    rawX2 = _touchRawX2;
    rawY2 = _touchRawY2;
}
