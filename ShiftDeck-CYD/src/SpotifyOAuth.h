/*-------------------------------------------------------------------------------------------------
** SpotifyOAuth.h - Spotify login through the board's web page
**
** fetchSpotifyAuthCode():
**   1. Starts a web server on port 80 with an "Autorizar Spotify" button
**   2. Spotify sends the browser to OAUTH_REDIRECT_URI (static page on GitHub Pages),
**      which forwards it to http://<board IP>/code?code=... (or the user pastes the code)
**   3. Returns the code when received (or empty after 5 minutes)
**
** PKCE (RFC 7636): the authorize link carries a SHA-256 challenge of a random verifier, and the
** token exchange sends the verifier instead of a Client Secret, so the board needs only the
** Client ID and never stores a secret.
** ------------------------------------------------------------------------------------------------
*/
#pragma once
#include <Arduino.h>

String fetchSpotifyAuthCode();
// PKCE verifier of the last fetchSpotifyAuthCode() session, for the token exchange.
String spotifyCodeVerifier();
