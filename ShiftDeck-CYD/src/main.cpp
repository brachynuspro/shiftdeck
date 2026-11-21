/*-------------------------------------------------------------------------------------------------
** main.cpp - ShiftDeck main entry point for ESP32-2432S028 (CYD)
**
** Boot flow:
**   1. Display + Touch + LittleFS init
**   2. WiFi check (up to 5 saved networks, Vault):
**      - None saved → WiFiSetup screen → captive portal → save → restart
**      - Saved → join the strongest one in range
**        - None in range → "Wi-Fi não encontrado" + portal; rescans every 30 s, restarts when one is back
**        - OK → continue
**   3. Spotify credentials check:
**      - If default → SpotifyConfig screen → web page → save → restart
**      - If configured → continue
**   4. BootView checklist: Display, Touch, WiFi, Backend, Spotify
**   5. Spotify OAuth (if needed)
**   6. Auto-advance to HomeView
**   7. Start background Spotify refresh + UI task
**
** Dual-core:
**   Core 0 (PRO): UI handler (60 FPS)
**   Core 1 (APP): Spotify refresh task + main loop
** ------------------------------------------------------------------------------------------------
*/
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

#include "config.h"
#include "scui.h"
#include "logTags.h"
#include "SCLogger.h"
#include "Monitor.h"
#include "Vault.h"
#include "SpotifyPlayer.h"
#include "SpotifyArtMgr.h"
#include "SpotifyCertWrapper.h"
#include "DisplayUI.h"
#include "XPT2046Touch.h"
#include "TouchCalibration.h"
#include "WiFiSetup.h"
#include "SpotifyConfigServer.h"
#include "SpotifyOAuth.h"
#include "UIViewManager.h"

#include "Views/WiFiSetupView.h"
#include "Views/SpotifyConfigView.h"
#include "Views/SpotifyAuthView.h"
#include "Views/BootView.h"
#include "Views/HomeView.h"
#include "Views/SearchView.h"
#include "Views/PlaylistView.h"
#include "Views/SettingsView.h"
#include "Views/WelcomeView.h"
#include "Views/UpdateView.h"
#include "Views/AccountView.h"
#include "Views/QueueView.h"
#include "CertBundle.h"

TFT_eSPI       tft = TFT_eSPI();
DisplayUI      displayUI(&tft);
XPT2046Touch   touch;

SpotifyPlayer& spotifyPlayer = SpotifyPlayer::getInstance();
Vault&         vault         = Vault::getInstance();

UIViewManager& viewMgr   = UIViewManager::getInstance();
WiFiSetupView*    wifiSetupView    = nullptr;
SpotifyConfigView* spotifyCfgView  = nullptr;
SpotifyAuthView*   spotifyAuthView = nullptr;
BootView*         bootView         = nullptr;
HomeView*      homeView  = nullptr;
SearchView*    searchView= nullptr;
PlaylistView*  plView    = nullptr;
SettingsView*  setView   = nullptr;
WelcomeView*   welcomeView = nullptr;
UpdateView*    updateView  = nullptr;
AccountView*   accountView = nullptr;
QueueView*     queueView   = nullptr;

QueueHandle_t scuiQueue;
TaskHandle_t  uiTaskHandle;

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

void setupLogging();
void registerMonitorDescriptions();
void uiHandlerTask(void *pvParams);
void runOAuthFlow();
void waitInSetupPortal(bool spotify);
void checkFactoryResetButton();
void handleTouchInput();
void gpioMonitorTask(void *pvParams);
void acceptFirmwareWhenStable();

// A fresh OTA image boots on probation. Arduino would accept it before setup(); we accept it
// only after 30 s of normal running (acceptFirmwareWhenStable). If it resets before that
// (crash, watchdog, boot loop), the bootloader goes back to the previous image by itself.
extern "C" bool verifyRollbackLater() { return true; }

// Reset button pin and enable flag. We use the BOOT button on
// GPIO 0 — the same button the user has been pressing all along.
// Behavior:
//   - A short tap (released before 5 s) does NOTHING (no reset).
//   - Holding for 5 s shows a red progress bar overlay; if released
//     before the bar fills, the overlay disappears and no reset
//     happens.
//   - At 5 s the vault is erased and the ESP restarts.
//
// GPIO 0 shares the SPI bus with display+touch, so we debounce
// heavily (50 consecutive LOW samples) to avoid false triggers
// from SPI traffic. This is exactly the debounce level used by
// the original code, so a short tap (a few ms) never reaches the
// hold threshold.
#define FACTORY_RESET_PIN 0
#define FACTORY_RESET_HOLD_MS 5000
#define FACTORY_RESET_ENABLED 1

// GPIOs the monitor task polls. Used only when FACTORY_RESET_ENABLED
// is 0 to identify an unknown pin; with GPIO 0 as the reset pin
// this is unused but kept for future debugging.
static const int RESET_BUTTON_CANDIDATES[] = { 22, 27, 17, 16, 4 };
static int g_detectedResetPin = -1;

// Set to true while the user is actively touching the screen.
volatile bool g_touchInProgress = false;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== " APP_NAME " v" VERSION " ===\n");

    setupLogging();
    registerMonitorDescriptions();

    // Increase the task watchdog timeout to 30s and configure it to
    // panic (not just reset) on trigger. The iTunes fallback HTTPS
    // request and the JPEG decode can together take 5-10s; with the
    // default 5s timeout the watchdog trips every time the user
    // presses a control button on a song that needs the fallback.
    esp_task_wdt_init(30, true);

    // --- Display init ---
    spLogI(LOGTAG_BOOT, "Initializing ILI9341 display...");
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(C_BG);
    tft.setTextColor(C_WHITE, C_BG);

    ledcSetup(0, 5000, 8);
    ledcAttachPin(21, 0);
    ledcWrite(0, 200);

    displayUI.init();
    TJpgDec.setJpgScale(1);

    // --- Touch init ---
    spLogI(LOGTAG_BOOT, "Initializing XPT2046 touch...");
    touch.begin(SCREEN_W, SCREEN_H);
    touch.setRotation(1);

    // --- Filesystem ---
    spLogI(LOGTAG_BOOT, "Mounting LittleFS...");
    if (!LittleFS.begin(false)) {
        spLogW(LOGTAG_BOOT, "LittleFS mount failed - reformatting (config will be lost!)");
        if (!LittleFS.begin(true)) {
            tft.fillScreen(C_BG);
            tft.setCursor(10, 10);
            tft.setTextColor(C_ERROR, C_BG);
            tft.println("FATAL: LittleFS mount failed!");
            while (1) { delay(1000); }
        }
    }

    // --- Factory reset button (currently disabled, see FACTORY_RESET_ENABLED) ---
    // While we identify the correct GPIO, configure the candidate
    // pins as safe inputs. Skip any pin that is already in use by
    // the display SPI, touch SPI, backlight, or strapping logic.
    auto isReserved = [](int pin) -> bool {
        // TFT SPI bus: 12 (MISO), 13 (MOSI), 14 (CLK), 15 (CS), 2 (DC), 21 (BL)
        // Touch SPI: 33 (CS), 36 (IRQ)
        // Strapping: 0 (BOOT), 2 (must be floating or LOW at boot)
        if (pin == 2 || pin == 12 || pin == 13 || pin == 14 || pin == 15 || pin == 21) return true;
        if (pin == 33 || pin == 36) return true;
        return false;
    };

    for (size_t i = 0; i < sizeof(RESET_BUTTON_CANDIDATES) / sizeof(RESET_BUTTON_CANDIDATES[0]); i++) {
        int pin = RESET_BUTTON_CANDIDATES[i];
        if (isReserved(pin)) continue;
        if (pin == 34 || pin == 35 || pin == 36 || pin == 37 || pin == 38 || pin == 39) {
            // Input-only GPIOs (34, 35, 36, 37, 38, 39) — no
            // internal pull-up, but reading still works if the user
            // wired an external pull-up. Use plain INPUT.
            pinMode(pin, INPUT);
        } else {
            pinMode(pin, INPUT_PULLUP);
        }
    }

    // Boot-time probe: log the initial state of every candidate so
    // wiring issues show up in the serial log without needing a
    // screen. Skips reserved pins to avoid corrupting SPI state.
    spLogI(LOGTAG_BOOT, "GPIO monitor candidates (initial state, H=idle L=pressed):");
    for (size_t i = 0; i < sizeof(RESET_BUTTON_CANDIDATES) / sizeof(RESET_BUTTON_CANDIDATES[0]); i++) {
        int pin = RESET_BUTTON_CANDIDATES[i];
        if (isReserved(pin)) continue;
        spLogI(LOGTAG_BOOT, "  GPIO %2d: %s",
               pin, digitalRead(pin) == LOW ? "L" : "H");
    }

    // --- Config ---
    vault.initialize();
    displayUI.setBacklight(vault.getBrightness());
    spotifyPlayer.initialize(&scuiQueue);

    // --- Touch calibration (first boot / after factory reset) ---
    if (!vault.isTouchCalibrated()) {
        runTouchCalibration(displayUI, touch);
    }

    // --- Create all views ---
    wifiSetupView   = new WiFiSetupView(&displayUI);
    spotifyCfgView  = new SpotifyConfigView(&displayUI);
    spotifyAuthView = new SpotifyAuthView(&displayUI);
    bootView        = new BootView(&displayUI);
    // Tell the user why the last boot was a reset: a weak supply shows up as brownout.
    esp_reset_reason_t resetReason = esp_reset_reason();
    spLogI(LOGTAG_BOOT, "Reset reason: %d", (int)resetReason);
    if (resetReason == ESP_RST_BROWNOUT)
        bootView->setNotice("Energia fraca: use fonte de 1 A", C_WARN);
    else if (resetReason == ESP_RST_PANIC || resetReason == ESP_RST_INT_WDT || resetReason == ESP_RST_TASK_WDT || resetReason == ESP_RST_WDT)
        bootView->setNotice("Reiniciou por erro", C_ERROR);
    esp_ota_img_states_t otaState = ESP_OTA_IMG_UNDEFINED;
    esp_ota_get_state_partition(esp_ota_get_running_partition(), &otaState);
    spLogI(LOGTAG_BOOT, "Running %s, OTA state %d", esp_ota_get_running_partition()->label, (int)otaState);
    // An update was installed but this is not it: the new image never got accepted and the
    // bootloader brought this one back. Say so, and stop offering that version on its own.
    String pendingUpdate = vault.getPendingUpdate();
    if (pendingUpdate.length() && pendingUpdate != VERSION) {
        spLogW(LOGTAG_BOOT, "Update to %s rolled back", pendingUpdate.c_str());
        vault.setSkipVersion(pendingUpdate);
        vault.setPendingUpdate("");
        bootView->setNotice("Atualização falhou: versão anterior mantida", C_WARN);
    }
    homeView      = new HomeView(&displayUI);
    searchView    = new SearchView(&displayUI);
    plView        = new PlaylistView(&displayUI);
    setView       = new SettingsView(&displayUI);
    welcomeView   = new WelcomeView(&displayUI);
    updateView    = new UpdateView(&displayUI);
    accountView   = new AccountView(&displayUI);
    queueView     = new QueueView(&displayUI);

    viewMgr.setDisplayUI(&displayUI);
    viewMgr.registerView(ViewID::Boot, bootView);
    viewMgr.registerView(ViewID::Home, homeView);
    viewMgr.registerView(ViewID::Search, searchView);
    viewMgr.registerView(ViewID::Playlist, plView);
    viewMgr.registerView(ViewID::Settings, setView);
    viewMgr.registerView(ViewID::Welcome, welcomeView);
    viewMgr.registerView(ViewID::Update, updateView);
    viewMgr.registerView(ViewID::Account, accountView);
    viewMgr.registerView(ViewID::Queue, queueView);

    // Fresh install (nothing saved yet): present the product and its requirements,
    // then go straight to the Wi-Fi step. waitInSetupPortal() never returns.
    if (!vault.hasWiFi() && vault.getSpotifyRefreshToken().isEmpty()) {
        viewMgr.gotoView(ViewID::Welcome);
        while (!welcomeView->done()) {
            handleTouchInput();
            delay(10);
        }
        waitInSetupPortal(false);
    }

    // ======================================================================
    // Boot checklist (IMGS/inicializacao.png): every row reflects a real check
    // ======================================================================
    unsigned long bootStart = millis();
    viewMgr.gotoView(ViewID::Boot);
    bootView->setStep(BootView::STEP_DISPLAY, BootView::OK);
    bootView->setStep(BootView::STEP_TOUCH, BootView::OK, vault.isTouchCalibrated() ? "Calibrado" : "Conectado");
    bootView->setProgress(20);

    // ---- Wi-Fi ----
    if (!vault.hasWiFi()) {
        spLogI(LOGTAG_BOOT, "No WiFi credentials - WiFi setup mode");
        bootView->setStep(BootView::STEP_WIFI, BootView::FAILED, "Não configurado");
        delay(1200);
        waitInSetupPortal(false);
    }
    bootView->setStep(BootView::STEP_WIFI, BootView::LOADING, "Procurando...");
    bootView->setProgress(30);
    if (!WiFiSetup::getInstance().connectSaved([] { bootView->animate(); })) {
        spLogW(LOGTAG_BOOT, "No saved WiFi network joined, starting setup mode");
        bootView->setStep(BootView::STEP_WIFI, BootView::TIMEOUT, "Nenhuma rede");
        delay(1500);
        waitInSetupPortal(false);
    }
    spLogI(LOGTAG_BOOT, "WiFi connected: %s", WiFi.localIP().toString().c_str());
    // Clock in the status bar. Vault keeps "UTC-3"; POSIX TZ flips the sign ("UTC3").
    String tz = vault.getTimezone();
    String posixTz = tz.startsWith("UTC") ? "UTC" + String(-tz.substring(3).toInt()) : tz;
    configTzTime(posixTz.c_str(), "pool.ntp.org", "time.google.com");
    bootView->setStep(BootView::STEP_WIFI, BootView::OK);
    bootView->setProgress(50);

    // ---- Backend: Spotify app credentials + account token ----
    String cid = vault.getSpotifyClientID();
    if (cid.isEmpty() || cid.startsWith("your_")) {
        spLogI(LOGTAG_BOOT, "Spotify credentials not configured");
        bootView->setStep(BootView::STEP_BACKEND, BootView::FAILED, "Não configurado");
        delay(1200);
        waitInSetupPortal(true);
    }
    if (!spotifyPlayer.isRefreshTokenAvailable()) {
        spLogI(LOGTAG_BOOT, "No refresh token, OAuth...");
        runOAuthFlow();               // restarts the board on failure
        bootView->draw();
    }
    bootView->setStep(BootView::STEP_BACKEND, BootView::LOADING, "Autenticando...");
    bootView->animate();
    bool backendOk = spotifyPlayer.login();
    bootView->setStep(BootView::STEP_BACKEND, backendOk ? BootView::OK : BootView::FAILED, backendOk ? nullptr : "Sem token");
    bootView->setProgress(75);

    // ---- Spotify: first playback poll, so Home opens populated ----
    bootView->setStep(BootView::STEP_SPOTIFY, BootView::LOADING);
    bootView->animate();
    if (backendOk) spotifyPlayer.refreshCurrentTrack();
    bool spotifyOk = spotifyPlayer.isApiOnline();
    bootView->setStep(BootView::STEP_SPOTIFY, spotifyOk ? BootView::OK : BootView::FAILED, spotifyOk ? nullptr : "Offline");
    bootView->setProgress(100);
    spLogI(LOGTAG_BOOT, "Checklist done in %lums", millis() - bootStart);
    delay(spotifyOk ? 400 : 1800);    // let a failure be read; Home keeps retrying

    // ======================================================================
    // Home + background tasks
    // ======================================================================
    viewMgr.gotoView(ViewID::Home);

    // Create SCUI queue BEFORE starting any task that posts to it.
    // The refresh task posts UM_PLAYER_REFRESH / UM_MARK_DIRTY messages;
    // if the queue doesn't exist yet, xQueueSend will panic and reboot.
    scuiQueue = xQueueCreate(10, sizeof(SCUIMessage));

    // Start background refresh
    spotifyPlayer.startBackgroundRefreshes();

    // Launch UI handler on Core 0
    spLogI(LOGTAG_MULTITASK, "Starting UI handler on Core 0");
    xTaskCreatePinnedToCore(
        uiHandlerTask, "UIHandler", 16384, NULL, 2, &uiTaskHandle, 0
    );

#if !FACTORY_RESET_ENABLED
    // GPIO monitor task: while the reset feature is disabled, this
    // task logs the live state of every candidate GPIO so the user
    // can identify which physical pin the "second button" is wired
    // to. Run on Core 1, low priority — won't disturb the network
    // refresh task or the UI handler.
    spLogI("GPIOMon", "Starting GPIO monitor task on Core 1");
    xTaskCreatePinnedToCore(
        gpioMonitorTask, "GPIOMon", 4096, NULL, 1, NULL, 1
    );
#endif

    spLogI(LOGTAG_BOOT, "Boot complete. Heap: %u", ESP.getFreeHeap());
}

// Blocking OAuth (SpotifyAuthView + web page). Returns with a refresh token
// stored, or restarts the board after showing the error.
void runOAuthFlow() {
    spotifyAuthView->setIP(WiFi.localIP().toString());
    spotifyAuthView->draw();

    // Block waiting for auth
    String authCode = fetchSpotifyAuthCode();
    if (!authCode.isEmpty()) {
        String redirectUri = OAUTH_REDIRECT_URI;
        String cid = vault.getSpotifyClientID();

        spLogI(LOGTAG_BOOT, "Exchanging code (%d chars), client id %u chars", authCode.length(), (unsigned)cid.length());

        // PKCE: the verifier replaces the Client Secret. All fields percent-encoded.
        String body = "grant_type=authorization_code"
                      "&code=" + urlEncode(authCode) +
                      "&redirect_uri=" + urlEncode(redirectUri) +
                      "&client_id=" + urlEncode(cid) +
                      "&code_verifier=" + urlEncode(spotifyCodeVerifier());
        
        WiFiClientSecure ssl;
        // Root CA bundle, not the old pinned DigiCert G2 root: Spotify moved
        // this host to another chain and pinning broke the OAuth exchange.
        ssl.setCACertBundle(x509_crt_bundle_start);
        ssl.setTimeout(15);
        
        HTTPClient http;
        http.begin(ssl, "accounts.spotify.com", 443, "/api/token", true);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        
        int status = http.POST(body);
        spLogI(LOGTAG_BOOT, "Token exchange HTTP: %d", status);

        if (status == 200) {
            String response = http.getString();
            http.end();

            JsonDocument doc;
            deserializeJson(doc, response);
            const char* refreshToken = doc["refresh_token"];
            const char* accessToken = doc["access_token"];

            if (refreshToken && strlen(refreshToken) > 5) {
                spLogI(LOGTAG_BOOT, "Authorized: refresh token %d chars", strlen(refreshToken));
                Vault::getInstance().setSpotifyRefreshToken(String(refreshToken));

                // Brief delay to ensure LittleFS commit is fully flushed
                // before any subsequent network activity that could panic.
                delay(50);

                // Also give the token to SpotifyArduino
                spotify.setRefreshToken(refreshToken);
                spotifyPlayer.isRefreshTokenAvailable();   // setup() logs in next
            } else {
                spLogE(LOGTAG_BOOT, "No refresh_token in response");

                // Show error on screen and restart, symmetric with the
                // HTTP-error branch below. Previously this was a silent
                // failure that fell through to Phase 5 with no token.
                spotifyAuthView->showError("Sem token, tente de novo");
                delay(10000);
                spLogE(LOGTAG_BOOT, "Restarting to retry OAuth...");
                ESP.restart();
            }
        } else {
            String err = http.getString();
            http.end();
            spLogE(LOGTAG_BOOT, "Token exchange failed: %d", status);
            spLogE(LOGTAG_BOOT, "Error: %s", err.c_str());

            spotifyAuthView->showError("Confira o Client ID");
            delay(10000);
            spLogE(LOGTAG_BOOT, "Restarting to retry OAuth...");
            ESP.restart();
        }
    } else {
        spLogE(LOGTAG_BOOT, "No auth code received!");
        spLogE(LOGTAG_BOOT, "Restarting to retry OAuth...");
        ESP.restart();
    }
}

// Shows the WiFi (captive portal) or Spotify credentials setup screen and
// serves it forever; saving there restarts the board.
void waitInSetupPortal(bool spotify) {
    if (spotify) {
        spotifyCfgView->setIP(WiFi.localIP().toString());
        spotifyCfgView->draw();
        SpotifyConfigServer::getInstance().begin();
    } else {
        WiFiSetup::getInstance().startCaptivePortal();
        wifiSetupView->setIP(WiFi.softAPIP().toString());
        wifiSetupView->setNetwork(WiFiSetup::getInstance().getAPSSID(), WiFiSetup::getInstance().getAPPassword());
        wifiSetupView->setNoNetwork(vault.hasWiFi());
        wifiSetupView->draw();
    }
    unsigned long lastScan = millis();
    while (true) {
        if (spotify) SpotifyConfigServer::getInstance().handleClient();
        else         WiFiSetup::getInstance().handleLoop();
        checkFactoryResetButton();
        acceptFirmwareWhenStable();
        // Saved networks exist but none was in range (car before the hotspot is up, router
        // rebooting): look again every 30 s and boot normally once one shows up. Not while a
        // phone is on the access point: a scan briefly takes the radio off its channel.
        if (!spotify && vault.hasWiFi() && millis() - lastScan > 30000 && WiFi.softAPgetStationNum() == 0) {
            lastScan = millis();
            if (WiFiSetup::getInstance().savedNetworkInRange()) {
                spLogI(LOGTAG_WIFI, "A saved network is back in range, restarting");
                ESP.restart();
            }
        }
        delay(5);
    }
}

// See verifyRollbackLater(). Also clears the "update in progress" mark once the new version proved itself.
void acceptFirmwareWhenStable() {
    static bool accepted = false;
    if (accepted || millis() < 30000) return;
    accepted = true;
    esp_ota_mark_app_valid_cancel_rollback();
    if (vault.getPendingUpdate() == VERSION) vault.setPendingUpdate("");
    spLogI(LOGTAG_BOOT, "Firmware %s accepted", VERSION);
}

void setupLogging() {
    SCLogger& logger = SCLogger::getInstance();
    logger.setLogLevel("*", ESP_LOG_WARN);
    logger.setLogLevel(LOGTAG_BOOT, ESP_LOG_INFO);
    logger.setLogLevel(LOGTAG_GENERAL, ESP_LOG_INFO);
    logger.setLogLevel(LOGTAG_PLAYER, ESP_LOG_INFO);
    logger.setLogLevel(LOGTAG_WIFI, ESP_LOG_INFO);
    logger.setLogLevel(LOGTAG_MULTITASK, ESP_LOG_INFO);
    logger.setLogLevel(LOGTAG_GUI, ESP_LOG_INFO);
    logger.setLogLevel(LOGTAG_SONG_DATA, ESP_LOG_INFO);
    logger.setLogLevel(LOGTAG_INPUT, ESP_LOG_INFO);
    logger.setLogLevel(LOGTAG_CACHE, ESP_LOG_INFO);
    spLogI(LOGTAG_GENERAL, "Logging initialized");
}

void registerMonitorDescriptions() {
    Monitor::registerDescription(MONITOR_ID_SPOTIFY_GET_CURRENTLY_PLAYING, "Current Playing");
    Monitor::registerDescription(MONITOR_ID_FETCH_ALBUM_ART, "Art Fetch");
    Monitor::registerDescription(MONITOR_ID_SPOTIFY_IMAGE_CACHE_LOAD, "Cache Load");
    Monitor::registerDescription(MONITOR_ID_SPOTIFY_IMAGE_CACHE_SAVE, "Cache Save");
    Monitor::registerDescription(MONITOR_ID_SPOTIFY_IMAGE_FILE_LOAD, "File Load");
    Monitor::registerDescription(MONITOR_ID_SCUI_QUEUE_DELAY, "Queue Delay");
}

void handleTouchInput() {
    static bool wasTouched = false;
    static uint16_t lastX = 0, lastY = 0;
    static unsigned long touchStartMs = 0;
    // Feed the FreeRTOS watchdog so a slow touch sample (or
    // synchronous SPI access to the touch controller) doesn't
    // cause a task watchdog reset.
    esp_task_wdt_reset();

    if (touch.touched()) {
        // Resistive panel reads off-target while the finger is still
        // pressing down: let the contact settle before the first point.
        if (!wasTouched) {
            vTaskDelay(pdMS_TO_TICKS(20));
            if (!touch.touched()) return;
        }
        TS_Point p = touch.getPoint();
        UIView* view = viewMgr.getActiveView();
        if (view && !wasTouched) {
            // Mark the touch as in-progress so the SCUI dispatcher
            // can skip expensive redraws that would recurse into the
            // view via a JPEG decode / Spotify API call happening in
            // the same frame.
            g_touchInProgress = true;
            lastX = p.x;
            lastY = p.y;
            touchStartMs = millis();
            view->onTouchDown(p.x, p.y);
            spLogD(LOGTAG_INPUT, "Touch DOWN at x=%d y=%d z=%d", p.x, p.y, p.z);
            // Yield after a button press so a slow HTTPS call inside
            // the action (e.g. spotify.pause / nextSong) doesn't block
            // the task watchdog.
            vTaskDelay(pdMS_TO_TICKS(10));
        } else if (view && wasTouched) {
            lastX = p.x;
            lastY = p.y;
            view->onTouchMove(p.x, p.y);
        }
        wasTouched = true;
    } else {
        if (wasTouched) {
            UIView* view = viewMgr.getActiveView();
            if (view) {
                view->onTouchUp(lastX, lastY);
                spLogD(LOGTAG_INPUT, "Touch UP at x=%d y=%d (held %lums)",
                       lastX, lastY, millis() - touchStartMs);
            }
            g_touchInProgress = false;
        }
        wasTouched = false;
    }
}

// Tracks whether the user is currently pressing the screen. The UI
// task will skip redraws while a touch is in progress to avoid
// stack-expensive reentrant calls into the view (e.g. JPEG decode
// happening inside a touch handler can exceed the task watchdog).
extern volatile bool g_touchInProgress;

void dispatchSCUIQueueMessages() {
    static SCUIMessage msg;
    if (scuiQueue == NULL) return;
    esp_task_wdt_reset();

    if (xQueueReceive(scuiQueue, &msg, 0) != pdPASS) {
        msg.type = SCUIMessageType::UM_IDLE;
        msg.str[0] = '\0';
        msg.num = 0;
    }

    if (msg.type == SCUIMessageType::UM_TOAST) viewMgr.showToast(msg.str);
    // A newer firmware was found: ask once per version, and only over Home (never mid-task).
    if (msg.type == SCUIMessageType::UM_UPDATE_STATUS && msg.num == 1) {
        static char prompted[sizeof(msg.str)] = "";
        UIView* v = viewMgr.getActiveView();
        // A version that already failed and was rolled back is not offered again on its own.
        if (strcmp(prompted, msg.str) != 0 && v && v->getID() == ViewID::Home && vault.getSkipVersion() != msg.str) {
            strlcpy(prompted, msg.str, sizeof(prompted));
            viewMgr.gotoView(ViewID::Update);
        }
    }
    // Account the app does not accept (not allowlisted / not Premium): explain once, over Home.
    static int shownIssue = 0;
    int issue = spotifyPlayer.getAccountIssue();
    if (issue && issue != shownIssue && viewMgr.getActiveView() && viewMgr.getActiveView()->getID() == ViewID::Home) {
        shownIssue = issue;
        accountView->setIssue(issue);
        viewMgr.gotoView(ViewID::Account);
    }
    viewMgr.tick();
    UIView* view = viewMgr.getActiveView();
    if (view) view->handleMessage(&msg);

    // Skip the redraw if the user is currently holding the screen.
    // A full-screen redraw mid-touch can recurse into the same view
    // (via the message handlers) and exceed the task watchdog. The
    // view will be redrawn on the next frame when the touch is up.
    if (view && view->isDirty() && !g_touchInProgress) {
        view->draw();
        view->markClean();
        esp_task_wdt_reset();
    }
}

void checkFactoryResetButton() {
#if !FACTORY_RESET_ENABLED
    // Disabled by compile flag.
    (void)digitalRead(FACTORY_RESET_PIN);
    return;
#else
    static unsigned long pressStart = 0;
    static bool overlayShown = false;
    static bool triggeredThisHold = false;

    // GPIO 0 (BOOT) shares the SPI bus with display+touch, so any
    // SPI transaction can briefly pull it LOW. Require the line to
    // be held LOW for many consecutive samples before treating it
    // as a real button press — a quick tap (a few ms) will never
    // reach this counter, so it does nothing.
    static uint8_t lowStreak = 0;
    const uint8_t LOW_STREAK_REQUIRED = 50;

    bool rawPressed = (digitalRead(FACTORY_RESET_PIN) == LOW);
    if (rawPressed) {
        if (lowStreak < LOW_STREAK_REQUIRED) lowStreak++;
    } else {
        lowStreak = 0;
    }
    bool pressed = (lowStreak >= LOW_STREAK_REQUIRED);

    if (pressed) {
        if (pressStart == 0) {
            pressStart = millis();
            triggeredThisHold = false;
        }
        unsigned long held = millis() - pressStart;

        if (held >= FACTORY_RESET_HOLD_MS && !triggeredThisHold) {
            triggeredThisHold = true;
            spLogW(LOGTAG_GENERAL, "Factory reset triggered (BOOT held %lums)",
                   held);
            displayUI.clear(C_BG);
            displayUI.setTextColor(C_ERROR, C_BG);
            displayUI.drawCentreString("Reset de fábrica!", CENTER_X, CENTER_Y - 26, FONT_SM);
            displayUI.setTextColor(C_SECONDARY, C_BG);
            displayUI.drawCentreString("Apagando configuração...",
                                       CENTER_X, CENTER_Y - 4, 1);
            displayUI.drawProgressBar(CENTER_X - 100, CENTER_Y + 16, 200, 10, 100, C_BG, C_ERROR);
            displayUI.setTextColor(C_WHITE, C_BG);
            displayUI.drawCentreString("Reiniciando...", CENTER_X, CENTER_Y + 38, 1);
            vault.factoryReset();
            delay(900);
            ESP.restart();
            return;
        }

        if (held > 800) {
            overlayShown = true;
            int pct = (int)min((unsigned long)100, held * 100 / FACTORY_RESET_HOLD_MS);
            UIView* view = viewMgr.getActiveView();
            if (view) view->draw();
            displayUI.fillRoundRect(CENTER_X - 120, CENTER_Y - 50, 240, 100, 14, C_CARD);
            displayUI.drawRoundRect(CENTER_X - 120, CENTER_Y - 50, 240, 100, 14, C_ERROR);
            displayUI.setTextColor(C_ERROR, C_CARD);
            displayUI.drawCentreString("Reset de fábrica",
                                       CENTER_X, CENTER_Y - 36, FONT_SM);
            int secondsLeft = (int)((FACTORY_RESET_HOLD_MS - held + 999) / 1000);
            if (secondsLeft < 1) secondsLeft = 1;
            char countdown[16];
            snprintf(countdown, sizeof(countdown), "Segure %ds...", secondsLeft);
            displayUI.setTextColor(C_SECONDARY, C_CARD);
            displayUI.drawCentreString(countdown, CENTER_X, CENTER_Y - 14, 1);
            displayUI.drawProgressBar(CENTER_X - 96, CENTER_Y + 6, 192, 12, pct, C_BG, C_ERROR);
            displayUI.setTextColor(C_WHITE, C_CARD);
            displayUI.drawCentreString("Solte para cancelar",
                                       CENTER_X, CENTER_Y + 30, 1);
        }
    } else {
        if (overlayShown) {
            overlayShown = false;
            UIView* view = viewMgr.getActiveView();
            if (view) view->draw();
        }
        pressStart = 0;
        triggeredThisHold = false;
    }
#endif
}

void uiHandlerTask(void *pvParams) {
    spLogI(LOGTAG_MULTITASK, "UI handler task running");
    while (true) {
        handleTouchInput();
        dispatchSCUIQueueMessages();
        checkFactoryResetButton();
        // Give the lower-priority network/refresh tasks a chance to
        // run so a blocking HTTPS call from a button press doesn't
        // starve them and trigger the watchdog.
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// GPIO monitor — runs as a FreeRTOS task on Core 1 only while
// FACTORY_RESET_ENABLED is 0. Polls every candidate GPIO every
// ~200 ms and prints a single line listing all of them. The user
// can press the physical button and watch the log to see which
// GPIO transitions from H to L, then report the number back.
static const unsigned long MONITOR_PERIOD_MS = 200;

void gpioMonitorTask(void *pvParams) {
    (void)pvParams;
    spLogI("GPIOMon", "GPIO monitor active. Press each physical button.");
    spLogI("GPIOMon", "Line format: GPIO<pin>=<H|L> (H = idle, L = pressed)");
    char list[160];
    list[0] = '\0';
    for (size_t i = 0; i < sizeof(RESET_BUTTON_CANDIDATES) / sizeof(RESET_BUTTON_CANDIDATES[0]); i++) {
        char tmp[8];
        snprintf(tmp, sizeof(tmp), "%d%s",
                 RESET_BUTTON_CANDIDATES[i],
                 i + 1 < sizeof(RESET_BUTTON_CANDIDATES) / sizeof(RESET_BUTTON_CANDIDATES[0]) ? "," : "");
        strncat(list, tmp, sizeof(list) - strlen(list) - 1);
    }
    spLogI("GPIOMon", "Watching: %s", list);

    unsigned long lastLog = 0;
    while (true) {
        unsigned long now = millis();
        if (now - lastLog < MONITOR_PERIOD_MS) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        lastLog = now;
        char line[200];
        int off = 0;
        for (size_t i = 0; i < sizeof(RESET_BUTTON_CANDIDATES) / sizeof(RESET_BUTTON_CANDIDATES[0]); i++) {
            int pin = RESET_BUTTON_CANDIDATES[i];
            int level = digitalRead(pin);
            int n = snprintf(line + off, sizeof(line) - off,
                             "GPIO%d=%s ",
                             pin, level == LOW ? "L" : "H");
            if (n < 0 || n >= (int)(sizeof(line) - off)) break;
            off += n;
        }
        if (off > 0) {
            spLogI("GPIOMon", "%s", line);
        }
    }
}

void loop() {
    acceptFirmwareWhenStable();
    // The Arduino loop() task is unused now that the GPIO monitor
    // runs as its own FreeRTOS task. Keep a low-overhead idle here
    // so the scheduler still has something to schedule.
    static unsigned long lastMetrics = 0;
    if (millis() - lastMetrics > 60000) {
        lastMetrics = millis();
        spLogI(LOGTAG_GENERAL, "Heap: %u bytes free", ESP.getFreeHeap());
    }
    vTaskDelay(pdMS_TO_TICKS(500));
}
