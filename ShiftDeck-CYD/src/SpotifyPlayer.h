/*-------------------------------------------------------------------------------------------------
** SpotifyPlayer.h - Spotify playback control, library, search and devices
**
** Threading: every HTTPS call runs on the refresh task (Core 1). The UI
** (Core 0) only queues commands (nextSong, search, fetchPlaylists, ...) and
** reads thread-safe snapshots; results arrive as SCUI messages.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <SpotifyArduino.h>
#include <ArduinoJson.h>
#include "PlayingMetadata.h"
#include "scui.h"
#include <vector>

struct MediaItem {
    enum Kind : uint8_t { Track, Artist, Album, Playlist, Liked };
    Kind    kind = Track;
    String  name;
    String  subtitle;     // ready to display ("312 músicas", "Música · Xamã")
    String  uri;
    String  contextUri;   // tracks: album to play them in
    String  imageUrl;
    uint8_t thumb = 0;    // 0 = not fetched yet, 1 = cached on LittleFS, 2 = none/failed
};

struct DeviceInfo {
    String id;
    String name;
    String type;          // Spotify type: Smartphone, Computer, Speaker, ...
    bool   active = false;
    int    volume = -1;   // volume_percent, -1 if the device does not report it
};

class SpotifyPlayer {
public:
    static SpotifyPlayer& getInstance();

    void initialize(QueueHandle_t *pScuiQueue);
    bool isRefreshTokenAvailable();
    bool login();                       // false if the token refresh failed
    void startBackgroundRefreshes();
    void refreshCurrentTrack();         // also callable once at boot (before the task)

    // ---- commands (queued, executed on Core 1) ----
    void nextSong();
    void previousSong();
    void pauseSong();                   // toggles play/pause
    void toggleLike();                  // optimistic: isLiked() flips immediately
    void playItem(const MediaItem& item);
    void search(const String& query);   // -> UM_SEARCH_DONE
    void fetchPlaylists();              // -> UM_PLAYLISTS_DONE
    void fetchDevices();                // -> UM_DEVICES_DONE
    void transferTo(const String& deviceId);

    // ---- state (safe to call from the UI task) ----
    bool isMusicAvailable();
    bool isApiOnline();                 // last Spotify call answered
    bool isCoverArtReady();             // /cover.jpg belongs to the current track
    bool isLiked();
    bool isSearchBusy();
    bool isPlaylistsBusy();
    std::vector<MediaItem> getSearchResults();
    std::vector<MediaItem> getPlaylists();
    std::vector<DeviceInfo> getDevices();
    String getActiveDeviceType();       // localized ("Celular"), "" if none
    String getActiveDeviceName();
    const PlayingMetadata& getCurrentlyPlayingMetadata();
    unsigned long getLastRefreshMs();

    // Firmware updates (Updater.h), run on the player task.
    void checkForUpdate();              // posts UM_UPDATE_STATUS
    void installUpdate();               // posts UM_UPDATE_PROGRESS; restarts on success
    String getLatestVersion();          // "" until a check succeeded

    // Spotify refused the account (403): not in the app's User Management, or not Premium.
    enum : uint8_t { ACCOUNT_OK = 0, ACCOUNT_NOT_ALLOWED = 1, ACCOUNT_NOT_PREMIUM = 2 };
    int getAccountIssue() { return _accountIssue; }

    void setVolume(int percent);        // active device, 0..100
    int getActiveVolume();              // -1 when no active device reports one

    static String thumbPath(const String& url);     // LittleFS path of a cached thumbnail
    static String deviceTypeLabel(const String& spotifyType);

private:
    SpotifyPlayer();

    enum CmdType : uint8_t { CMD_PAUSE = 1, CMD_PLAY, CMD_NEXT, CMD_PREV, CMD_LIKE, CMD_PLAY_ITEM,
                             CMD_SEARCH, CMD_PLAYLISTS, CMD_DEVICES, CMD_TRANSFER,
                             CMD_CHECK_UPDATE, CMD_INSTALL_UPDATE, CMD_VOLUME };
    struct PlayerCmd {
        CmdType type;
        uint8_t kind;
        char    arg[96];
        char    arg2[96];
    };
    void enqueue(CmdType type, const String& arg = "", const String& arg2 = "", uint8_t kind = 0);
    bool execute(const PlayerCmd& cmd);        // true -> resync playback right away

    int  api(const char* method, const String& url, const String& body = "",
             JsonDocument* out = nullptr, JsonDocument* filter = nullptr);
    bool ensureAccessToken();
    void doSearch(const String& query);
    void doPlaylists();
    void doDevices();
    bool doPlayItem(uint8_t kind, const String& uri, const String& contextUri);
    void checkLiked();
    void doCheckUpdate();
    bool fetchNextThumbnail();

    QueueHandle_t *_pScuiQueue = nullptr;
    QueueHandle_t _cmdQueue = nullptr;
    String _spotifyRefreshToken;
    String _spotifyAccessToken;
    String _spotifyClientId;
    String _spotifyClientSecret;
    String _currentTrackUri;
    String _userId;
    String _activeDeviceId;
    String _latestVersion;      // guarded by _xSemaphoreDataCopy
    bool _isPlaying = false;
    bool _isCoverArtAvailable = false;
    bool _isMusicAvailable = false;
    bool _isNewTrack = false;
    volatile bool _apiOnline = false;
    volatile bool _liked = false;
    volatile bool _searchBusy = false;
    volatile bool _playlistsBusy = false;
    volatile uint8_t _accountIssue = ACCOUNT_OK;

    PlayingMetadata _currentlyPlayingMetadata;
    PlayingMetadata _currentlyPlayingMetadataDTO;
    std::vector<MediaItem> _searchResults;
    std::vector<MediaItem> _playlists;
    std::vector<DeviceInfo> _devices;

    SemaphoreHandle_t _xSemaphoreNetwork = xSemaphoreCreateMutex();
    SemaphoreHandle_t _xSemaphoreDataCopy = xSemaphoreCreateMutex();
    TaskHandle_t _refreshTaskHandle = nullptr;

    unsigned long _lastRefreshMs = 0;
    unsigned long _accessTokenExpiresAtMs = 0;

    static void getCurrentlyPlayingCallback(CurrentlyPlaying currentlyPlaying);
    static void refreshCurrentSongTask(void *pvParameters);

    void refreshCurrentSong(CurrentlyPlaying currentlyPlaying);
    void refreshCoverArt();
    void postScuiMessage(SCUIMessageType type, const char* str = "", int num = 0);
};

// Global SpotifyArduino instance (needs client reference at init)
extern SpotifyArduino spotify;
extern WiFiClientSecure client;
