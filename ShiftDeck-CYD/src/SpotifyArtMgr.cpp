/*-------------------------------------------------------------------------------------------------
** SpotifyArtMgr.cpp - Album art download & cache manager implementation
** Ported from original project, simplified for smaller flash
** ------------------------------------------------------------------------------------------------
*/
#include "SpotifyArtMgr.h"
#include "logTags.h"
#include "SCLogger.h"
#include "Monitor.h"
#include <LittleFS.h>

extern const char* gCombinedCerts;

SpotifyArtMgr* SpotifyArtMgr::gInstance = nullptr;

SpotifyArtMgr::SpotifyArtMgr() {}

SpotifyArtMgr* SpotifyArtMgr::getInstance() {
    if (gInstance == nullptr) {
        gInstance = new SpotifyArtMgr();
        gInstance->loadCacheIndex();
    }
    return gInstance;
}

String SpotifyArtMgr::getLocalFileName(const String& url) {
    auto it = _urlToFileMap.find(url);
    if (it != _urlToFileMap.end()) return it->second;
    return SP_NO_COVER_JPG_FILENAME;
}

String SpotifyArtMgr::acquireAlbumArt(const String& url) {
    auto it = _urlToFileMap.find(url);
    if (it != _urlToFileMap.end()) {
        _cacheList.remove(url);
        _cacheList.push_front(url);
        _isDirty = true;
        return it->second;
    }

    String localFileName = generateLocalFileName();
    if (!downloadFile(url, localFileName)) {
        return SP_NO_COVER_JPG_FILENAME;
    }

    if (_cacheList.size() >= _maxCacheSize) evictOldestFile();

    _cacheList.push_front(url);
    _urlToFileMap[url] = localFileName;
    _isDirty = true;
    return localFileName;
}

String SpotifyArtMgr::generateLocalFileName() {
    if (_cacheList.size() >= _maxCacheSize) {
        String oldestUrl = _cacheList.back();
        auto it = _urlToFileMap.find(oldestUrl);
        if (it != _urlToFileMap.end()) return it->second;
    }
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "/art_%03zu.jpg", _nextFileId++);
    return String(buffer);
}

void SpotifyArtMgr::evictOldestFile() {
    if (_cacheList.empty()) return;
    String oldestUrl = _cacheList.back();
    _cacheList.pop_back();
    _urlToFileMap.erase(oldestUrl);
}

bool SpotifyArtMgr::downloadFile(String url, String filename) {
    spLogD(LOGTAG_CACHE, "Downloading: %s -> %s", url.c_str(), filename.c_str());

    HTTPClient http;
    WiFiClientSecure wifiClient;
    // Album art is public, non-sensitive image data. Spotify's image CDN (i.scdn.co)
    // cert chain isn't reliably pinned by gCombinedCerts, causing silent TLS failures
    // and art never displaying. Use setInsecure() here instead of cert pinning.
    wifiClient.setInsecure();

    http.begin(wifiClient, url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        File f = LittleFS.open(filename, "w");
        if (f) {
            size_t written = http.writeToStream(&f);
            f.close();
            http.end();
            if (written == 0) {
                spLogW(LOGTAG_CACHE, "Downloaded cover art was empty, removing: %s", filename.c_str());
                LittleFS.remove(filename);
                return false;
            }
            spLogI(LOGTAG_CACHE, "Downloaded cover art: %s (%u bytes)", filename.c_str(), (unsigned)written);
            return true;
        }
        spLogW(LOGTAG_CACHE, "Failed to open file for writing: %s", filename.c_str());
        http.end();
        return false;
    }

    spLogW(LOGTAG_CACHE, "Download failed: HTTP %d, url=%s", httpCode, url.c_str());
    http.end();
    return false;
}

void SpotifyArtMgr::loadCacheIndex() {
    if (!LittleFS.exists("/cache_idx.txt")) return;
    File f = LittleFS.open("/cache_idx.txt", "r");
    if (!f) return;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        int sep = line.indexOf('|');
        if (sep > 0) {
            String url = line.substring(0, sep);
            String file = line.substring(sep + 1);
            _urlToFileMap[url] = file;
            _cacheList.push_back(url);
        }
    }
    f.close();
}

void SpotifyArtMgr::saveCacheIndex() {
    if (!_isDirty) return;
    Monitor::start(MONITOR_ID_SPOTIFY_IMAGE_CACHE_SAVE, LOGTAG_CACHE, "saveCacheIndex()");
    File f = LittleFS.open("/cache_idx.txt", "w");
    if (!f) return;
    for (const auto& url : _cacheList) {
        f.println(url + "|" + _urlToFileMap[url]);
    }
    f.close();
    _isDirty = false;
    Monitor::stop(MONITOR_ID_SPOTIFY_IMAGE_CACHE_SAVE);
}

bool SpotifyArtMgr::isCacheDirty() { return _isDirty; }
