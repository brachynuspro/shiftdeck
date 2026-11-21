/*-------------------------------------------------------------------------------------------------
** scui.h - SCUI messaging (FreeRTOS queue types)
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>

enum class SCUIMessageType : uint8_t {
    UM_IDLE            = 0,
    UM_STATUS_BOX      = 1,
    UM_DOWNLOAD_BOX    = 2,
    UM_MARK_DIRTY      = 3,   // cover art changed -> full redraw
    UM_PLAYER_REFRESH  = 4,   // playback state polled
    UM_BOOT_CHECK      = 5,
    UM_NEW_DATA        = 6,
    UM_ACTION_RESULT   = 7,   // queued player action finished; num = 1 ok / 0 failed
    UM_SEARCH_DONE     = 8,
    UM_PLAYLISTS_DONE  = 9,
    UM_DEVICES_DONE    = 10,
    UM_THUMB_READY     = 11,  // a list thumbnail landed on LittleFS
    UM_LIKE_CHANGED    = 12,
    UM_TOAST           = 13,  // str = short user-facing message
    UM_UPDATE_STATUS   = 14,  // str = latest version ("" = check failed), num = 1 if newer than VERSION
    UM_UPDATE_PROGRESS = 15,  // num = 0..100, or -1 = failed (str = reason)
    UM_QUEUE_DONE      = 16   // up-next list and shuffle/repeat state read; num = count, -1 = failed
};

// Plain-old-data on purpose: FreeRTOS queues copy messages with memcpy,
// which would duplicate (and later double-free) a String's heap buffer.
struct SCUIMessage {
    SCUIMessageType type;
    char            str[48];
    int             num;
};
