/*-------------------------------------------------------------------------------------------------
** SpotifyConfigServer.cpp - Web UI for the Spotify Client ID (PKCE, no secret)
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
.btn2{display:block;width:100%;padding:12px;background:#27292E;color:#fff;border:none;border-radius:6px;font-size:14px;font-weight:700;margin-top:10px;text-align:center;text-decoration:none;cursor:pointer}
.step{color:#fff;font-size:13px;margin-top:18px}
.step b{color:#1ED760}
.row{display:flex;gap:8px;margin-top:6px}
.row input{flex:1;font-size:12px}
.row .btn2{width:auto;margin-top:0;padding:0 14px}
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
<h2>Conectar o Spotify</h2>

<div class="step"><b>1.</b> Abra o Spotify Dashboard e entre na sua conta.
Crie um app (marque <b>Web API</b>) ou abra o que você já tem.</div>
<a class="btn2" href="https://developer.spotify.com/dashboard" target="_blank" rel="noopener">Abrir Spotify Dashboard</a>
<a class="btn2" href=")raw" GUIDE_URL R"raw(" target="_blank" rel="noopener">Primeira vez? Guia passo a passo</a>

<div class="step"><b>2.</b> No app, em <b>Settings</b> &rsaquo; Redirect URIs, adicione este endereço e salve:</div>
<div class="row"><input id="ru" type="text" readonly value=")raw" OAUTH_REDIRECT_URI R"raw("><button class="btn2" type="button" onclick="cp(this)">Copiar</button></div>

<div class="step"><b>3.</b> Ainda em Settings, copie o <b>Client ID</b> e cole aqui. O Client Secret não é necessário.</div>
<form action="/save" method="POST">
<label>Client ID</label>
<input type="text" name="client_id" placeholder="Cole seu Client ID..." autocapitalize="off" autocorrect="off" required>

<button class="btn" type="submit">Salvar e Conectar</button>
</form>
<div class="footer">ShiftDeck v)raw" VERSION R"raw(</div>
</div>
<script>
// navigator.clipboard needs HTTPS; the board serves HTTP, so copy the old way.
function cp(b){var i=document.getElementById("ru");i.select();i.setSelectionRange(0,999);
try{document.execCommand("copy");b.textContent="Copiado!";}catch(e){b.textContent="Selecione";}}
</script></body></html>)raw";

    _server.sendHeader("Cache-Control", "no-cache");
    _server.send(200, "text/html", html);
}

void SpotifyConfigServer::handleSave() {
    if (!_server.hasArg("client_id") || _server.arg("client_id").isEmpty()) {
        _server.send(400, "text/html", "Erro: Client ID obrigatorio. <a href='/'>Voltar</a>");
        return;
    }

    String cid = _server.arg("client_id");

    spLogI(LOGTAG_GENERAL, "Saving Spotify credentials...");

    Vault& vault = Vault::getInstance();
    cid.trim();
    vault.setSpotifyCredentials(cid, "");   // PKCE: no secret on the board

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
