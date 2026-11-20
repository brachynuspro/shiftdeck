/*-------------------------------------------------------------------------------------------------
** Updater.h - Over-the-air updates from the public web installer (UPDATE_BASE_URL)
**
** The flash has two app slots (partitions/cyd_4mb.csv): the new image is written to the
** idle slot while the current one keeps running, and only becomes the boot slot once it
** downloaded completely and its checksum matched. LittleFS (Wi-Fi, account) is untouched.
** Both calls block for seconds: run them on the player task, never on the UI task.
** ------------------------------------------------------------------------------------------------
*/
#pragma once
#include <Arduino.h>
#include <stdio.h>

namespace Updater {
    // Version published in manifest-update.json, "" when it could not be read.
    String latestVersion();
    // Downloads and flashes the app image. progress(done, total). "" on success, else the reason.
    String install(void (*progress)(int done, int total));

    // "1.2.10" > "1.2.9": compares major.minor.patch numerically.
    inline bool isNewer(const char* latest, const char* current) {
        int a[3] = {0, 0, 0}, b[3] = {0, 0, 0};
        if (sscanf(latest, "%d.%d.%d", &a[0], &a[1], &a[2]) < 2) return false;
        sscanf(current, "%d.%d.%d", &b[0], &b[1], &b[2]);
        for (int i = 0; i < 3; i++) if (a[i] != b[i]) return a[i] > b[i];
        return false;
    }
}
