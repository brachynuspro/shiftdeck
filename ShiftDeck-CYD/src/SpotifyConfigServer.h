/*-------------------------------------------------------------------------------------------------
** SpotifyConfigServer.h - Web server to configure Spotify credentials
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>
#include <WebServer.h>

class SpotifyConfigServer {
public:
    static SpotifyConfigServer& getInstance();

    void begin();
    void stop();
    void handleClient();

private:
    SpotifyConfigServer() {}

    WebServer _server;
    bool _running = false;

    void handleRoot();
    void handleSave();
    static const char* PAGE_HTML;
};
