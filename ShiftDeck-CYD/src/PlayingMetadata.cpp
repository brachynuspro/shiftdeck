/*-------------------------------------------------------------------------------------------------
** PlayingMetadata.cpp - DTO for track metadata
** ------------------------------------------------------------------------------------------------
*/
#include "PlayingMetadata.h"
#include <cstring>

PlayingMetadata::PlayingMetadata() {
    clear();
}

void PlayingMetadata::clear() {
    memset(trackName, 0, sizeof(trackName));
    memset(trackUri, 0, sizeof(trackUri));
    memset(albumName, 0, sizeof(albumName));
    memset(artistName, 0, sizeof(artistName));
    numImages = 0;
    durationMs = 0;
    progressMs = 0;
    isPlaying = false;
    for (int i = 0; i < 4; i++) {
        // Make every albumImages[i].url point at our owned buffer, NOT
        // at whatever happens to be at the pointer's old address.
        // refreshCoverArt() reads albumImages[i].url as a C string, so
        // a nullptr there would also be fine — we just give it a
        // stable target.
        albumImages[i].url = imageUrlBuffers[i];
        imageUrlBuffers[i][0] = '\0';
        albumImages[i].height = 0;
        albumImages[i].width = 0;
    }
}

void PlayingMetadata::copyFrom(const CurrentlyPlaying& cp) {
    if (cp.trackName) strncpy(trackName, cp.trackName, sizeof(trackName) - 1);
    if (cp.trackUri) strncpy(trackUri, cp.trackUri, sizeof(trackUri) - 1);
    if (cp.albumName) strncpy(albumName, cp.albumName, sizeof(albumName) - 1);

    artistName[0] = '\0';
    for (int i = 0; i < cp.numArtists && i < SPOTIFY_MAX_NUM_ARTISTS; i++) {
        if (i > 0) strncat(artistName, ", ", sizeof(artistName) - strlen(artistName) - 1);
        if (cp.artists[i].artistName) {
            strncat(artistName, cp.artists[i].artistName, sizeof(artistName) - strlen(artistName) - 1);
        }
    }

    numImages = cp.numImages < 4 ? cp.numImages : 4;
    for (int i = 0; i < numImages; i++) {
        albumImages[i].height = cp.albumImages[i].height;
        albumImages[i].width = cp.albumImages[i].width;
        // cp.albumImages[i].url is a const char* that points into the
        // library's DynamicJsonDocument. That document is local to
        // SpotifyArduino::getCurrentlyPlaying() and is destroyed the
        // moment that function returns. Storing the raw pointer would
        // be a use-after-free (the original cause of the boot loop
        // and the empty URL the cover-art fetcher was logging).
        // Copy the string into imageUrlBuffers[i] and point url at it.
        if (cp.albumImages[i].url != nullptr) {
            strncpy(imageUrlBuffers[i], cp.albumImages[i].url, sizeof(imageUrlBuffers[i]) - 1);
            imageUrlBuffers[i][sizeof(imageUrlBuffers[i]) - 1] = '\0';
        } else {
            imageUrlBuffers[i][0] = '\0';
        }
        albumImages[i].url = imageUrlBuffers[i];
    }

    durationMs = cp.durationMs;
    progressMs = cp.progressMs;
    isPlaying = cp.isPlaying;
}

void PlayingMetadata::copyFrom(const PlayingMetadata& other) {
    strncpy(trackName, other.trackName, sizeof(trackName) - 1);
    strncpy(trackUri, other.trackUri, sizeof(trackUri) - 1);
    strncpy(albumName, other.albumName, sizeof(albumName) - 1);
    strncpy(artistName, other.artistName, sizeof(artistName) - 1);
    numImages = other.numImages;
    for (int i = 0; i < numImages; i++) {
        albumImages[i].height = other.albumImages[i].height;
        albumImages[i].width = other.albumImages[i].width;
        // other.albumImages[i].url already points at other.imageUrlBuffers[i]
        // which is owned storage — safe to copy the pointer over.
        albumImages[i].url = imageUrlBuffers[i];
        strncpy(imageUrlBuffers[i], other.imageUrlBuffers[i], sizeof(imageUrlBuffers[i]) - 1);
        imageUrlBuffers[i][sizeof(imageUrlBuffers[i]) - 1] = '\0';
    }
    durationMs = other.durationMs;
    progressMs = other.progressMs;
    isPlaying = other.isPlaying;
}
