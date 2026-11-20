/*-------------------------------------------------------------------------------------------------
** Monitor.h - Performance monitoring & heap tracking
** Ported from original project, simplified
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>
#include "SCLogger.h"

#define MONITOR_ID_SPOTIFY_GET_CURRENTLY_PLAYING  1
#define MONITOR_ID_FETCH_ALBUM_ART                2
#define MONITOR_ID_SPOTIFY_IMAGE_CACHE_LOAD       3
#define MONITOR_ID_SPOTIFY_IMAGE_CACHE_SAVE       4
#define MONITOR_ID_SPOTIFY_IMAGE_FILE_LOAD        5
#define MONITOR_ID_SCUI_QUEUE_DELAY               6

class Monitor {
public:
    static void registerDescription(int id, const char* desc);
    static void start(int id, const char* tag, const char* msg);
    static void stop(int id);
    static bool watchHeap(uint32_t threshold);
    static uint32_t getFreeHeap();
    static void dumpStats(const char* tag);
    static void logUptime(const char* tag);
    static void watchQueue(QueueHandle_t queue, int threshold);

private:
    struct Metric {
        const char* description;
        unsigned long startTime;
        unsigned long totalTime;
        int count;
        int maxTime;
    };
    static Metric _metrics[10];
    static unsigned long _bootTime;
};
