# SpotifyArduino — patches applied

This is a vendored copy of
[witnessmenow/spotify-api-arduino](https://github.com/witnessmenow/spotify-api-arduino).
Two patches were applied on top of upstream to fix a stack-smashing
reboot that hit ShiftDeck on every pause / skip / next / previous
button press after the Spotify access token had been alive for ~1h.

## 1) `refreshAccessToken()` body buffer

`src/SpotifyArduino.cpp` line ~175

```diff
 bool SpotifyArduino::refreshAccessToken()
 {
-    char body[300];
+    // Bumped from 300 to 1024. Original overflows with a refresh
+    // token longer than ~177 chars (Spotify refresh tokens are
+    // routinely 150-220 chars; client_id + client_secret add
+    // another 64).  Stack smashing -> panic -> reboot.
+    char body[1024];
     sprintf(body, refreshAccessTokensBody, _refreshToken, _clientId, _clientSecret);
```

Same kind of overflow that the upstream `requestAccessTokens()` had,
which DEVELOPMENT_JOURNEY.md Phase 5 attempt 8 already documented.
ShiftDeck already bypasses `requestAccessTokens()` via a direct
HTTPClient call in `main.cpp`; the matching bump in this file is
defense-in-depth so a future re-enable of that path doesn't break
the same way.

## 2) `setBearerToken()` public setter

`src/SpotifyArduino.h` (declaration in the Auth Methods block)
and `src/SpotifyArduino.cpp` (implementation next to `setRefreshToken`).

ShiftDeck now manages the access token itself via
`SpotifyPlayer::ensureAccessToken()` (which uses HTTPClient with a
`String` body — no fixed buffer) and pushes the freshly-minted
bearer into the library with `spotify.setBearerToken(token)`. The
library's `autoTokenRefresh` is set to `false` after the initial
login so the buggy refresh path inside the library is never
re-entered at runtime. This decouples ShiftDeck from any future
upstream regressions in the token-refresh buffer sizing.

## 3) `SPOTIFY_ACCESS_TOKEN_LENGTH` 309 → 1024

`src/SpotifyArduino.h`. Spotify access tokens now exceed 309 chars, so
`setBearerToken()` truncated them into the 319-byte `_bearerToken` and every
library call (e.g. `getCurrentlyPlaying`) got 401 while `SpotifyPlayer::api()`
calls, which send the full `String`, still got 200. UI showed Spotify offline.

## How to re-apply after a `pio run -t clean`

Just rerun the build. `platformio.ini` points `lib_extra_dirs` at
`lib/`, so the local vendored copy is picked up automatically and
the upstream git repo is no longer fetched. To go back to upstream,
remove the `lib_extra_dirs` line and re-add the
`https://github.com/witnessmenow/spotify-api-arduino.git` entry in
`lib_deps`.
