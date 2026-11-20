/*-------------------------------------------------------------------------------------------------
** SpotifyOAuth.cpp - OAuth via plain HTML form (no JS, 100% reliable)
** ------------------------------------------------------------------------------------------------
*/
#include "SpotifyOAuth.h"
#include "Vault.h"
#include "logTags.h"
#include "SCLogger.h"
#include "DisplayUI.h"
#include "config.h"
#include "Views/SpotifyAuthView.h"
#include <WiFi.h>
#include <WebServer.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

extern DisplayUI displayUI;
extern SpotifyAuthView* spotifyAuthView;

static WebServer* s_server = nullptr;
static String s_authCode = "";
static bool s_codeReceived = false;
static String s_verifier, s_challenge;   // PKCE pair of the current session

// base64url(SHA-256(verifier)) without padding (RFC 7636 S256).
static String pkceChallenge(const String& verifier) {
    uint8_t hash[32];
    mbedtls_sha256((const unsigned char*)verifier.c_str(), verifier.length(), hash, 0);
    unsigned char b64[48];
    size_t n = 0;
    mbedtls_base64_encode(b64, sizeof(b64), &n, hash, sizeof(hash));
    String out;
    for (size_t i = 0; i < n && b64[i] != '='; i++) out += b64[i] == '+' ? '-' : b64[i] == '/' ? '_' : (char)b64[i];
    return out;
}

static String newVerifier() {
    static const char CS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    String v;
    for (int i = 0; i < 64; i++) v += CS[esp_random() % (sizeof(CS) - 1)];   // hardware RNG (radio on)
    return v;
}

String spotifyCodeVerifier() { return s_verifier; }

static String urlEncode(const String& str) {
    String encoded;
    char hex[4];
    for (size_t i = 0; i < str.length(); i++) {
        char c = str[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            encoded += c;
        else if (c == ' ')
            encoded += "%20";
        else {
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            encoded += hex;
        }
    }
    return encoded;
}

static String pageHTML(const String& spotifyUrl, const String& msg) {
    String html = R"raw(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ShiftDeck</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#121212;color:#fff;font-family:-apple-system,sans-serif;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}
.box{background:#1B1B1B;padding:24px 20px;border-radius:14px;text-align:center;width:92%;max-width:380px;margin:8px}
h1{color:#1ED760;font-size:22px;margin-bottom:14px}
.btn{display:block;width:100%;padding:14px;background:#1ED760;color:#000;text-decoration:none;border-radius:10px;font-size:17px;font-weight:700;margin-bottom:14px;border:none;cursor:pointer}
.btn:active{background:#1af660}
.btn2{display:block;width:100%;padding:12px;background:#222;color:#1ED760;border-radius:10px;font-size:15px;font-weight:700;margin-top:10px;border:1px solid #1ED760;cursor:pointer}
.divider{display:flex;align-items:center;margin:16px 0;color:#444;font-size:11px}
.divider::before,.divider::after{content:'';flex:1;border-bottom:1px solid #333}
.divider::before{margin-right:10px}.divider::after{margin-left:10px}
input{width:100%;padding:12px;background:#121212;border:1px solid #333;border-radius:8px;color:#fff;font-size:15px;text-align:center}
input:focus{outline:none;border-color:#1ED760}
.info{color:#B3B3B3;font-size:11px;line-height:1.4;text-align:left;margin-top:12px}
.footer{color:#444;font-size:10px;margin-top:14px}
.msg{color:#1ED760;font-size:13px;margin:10px 0}.msg2{color:#f55;font-size:13px;margin:10px 0}
</style></head><body><div class="box">
<h1>ShiftDeck</h1>
<a class="btn" href=")raw" + spotifyUrl + R"raw(">AUTORIZAR SPOTIFY</a>
<div class="divider">ou cole o codigo</div>
<form method="POST" action="/savecode"><input type="text" name="code" placeholder="Cole o codigo aqui..." required><button type="submit" class="btn2">SALVAR CODIGO</button></form>
)raw";

    if (!msg.isEmpty()) {
        if (msg.indexOf("Erro") >= 0) html += "<div class=\"msg2\">" + msg + "</div>";
        else html += "<div class=\"msg\">" + msg + "</div>";
    }

    html += R"raw(<div class="info">1. Clique AUTORIZAR SPOTIFY<br>2. Faca login e autorize<br>3. Copie o codigo da tela<br>4. Volte aqui e cole<br>5. Clique SALVAR CODIGO<br><br><a href="/reconfig" style="color:#f55;font-size:10px">Reconfigurar credenciais</a></div>
<div class="footer">ShiftDeck v1.0.0</div></div></body></html>)raw";

    return html;
}

static void serveRoot() {
    String clientId = Vault::getInstance().getSpotifyClientID();
    String espIP = WiFi.localIP().toString();

    String encodedRedirect = urlEncode(OAUTH_REDIRECT_URI);
    String encodedState = urlEncode(espIP);
    String scope = urlEncode("user-read-playback-state user-modify-playback-state "
                             "user-read-currently-playing playlist-read-private "
                             "playlist-read-collaborative user-library-read user-library-modify "
                             "user-read-recently-played");

    String spotifyUrl = "https://accounts.spotify.com/authorize"
                        "?client_id=" + clientId +
                        "&response_type=code" +
                        "&redirect_uri=" + encodedRedirect +
                        "&state=" + encodedState +
                        "&code_challenge_method=S256&code_challenge=" + s_challenge +
                        "&scope=" + scope;

    String msg;
    if (s_server->hasArg("saved")) msg = "Codigo enviado! Conectando...";
    if (s_server->hasArg("error")) msg = "Erro: codigo nao enviado";

    s_server->sendHeader("Cache-Control", "no-cache");
    s_server->send(200, "text/html", pageHTML(spotifyUrl, msg));
}

static void serveSaveCode() {
    if (s_server->hasArg("code") && !s_server->arg("code").isEmpty()) {
        s_authCode = s_server->arg("code");
        s_authCode.replace(" ", "");
        s_codeReceived = true;
        spLogI(LOGTAG_PLAYER, "Code received: %d chars", s_authCode.length());
        s_server->sendHeader("Location", "/?saved=1", true);
        s_server->send(302, "text/plain", "");
    } else {
        s_server->sendHeader("Location", "/?error=1", true);
        s_server->send(302, "text/plain", "");
    }
}

// The login return page (OAUTH_REDIRECT_URI, flasher/callback.html) sends the
// browser here with the code; same result as pasting it into /savecode.
static void serveCode() {
    if (s_server->hasArg("code") && !s_server->arg("code").isEmpty()) {
        s_authCode = s_server->arg("code");
        s_authCode.replace(" ", "");
        s_codeReceived = true;
        spLogI(LOGTAG_PLAYER, "Code received via /code: %d chars", s_authCode.length());
        // Show a friendly confirmation in the browser and stop polling.
        s_server->send(200, "text/html",
            "<html><head><meta charset='UTF-8'></head>"
            "<body style='background:#121212;color:#fff;font-family:-apple-system,sans-serif;"
            "text-align:center;padding-top:30vh;margin:0'>"
            "<h1 style='color:#1ED760;font-size:26px'>Conectado!</h1>"
            "<p style='color:#B3B3B3;font-size:15px'>Pode fechar esta pagina.</p>"
            "<p style='color:#444;font-size:11px;margin-top:30px'>ShiftDeck v1.0.0</p>"
            "</body></html>");
    } else {
        s_server->send(400, "text/plain", "Missing 'code' parameter");
    }
}

String fetchSpotifyAuthCode() {
    s_authCode = "";
    s_codeReceived = false;
    // Known answer (Python: base64url(sha256(v)) without "="): a wrong hash/encoding would
    // otherwise only show up as a 400 from Spotify at the token exchange.
    String probe = pkceChallenge("dBjftJeZ4CVP-mJ92K9qO0X9vgpthM3mU8rT5NAqWNc");
    if (probe != "e4FYwADvkUVAEf0-DZVNAv0LGhO8wG17mt1zq_FAUTY")
        spLogE(LOGTAG_PLAYER, "PKCE self-test FAILED: %s", probe.c_str());
    s_verifier = newVerifier();
    s_challenge = pkceChallenge(s_verifier);

    WebServer server(80);
    s_server = &server;
    server.on("/", serveRoot);
    server.on("/savecode", HTTP_POST, serveSaveCode);
    // callback.html redirects here with ?code=...
    server.on("/code", HTTP_GET, serveCode);
    server.on("/reconfig", []() {
        // Reset spotify credentials and restart
        Vault::getInstance().setSpotifyRefreshToken("");
        Vault::getInstance().setSpotifyCredentials("", "");   // back to the credentials step
        Vault::getInstance().save();
        s_server->send(200, "text/html", "<html><body style='background:#121212;color:#fff;font-family:sans-serif;text-align:center;padding-top:30vh'><h1 style='color:#1ED760'>Reiniciando...</h1><p>Reconfigure o dispositivo</p></body></html>");
        delay(1000);
        ESP.restart();
    });
    server.begin();

    IPAddress ip = WiFi.localIP();
    spLogI(LOGTAG_PLAYER, "OAuth at http://%s", ip.toString().c_str());

    unsigned long startTime = millis();
    unsigned long lastLog = 0;

    while (!s_codeReceived && (millis() - startTime < 300000)) {
        server.handleClient();

        if (millis() - lastLog > 10000) {
            lastLog = millis();
            int elapsed = (millis() - startTime) / 1000;
            spLogI(LOGTAG_PLAYER, "Waiting... %ds", elapsed);
            if (spotifyAuthView) spotifyAuthView->setWaitingSeconds(elapsed);
        }
        delay(5);
    }

    server.stop();
    s_server = nullptr;

    if (s_codeReceived && !s_authCode.isEmpty()) {
        if (spotifyAuthView) spotifyAuthView->showConnected();
        return s_authCode;
    }

    spLogE(LOGTAG_PLAYER, "Timeout");
    return "";
}
