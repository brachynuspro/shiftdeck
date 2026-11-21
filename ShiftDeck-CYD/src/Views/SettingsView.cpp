/*-------------------------------------------------------------------------------------------------
** SettingsView.cpp - Settings screen implementation (IMGS/config.png)
** ------------------------------------------------------------------------------------------------
*/
#include "SettingsView.h"
#include "UIViewManager.h"
#include "Vault.h"
#include "XPT2046Touch.h"
#include "TouchCalibration.h"
#include "Updater.h"
#include "WiFiSetup.h"
#include <WiFi.h>

// Global in main.cpp; reused for recalibration from Diagnóstico.
extern XPT2046Touch touch;

SettingsView::Panel SettingsView::s_nextPanel = SettingsView::PANEL_LIST;

static const char* const PANEL_TITLES[] = { "Configurações", "Wi-Fi", "Conta Spotify", "Brilho", "Dispositivo", "Atualização", "Diagnóstico",
                                            "Adicionar rede" };

SettingsView::SettingsView(DisplayUI *ui) : UIView(ui) {
    _id = ViewID::Settings;
}

void SettingsView::onEnter() {
    _brightness = _savedBrightness = Vault::getInstance().getBrightness();
    _panel = PANEL_LIST;
    _confirmUntil = 0;
    WiFiSetup::getInstance().stop();
    _devices = SpotifyPlayer::getInstance().getDevices();
    if (s_nextPanel != PANEL_LIST) openPanel(s_nextPanel);
    s_nextPanel = PANEL_LIST;
    _dirty = true;
}

void SettingsView::openPanel(Panel p) {
    // The Wi-Fi page is served only while its QR is on screen.
    if (p == PANEL_WIFI_ADD) WiFiSetup::getInstance().startWebPage();
    else                     WiFiSetup::getInstance().stop();
    _panel = p;
    _confirmUntil = 0;
    _confirmRow = -1;
    if (p == PANEL_DEVICE) {
        _devicesLoaded = false;
        SpotifyPlayer::getInstance().fetchDevices();
    }
}

bool SettingsView::diagnosticsOk() {
    return WiFi.status() == WL_CONNECTED && SpotifyPlayer::getInstance().isApiOnline() && ESP.getFreeHeap() > 40000;
}

/* ---------------------------------------------------------------- drawing */

void SettingsView::draw() {
    _ui->clear(C_BG);
    drawBody();
    _dirty = false;
}

void SettingsView::drawBody() {
    _ui->fillRect(0, 0, SCREEN_W, SCREEN_H, C_BG);
    drawHeader(PANEL_TITLES[_panel]);
    switch (_panel) {
        case PANEL_LIST:
            drawListRows();
            drawFooter("Voltar", IconID::Undo, "Salvar", IconID::Save, true);
            break;
        case PANEL_WIFI:       drawWifiPanel(); break;
        case PANEL_ACCOUNT:    drawAccountPanel(); break;
        case PANEL_BRIGHTNESS: drawBrightnessPanel(); break;
        case PANEL_DEVICE:     drawDevicePanel(); break;
        case PANEL_UPDATE:     drawUpdatePanel(); break;
        case PANEL_DIAG:       drawDiagPanel(); break;
        case PANEL_WIFI_ADD:   drawWifiAddPanel(); break;
    }
}

void SettingsView::drawHeader(const char* title) {
    _ui->drawIcon(IconID::Back, 18, HEADER_H / 2, 18, C_WHITE, C_BG);
    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawCentreString(title, CENTER_X, _ui->textY(FONT_LG, HEADER_H / 2), FONT_LG);
}

void SettingsView::drawRow(int i, IconID icon, const char* label, const String& value, uint16_t valueColor, bool chevron) {
    int32_t y = ROW_Y + i * ROW_PITCH, cy = y + ROW_H / 2;
    _ui->fillRoundRect(ROW_X, y, ROW_W, ROW_H, 9, C_CARD, C_BG);
    _ui->drawIcon(icon, ROW_X + 20, cy, 18, C_WHITE, C_CARD);
    _ui->setTextColor(C_WHITE, C_CARD);
    int32_t labelW = _ui->textWidth(label, FONT_MD);
    _ui->drawString(label, ROW_X + 38, _ui->textY(FONT_MD, cy), FONT_MD);
    int32_t right = ROW_X + ROW_W - (chevron ? 30 : 12);
    if (value.length()) {
        _ui->setTextColor(valueColor, C_CARD);
        String v = _ui->fitText(value, right - (ROW_X + 38 + labelW + 14), FONT_MD);
        _ui->drawRightString(v.c_str(), right, _ui->textY(FONT_MD, cy), FONT_MD);
    }
    if (chevron) _ui->drawIcon(IconID::ChevronRight, ROW_X + ROW_W - 16, cy, 12, C_WHITE, C_CARD);
}

void SettingsView::drawFooter(const char* left, IconID leftIcon, const char* right, IconID rightIcon, bool rightPrimary) {
    _ui->fillRect(0, FOOT_Y - 2, SCREEN_W, SCREEN_H - FOOT_Y + 2, C_BG);
    if (!right) {
        _ui->drawButton(ROW_X, FOOT_Y, ROW_W, FOOT_H, left, leftIcon, true, C_BUTTON, C_WHITE, C_BG, FONT_MD, 10);
        return;
    }
    _ui->drawButton(ROW_X, FOOT_Y, FOOT_W, FOOT_H, left, leftIcon, true, C_BUTTON, C_WHITE, C_BG, FONT_MD, 10);
    bool confirming = _confirmUntil && millis() < _confirmUntil && _confirmRow < 0;
    uint16_t fill = confirming ? C_ERROR : (rightPrimary ? C_GREEN_DARK : C_BUTTON);
    _ui->drawButton(ROW_X + ROW_W - FOOT_W, FOOT_Y, FOOT_W, FOOT_H, right, rightIcon, true, fill, C_WHITE, C_BG, FONT_MD, 10);
}

void SettingsView::drawListRows() {
    bool wifiOk = WiFi.status() == WL_CONNECTED;
    String dev = SpotifyPlayer::getInstance().getActiveDeviceType();
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", _brightness);
    drawRow(0, IconID::Wifi, "Wi-Fi", wifiOk ? WiFi.SSID() : String("Desconectado"), wifiOk ? C_GREEN : C_ERROR, true);
    bool api = SpotifyPlayer::getInstance().isApiOnline();
    drawRow(1, IconID::Spotify, "Conta Spotify", api ? "Conectada" : "Offline", api ? C_GREEN : C_ERROR, true);
    drawRow(2, IconID::Brightness, "Brilho", pct, C_GREEN, true);
    drawRow(3, IconID::Phone, "Dispositivo", dev.length() ? dev : String("Nenhum"), dev.length() ? C_GREEN : C_MUTED, true);
    bool newer = Updater::isNewer(SpotifyPlayer::getInstance().getLatestVersion().c_str(), VERSION);
    drawRow(4, IconID::Update, "Atualização", newer ? "Nova versão" : VERSION_SHORT, newer ? C_WARN : C_GREEN, true);
    bool ok = diagnosticsOk();
    drawRow(5, IconID::Diagnostics, "Diagnóstico", ok ? "OK" : "Atenção", ok ? C_GREEN : C_WARN, true);
}

// Saved networks, most recent first. Tap one twice to delete it; "Adicionar" serves the page
// that adds and edits them (WiFiSetup), since typing a password needs the phone.
void SettingsView::drawWifiPanel() {
    std::vector<WiFiNet> nets = Vault::getInstance().getWiFiNetworks();
    bool online = WiFi.status() == WL_CONNECTED;
    bool confirming = _confirmUntil && millis() < _confirmUntil;
    for (size_t i = 0; i < nets.size(); i++) {
        bool current = online && WiFi.SSID() == nets[i].ssid;
        char sig[24];
        snprintf(sig, sizeof(sig), "Conectada %d dBm", current ? (int)WiFi.RSSI() : 0);
        if (confirming && _confirmRow == (int)i)
            drawRow(i, IconID::Close, _ui->fitText(nets[i].ssid, 150, FONT_MD).c_str(), "Excluir?", C_ERROR, false);
        else
            drawRow(i, current ? IconID::Wifi : IconID::WifiOff, _ui->fitText(nets[i].ssid, 150, FONT_MD).c_str(),
                    current ? String(sig) : String("Salva"), current ? C_GREEN : C_MUTED, false);
    }
    _ui->setTextColor(C_MUTED, C_BG);
    int32_t hintY = ROW_Y + (int)nets.size() * ROW_PITCH + 10;
    if (nets.empty()) _ui->drawCentreString("Nenhuma rede salva", CENTER_X, _ui->textY(FONT_SM, 90), FONT_SM);
    else if (nets.size() < Vault::MAX_WIFI) {
        _ui->drawCentreString("Conecta sozinha na de sinal mais forte.", CENTER_X, _ui->textY(FONT_XS, hintY), FONT_XS);
        _ui->drawCentreString("Toque 2 vezes numa rede para excluir.", CENTER_X, _ui->textY(FONT_XS, hintY + 16), FONT_XS);
    }
    drawFooter("Voltar", IconID::Undo, "Adicionar", IconID::Wifi, true);
}

void SettingsView::drawWifiAddPanel() {
    if (WiFi.status() != WL_CONNECTED) {   // the player task restarts into the setup portal after 60 s
        _ui->drawIcon(IconID::WifiOff, CENTER_X, 90, 34, C_MUTED, C_BG);
        _ui->setTextColor(C_WHITE, C_BG);
        _ui->drawCentreString("Sem Wi-Fi agora", CENTER_X, _ui->textY(FONT_MD_B, 128), FONT_MD_B);
        _ui->setTextColor(C_MUTED, C_BG);
        _ui->drawCentreString("Em 1 minuto a ShiftDeck abre a rede", CENTER_X, _ui->textY(FONT_SM, 152), FONT_SM);
        _ui->drawCentreString("dela, com QR, para você configurar", CENTER_X, _ui->textY(FONT_SM, 172), FONT_SM);
        drawFooter("Voltar", IconID::Undo, nullptr, IconID::Undo, false);
        return;
    }
    const int32_t qx = 14, qy = 44, qs = 124, tx = qx + qs + 14;
    String url = "http://" + WiFi.localIP().toString() + "/";
    _ui->fillRoundRect(qx, qy, qs, qs, 10, C_WHITE, C_BG);
    drawQR(qx, qy, qs, url);
    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawString("Aponte a câmera", tx, _ui->textY(FONT_MD_B, 56), FONT_MD_B);
    _ui->drawString("do celular", tx, _ui->textY(FONT_MD_B, 76), FONT_MD_B);
    _ui->setTextColor(C_SECONDARY, C_BG);
    _ui->drawString("Ele precisa estar", tx, _ui->textY(FONT_SM, 104), FONT_SM);
    _ui->drawString("nesta mesma rede.", tx, _ui->textY(FONT_SM, 122), FONT_SM);
    _ui->drawString("Lá você adiciona,", tx, _ui->textY(FONT_SM, 144), FONT_SM);
    _ui->drawString("edita e exclui.", tx, _ui->textY(FONT_SM, 162), FONT_SM);
    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawCentreString(url.c_str(), CENTER_X, _ui->textY(FONT_SM, 188), FONT_SM);
    drawFooter("Voltar", IconID::Undo, nullptr, IconID::Undo, false);
}

void SettingsView::drawAccountPanel() {
    bool api = SpotifyPlayer::getInstance().isApiOnline();
    String cid = Vault::getInstance().getSpotifyClientID();
    drawRow(0, IconID::Spotify, "Login", api ? "Conectado" : "Offline", api ? C_GREEN : C_ERROR, false);
    drawRow(1, IconID::Settings, "App (Client ID)", cid.length() > 6 ? cid.substring(0, 6) + "..." : cid, C_SECONDARY, false);
    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawCentreString("Sair apaga o login desta ShiftDeck.", CENTER_X, _ui->textY(FONT_SM, 140), FONT_SM);
    _ui->drawCentreString("Ela reinicia e mostra o QR para entrar", CENTER_X, _ui->textY(FONT_SM, 160), FONT_SM);
    _ui->drawCentreString("de novo, com a mesma ou outra conta.", CENTER_X, _ui->textY(FONT_SM, 180), FONT_SM);
    bool confirming = _confirmUntil && millis() < _confirmUntil;
    drawFooter("Voltar", IconID::Undo, confirming ? "Confirmar?" : "Sair da conta", IconID::Close, false);
}

void SettingsView::drawBrightnessValue() {
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", _brightness);
    _ui->fillRect(80, 70, 160, 40, C_BG);
    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawCentreString(pct, CENTER_X, _ui->textY(FONT_XL, 90), FONT_XL);
    _ui->drawSlider(SLIDER_X, SLIDER_CY, SLIDER_W, _brightness * 10, C_GREEN, C_TRACK, C_BG, 9, &_knobX);
}

void SettingsView::drawBrightnessPanel() {
    _knobX = -1;
    _ui->drawIcon(IconID::Brightness, 22, SLIDER_CY, 14, C_MUTED, C_BG);
    _ui->drawIcon(IconID::Brightness, SCREEN_W - 22, SLIDER_CY, 22, C_WHITE, C_BG);
    drawBrightnessValue();
    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawCentreString("Toque ou arraste na barra", CENTER_X, _ui->textY(FONT_XS, 172), FONT_XS);
    drawFooter("Voltar", IconID::Undo, "Salvar", IconID::Save, true);
}

void SettingsView::drawDevicePanel() {
    if (_devices.empty()) {
        _ui->drawIcon(IconID::Phone, CENTER_X, 90, 34, C_MUTED, C_BG);
        _ui->setTextColor(C_WHITE, C_BG);
        const char* l1 = _devicesLoaded ? "Nenhum dispositivo" : "Procurando dispositivos...";
        _ui->drawCentreString(l1, CENTER_X, _ui->textY(FONT_MD_B, 128), FONT_MD_B);
        _ui->setTextColor(C_MUTED, C_BG);
        _ui->drawCentreString("Abra o Spotify no celular ou PC", CENTER_X, _ui->textY(FONT_SM, 150), FONT_SM);
    }
    for (size_t i = 0; i < _devices.size() && i < 5; i++) {
        const DeviceInfo& d = _devices[i];
        IconID icon = d.type == "Smartphone" || d.type == "Tablet" ? IconID::Phone
                    : d.type == "Computer" || d.type == "TV" ? IconID::Display : IconID::Speaker;
        String type = SpotifyPlayer::deviceTypeLabel(d.type);
        int32_t cy = ROW_Y + i * ROW_PITCH + ROW_H / 2;
        drawRow(i, icon, _ui->fitText(d.name, 150, FONT_MD).c_str(), d.active ? String("Ativo") : type,
                d.active ? C_GREEN : C_MUTED, d.active);   // the chevron slot holds the check
        if (d.active) {
            _ui->fillRect(ROW_X + ROW_W - 26, cy - 9, 20, 18, C_CARD);
            _ui->drawIcon(IconID::CheckCircle, ROW_X + ROW_W - 16, cy, 15, C_GREEN, C_CARD);
        }
    }
    drawFooter("Voltar", IconID::Undo, "Atualizar", IconID::Update, false);
}

void SettingsView::drawUpdatePanel() {
    String latest = SpotifyPlayer::getInstance().getLatestVersion();
    bool newer = Updater::isNewer(latest.c_str(), VERSION);
    drawRow(0, IconID::Update, "Instalada", VERSION, C_GREEN, false);
    drawRow(1, IconID::Cloud, "Mais recente", latest.length() ? latest : String("Verificando..."),
            newer ? C_WARN : latest.length() ? C_GREEN : C_MUTED, false);
    drawRow(2, IconID::Display, "Placa", BOARD_NAME, C_SECONDARY, false);
    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawCentreString(newer ? "Atualiza pela internet, sem cabo," : "A ShiftDeck procura versões novas",
                          CENTER_X, _ui->textY(FONT_SM, 154), FONT_SM);
    _ui->drawCentreString(newer ? "e mantém o Wi-Fi e a conta." : "sozinha a cada 6 horas.",
                          CENTER_X, _ui->textY(FONT_SM, 174), FONT_SM);
    drawFooter("Voltar", IconID::Undo, newer ? "Atualizar" : "Verificar", IconID::Update, newer);
}

void SettingsView::drawDiagPanel() {
    bool wifiOk = WiFi.status() == WL_CONNECTED;
    bool apiOk = SpotifyPlayer::getInstance().isApiOnline();
    char buf[40];
    snprintf(buf, sizeof(buf), "%d dBm", wifiOk ? (int)WiFi.RSSI() : 0);
    drawRow(0, IconID::Wifi, "Wi-Fi", wifiOk ? String(buf) : String("Desconectado"), wifiOk ? C_GREEN : C_ERROR, false);
    drawRow(1, IconID::Spotify, "Spotify API", apiOk ? "Online" : "Offline", apiOk ? C_GREEN : C_ERROR, false);
    uint32_t heap = ESP.getFreeHeap() / 1024, minHeap = ESP.getMinFreeHeap() / 1024;
    snprintf(buf, sizeof(buf), "%u KB (mín %u)", (unsigned)heap, (unsigned)minHeap);
    drawRow(2, IconID::Diagnostics, "Memória", buf, heap > 40 ? C_GREEN : C_WARN, false);
    unsigned long s = millis() / 1000;
    snprintf(buf, sizeof(buf), "%luh %02lumin", s / 3600, (s / 60) % 60);
    drawRow(3, IconID::Update, "Ligado há", buf, C_SECONDARY, false);
    drawFooter("Voltar", IconID::Undo, "Calibrar", IconID::Touch, false);
}

/* ---------------------------------------------------------------- input */

void SettingsView::leave() {
    if (_brightness != _savedBrightness) {             // discard the live preview
        _brightness = _savedBrightness;
        _ui->setBacklight(_brightness);
    }
    UIViewManager::getInstance().back();
}

void SettingsView::setBrightnessAt(int x) {
    int v = constrain((x - SLIDER_X) * 100 / SLIDER_W, 0, 100);
    if (v == _brightness) return;
    _brightness = v;
    _ui->setBacklight(v);
    drawBrightnessValue();
}

void SettingsView::onTap(uint16_t x, uint16_t y) {
    const bool header = y < HEADER_H && x < 80;
    const bool footer = y >= FOOT_Y - 4;
    const bool leftBtn = footer && x < ROW_X + FOOT_W + 4;
    const bool rightBtn = footer && x > ROW_X + ROW_W - FOOT_W - 4;
    const int row = (y >= ROW_Y && !footer) ? (int)(y - ROW_Y) / ROW_PITCH : -1;

    if (_panel == PANEL_LIST) {
        if (header || leftBtn) { leave(); return; }
        if (rightBtn) {
            if (_brightness != _savedBrightness) Vault::getInstance().setBrightness(_brightness);
            _savedBrightness = _brightness;
            UIViewManager::getInstance().back();
            return;
        }
        if (row >= 0 && row < 6) {
            static const Panel targets[6] = { PANEL_WIFI, PANEL_ACCOUNT, PANEL_BRIGHTNESS, PANEL_DEVICE, PANEL_UPDATE, PANEL_DIAG };
            openPanel(targets[row]);
            drawBody();
        }
        return;
    }

    // Every panel: header arrow or the left button returns to the list
    // (brightness keeps its preview until Salvar/Voltar on the list).
    if (header || leftBtn || (_panel == PANEL_WIFI_ADD && footer)) {
        if (_panel == PANEL_BRIGHTNESS && leftBtn) { _brightness = _savedBrightness; _ui->setBacklight(_brightness); }
        openPanel(_panel == PANEL_WIFI_ADD ? PANEL_WIFI : PANEL_LIST);
        drawBody();
        return;
    }

    switch (_panel) {
        case PANEL_WIFI: {
            if (rightBtn) { openPanel(PANEL_WIFI_ADD); drawBody(); break; }
            std::vector<WiFiNet> nets = Vault::getInstance().getWiFiNetworks();
            if (row < 0 || row >= (int)nets.size()) break;
            if (_confirmRow == row && _confirmUntil && millis() < _confirmUntil) {
                Vault::getInstance().removeWiFiNetwork(nets[row].ssid);
                _confirmRow = -1;
                _confirmUntil = 0;
                UIViewManager::getInstance().showToast("Rede excluída");
            } else {
                _confirmRow = row;
                _confirmUntil = millis() + 3000;
            }
            drawBody();
            break;
        }
        case PANEL_ACCOUNT:
            if (!rightBtn) break;
            if (_confirmUntil && millis() < _confirmUntil) {
                // Keep the Client ID: the board reboots straight into "Autorize o Spotify".
                // Dropping a pre-PKCE secret makes the new login use PKCE.
                Vault& v = Vault::getInstance();
                v.setSpotifyCredentials(v.getSpotifyClientID(), "");
                v.setSpotifyRefreshToken("");
                _ui->clear(C_BG);
                _ui->setTextColor(C_WHITE, C_BG);
                _ui->drawCentreString("Saindo da conta...", CENTER_X, _ui->textY(FONT_LG, CENTER_Y), FONT_LG);
                delay(800);
                ESP.restart();
            }
            _confirmUntil = millis() + 3000;
            drawAccountPanel();
            break;
        case PANEL_BRIGHTNESS:
            if (rightBtn) {
                Vault::getInstance().setBrightness(_brightness);
                _savedBrightness = _brightness;
                _panel = PANEL_LIST;
                drawBody();
            } else if (y > SLIDER_CY - 30 && y < SLIDER_CY + 30) {
                setBrightnessAt(x);
            }
            break;
        case PANEL_DEVICE:
            if (rightBtn) {
                _devices.clear();
                _devicesLoaded = false;
                SpotifyPlayer::getInstance().fetchDevices();
                drawBody();
            } else if (row >= 0 && row < (int)_devices.size() && !_devices[row].active) {
                SpotifyPlayer::getInstance().transferTo(_devices[row].id);
                drawRow(row, IconID::Phone, _ui->fitText(_devices[row].name, 150, FONT_MD).c_str(), "Conectando...", C_SECONDARY, false);
            }
            break;
        case PANEL_UPDATE:
            if (!rightBtn) break;
            if (Updater::isNewer(SpotifyPlayer::getInstance().getLatestVersion().c_str(), VERSION)) {
                UIViewManager::getInstance().gotoView(ViewID::Update);
            } else {
                SpotifyPlayer::getInstance().checkForUpdate();
                UIViewManager::getInstance().showToast("Verificando...");
            }
            break;
        case PANEL_DIAG:
            if (rightBtn) {
                runTouchCalibration(*_ui, touch);
                _dirty = true;
            }
            break;
        default:
            break;
    }
}

void SettingsView::onDrag(uint16_t startX, uint16_t startY, int dy) {
    // the slider follows the finger horizontally
    if (_panel == PANEL_BRIGHTNESS && startY > SLIDER_CY - 30 && startY < SLIDER_CY + 30) setBrightnessAt(_curX);
}

void SettingsView::handleMessage(SCUIMessage *msg) {
    switch (msg->type) {
        case SCUIMessageType::UM_DEVICES_DONE:
            _devices = SpotifyPlayer::getInstance().getDevices();
            _devicesLoaded = true;
            if (_dirty) break;
            if (_panel == PANEL_DEVICE) drawBody();
            else if (_panel == PANEL_LIST) drawListRows();
            break;
        case SCUIMessageType::UM_PLAYER_REFRESH:
            if (!_dirty && _panel == PANEL_DIAG) drawDiagPanel();
            break;
        case SCUIMessageType::UM_UPDATE_STATUS:
            if (_dirty) break;
            if (_panel == PANEL_UPDATE) drawBody();
            else if (_panel == PANEL_LIST) drawListRows();
            if (_panel == PANEL_UPDATE && !msg->num)
                UIViewManager::getInstance().showToast(msg->str[0] ? "Você já está na versão mais recente" : "Sem conexão com o servidor");
            break;
        case SCUIMessageType::UM_IDLE:
            if (_panel == PANEL_WIFI_ADD) {
                // Checked before serving, so the "Rede salva" page went out a frame ago.
                if (WiFiSetup::getInstance().takeSaved()) {
                    openPanel(PANEL_WIFI);
                    if (!_dirty) drawBody();
                    UIViewManager::getInstance().showToast("Rede salva");
                    break;
                }
                WiFiSetup::getInstance().handleLoop();
            }
            if (_confirmUntil && millis() > _confirmUntil) {
                _confirmUntil = 0;
                _confirmRow = -1;
                if (!_dirty && _panel == PANEL_WIFI) drawBody();
                if (!_dirty && _panel == PANEL_ACCOUNT) drawAccountPanel();
            }
            break;
        default:
            UIView::handleMessage(msg);
            break;
    }
}
