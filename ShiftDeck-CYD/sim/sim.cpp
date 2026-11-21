// Desktop preview of the ShiftDeck screens.
//
// Compiles the real TFT_eSPI (generic backend), DisplayUI and views against
// small shims; SPI traffic feeds an ILI9341 emulator that fills a 320x240
// framebuffer, dumped as PPM per scenario. Run: sim/run.sh
#include <map>
#include <string>
#include <vector>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include "DisplayUI.h"
#include "UIViewManager.h"
#include "SpotifyPlayer.h"
#include "Vault.h"
#include "TouchCalibration.h"
#include "Views/BootView.h"
#include "Views/HomeView.h"
#include "Views/SearchView.h"
#include "Views/PlaylistView.h"
#include "Views/SettingsView.h"
#include "Views/WiFiSetupView.h"
#include "Views/SpotifyConfigView.h"
#include "Views/SpotifyAuthView.h"
#include "Views/WelcomeView.h"
#include "Views/UpdateView.h"
#include "Updater.h"
#include <cassert>
#include "SCLogger.h"

// ------------------------------------------------------------ ILI9341 emulator
static uint16_t fb[240][320];
static int dcHigh = 1, cmd = 0, argi = 0, hiByte = -1;
static uint8_t args[4];
static int xs = 0, xe = 319, ys = 0, ye = 239, px = 0, py = 0;

uint8_t SPIClass::transfer(uint8_t b) {
    if (!dcHigh) {
        cmd = b; argi = 0; hiByte = -1;
        if (cmd == 0x2C) { px = xs; py = ys; }
        return 0;
    }
    if (cmd == 0x2A || cmd == 0x2B) {
        if (argi < 4) args[argi++] = b;
        if (argi == 4) {
            int a = (args[0] << 8) | args[1], c = (args[2] << 8) | args[3];
            if (cmd == 0x2A) { xs = a; xe = c; } else { ys = a; ye = c; }
        }
    } else if (cmd == 0x2C) {
        if (hiByte < 0) { hiByte = b; return 0; }
        uint16_t col = (uint16_t)((hiByte << 8) | b);
        hiByte = -1;
        if (px >= 0 && px < 320 && py >= 0 && py < 240) fb[py][px] = col;
        if (++px > xe) { px = xs; py++; }
    }
    return 0;
}

SPIClass SPI;
SerialStub Serial;
EspStub ESP;
WiFiStub WiFi;
LittleFSStub LittleFS;
TJpg_Decoder TJpgDec;

static unsigned long g_ms = 100000;
unsigned long millis() { return g_ms; }
unsigned long micros() { return g_ms * 1000; }
void delay(unsigned long ms) { g_ms += ms; }
void pinMode(int, int) {}
int digitalRead(int) { return 1; }
void digitalWrite(int pin, int v) { if (pin == TFT_DC) dcHigh = v; }

// ------------------------------------------------------------ assets / JPEG
static std::string g_assetDir;
static std::map<std::string, std::string> g_files;   // LittleFS path -> asset name

static bool loadAsset(const char* path, std::vector<uint16_t>& px, uint16_t& w, uint16_t& h) {
    auto it = g_files.find(path);
    if (it == g_files.end()) return false;
    FILE* f = fopen((g_assetDir + "/" + it->second + ".rgb").c_str(), "rb");
    if (!f) return false;
    fread(&w, 2, 1, f);
    fread(&h, 2, 1, f);
    px.resize((size_t)w * h);
    fread(px.data(), 2, px.size(), f);
    fclose(f);
    return true;
}

bool LittleFSStub::exists(const char* path) { return g_files.count(path) > 0; }

JRESULT TJpg_Decoder::getFsJpgSize(uint16_t* w, uint16_t* h, const char* file, LittleFSStub&) {
    std::vector<uint16_t> px;
    if (!loadAsset(file, px, *w, *h)) { *w = *h = 0; return JDR_INP; }
    return JDR_OK;
}

JRESULT TJpg_Decoder::drawFsJpg(int32_t x, int32_t y, const char* file, LittleFSStub&) {
    std::vector<uint16_t> src;
    uint16_t w, h;
    if (!loadAsset(file, src, w, h)) return JDR_INP;
    int ow = (w + _scale - 1) / _scale, oh = (h + _scale - 1) / _scale;
    for (int by = 0; by < oh; by += 16) {
        for (int bx = 0; bx < ow; bx += 16) {
            int bw = std::min(16, ow - bx), bh = std::min(16, oh - by);
            uint16_t block[256];
            for (int j = 0; j < bh; j++)
                for (int i = 0; i < bw; i++) {
                    uint16_t c = src[(size_t)std::min((by + j) * _scale, h - 1) * w + std::min((bx + i) * _scale, w - 1)];
                    block[j * bw + i] = _swap ? (uint16_t)((c >> 8) | (c << 8)) : c;
                }
            _cb(x + bx, y + by, bw, bh, block);
        }
    }
    return JDR_OK;
}

// ------------------------------------------------------------ fakes
struct SimState {
    bool music = true, api = true, cover = true, liked = true, playing = true;
    long progress = 95000, duration = 178000;
    std::vector<MediaItem> playlists, results;
    std::vector<DeviceInfo> devices;
    String latest = "";
} g;

XPT2046Touch touch;
SCLogger& SCLogger::getInstance() { static SCLogger l; return l; }
void SCLogger::log(const char*, uint8_t, const char*, ...) {}
void runTouchCalibration(DisplayUI&, XPT2046Touch&) {}

Vault& Vault::getInstance() { static Vault v; return v; }
int Vault::getBrightness() { return _brightness; }
void Vault::setBrightness(int v) { _brightness = v; }
String Vault::getWiFiSSID() { return "Casa_5G"; }
String Vault::getWiFiPassword() { return ""; }
String Vault::getSpotifyClientID() { return "3f9a1c7e5b2d4e8f9a0b1c2d3e4f5a6b"; }
void Vault::setSpotifyCredentials(const String&, const String&) {}
void Vault::setSpotifyRefreshToken(const String&) {}
void Vault::setWiFiCredentials(const String&, const String&) {}

PlayingMetadata::PlayingMetadata() { clear(); }
void PlayingMetadata::clear() { memset(this, 0, sizeof(*this)); }
void PlayingMetadata::copyFrom(const PlayingMetadata& o) { memcpy(this, &o, sizeof(*this)); }

SpotifyPlayer::SpotifyPlayer() {}
SpotifyPlayer& SpotifyPlayer::getInstance() { static SpotifyPlayer p; return p; }
void SpotifyPlayer::nextSong() {}
void SpotifyPlayer::previousSong() {}
void SpotifyPlayer::pauseSong() {}
void SpotifyPlayer::toggleLike() { g.liked = !g.liked; }
void SpotifyPlayer::playItem(const MediaItem&) {}
void SpotifyPlayer::search(const String&) {}
void SpotifyPlayer::fetchPlaylists() {}
void SpotifyPlayer::fetchDevices() {}
void SpotifyPlayer::transferTo(const String&) {}
void SpotifyPlayer::checkForUpdate() {}
void SpotifyPlayer::installUpdate() {}
void SpotifyPlayer::setVolume(int) {}
int SpotifyPlayer::getActiveVolume() { return 60; }
String SpotifyPlayer::getLatestVersion() { return g.latest; }
bool SpotifyPlayer::isMusicAvailable() { return g.music; }
bool SpotifyPlayer::isApiOnline() { return g.api; }
bool SpotifyPlayer::isCoverArtReady() { return g.cover; }
bool SpotifyPlayer::isLiked() { return g.liked; }
bool SpotifyPlayer::isSearchBusy() { return false; }
bool SpotifyPlayer::isPlaylistsBusy() { return false; }
std::vector<MediaItem> SpotifyPlayer::getSearchResults() { return g.results; }
std::vector<MediaItem> SpotifyPlayer::getPlaylists() { return g.playlists; }
std::vector<DeviceInfo> SpotifyPlayer::getDevices() { return g.devices; }
unsigned long SpotifyPlayer::getLastRefreshMs() { return g_ms; }
String SpotifyPlayer::getActiveDeviceType() { for (auto& d : g.devices) if (d.active) return deviceTypeLabel(d.type); return ""; }
String SpotifyPlayer::getActiveDeviceName() { return ""; }
const PlayingMetadata& SpotifyPlayer::getCurrentlyPlayingMetadata() {
    static PlayingMetadata m;
    m.clear();
    if (g.music) {
        strcpy(m.trackName, "Malvadão 3");
        strcpy(m.artistName, "Xamã");
        strcpy(m.albumName, "Malvadão 3 - Single");
        strcpy(m.trackUri, "spotify:track:sim");
        m.isPlaying = g.playing;
        m.progressMs = g.progress;
        m.durationMs = g.duration;
    }
    return m;
}
String SpotifyPlayer::thumbPath(const String& url) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < url.length(); i++) { h ^= (uint8_t)url[i]; h *= 16777619u; }
    char buf[20];
    snprintf(buf, sizeof(buf), "/t_%08x.jpg", (unsigned)h);
    return buf;
}
String SpotifyPlayer::deviceTypeLabel(const String& t) {
    if (t == "Smartphone") return "Celular";
    if (t == "Computer") return "Computador";
    if (t == "Speaker") return "Alto-falante";
    return t;
}

// ------------------------------------------------------------ scenarios
static void dump(const std::string& outDir, const char* name) {
    std::string p = outDir + "/" + name + ".ppm";
    FILE* f = fopen(p.c_str(), "wb");
    fprintf(f, "P6\n320 240\n255\n");
    for (int y = 0; y < 240; y++)
        for (int x = 0; x < 320; x++) {
            uint16_t c = fb[y][x];
            uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) };
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
    printf("wrote %s\n", p.c_str());
}

static MediaItem item(MediaItem::Kind k, const char* name, const char* sub, const char* asset) {
    MediaItem m;
    m.kind = k;
    m.name = name;
    m.subtitle = sub;
    if (asset) {
        m.imageUrl = String("sim://") + asset;
        g_files[SpotifyPlayer::thumbPath(m.imageUrl).c_str()] = asset;
        m.thumb = 1;
    } else {
        m.thumb = 2;
    }
    return m;
}

static void tap(UIView* v, int x, int y) { v->onTouchDown(x, y); v->onTouchUp(x, y); }
static void send(UIView* v, SCUIMessageType t, int num = 0) { SCUIMessage m{}; m.type = t; m.num = num; v->handleMessage(&m); }
static void settle(UIView* v) { if (v->isDirty()) { v->draw(); v->markClean(); } }

int main(int argc, char** argv) {
    g_assetDir = argc > 1 ? argv[1] : "sim/assets";
    std::string out = argc > 2 ? argv[2] : "sim/out";
    g_files["/cover.jpg"] = "cover";

    TFT_eSPI tft;
    tft.init();
    tft.setRotation(1);
    DisplayUI ui(&tft);
    ui.init();

    BootView boot(&ui);
    HomeView home(&ui);
    SearchView search(&ui);
    PlaylistView playlists(&ui);
    SettingsView settings(&ui);
    UIViewManager& vm = UIViewManager::getInstance();
    vm.setDisplayUI(&ui);
    vm.registerView(ViewID::Boot, &boot);
    vm.registerView(ViewID::Home, &home);
    vm.registerView(ViewID::Search, &search);
    vm.registerView(ViewID::Playlist, &playlists);
    vm.registerView(ViewID::Settings, &settings);

    g.devices = { {"a1", "Galaxy S23", "Smartphone", true}, {"b2", "MacBook Pro", "Computer", false}, {"c3", "Sala JBL", "Speaker", false} };
    g.playlists = {
        item(MediaItem::Liked, "Curtidas", "312 músicas", nullptr),
        item(MediaItem::Playlist, "Rap Nacional", "98 músicas", "pl_rap"),
        item(MediaItem::Playlist, "Treino", "45 músicas", "pl_treino"),
        item(MediaItem::Playlist, "Descobertas", "74 músicas", "pl_desc"),
        item(MediaItem::Playlist, "Chill", "123 músicas", "pl_chill"),
        item(MediaItem::Playlist, "Rock Clássico", "210 músicas", nullptr),
    };

    // 1. boot
    vm.gotoView(ViewID::Boot);
    boot.setStep(BootView::STEP_DISPLAY, BootView::OK);
    boot.setStep(BootView::STEP_TOUCH, BootView::OK);
    boot.setStep(BootView::STEP_WIFI, BootView::OK);
    boot.setStep(BootView::STEP_BACKEND, BootView::OK);
    boot.setStep(BootView::STEP_SPOTIFY, BootView::LOADING);
    boot.setProgress(70);
    dump(out, "1_boot");
    boot.setNotice("Energia fraca: use fonte de 1 A", C_WARN);
    boot.markDirty();
    boot.draw();
    dump(out, "1b_boot_brownout");

    // 2. home
    vm.gotoView(ViewID::Home);
    dump(out, "2_home");
    g_ms += 900;
    send(&home, SCUIMessageType::UM_IDLE);
    dump(out, "2b_home_tick");
    vm.showToast("Nenhum dispositivo ativo");
    dump(out, "2c_home_toast");

    // 3. playlists with "Treino" selected
    vm.gotoView(ViewID::Playlist);
    send(&playlists, SCUIMessageType::UM_PLAYLISTS_DONE, 6);
    tap(&playlists, 160, 34 + 2 + 2 * 34 + 12);
    dump(out, "3_playlists");

    // 4. search: type "Xama" on the keyboard, then show results
    vm.back();
    vm.gotoView(ViewID::Search);
    dump(out, "4a_search_keyboard_empty");
    auto key = [&](int row, int col) {
        const int pitch = 31, kw = 28;
        int n = row == 0 ? 10 : row == 1 ? 9 : 7;
        int extra = row == 2 ? pitch - kw + 58 : 0;
        int total = n * pitch - (pitch - kw) + extra;
        int x0 = (320 - total) / 2;
        tap(&search, x0 + col * pitch + 14, 62 + row * 42 + 19);
        g_ms += 200;
        send(&search, SCUIMessageType::UM_IDLE);
    };
    key(2, 1); key(1, 0); key(2, 6); key(1, 0);   // X A M A
    dump(out, "4b_search_typing");
    g.results = {
        item(MediaItem::Track, "Malvadão 3", "Música · Xamã", "cover"),
        item(MediaItem::Artist, "Xamã", "Artista", "art_xama"),
        item(MediaItem::Album, "Malvadão 3", "Álbum · Xamã", "cover"),
        item(MediaItem::Playlist, "Rap Nacional", "Playlist", nullptr),
        item(MediaItem::Track, "Luxúria", "Música · Xamã, Brandão85", nullptr),
    };
    tap(&search, 260, 62 + 3 * 42 + 19);              // "Buscar" key
    dump(out, "4c_search_loading");
    send(&search, SCUIMessageType::UM_SEARCH_DONE, 5);
    dump(out, "4d_search_results");

    // 5. settings and panels
    vm.home();
    vm.gotoView(ViewID::Settings);
    dump(out, "5a_settings");
    tap(&settings, 160, 34 + 2 * 29 + 10);            // Brilho
    dump(out, "5b_settings_brightness");
    tap(&settings, 20, 16);                           // back to list
    tap(&settings, 160, 34 + 3 * 29 + 10);            // Dispositivo
    send(&settings, SCUIMessageType::UM_DEVICES_DONE, 3);
    dump(out, "5c_settings_device");
    tap(&settings, 20, 16);
    tap(&settings, 160, 34 + 5 * 29 + 10);            // Diagnóstico
    dump(out, "5d_settings_diag");
    tap(&settings, 20, 16);
    tap(&settings, 160, 34 + 0 * 29 + 10);            // Wi-Fi
    dump(out, "5e_settings_wifi");
    tap(&settings, 20, 16);
    g.latest = "9.9.9";                               // a newer version is published
    tap(&settings, 160, 34 + 4 * 29 + 10);            // Atualização
    dump(out, "5f_settings_update");
    tap(&settings, 20, 16);
    tap(&settings, 160, 34 + 1 * 29 + 10);            // Conta Spotify
    dump(out, "5g_settings_account");
    g.latest = VERSION;

    // version comparison used by the updater
    assert(Updater::isNewer("1.2.10", "1.2.9") && Updater::isNewer("2.0.0", "1.9.9") && Updater::isNewer("1.3", "1.2.9"));
    assert(!Updater::isNewer("1.2.2", "1.2.2") && !Updater::isNewer("1.2.1", "1.2.2") && !Updater::isNewer("", "1.2.2"));

    // 6. empty / offline states
    g.music = false; g.cover = false; g.api = false;
    g.devices.clear();
    WiFi.simStatus = 0;
    vm.home();
    home.markDirty();
    settle(&home);
    dump(out, "6_home_offline");

    // 7. first-run setup screens (not in the mockups, must stay legible)
    WiFiSetupView wifiSetup(&ui);
    wifiSetup.setNetwork("Shiftdeck-Spotify", "SftDckSp0t");
    wifiSetup.draw();
    dump(out, "7a_setup_wifi");
    SpotifyConfigView cfg(&ui);
    cfg.setIP("192.168.0.42");
    cfg.draw();
    dump(out, "7b_setup_spotify");
    SpotifyAuthView auth(&ui);
    auth.setIP("192.168.0.42");
    auth.setWaitingSeconds(40);
    auth.draw();
    dump(out, "7c_setup_oauth");
    auth.showError("Confira o Client ID");
    dump(out, "7d_setup_oauth_error");

    // 8. first boot presentation and firmware update
    WelcomeView welcome(&ui);
    welcome.draw();
    dump(out, "8a_welcome");
    welcome.showPage(1);
    welcome.draw();
    dump(out, "8b_welcome_requirements");
    UpdateView upd(&ui);
    upd.preview("1.3.0", -1);
    upd.draw();
    dump(out, "9a_update_prompt");
    upd.preview("1.3.0", 42);
    upd.draw();
    dump(out, "9b_update_progress");
    upd.preview("1.3.0", 0, "Falha no download");
    upd.draw();
    dump(out, "9c_update_failed");
    return 0;
}
