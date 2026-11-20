/*-------------------------------------------------------------------------------------------------
** SettingsView.cpp - Settings screen implementation (IMGS/config.png)
** ------------------------------------------------------------------------------------------------
*/
#include "SettingsView.h"
#include "UIViewManager.h"
#include "Vault.h"
#include "XPT2046Touch.h"
#include "TouchCalibration.h"
#include <WiFi.h>

// Global in main.cpp; reused for recalibration from Diagnóstico.
extern XPT2046Touch touch;

SettingsView::Panel SettingsView::s_nextPanel = SettingsView::PANEL_LIST;

static const char* const PANEL_TITLES[] = { "Configurações", "Wi-Fi", "Brilho", "Dispositivo", "Atualização", "Diagnóstico" };

SettingsView::SettingsView(DisplayUI *ui) : UIView(ui) {
    _id = ViewID::Settings;
}

void SettingsView::onEnter() {
    _brightness = _savedBrightness = Vault::getInstance().getBrightness();
    _panel = PANEL_LIST;
    _confirmUntil = 0;
    _devices = SpotifyPlayer::getInstance().getDevices();
    if (s_nextPanel != PANEL_LIST) openPanel(s_nextPanel);
    s_nextPanel = PANEL_LIST;
    _dirty = true;
}

void SettingsView::openPanel(Panel p) {
    _panel = p;
    _confirmUntil = 0;
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
        case PANEL_BRIGHTNESS: drawBrightnessPanel(); break;
        case PANEL_DEVICE:     drawDevicePanel(); break;
        case PANEL_UPDATE:     drawUpdatePanel(); break;
        case PANEL_DIAG:       drawDiagPanel(); break;
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
    bool confirming = _confirmUntil && millis() < _confirmUntil;
    uint16_t fill = confirming ? C_ERROR : (rightPrimary ? C_GREEN_DARK : C_BUTTON);
    _ui->drawButton(ROW_X + ROW_W - FOOT_W, FOOT_Y, FOOT_W, FOOT_H, right, rightIcon, true, fill, C_WHITE, C_BG, FONT_MD, 10);
}

void SettingsView::drawListRows() {
    bool wifiOk = WiFi.status() == WL_CONNECTED;
    String dev = SpotifyPlayer::getInstance().getActiveDeviceType();
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", _brightness);
    drawRow(0, IconID::Wifi, "Wi-Fi", wifiOk ? WiFi.SSID() : String("Desconectado"), wifiOk ? C_GREEN : C_ERROR, true);
    drawRow(1, IconID::Brightness, "Brilho", pct, C_GREEN, true);
    drawRow(2, IconID::Phone, "Dispositivo", dev.length() ? dev : String("Nenhum"), dev.length() ? C_GREEN : C_MUTED, true);
    drawRow(3, IconID::Update, "Atualização", VERSION_SHORT, C_GREEN, true);
    bool ok = diagnosticsOk();
    drawRow(4, IconID::Diagnostics, "Diagnóstico", ok ? "OK" : "Atenção", ok ? C_GREEN : C_WARN, true);
}

void SettingsView::drawWifiPanel() {
    bool ok = WiFi.status() == WL_CONNECTED;
    int rssi = ok ? WiFi.RSSI() : 0;
    const char* quality = rssi > -60 ? "Ótimo" : rssi > -70 ? "Bom" : "Fraco";
    char sig[32];
    snprintf(sig, sizeof(sig), "%d dBm · %s", rssi, quality);
    drawRow(0, IconID::Wifi, "Rede", Vault::getInstance().getWiFiSSID(), ok ? C_GREEN : C_ERROR, false);
    drawRow(1, IconID::Diagnostics, "Sinal", ok ? String(sig) : String("Desconectado"), ok ? C_GREEN : C_ERROR, false);
    drawRow(2, IconID::Cloud, "IP", ok ? WiFi.localIP().toString() : String("-"), C_SECONDARY, false);
    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawCentreString("Trocar rede reinicia no modo de configuração", CENTER_X, _ui->textY(FONT_XS, 152), FONT_XS);
    _ui->drawCentreString("(rede ShiftDeck + QR code)", CENTER_X, _ui->textY(FONT_XS, 168), FONT_XS);
    bool confirming = _confirmUntil && millis() < _confirmUntil;
    drawFooter("Reconectar", IconID::Update, confirming ? "Confirmar?" : "Trocar rede", IconID::Wifi, true);
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
    drawRow(0, IconID::Update, "Versão", VERSION, C_GREEN, false);
    drawRow(1, IconID::Save, "Compilado em", __DATE__, C_SECONDARY, false);
    drawRow(2, IconID::Display, "Placa", BOARD_NAME, C_SECONDARY, false);
    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawCentreString("Para atualizar, conecte o cabo USB", CENTER_X, _ui->textY(FONT_SM, 154), FONT_SM);
    _ui->drawCentreString("e use o instalador web no navegador.", CENTER_X, _ui->textY(FONT_SM, 174), FONT_SM);
    drawFooter("Voltar", IconID::Undo, nullptr, IconID::Undo, false);
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
        if (row >= 0 && row < 5) {
            static const Panel targets[5] = { PANEL_WIFI, PANEL_BRIGHTNESS, PANEL_DEVICE, PANEL_UPDATE, PANEL_DIAG };
            openPanel(targets[row]);
            drawBody();
        }
        return;
    }

    // Every panel: header arrow or the left button returns to the list
    // (brightness keeps its preview until Salvar/Voltar on the list).
    if (header || (leftBtn && _panel != PANEL_WIFI) || (footer && _panel == PANEL_UPDATE)) {
        if (_panel == PANEL_BRIGHTNESS && leftBtn) { _brightness = _savedBrightness; _ui->setBacklight(_brightness); }
        _panel = PANEL_LIST;
        drawBody();
        return;
    }

    switch (_panel) {
        case PANEL_WIFI:
            if (leftBtn) {
                WiFi.disconnect();
                WiFi.begin(Vault::getInstance().getWiFiSSID().c_str(), Vault::getInstance().getWiFiPassword().c_str());
                UIViewManager::getInstance().showToast("Reconectando...");
            } else if (rightBtn) {
                if (_confirmUntil && millis() < _confirmUntil) {
                    Vault::getInstance().setWiFiCredentials("", "");
                    _ui->clear(C_BG);
                    _ui->setTextColor(C_WHITE, C_BG);
                    _ui->drawCentreString("Reiniciando...", CENTER_X, _ui->textY(FONT_LG, CENTER_Y), FONT_LG);
                    delay(600);
                    ESP.restart();
                }
                _confirmUntil = millis() + 3000;
                drawWifiPanel();
            }
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
        case SCUIMessageType::UM_IDLE:
            if (_confirmUntil && millis() > _confirmUntil) {
                _confirmUntil = 0;
                if (!_dirty && _panel == PANEL_WIFI) drawWifiPanel();
            }
            break;
        default:
            UIView::handleMessage(msg);
            break;
    }
}
