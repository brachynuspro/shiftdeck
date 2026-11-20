/*-------------------------------------------------------------------------------------------------
** PlayingMetadata.h - Data transfer object for track metadata
** Ported from original project. Uses SpotifyArduino structs.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>
#include <SpotifyArduino.h>

class PlayingMetadata {
public:
    char trackName[128];
    char trackUri[128];
    char albumName[128];
    char artistName[256];
    int numImages;
    SpotifyImage albumImages[4];
    // Buffers that *own* the URL strings copied out of the library's
    // CurrentlyPlaying struct. SpotifyImage::url only holds `const
    // char*` pointers, so we can't just strdup into the heap (it would
    // leak on every copyFrom). Instead, every albumImages[i].url points
    // into imageUrlBuffers[i] below. 256 bytes per URL is enough for
    // the longest realistic i.scdn.co URL (~70 chars); truncation is
    // safe (the cover-art fetch will just fail gracefully and the
    // iTunes fallback will run).
    char imageUrlBuffers[4][256];
    int durationMs;
    int progressMs;
    bool isPlaying;

    PlayingMetadata();
    void copyFrom(const CurrentlyPlaying& cp);
    void copyFrom(const PlayingMetadata& other);
    void clear();
};
