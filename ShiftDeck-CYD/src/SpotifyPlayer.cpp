/*-------------------------------------------------------------------------------------------------
** SpotifyPlayer.cpp - Spotify playback control implementation
**
** All HTTPS traffic happens on the refresh task (Core 1). Calling the API
** from the UI task used to corrupt lwIP pbufs (two TLS sessions tearing
** down concurrently), so the UI only enqueues PlayerCmd structs.
** ------------------------------------------------------------------------------------------------
*/
#include "SpotifyPlayer.h"
#include <LittleFS.h>
#include "config.h"
#include <esp_task_wdt.h>
#include "logTags.h"
#include "SCLogger.h"
#include "Vault.h"
#include "Monitor.h"
#include "iTunesProxy.h"
#include "CertBundle.h"
#include "Updater.h"

#include <HTTPClient.h>
#include <cstring>
#include <ctype.h>

SpotifyArduino spotify(client);
WiFiClientSecure client;

static const char* API = "https://api.spotify.com/v1";
#define CURRENT_COVER_FILE "/cover.jpg"

static String urlEncode(const String& str) {
    String encoded;
    char hex[4];
    for (size_t i = 0; i < str.length(); i++) {
        char c = str[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            encoded += hex;
        }
    }
    return encoded;
}

// Smallest image at least minPx wide (Spotify lists them largest first;
// custom playlist covers come without a width).
static String pickImage(JsonArrayConst imgs, int minPx) {
    String best;
    int bestW = INT32_MAX;
    for (JsonObjectConst im : imgs) {
        int w = im["width"] | 640;
        if (w >= minPx && w < bestW) { bestW = w; best = im["url"] | ""; }
    }
    if (best.isEmpty() && imgs.size() > 0) best = imgs[imgs.size() - 1]["url"] | "";
    return best;
}

static String countLabel(int n) {
    return String(n) + (n == 1 ? " música" : " músicas");
}

SpotifyPlayer::SpotifyPlayer() {}

SpotifyPlayer& SpotifyPlayer::getInstance() {
    static SpotifyPlayer instance;
    return instance;
}

void SpotifyPlayer::initialize(QueueHandle_t *pScuiQueue) {
    _pScuiQueue = pScuiQueue;
    _cmdQueue = xQueueCreate(6, sizeof(PlayerCmd));
    _spotifyClientId = Vault::getInstance().getSpotifyClientID();
    _spotifyClientSecret = Vault::getInstance().getSpotifyClientSecret();
    spotify.lateInit(_spotifyClientId.c_str(), _spotifyClientSecret.c_str(), "");
    // Verify against the root CA bundle, not a pinned chain: Spotify rotates
    // intermediates (pinning broke once), roots stay.
    client.setCACertBundle(x509_crt_bundle_start);
    client.setTimeout(5);
    // We refresh the token ourselves (ensureAccessToken + setBearerToken).
    spotify.autoTokenRefresh = false;

    // Thumbnail cache: clear it at boot (never while a view may be reading
    // a file) once it takes more than 60% of the filesystem.
    if (LittleFS.usedBytes() > LittleFS.totalBytes() * 6 / 10) {
        File root = LittleFS.open("/");
        std::vector<String> doomed;
        for (File f = root.openNextFile(); f; f = root.openNextFile()) {
            String n = f.name();
            if (n.startsWith("/")) n.remove(0, 1);
            if (n.startsWith("t_")) doomed.push_back("/" + n);
        }
        for (auto& n : doomed) LittleFS.remove(n);
        spLogI(LOGTAG_CACHE, "Thumbnail cache cleared (%u files)", (unsigned)doomed.size());
    }
}

void SpotifyPlayer::startBackgroundRefreshes() {
    spLogI(LOGTAG_MULTITASK, "Creating song refresh task on Core 1");
    xTaskCreatePinnedToCore(SpotifyPlayer::refreshCurrentSongTask, "SongRefresh", 10240, NULL, 1, &_refreshTaskHandle, 1);
}

bool SpotifyPlayer::isRefreshTokenAvailable() {
    _spotifyRefreshToken = Vault::getInstance().getSpotifyRefreshToken();
    return _spotifyRefreshToken.length() > 10;
}

bool SpotifyPlayer::login() {
    spotify.setRefreshToken(_spotifyRefreshToken.c_str());
    _spotifyAccessToken = "";
    _accessTokenExpiresAtMs = 0;
    bool ok = ensureAccessToken();
    spLogI(LOGTAG_PLAYER, "login: token refresh %s", ok ? "ok" : "FAILED");
    return ok;
}

bool SpotifyPlayer::ensureAccessToken() {
    if (_spotifyAccessToken.length() > 0 && millis() < _accessTokenExpiresAtMs) {
        spotify.setBearerToken(_spotifyAccessToken.c_str());
        return true;
    }
    WiFiClientSecure secureClient;
    secureClient.setCACertBundle(x509_crt_bundle_start);
    HTTPClient http;
    http.begin(secureClient, "https://accounts.spotify.com/api/token");
    http.useHTTP10(true);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // PKCE setups refresh with the Client ID alone; setups from before v1.3.2 still
    // hold a secret their refresh token was issued for. Encode every field.
    String body = "grant_type=refresh_token&refresh_token=" + urlEncode(_spotifyRefreshToken) +
                  "&client_id=" + urlEncode(_spotifyClientId);
    if (Vault::getInstance().hasSpotifyClientSecret()) body += "&client_secret=" + urlEncode(_spotifyClientSecret);
    int status = http.POST(body);
    if (status != 200) {
        spLogW(LOGTAG_PLAYER, "ensureAccessToken: status=%d", status);
        http.end();
        _apiOnline = false;
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) {
        spLogW(LOGTAG_PLAYER, "ensureAccessToken: JSON %s", err.c_str());
        return false;
    }
    _spotifyAccessToken = doc["access_token"].as<String>();
    // PKCE refresh tokens rotate: keep the new one or the next boot cannot log in.
    String rotated = doc["refresh_token"] | "";
    if (rotated.length() > 10 && rotated != _spotifyRefreshToken) {
        _spotifyRefreshToken = rotated;
        Vault::getInstance().setSpotifyRefreshToken(rotated);
        spotify.setRefreshToken(rotated.c_str());
    }
    int expiresIn = doc["expires_in"] | 3600;
    _accessTokenExpiresAtMs = millis() + (unsigned long)(expiresIn - 60) * 1000UL;
    if (_spotifyAccessToken.length() > 0) spotify.setBearerToken(_spotifyAccessToken.c_str());
    return _spotifyAccessToken.length() > 0;
}

// One Web API call on a fresh TLS client. HTTPClient (not the library's
// writer path, which failed on the first header byte) and HTTP/1.0 so the
// body is never chunked and can stream straight into ArduinoJson.
int SpotifyPlayer::api(const char* method, const String& url, const String& body, JsonDocument* out, JsonDocument* filter) {
    if (!ensureAccessToken()) return -1;
    esp_task_wdt_reset();
    WiFiClientSecure c;
    c.setCACertBundle(x509_crt_bundle_start);
    c.setTimeout(6);
    HTTPClient http;
    if (!http.begin(c, url)) return -1;
    http.useHTTP10(true);
    http.setTimeout(6000);
    http.addHeader("Authorization", "Bearer " + _spotifyAccessToken);
    int status;
    if (strcmp(method, "GET") == 0) {
        status = http.GET();
    } else {
        if (body.length()) http.addHeader("Content-Type", "application/json");
        else               http.addHeader("Content-Length", "0");   // Spotify answers 411 without it
        status = http.sendRequest(method, (uint8_t*)body.c_str(), body.length());
    }
    if (out && status == 200) {
        DeserializationError err = filter
            ? deserializeJson(*out, http.getStream(), DeserializationOption::Filter(*filter))
            : deserializeJson(*out, http.getStream());
        if (err) {
            spLogW(LOGTAG_PLAYER, "%s %s: JSON %s", method, url.c_str(), err.c_str());
            status = -2;
        }
    }
    http.end();
    _apiOnline = status > 0 && status < 500 && status != 401;
    spLogI(LOGTAG_PLAYER, "%s %s -> %d", method, url.c_str() + 26, status);
    esp_task_wdt_reset();
    return status;
}

/* ---------------------------------------------------------------- commands (UI side) */

void SpotifyPlayer::enqueue(CmdType type, const String& arg, const String& arg2, uint8_t kind) {
    PlayerCmd cmd = {};
    cmd.type = type;
    cmd.kind = kind;
    strlcpy(cmd.arg, arg.c_str(), sizeof(cmd.arg));
    strlcpy(cmd.arg2, arg2.c_str(), sizeof(cmd.arg2));
    if (!_cmdQueue || xQueueSend(_cmdQueue, &cmd, 0) != pdTRUE) {
        spLogW(LOGTAG_PLAYER, "command queue full, dropped %d", type);
        // Close the loop for whoever waits on this command.
        if (type <= CMD_PREV) postScuiMessage(SCUIMessageType::UM_ACTION_RESULT, "", 0);
        if (type == CMD_SEARCH)    { _searchBusy = false;    postScuiMessage(SCUIMessageType::UM_SEARCH_DONE, "", -1); }
        if (type == CMD_PLAYLISTS) { _playlistsBusy = false; postScuiMessage(SCUIMessageType::UM_PLAYLISTS_DONE, "", -1); }
        if (type == CMD_INSTALL_UPDATE) postScuiMessage(SCUIMessageType::UM_UPDATE_PROGRESS, "Tente de novo", -1);
    }
}

void SpotifyPlayer::nextSong()     { enqueue(CMD_NEXT); }
void SpotifyPlayer::previousSong() { enqueue(CMD_PREV); }
void SpotifyPlayer::pauseSong()    { enqueue(_isPlaying ? CMD_PAUSE : CMD_PLAY); }

void SpotifyPlayer::toggleLike() {
    String uri = getCurrentlyPlayingMetadata().trackUri;
    if (!uri.startsWith("spotify:track:")) return;
    bool want = !_liked;
    _liked = want;
    enqueue(CMD_LIKE, want ? "1" : "0", uri);
}

void SpotifyPlayer::playItem(const MediaItem& item) { enqueue(CMD_PLAY_ITEM, item.uri, item.contextUri, item.kind); }

void SpotifyPlayer::search(const String& query) {
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(1000))) {
        _searchResults.clear();
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    _searchBusy = true;
    enqueue(CMD_SEARCH, query);
}

void SpotifyPlayer::fetchPlaylists() {
    if (_playlistsBusy) return;
    _playlistsBusy = true;
    enqueue(CMD_PLAYLISTS);
}

void SpotifyPlayer::fetchDevices()                   { enqueue(CMD_DEVICES); }
void SpotifyPlayer::transferTo(const String& id)     { enqueue(CMD_TRANSFER, id); }
void SpotifyPlayer::checkForUpdate()                 { enqueue(CMD_CHECK_UPDATE); }
void SpotifyPlayer::setVolume(int percent)           { enqueue(CMD_VOLUME, String(constrain(percent, 0, 100))); }

int SpotifyPlayer::getActiveVolume() {
    int v = -1;
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(500))) {
        for (auto& d : _devices) if (d.active) v = d.volume;
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    return v;
}
void SpotifyPlayer::installUpdate()                  { enqueue(CMD_INSTALL_UPDATE); }

String SpotifyPlayer::getLatestVersion() {
    String v;
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(1000))) {
        v = _latestVersion;
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    return v;
}

void SpotifyPlayer::doCheckUpdate() {
    String v = Updater::latestVersion();
    if (v.length() && xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(1000))) {
        _latestVersion = v;
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    postScuiMessage(SCUIMessageType::UM_UPDATE_STATUS, v.c_str(), v.length() && Updater::isNewer(v.c_str(), VERSION) ? 1 : 0);
}

/* ---------------------------------------------------------------- commands (Core 1 side) */

bool SpotifyPlayer::execute(const PlayerCmd& cmd) {
    switch (cmd.type) {
        case CMD_PAUSE: case CMD_PLAY: case CMD_NEXT: case CMD_PREV: {
            static const char* const verbs[] = { "", "pause", "play", "next", "previous" };
            bool put = cmd.type == CMD_PAUSE || cmd.type == CMD_PLAY;
            String url = String(API) + "/me/player/" + verbs[cmd.type];
            int st = api(put ? "PUT" : "POST", url);
            if (st == 404 && cmd.type == CMD_PLAY) {           // no active device: wake one up
                doDevices();
                if (_devices.size()) st = api("PUT", url + "?device_id=" + _devices[0].id);
            }
            bool ok = st == 200 || st == 202 || st == 204;
            if (ok && put) _isPlaying = (cmd.type == CMD_PLAY);
            postScuiMessage(SCUIMessageType::UM_ACTION_RESULT, "", ok ? 1 : 0);
            if (!ok) postScuiMessage(SCUIMessageType::UM_TOAST, st == 404 ? "Nenhum dispositivo ativo" : "Spotify não respondeu");
            return true;
        }
        case CMD_LIKE: {
            bool want = cmd.arg[0] == '1';
            int st = api(want ? "PUT" : "DELETE", String(API) + "/me/library?uris=" + urlEncode(cmd.arg2));
            if (st != 200 && st != 204) {
                _liked = !want;
                postScuiMessage(SCUIMessageType::UM_LIKE_CHANGED);
                postScuiMessage(SCUIMessageType::UM_TOAST, "Não foi possível curtir");
            }
            return false;
        }
        case CMD_PLAY_ITEM:
            if (!doPlayItem(cmd.kind, cmd.arg, cmd.arg2)) postScuiMessage(SCUIMessageType::UM_TOAST, "Não foi possível tocar");
            return true;
        case CMD_SEARCH:    doSearch(cmd.arg); return false;
        case CMD_PLAYLISTS: doPlaylists();     return false;
        case CMD_DEVICES:   doDevices();       return false;
        case CMD_CHECK_UPDATE: doCheckUpdate(); return false;
        case CMD_VOLUME: {
            int st = api("PUT", String(API) + "/me/player/volume?volume_percent=" + cmd.arg);
            if (st == 200 || st == 202 || st == 204) {
                if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(500))) {
                    for (auto& d : _devices) if (d.active) d.volume = atoi(cmd.arg);
                    xSemaphoreGive(_xSemaphoreDataCopy);
                }
            } else {
                // 403: phones/TVs that only take volume from their own buttons
                postScuiMessage(SCUIMessageType::UM_TOAST, st == 404 ? "Nenhum dispositivo ativo" :
                                st == 403 ? "Este aparelho não aceita volume" : "Spotify não respondeu");
            }
            return false;
        }
        case CMD_INSTALL_UPDATE: {
            static int lastPct;
            lastPct = -1;
            String err = Updater::install([](int done, int total) {
                esp_task_wdt_reset();
                int pct = total > 0 ? (int)((int64_t)done * 100 / total) : 0;
                if (pct != lastPct) { lastPct = pct; getInstance().postScuiMessage(SCUIMessageType::UM_UPDATE_PROGRESS, "", pct); }
            });
            if (err.isEmpty()) {
                postScuiMessage(SCUIMessageType::UM_UPDATE_PROGRESS, "", 100);
                vTaskDelay(pdMS_TO_TICKS(1500));   // let "Reiniciando..." show
                ESP.restart();
            }
            postScuiMessage(SCUIMessageType::UM_UPDATE_PROGRESS, err.c_str(), -1);
            return false;
        }
        case CMD_TRANSFER: {
            String body = String("{\"device_ids\":[\"") + cmd.arg + "\"],\"play\":true}";
            int st = api("PUT", String(API) + "/me/player", body);
            if (st != 200 && st != 202 && st != 204) postScuiMessage(SCUIMessageType::UM_TOAST, "Não foi possível trocar");
            vTaskDelay(pdMS_TO_TICKS(600));   // Spotify needs a moment before the device list reflects it
            doDevices();
            return true;
        }
    }
    return false;
}

bool SpotifyPlayer::doPlayItem(uint8_t kind, const String& uri, const String& contextUri) {
    String body;
    if (kind == MediaItem::Track) {
        body = contextUri.length()
             ? "{\"context_uri\":\"" + contextUri + "\",\"offset\":{\"uri\":\"" + uri + "\"}}"
             : "{\"uris\":[\"" + uri + "\"]}";
    } else if (kind == MediaItem::Liked) {
        body = "{\"context_uri\":\"spotify:user:" + _userId + ":collection\"}";
    } else {
        body = "{\"context_uri\":\"" + uri + "\"}";
    }
    String url = String(API) + "/me/player/play";
    if (_activeDeviceId.isEmpty()) {
        doDevices();
        if (_devices.size()) url += "?device_id=" + _devices[0].id;
    }
    int st = api("PUT", url, body);
    if (kind == MediaItem::Liked && st != 202 && st != 204) {
        // The collection context is not accepted for every account; play the
        // most recent liked tracks instead.
        JsonDocument filter;
        filter["items"][0]["track"]["uri"] = true;
        JsonDocument doc;
        if (api("GET", String(API) + "/me/tracks?limit=50", "", &doc, &filter) == 200) {
            body = "{\"uris\":[";
            bool first = true;
            for (JsonObjectConst it : doc["items"].as<JsonArrayConst>()) {
                const char* u = it["track"]["uri"];
                if (!u) continue;
                body += first ? "\"" : ",\"";
                body += u;
                body += "\"";
                first = false;
            }
            body += "]}";
            st = api("PUT", url, body);
        }
    }
    return st == 200 || st == 202 || st == 204;
}

void SpotifyPlayer::doSearch(const String& query) {
    JsonDocument filter;
    for (const char* t : { "tracks", "artists", "albums", "playlists" }) {
        JsonObject it = filter[t]["items"].add<JsonObject>();
        it["name"] = true;
        it["uri"] = true;
        it["images"][0]["url"] = true;
        it["images"][0]["width"] = true;
        it["artists"][0]["name"] = true;
        it["owner"]["display_name"] = true;
        it["album"]["uri"] = true;
        it["album"]["images"][0]["url"] = true;
        it["album"]["images"][0]["width"] = true;
    }
    JsonDocument doc;
    // limit is per type (max 10 since Feb 2026); 3 each fills the screen.
    int st = api("GET", String(API) + "/search?type=track,artist,album,playlist&limit=3&market=" SPOTIFY_MARKET "&q=" + urlEncode(query),
                 "", &doc, &filter);

    std::vector<MediaItem> out;
    if (st == 200) {
        JsonArrayConst tr = doc["tracks"]["items"], ar = doc["artists"]["items"];
        JsonArrayConst al = doc["albums"]["items"], pl = doc["playlists"]["items"];
        // Round-robin keeps the best match of every type near the top.
        for (size_t i = 0; i < 3; i++) {
            if (i < tr.size()) {
                MediaItem m;
                m.kind = MediaItem::Track;
                m.name = tr[i]["name"] | "";
                m.subtitle = String("Música · ") + (tr[i]["artists"][0]["name"] | "");
                m.uri = tr[i]["uri"] | "";
                m.contextUri = tr[i]["album"]["uri"] | "";
                m.imageUrl = pickImage(tr[i]["album"]["images"], 40);
                out.push_back(m);
            }
            if (i < ar.size()) {
                MediaItem m;
                m.kind = MediaItem::Artist;
                m.name = ar[i]["name"] | "";
                m.subtitle = "Artista";
                m.uri = ar[i]["uri"] | "";
                m.imageUrl = pickImage(ar[i]["images"], 40);
                out.push_back(m);
            }
            if (i < al.size()) {
                MediaItem m;
                m.kind = MediaItem::Album;
                m.name = al[i]["name"] | "";
                m.subtitle = String("Álbum · ") + (al[i]["artists"][0]["name"] | "");
                m.uri = al[i]["uri"] | "";
                m.imageUrl = pickImage(al[i]["images"], 40);
                out.push_back(m);
            }
            if (i < pl.size() && !pl[i].isNull()) {   // Spotify returns null entries here
                MediaItem m;
                m.kind = MediaItem::Playlist;
                m.name = pl[i]["name"] | "";
                m.subtitle = "Playlist";
                m.uri = pl[i]["uri"] | "";
                m.imageUrl = pickImage(pl[i]["images"], 40);
                out.push_back(m);
            }
        }
    }
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(2000))) {
        _searchResults = out;
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    _searchBusy = false;
    postScuiMessage(SCUIMessageType::UM_SEARCH_DONE, "", st == 200 ? (int)out.size() : -1);
}

void SpotifyPlayer::doPlaylists() {
    std::vector<MediaItem> out;

    if (_userId.isEmpty()) {
        JsonDocument f, d;
        f["id"] = true;
        if (api("GET", String(API) + "/me", "", &d, &f) == 200) _userId = d["id"] | "";
    }
    {
        JsonDocument f, d;
        f["total"] = true;
        if (api("GET", String(API) + "/me/tracks?limit=1", "", &d, &f) == 200) {
            MediaItem liked;
            liked.kind = MediaItem::Liked;
            liked.name = "Curtidas";
            liked.subtitle = countLabel(d["total"] | 0);
            liked.uri = "liked";
            liked.thumb = 2;   // drawn as a gradient tile, no image
            out.push_back(liked);
        }
    }
    JsonDocument filter;
    JsonObject it = filter["items"].add<JsonObject>();
    it["name"] = true;
    it["uri"] = true;
    it["images"][0]["url"] = true;
    it["images"][0]["width"] = true;
    it["items"]["total"] = true;    // Feb 2026 rename of "tracks"
    it["tracks"]["total"] = true;   // deprecated, still sent
    JsonDocument doc;
    int st = api("GET", String(API) + "/me/playlists?limit=50", "", &doc, &filter);
    if (st == 200) {
        for (JsonObjectConst p : doc["items"].as<JsonArrayConst>()) {
            MediaItem m;
            m.kind = MediaItem::Playlist;
            m.name = p["name"] | "";
            m.uri = p["uri"] | "";
            m.subtitle = countLabel(p["items"]["total"] | (p["tracks"]["total"] | 0));
            m.imageUrl = pickImage(p["images"], 40);
            out.push_back(m);
        }
    }
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(2000))) {
        _playlists = out;
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    _playlistsBusy = false;
    postScuiMessage(SCUIMessageType::UM_PLAYLISTS_DONE, "", st == 200 ? (int)out.size() : -1);
}

void SpotifyPlayer::doDevices() {
    JsonDocument filter;
    JsonObject it = filter["devices"].add<JsonObject>();
    it["id"] = true;
    it["name"] = true;
    it["type"] = true;
    it["is_active"] = true;
    it["volume_percent"] = true;
    JsonDocument doc;
    if (api("GET", String(API) + "/me/player/devices", "", &doc, &filter) != 200) return;
    std::vector<DeviceInfo> list;
    String activeId;
    for (JsonObjectConst d : doc["devices"].as<JsonArrayConst>()) {
        DeviceInfo di;
        di.id = d["id"] | "";
        di.name = d["name"] | "";
        di.type = d["type"] | "";
        di.active = d["is_active"] | false;
        di.volume = d["volume_percent"] | -1;
        if (di.id.isEmpty()) continue;   // restricted devices have no id
        if (di.active) activeId = di.id;
        // active first, then phones: the default target when nothing is active
        if (di.active || (di.type == "Smartphone" && (list.empty() || !list[0].active))) list.insert(list.begin(), di);
        else list.push_back(di);
    }
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(2000))) {
        _devices = list;
        _activeDeviceId = activeId;
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    postScuiMessage(SCUIMessageType::UM_DEVICES_DONE, "", (int)list.size());
}

void SpotifyPlayer::checkLiked() {
    String uri = _currentlyPlayingMetadata.trackUri;
    bool liked = false;
    if (uri.startsWith("spotify:track:")) {
        JsonDocument doc;
        if (api("GET", String(API) + "/me/library/contains?uris=" + urlEncode(uri), "", &doc) == 200) liked = doc[0] | false;
    }
    _liked = liked;
    postScuiMessage(SCUIMessageType::UM_LIKE_CHANGED);
}

String SpotifyPlayer::thumbPath(const String& url) {
    uint32_t h = 2166136261u;   // FNV-1a
    for (size_t i = 0; i < url.length(); i++) { h ^= (uint8_t)url[i]; h *= 16777619u; }
    char buf[20];
    snprintf(buf, sizeof(buf), "/t_%08x.jpg", (unsigned)h);
    return buf;
}

// Downloads one missing list thumbnail. Returns false when nothing is pending.
bool SpotifyPlayer::fetchNextThumbnail() {
    String url;
    if (!xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(500))) return false;
    for (auto* list : { &_searchResults, &_playlists }) {
        for (auto& m : *list) {
            if (m.thumb != 0) continue;
            if (m.imageUrl.isEmpty()) { m.thumb = 2; continue; }
            url = m.imageUrl;
            break;
        }
        if (url.length()) break;
    }
    xSemaphoreGive(_xSemaphoreDataCopy);
    if (url.isEmpty()) return false;

    String path = thumbPath(url);
    bool ok, room;
    {
        FsLock fs;
        ok = fs.ok && LittleFS.exists(path);
        room = fs.ok && LittleFS.usedBytes() < LittleFS.totalBytes() * 8 / 10;
    }
    if (!ok && room) {
        // download beside, then rename: a view never opens a half-written file
        String tmp = path + ".tmp";
        ok = iTunesProxy::downloadJpeg(url, tmp);
        FsLock fs;
        ok = ok && fs.ok && LittleFS.rename(tmp, path);
        if (!ok && fs.ok) LittleFS.remove(tmp);
    }
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(500))) {
        for (auto* list : { &_searchResults, &_playlists })
            for (auto& m : *list)
                if (m.imageUrl == url) m.thumb = ok ? 1 : 2;
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    if (ok) postScuiMessage(SCUIMessageType::UM_THUMB_READY);
    return true;
}

/* ---------------------------------------------------------------- state */

bool SpotifyPlayer::isMusicAvailable() { return _isMusicAvailable; }
bool SpotifyPlayer::isApiOnline()      { return _apiOnline; }
bool SpotifyPlayer::isCoverArtReady()  { return _isCoverArtAvailable; }
bool SpotifyPlayer::isLiked()          { return _liked; }
bool SpotifyPlayer::isSearchBusy()     { return _searchBusy; }
bool SpotifyPlayer::isPlaylistsBusy()  { return _playlistsBusy; }
unsigned long SpotifyPlayer::getLastRefreshMs() { return _lastRefreshMs; }

std::vector<MediaItem> SpotifyPlayer::getSearchResults() {
    std::vector<MediaItem> copy;
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(1000))) {
        copy = _searchResults;
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    return copy;
}

std::vector<MediaItem> SpotifyPlayer::getPlaylists() {
    std::vector<MediaItem> copy;
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(1000))) {
        copy = _playlists;
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    return copy;
}

std::vector<DeviceInfo> SpotifyPlayer::getDevices() {
    std::vector<DeviceInfo> copy;
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(1000))) {
        copy = _devices;
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    return copy;
}

String SpotifyPlayer::deviceTypeLabel(const String& t) {
    if (t == "Smartphone") return "Celular";
    if (t == "Computer")   return "Computador";
    if (t == "Speaker")    return "Alto-falante";
    if (t == "Tablet")     return "Tablet";
    if (t == "TV")         return "TV";
    if (t == "Automobile") return "Carro";
    if (t == "CastAudio" || t == "CastVideo") return "Chromecast";
    if (t == "GameConsole") return "Console";
    return t.length() ? t : "Dispositivo";
}

String SpotifyPlayer::getActiveDeviceType() {
    for (auto& d : getDevices()) if (d.active) return deviceTypeLabel(d.type);
    return "";
}

String SpotifyPlayer::getActiveDeviceName() {
    for (auto& d : getDevices()) if (d.active) return d.name;
    return "";
}

const PlayingMetadata& SpotifyPlayer::getCurrentlyPlayingMetadata() {
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(5000))) {
        _currentlyPlayingMetadataDTO.copyFrom(_currentlyPlayingMetadata);
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    return _currentlyPlayingMetadataDTO;
}

/* ---------------------------------------------------------------- polling */

void SpotifyPlayer::refreshCurrentTrack() {
    int status = 0;
    if (xSemaphoreTake(_xSemaphoreNetwork, pdMS_TO_TICKS(5000))) {
        if (ensureAccessToken()) {
            Monitor::start(MONITOR_ID_SPOTIFY_GET_CURRENTLY_PLAYING, LOGTAG_METRICS, "getCurrentlyPlaying");
            status = spotify.getCurrentlyPlaying(SpotifyPlayer::getCurrentlyPlayingCallback, SPOTIFY_MARKET);
            Monitor::stop(MONITOR_ID_SPOTIFY_GET_CURRENTLY_PLAYING);
        }
        xSemaphoreGive(_xSemaphoreNetwork);
    }
    _apiOnline = (status == 200 || status == 204);
    if (status == 204) _isMusicAvailable = false;   // no active session anywhere
    _lastRefreshMs = millis();
    postScuiMessage(SCUIMessageType::UM_PLAYER_REFRESH);
}

void SpotifyPlayer::refreshCurrentSong(CurrentlyPlaying cp) {
    if (cp.currentlyPlayingType != SpotifyPlayingType::track && cp.currentlyPlayingType != SpotifyPlayingType::episode) {
        _isMusicAvailable = false;
        return;
    }
    if (xSemaphoreTake(_xSemaphoreDataCopy, pdMS_TO_TICKS(5000))) {
        if (cp.trackUri != nullptr && strcmp(cp.trackUri, _currentlyPlayingMetadata.trackUri) != 0) {
            _isNewTrack = true;
            _isCoverArtAvailable = false;   // before the UI hears about the new track
        }
        _currentlyPlayingMetadata.copyFrom(cp);
        xSemaphoreGive(_xSemaphoreDataCopy);
    }
    _isPlaying = cp.isPlaying;
    _isMusicAvailable = (cp.trackName != nullptr);
    if (_isNewTrack && cp.trackName) spLogI(LOGTAG_SONG_DATA, "Track: %s", cp.trackName);
}

void SpotifyPlayer::refreshCoverArt() {
    esp_task_wdt_reset();
    if (_currentTrackUri != _currentlyPlayingMetadata.trackUri) {
        _isCoverArtAvailable = false;
        _currentTrackUri = _currentlyPlayingMetadata.trackUri;
        // Never remove /cover.jpg here: the UI may be decoding it (LittleFS
        // asserts on removing an open file). downloadJpeg truncates it.
    }
    if (_isCoverArtAvailable || !_isMusicAvailable) return;

    bool attempted = false;
    // Spotify sends 640/300/64 px: take the smallest >= 120 that is <= 300
    // (bigger JPEGs exhaust the heap on a board without PSRAM).
    int bestIdx = -1, bestPixels = INT32_MAX;
    for (int i = 0; i < _currentlyPlayingMetadata.numImages; i++) {
        int h = _currentlyPlayingMetadata.albumImages[i].height;
        int w = _currentlyPlayingMetadata.albumImages[i].width;
        if (h < 64 || w < 64 || h > 300 || w > 300) continue;
        if (h * w < bestPixels) { bestPixels = h * w; bestIdx = i; }
        if (h >= 120 && w >= 120) break;
    }
    if (bestIdx >= 0) {
        attempted = true;
        _isCoverArtAvailable = iTunesProxy::downloadJpeg(_currentlyPlayingMetadata.albumImages[bestIdx].url, CURRENT_COVER_FILE);
    }
    esp_task_wdt_reset();
    if (!_isCoverArtAvailable) {
        String artist(_currentlyPlayingMetadata.artistName), album(_currentlyPlayingMetadata.albumName);
        if (artist.length() && album.length()) {
            String itunesUrl = iTunesProxy::fetchArtworkUrl(artist, album);
            if (itunesUrl.length()) {
                attempted = true;
                _isCoverArtAvailable = iTunesProxy::downloadJpeg(itunesUrl, CURRENT_COVER_FILE);
            }
        }
    }
    if (attempted) postScuiMessage(SCUIMessageType::UM_MARK_DIRTY, "", _isCoverArtAvailable ? 1 : 0);
}

void SpotifyPlayer::postScuiMessage(SCUIMessageType type, const char* str, int num) {
    if (_pScuiQueue == NULL || *_pScuiQueue == NULL) return;
    SCUIMessage msg;
    msg.type = type;
    strlcpy(msg.str, str ? str : "", sizeof(msg.str));
    msg.num = num;
    xQueueSend(*_pScuiQueue, &msg, pdMS_TO_TICKS(10));
}

void SpotifyPlayer::getCurrentlyPlayingCallback(CurrentlyPlaying currentlyPlaying) {
    SpotifyPlayer::getInstance().refreshCurrentSong(currentlyPlaying);
}

void SpotifyPlayer::refreshCurrentSongTask(void *pvParameters) {
    SpotifyPlayer& p = SpotifyPlayer::getInstance();
    unsigned long lastDevicesMs = 0, lastUpdateCheckMs = 0;
    bool updateChecked = false;
    for (;;) {
        esp_task_wdt_reset();
        p.refreshCurrentTrack();
        p.refreshCoverArt();
        if (p._isNewTrack) {
            p._isNewTrack = false;
            p.checkLiked();
        }
        if (lastDevicesMs == 0 || millis() - lastDevicesMs > 30000) {
            p.doDevices();
            lastDevicesMs = millis();
        }
        if (!updateChecked || millis() - lastUpdateCheckMs > UPDATE_CHECK_MS) {
            p.doCheckUpdate();
            lastUpdateCheckMs = millis();
            updateChecked = true;
        }

        // Until the next poll: run queued commands first, then fetch
        // thumbnails, otherwise sleep on the queue. Playback commands cut
        // the wait short so the UI resyncs right away.
        unsigned long until = millis() + 3000;
        while ((long)(until - millis()) > 0) {
            PlayerCmd cmd;
            if (xQueueReceive(p._cmdQueue, &cmd, 0) == pdTRUE) {
                if (p.execute(cmd)) break;
                continue;
            }
            if (p.fetchNextThumbnail()) continue;
            long left = (long)(until - millis());
            if (left > 0 && xQueueReceive(p._cmdQueue, &cmd, pdMS_TO_TICKS(left)) == pdTRUE && p.execute(cmd)) break;
        }
    }
}
