/*-------------------------------------------------------------------------------------------------
** WiFiSetup.cpp - Working captive portal with AP password
** ------------------------------------------------------------------------------------------------
*/
#include "WiFiSetup.h"
#include "Vault.h"
#include "logTags.h"
#include "SCLogger.h"

const char* WiFiSetup::HTML_CSS = R"raw(
*{margin:0;padding:0;box-sizing:border-box}
body{background:#121212;color:#fff;font-family:-apple-system,sans-serif;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}
.container{background:#1B1B1B;padding:24px;border-radius:12px;width:92%;max-width:380px;margin:8px}
.logo{font-size:22px;font-weight:700;color:#1ED760;text-align:center;margin-bottom:16px}
h1{font-size:16px;text-align:center;margin-bottom:16px;font-weight:500}
label{display:block;font-size:12px;color:#B3B3B3;margin-bottom:4px;margin-top:12px}
select,input{width:100%;padding:10px;background:#222;border:1px solid:#333;border-radius:6px;color:#fff;font-size:14px;-webkit-appearance:none}
select:focus,input:focus{outline:none;border-color:#1ED760}
.btn{width:100%;padding:12px;background:#1ED760;color:#000;border:none;border-radius:6px;font-size:15px;font-weight:700;margin-top:16px;cursor:pointer}
.btn:hover{background:#1ae660}
.footer{text-align:center;color:#444;font-size:10px;margin-top:16px}
)raw";

const char* WiFiSetup::HTML_FOOT = R"raw(<div class="footer">ShiftDeck v1.0.0 - ESP32</div></div></body></html>)raw";

const char* WiFiSetup::HTML_SAVED = R"raw(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ShiftDeck - Conectado</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#121212;color:#fff;font-family:-apple-system,sans-serif;display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0}
.box{background:#1B1B1B;padding:32px;border-radius:12px;text-align:center;margin:8px}
h1{color:#1ED760;font-size:22px;margin-bottom:12px}
p{color:#B3B3B3;font-size:14px}
</style>
</head><body><div class="box"><h1>Conectado!</h1><p>O dispositivo vai reiniciar.</p><p>Feche esta pagina.</p></div></body></html>
)raw";

WiFiSetup& WiFiSetup::getInstance() {
    static WiFiSetup instance;
    return instance;
}

void WiFiSetup::startCaptivePortal() {
    spLogI(LOGTAG_WIFI, "Starting AP: %s password: %s", AP_SSID, AP_PASSWORD);
    WiFi.mode(WIFI_AP);
    // The phone is centimetres away; full TX power only adds current peaks that make
    // weak USB hubs cut power (the board restarted right here on a 500 mA hub).
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    IPAddress apIP = WiFi.softAPIP();
    spLogI(LOGTAG_WIFI, "AP IP: %s", apIP.toString().c_str());

    // DNS server - redirect all domains to AP IP
    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer.start(53, "*", apIP);

    // Web server
    _server.on("/", [this]() { handleRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });
    _server.onNotFound([this]() {
        // Redirect any not-found to root
        _server.sendHeader("Location", "/", true);
        _server.send(302, "text/plain", "");
    });

    _server.begin();
    _portalRunning = true;
    spLogI(LOGTAG_WIFI, "Captive portal ready at http://%s", apIP.toString().c_str());
}

void WiFiSetup::stopCaptivePortal() {
    _server.stop();
    _dnsServer.stop();
    WiFi.softAPdisconnect(true);
    _portalRunning = false;
}

void WiFiSetup::handleLoop() {
    if (!_portalRunning) return;
    _dnsServer.processNextRequest();
    _server.handleClient();
}

String WiFiSetup::getAPPassword() {
    return String(AP_PASSWORD);
}

String WiFiSetup::scanNetworks() {
    int n = WiFi.scanNetworks();
    String html;
    for (int i = 0; i < n; i++) {
        bool encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        html += "<option value=\"";
        html += WiFi.SSID(i);
        html += "\">";
        html += WiFi.SSID(i);
        html += " (";
        html += String(WiFi.RSSI(i));
        html += "dBm)";
        if (encrypted) html += " *";
        html += "</option>";
    }
    WiFi.scanDelete();
    return html;
}

void WiFiSetup::handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
    html += "<title>ShiftDeck Setup</title><style>";
    html += HTML_CSS;
    html += "</style></head><body><div class=\"container\">";
    html += "<div class=\"logo\">ShiftDeck</div>";
    html += "<h1>WiFi Setup</h1>";

    html += "<form action=\"/save\" method=\"POST\">";
    html += "<label>Rede WiFi</label>";
    html += "<select name=\"ssid\" id=\"ssid\">";
    html += scanNetworks();
    html += "</select>";

    html += "<label>Senha da rede</label>";
    html += "<input type=\"password\" name=\"pass\" id=\"pass\" placeholder=\"Senha do WiFi...\">";

    html += "<button class=\"btn\" type=\"submit\">Conectar</button>";
    html += "</form>";

    html += "<p style=\"color:#B3B3B3;font-size:11px;margin-top:12px;text-align:center\">";
    html += "Se a lista estiver vazia, atualize a pagina.</p>";

    html += HTML_FOOT;

    _server.sendHeader("Cache-Control", "no-cache");
    _server.send(200, "text/html", html);
}

void WiFiSetup::handleSave() {
    if (!_server.hasArg("ssid") || _server.arg("ssid").isEmpty()) {
        _server.send(400, "text/html", "SSID required. <a href='/'>Back</a>");
        return;
    }

    String ssid = _server.arg("ssid");
    String pass = _server.arg("pass");

    spLogI(LOGTAG_WIFI, "Saving WiFi: %s", ssid.c_str());
    Vault::getInstance().setWiFiCredentials(ssid, pass);

    _server.send(200, "text/html", HTML_SAVED);

    delay(1500);
    ESP.restart();
}
