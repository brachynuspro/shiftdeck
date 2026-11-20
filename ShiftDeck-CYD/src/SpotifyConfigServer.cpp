/*-------------------------------------------------------------------------------------------------
** SpotifyConfigServer.cpp - Web UI for Spotify Client ID & Secret
** ------------------------------------------------------------------------------------------------
*/
#include "SpotifyConfigServer.h"
#include "Vault.h"
#include "config.h"
#include "logTags.h"
#include "SCLogger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

const char* SpotifyConfigServer::PAGE_HTML = R"raw(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ShiftDeck - Spotify</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#121212;color:#fff;font-family:-apple-system,sans-serif;display:flex;justify-content:center;align-items:center;min-height:100vh}
.container{background:#1B1B1B;padding:24px;border-radius:12px;width:92%;max-width:400px}
h1{color:#1ED760;font-size:20px;text-align:center;margin-bottom:4px}
h2{color:#B3B3B3;font-size:13px;text-align:center;margin-bottom:20px;font-weight:400}
label{display:block;font-size:12px;color:#B3B3B3;margin-bottom:4px;margin-top:14px}
input{width:100%;padding:11px;background:#222;border:1px solid #333;border-radius:6px;color:#fff;font-size:14px}
input:focus{outline:none;border-color:#1ED760}
small{display:block;color:#666;font-size:10px;margin-top:4px}
.btn{width:100%;padding:13px;background:#1ED760;color:#000;border:none;border-radius:6px;font-size:15px;font-weight:700;margin-top:20px;cursor:pointer}
.btn:hover{background:#1ae660}
.saved{text-align:center;padding:20px}
.saved h1{margin-bottom:12px}
.saved p{color:#B3B3B3;font-size:14px}
.footer{text-align:center;color:#444;font-size:10px;margin-top:18px}
</style></head><body>)raw";

SpotifyConfigServer& SpotifyConfigServer::getInstance() {
    static SpotifyConfigServer instance;
    return instance;
}

void SpotifyConfigServer::begin() {
    spLogI(LOGTAG_GENERAL, "Starting Spotify config server on port 80");

    _server.on("/", [this]() { handleRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });
    _server.begin();
    _running = true;
}

void SpotifyConfigServer::stop() {
    _server.stop();
    _running = false;
}

void SpotifyConfigServer::handleClient() {
    if (_running) _server.handleClient();
}

void SpotifyConfigServer::handleRoot() {
    String html = PAGE_HTML;
    html += R"raw(<div class="container">
<h1>ShiftDeck</h1>
<h2>Configurar Spotify</h2>

<small>Obtenha Client ID e Secret em: developer.spotify.com/dashboard</small>

<form action="/save" method="POST">
<label>Client ID</label>
<input type="text" name="client_id" placeholder="Cole seu Client ID..." required>

<label>Client Secret</label>
<input type="password" name="client_secret" placeholder="Cole seu Client Secret..." required>

<label style="margin-top:16px;color:#1ED760">Apps Script URL</label>
<small>Obrigatorio para autenticacao segura</small>
<small>Veja instrucoes no arquivo data/appscript_template.gs</small>
<input type="text" name="apps_script_url" placeholder="https://script.google.com/macros/s/...">

<button class="btn" type="submit">Salvar e Conectar</button>
</form>
<div class="footer">ShiftDeck v1.0.0</div>
</div></body></html>)raw";

    _server.sendHeader("Cache-Control", "no-cache");
    _server.send(200, "text/html", html);
}

void SpotifyConfigServer::handleSave() {
    if (!_server.hasArg("client_id") || _server.arg("client_id").isEmpty()) {
        _server.send(400, "text/html", "Erro: Client ID obrigatorio. <a href='/'>Voltar</a>");
        return;
    }
    if (!_server.hasArg("client_secret") || _server.arg("client_secret").isEmpty()) {
        _server.send(400, "text/html", "Erro: Client Secret obrigatorio. <a href='/'>Voltar</a>");
        return;
    }

    String cid = _server.arg("client_id");
    String cse = _server.arg("client_secret");
    String asu = _server.hasArg("apps_script_url") ? _server.arg("apps_script_url") : "";

    spLogI(LOGTAG_GENERAL, "Saving Spotify credentials...");

    Vault& vault = Vault::getInstance();
    vault.setSpotifyCredentials(cid, cse);
    if (!asu.isEmpty()) vault.setAppsScriptUrl(asu);

    String html = R"raw(<div class="container saved">
<h1 style="color:#1ED760">Salvo com sucesso!</h1>
<p>O dispositivo vai reiniciar agora.</p>
<p>Feche esta pagina.</p>
<div class="footer">ShiftDeck v1.0.0</div>
</div></body></html>)raw";

    _server.send(200, "text/html", PAGE_HTML + html);

    delay(1500);
    ESP.restart();
}
