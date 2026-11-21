/*-------------------------------------------------------------------------------------------------
** WiFiSetup.cpp - Saved Wi-Fi networks: joining the best one, and the page that manages them
** ------------------------------------------------------------------------------------------------
*/
#include "WiFiSetup.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <algorithm>
#include <climits>
#include "config.h"
#include "Vault.h"
#include "logTags.h"
#include "SCLogger.h"

static WebServer server(80);
static DNSServer dnsServer;
static bool running = false, captive = false, routesReady = false;
static String nearby;                  // networks in range, as page HTML, from the last background scan
static unsigned long nearbyAt = 0;     // when that scan finished (0 = none yet)
static bool savedFlag = false;         // see takeSaved()

static const char CSS[] = R"raw(
*{margin:0;padding:0;box-sizing:border-box}
body{background:#121212;color:#fff;font-family:-apple-system,sans-serif}
.container{background:#1B1B1B;padding:22px;border-radius:12px;width:92%;max-width:400px;margin:12px auto}
.logo{font-size:22px;font-weight:700;color:#1ED760;text-align:center;margin-bottom:10px}
h1{font-size:16px;text-align:center;margin-bottom:8px;font-weight:500}
label{display:block;font-size:12px;color:#B3B3B3;margin:16px 0 6px}
input{width:100%;padding:11px;background:#222;border:1px solid #333;border-radius:6px;color:#fff;font-size:15px;margin-bottom:8px;-webkit-appearance:none}
input:focus{outline:none;border-color:#1ED760}
button{font:inherit;color:#fff;cursor:pointer;border:none}
.btn{width:100%;padding:12px;background:#1ED760;color:#000;border-radius:6px;font-size:15px;font-weight:700}
.row{display:flex;align-items:center;gap:8px;background:#222;border-radius:6px;padding:8px 10px;margin-bottom:6px}
.row span{flex:1;overflow-wrap:anywhere}
.sm{background:#333;border-radius:5px;padding:7px 10px;font-size:13px}
.del{background:#5a1d1d}
.pick{display:flex;justify-content:space-between;width:100%;background:#222;border-radius:6px;padding:10px;margin-bottom:6px;font-size:14px;text-align:left}
.pick small{color:#888;margin-left:8px;white-space:nowrap}
.msg{background:#16351f;color:#1ED760;border-radius:6px;padding:9px;text-align:center;font-size:14px}
.err{background:#3a1717;color:#ff7b7b}
.hint{color:#888;font-size:12px;margin-top:10px;text-align:center}
.footer{text-align:center;color:#444;font-size:10px;margin-top:16px}
)raw";

static String savedPage(const char* line) {
    return String("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                  "<title>ShiftDeck - Rede salva</title><style>body{background:#121212;color:#B3B3B3;font-family:-apple-system,sans-serif;"
                  "text-align:center;padding:60px 20px}h1{color:#1ED760;font-size:22px;margin-bottom:12px}</style></head>"
                  "<body><h1>Rede salva!</h1><p>") + line + "</p><p>Pode fechar esta página.</p></body></html>";
}

// SSIDs come from the air: never put them in the page unescaped.
static String esc(const String& s) {
    String o;
    o.reserve(s.length() + 8);
    for (unsigned i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else if (c == '"') o += "&quot;";
        else if (c == '\'') o += "&#39;";
        else o += c;
    }
    return o;
}

// 150 ms per channel instead of the default 300 (a scan while connected took ~10 s).
static int scan(bool async = false) { return WiFi.scanNetworks(async, false, false, 150); }

// Strongest signal of ssid in the last scan of n results, or INT_MIN if absent.
static int bestRssi(int n, const String& ssid) {
    int best = INT_MIN;
    for (int i = 0; i < n; i++)
        if (WiFi.SSID(i) == ssid) best = std::max(best, (int)WiFi.RSSI(i));
    return best;
}

static String nearbyList(int n) {
    std::vector<String> seen;
    String h;
    for (int i = 0; i < n && seen.size() < 12; i++) {
        String s = WiFi.SSID(i);
        // hidden networks have no name; a mesh shows the same name on several access points
        if (s.isEmpty() || std::find(seen.begin(), seen.end(), s) != seen.end()) continue;
        seen.push_back(s);
        String e = esc(s);
        h += "<button type=\"button\" class=\"pick\" data-s=\"" + e + "\">" + e + "<small>" + String(bestRssi(n, s)) + " dBm";
        if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) h += " · senha";
        h += "</small></button>";
    }
    return seen.empty() ? String("<p class=\"hint\">Nenhuma rede encontrada. Atualize a página.</p>") : h;
}

static void sendPage(const String& msg, bool err = false) {
    String h = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
               "<title>ShiftDeck Wi-Fi</title><style>";
    h += CSS;
    h += "</style></head><body><div class=\"container\"><div class=\"logo\">ShiftDeck</div><h1>Redes Wi-Fi</h1>";
    if (msg.length()) h += String("<p class=\"msg") + (err ? " err" : "") + "\">" + msg + "</p>";
    std::vector<WiFiNet> saved = Vault::getInstance().getWiFiNetworks();
    if (saved.size()) {
        h += "<label>Salvas (conecta na de sinal mais forte)</label>";
        for (auto& net : saved) {
            String s = esc(net.ssid);
            h += "<div class=\"row\"><span>" + s + "</span><button type=\"button\" class=\"sm\" data-s=\"" + s + "\">Editar</button>"
                 "<form method=\"POST\" action=\"/delete\"><input type=\"hidden\" name=\"ssid\" value=\"" + s + "\">"
                 "<button class=\"sm del\">Excluir</button></form></div>";
        }
    }
    h += "<form method=\"POST\" action=\"/save\"><label id=\"t\">Adicionar rede</label>"
         "<input name=\"ssid\" id=\"ssid\" maxlength=\"32\" required placeholder=\"Nome da rede\" autocapitalize=\"off\">"
         "<input type=\"password\" name=\"pass\" id=\"pass\" maxlength=\"63\" placeholder=\"Senha (vazia se a rede for aberta)\">"
         "<button class=\"btn\">Salvar</button></form><label>Por perto (toque para escolher)</label>";
    // The page never waits for a scan: a blocking one held every request (and the phone's
    // favicon request) for ~10 s, so "Salvar" looked dead. Results come from the background.
    if (WiFi.scanComplete() == WIFI_SCAN_FAILED && (!nearbyAt || millis() - nearbyAt > 30000)) scan(true);
    h += nearbyAt ? nearby : String("<p class=\"hint\">Procurando redes... <a href=\"/\" style=\"color:#1ED760\">atualizar</a></p>");
    h += "<p class=\"hint\">Até 5 redes; a mais antiga sai. Salvar uma rede que já existe troca a senha.</p>"
         "<script>document.forms[document.forms.length-1].onsubmit=function(){var b=this.querySelector('.btn');b.textContent='Salvando...';b.disabled=true};"
         "document.querySelectorAll('[data-s]').forEach(function(b){b.onclick=function(){ssid.value=b.dataset.s;"
         "t.textContent=b.classList.contains('sm')?'Editar rede':'Adicionar rede';pass.value='';pass.focus()}})</script>"
         "<div class=\"footer\">ShiftDeck v" VERSION "</div></div></body></html>";
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "text/html", h);
}

static void redirect(const char* to) {
    server.sendHeader("Location", to, true);
    server.send(303, "text/plain", "");
}

static void handleSave() {
    String ssid = server.arg("ssid"), pass = server.arg("pass");
    if (ssid.isEmpty() || ssid.length() > 32) return sendPage("Nome da rede inválido.", true);
    if (pass.length() && (pass.length() < 8 || pass.length() > 63)) return sendPage("A senha do Wi-Fi tem de 8 a 63 caracteres.", true);
    spLogI(LOGTAG_WIFI, "Saving WiFi: %s", ssid.c_str());
    Vault::getInstance().saveWiFiNetwork(ssid, pass);
    if (captive) {                       // join it: restart, boot picks the best saved network
        server.send(200, "text/html", savedPage("A ShiftDeck vai reiniciar e conectar."));
        delay(1500);
        ESP.restart();
    }
    server.send(200, "text/html", savedPage("Ela aparece na lista da ShiftDeck."));
    savedFlag = true;
}

static void handleDelete() {
    spLogI(LOGTAG_WIFI, "Removing WiFi: %s", server.arg("ssid").c_str());
    Vault::getInstance().removeWiFiNetwork(server.arg("ssid"));
    redirect("/?m=2");
}

static void setupRoutes() {
    if (routesReady) return;
    routesReady = true;
    server.on("/", [] {
        sendPage(server.arg("m") == "2" ? "Rede excluída." : "");
    });
    server.on("/save", HTTP_POST, handleSave);
    server.on("/delete", HTTP_POST, handleDelete);
    // Captive-portal probes land on the page; on the home network (favicon...) just 404.
    server.onNotFound([] { if (captive) redirect("/"); else server.send(404, "text/plain", ""); });
}

WiFiSetup& WiFiSetup::getInstance() {
    static WiFiSetup instance;
    return instance;
}

bool WiFiSetup::connectSaved(void (*tick)()) {
    std::vector<WiFiNet> saved = Vault::getInstance().getWiFiNetworks();
    if (saved.empty()) return false;
    WiFi.mode(WIFI_STA);
    // Saved networks in range, strongest first; if none shows up, the most recent one (hidden SSIDs).
    std::vector<std::pair<int, WiFiNet>> order;
    int n = scan();
    for (auto& s : saved) {
        int rssi = bestRssi(n, s.ssid);
        if (rssi != INT_MIN) order.push_back({ rssi, s });
    }
    WiFi.scanDelete();
    std::sort(order.begin(), order.end(), [](const std::pair<int, WiFiNet>& a, const std::pair<int, WiFiNet>& b) { return a.first > b.first; });
    if (order.empty()) order.push_back({ 0, saved[0] });

    for (auto& o : order) {
        spLogI(LOGTAG_WIFI, "Connecting to %s (%d dBm)", o.second.ssid.c_str(), o.first);
        WiFi.begin(o.second.ssid.c_str(), o.second.pass.c_str());
        // 15 dBm instead of 19.5: a little less range, much smaller current peaks, which
        // reset the board on weak supplies (USB hubs, thin cables, car ports).
        // Only takes effect once the station is started, i.e. after begin().
        WiFi.setTxPower(WIFI_POWER_15dBm);
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) {
            if (tick) tick();
            delay(20);
        }
        if (WiFi.status() == WL_CONNECTED) {
            Vault::getInstance().markWiFiUsed(o.second.ssid);
            return true;
        }
        WiFi.disconnect();
    }
    return false;
}

bool WiFiSetup::savedNetworkInRange() {
    std::vector<WiFiNet> saved = Vault::getInstance().getWiFiNetworks();
    int n = scan();
    bool found = false;
    for (auto& s : saved) found = found || bestRssi(n, s.ssid) != INT_MIN;
    WiFi.scanDelete();
    return found;
}

void WiFiSetup::startCaptivePortal() {
    spLogI(LOGTAG_WIFI, "Starting AP: %s password: %s", AP_SSID, AP_PASSWORD);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    // The phone is centimetres away; full TX power only adds current peaks that make
    // weak USB hubs cut power (the board restarted right here on a 500 mA hub).
    // Only takes effect once the AP is up, i.e. after softAP().
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    IPAddress apIP = WiFi.softAPIP();
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", apIP);   // every domain -> the page
    setupRoutes();
    server.begin();
    captive = running = true;
    nearbyAt = 0;
    scan(true);   // before a phone joins: a scan takes the radio off the AP's channel for a moment
    spLogI(LOGTAG_WIFI, "Captive portal ready at http://%s", apIP.toString().c_str());
}

void WiFiSetup::startWebPage() {
    if (running) return;
    setupRoutes();
    server.begin();
    captive = false;
    running = true;
    savedFlag = false;
    nearbyAt = 0;
    scan(true);
    spLogI(LOGTAG_WIFI, "Wi-Fi page at http://%s", WiFi.localIP().toString().c_str());
}

void WiFiSetup::stop() {
    if (!running) return;
    server.stop();
    if (captive) {
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
    }
    running = false;
}

bool WiFiSetup::takeSaved() {
    bool s = savedFlag;
    savedFlag = false;
    return s;
}

void WiFiSetup::handleLoop() {
    if (!running) return;
    if (captive) dnsServer.processNextRequest();
    int n = WiFi.scanComplete();
    if (n >= 0) {
        nearby = nearbyList(n);
        nearbyAt = millis();
        WiFi.scanDelete();
    }
    server.handleClient();
}
