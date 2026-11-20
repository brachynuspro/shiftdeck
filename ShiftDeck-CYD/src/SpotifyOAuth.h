/*-------------------------------------------------------------------------------------------------
** SpotifyOAuth.h - OAuth via web page + Apps Script bridge
**
** fetchSpotifyAuthCode():
**   1. Starts web server on port 80
**   2. Serves page with "Autorizar Spotify" button → links to Spotify OAuth
**   3. Polls Apps Script for code every 2s
**   4. Returns code when received (or empty on timeout)
** ------------------------------------------------------------------------------------------------
*/
#pragma once
#include <Arduino.h>

String fetchSpotifyAuthCode();
