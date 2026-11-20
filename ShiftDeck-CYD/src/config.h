/*-------------------------------------------------------------------------------------------------
** config.h - ShiftDeck Theme & Layout Constants
** 8px grid system adapted for 320x240 ILI9341
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>

#define APP_NAME "ShiftDeck"
#define VERSION  "1.3.2"
#define VERSION_SHORT "v1.3"
#define BOARD_NAME "ESP32-2432S028"

// Updates come from the public web installer: manifest-update.json ("version")
// and firmware/shiftdeck-app.bin (app image only), published by tools/release.sh.
#define UPDATE_BASE_URL "https://brachynuspro.github.io/shiftdeck-flasher/"
#define UPDATE_CHECK_MS (6UL * 3600UL * 1000UL)
// Phone guide to create the Spotify app (flasher/guia.html). No ".html": keeps the QR at version 3.
#define GUIDE_URL UPDATE_BASE_URL "guia"
// Spotify login return page (flasher/callback.html): forwards the code to http://<board IP>/code.
// Must be registered, exactly, as a Redirect URI of the user's Spotify app.
#define OAUTH_REDIRECT_URI UPDATE_BASE_URL "callback.html"

#define SPOTIFY_MARKET "BR"
#define SPOTIFY_ESPOTIFIER_NODE_NAME "shiftdeck"
#define SPOTIFY_REFRESH_TOKEN_FILE_NAME "/rfsh_tkn.txt"

#define CLOCK_TASK_INTERVAL_MILLIS 3600000UL
#define HEAP_LOW_THRESHOLD 60000

/* =========================================================================
   THEME COLORS (RGB565) -- sampled from the IMGS/ mockups
   ========================================================================= */
#define C_BG          0x1082  // #111316 screen background
#define C_CARD        0x18E4  // #1E1F23 cards / panels
#define C_BUTTON      0x2145  // #27292E secondary buttons, rows
#define C_DIVIDER     0x2966  // #2C2E33 hairlines, list dividers
#define C_TRACK       0x39E8  // #3A3D42 progress track (unfilled)
#define C_GREEN       0x1EAC  // #1ED760 Spotify green (accent)
#define C_GREEN_DARK  0x1CE9  // #1F9D48 primary button fill (white text)
#define C_WHITE       0xFFFF
#define C_SECONDARY   0xB596  // #B3B3B3 secondary text
#define C_MUTED       0x8C72  // #8A8D93 tertiary text, inactive icons
#define C_ERROR       0xF2ED  // #F15E6C errors
#define C_WARN        0xF524  // #F5A524 warnings / timeouts
#define C_ON_GREEN    0x0861  // #0B0C0E text on a green fill
#define C_LIKED_TOP   0x51DD  // #5038E8 "Curtidas" tile gradient
#define C_LIKED_BOT   0x3916  // #3A22B0

// Setup screens (WiFi / Spotify config / OAuth) info bars.
#define C_TERMINAL_BG     C_GREEN
#define C_TERMINAL_BORDER C_BG

/* =========================================================================
   LAYOUT GRID (8px grid, 12px margins)
   ========================================================================= */

#define GRID          8
#define MARGIN        12
#define SCREEN_W      320
#define SCREEN_H      240
#define CENTER_X      160
#define CENTER_Y      120

#define STATUS_BAR_H  24
#define STATUS_H      24
#define PROGRESS_BAR_H 8
#define PROGRESS_BAR_R 4

// Inset (px) from each screen edge used for the two touch calibration target
// points. Shared between TouchCalibration.cpp (where the targets are drawn)
// and XPT2046Touch.cpp (where the raw->screen mapping is computed).
#define TOUCH_CAL_MARGIN 30

/* =========================================================================
   TYPOGRAPHY -- Inter, anti-aliased, Latin-1 (accents). Ids index
   FONT_TABLE in fonts/InterFonts.h (tools/make_fonts.py); keep in sync.
   ========================================================================= */
#define FONT_XS       1   // 12 Regular   captions, footer, status values
#define FONT_SM       2   // 14 Regular   secondary lines, times
#define FONT_MD       3   // 16 Regular   body, artist, row labels
#define FONT_MD_B     4   // 16 SemiBold  list titles
#define FONT_LG       5   // 20 Bold      track title, screen titles
#define FONT_XL       6   // 28 Bold      splash logo
#define FONT_XS_B     7   // 12 SemiBold  status-bar labels
#define FONT_SM_B     8   // 14 SemiBold  list titles, footer buttons

/* =========================================================================
   VIEW IDs
   ========================================================================= */
enum class ViewID : uint8_t {
    Boot = 0,
    Home = 1,
    Search = 2,
    Playlist = 3,
    Settings = 4,
    Welcome = 5,
    Update = 6
};
