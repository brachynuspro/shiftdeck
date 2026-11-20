/*-------------------------------------------------------------------------------------------------
** iTunesProxy.h - iTunes Search API proxy for album artwork fallback
**
** When the Spotify image CDN (i.scdn.co) fails to deliver album art
** reliably on ESP32, this module queries the public iTunes Search API
** for the same artist + album, returns a direct artwork URL, and
** downloads the JPEG to LittleFS.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>

// LittleFS asserts (lfs_file_read flags) when the UI task decodes a JPEG
// while the player task writes a download: both hold this while touching
// LittleFS. Recursive so nested helpers can take it again.
struct FsLock {
    static SemaphoreHandle_t mutex() { static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex(); return m; }
    const bool ok;
    explicit FsLock(TickType_t wait = pdMS_TO_TICKS(2000)) : ok(xSemaphoreTakeRecursive(mutex(), wait) == pdTRUE) {}
    ~FsLock() { if (ok) xSemaphoreGiveRecursive(mutex()); }
};

class iTunesProxy {
public:
    // Query iTunes Search API for an album and return a high-res
    // artwork URL. Empty string on failure.
    static String fetchArtworkUrl(const String& artist, const String& album);

    // Download a JPEG from any HTTPS URL into LittleFS. Returns true
    // on success.
    static bool downloadJpeg(const String& url, const String& filename);
};
