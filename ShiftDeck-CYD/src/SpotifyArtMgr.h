/*-------------------------------------------------------------------------------------------------
** SpotifyArtMgr.h - Album art download & cache manager
** Ported from original project, simplified
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <map>
#include <list>
#include <Arduino.h>
#include <HTTPClient.h>

#define SP_NO_COVER_JPG_FILENAME "/default_cover.jpg"

class SpotifyArtMgr {
public:
    static SpotifyArtMgr* getInstance();

    String acquireAlbumArt(const String& url);
    String getLocalFileName(const String& url);
    void saveCacheIndex();
    bool isCacheDirty();

private:
    SpotifyArtMgr();

    void   loadCacheIndex();
    bool   downloadFile(String url, String filename);
    String generateLocalFileName();
    void   evictOldestFile();

    static SpotifyArtMgr* gInstance;

    std::list<String>        _cacheList;
    std::map<String, String> _urlToFileMap;
    size_t                   _maxCacheSize = 8;
    size_t                   _nextFileId = 1;
    bool                     _isDirty = false;
};
