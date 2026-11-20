/*-------------------------------------------------------------------------------------------------
** SettingsView.h - Settings screen (IMGS/config.png) and its detail panels
**
** The list and the five panels share the header ("<  Title") and the
** two-button footer. Brightness previews live and is only persisted on
** "Salvar"; leaving with "Voltar" restores the saved value.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include "UIView.h"
#include "SpotifyPlayer.h"
#include <vector>

class SettingsView : public UIView {
public:
    enum Panel : uint8_t { PANEL_LIST, PANEL_WIFI, PANEL_BRIGHTNESS, PANEL_DEVICE, PANEL_UPDATE, PANEL_DIAG };
    // Lets Home open "Dispositivo" directly.
    static void openPanelOnEnter(Panel p) { s_nextPanel = p; }

    SettingsView(DisplayUI *ui);

    void draw() override;
    void onTap(uint16_t x, uint16_t y) override;
    void onDrag(uint16_t startX, uint16_t startY, int dy) override;
    void onEnter() override;
    void handleMessage(SCUIMessage *msg) override;

private:
    void drawHeader(const char* title);
    void drawRow(int i, IconID icon, const char* label, const String& value, uint16_t valueColor, bool chevron);
    void drawFooter(const char* left, IconID leftIcon, const char* right, IconID rightIcon, bool rightPrimary);
    void drawBody();
    void drawListRows();
    void drawWifiPanel();
    void drawBrightnessPanel();
    void drawBrightnessValue();
    void drawDevicePanel();
    void drawUpdatePanel();
    void drawDiagPanel();
    void openPanel(Panel p);
    void leave();
    void setBrightnessAt(int x);
    bool diagnosticsOk();

    static Panel s_nextPanel;
    Panel _panel = PANEL_LIST;
    int _brightness = 80;
    int _savedBrightness = 80;
    int16_t _knobX = -1;
    std::vector<DeviceInfo> _devices;
    bool _devicesLoaded = false;
    unsigned long _confirmUntil = 0;   // "Trocar rede" asks for a second tap

    static const int HEADER_H = 32;
    static const int ROW_X = 8, ROW_Y = 34, ROW_W = 304, ROW_H = 30, ROW_PITCH = 35;
    static const int FOOT_Y = 208, FOOT_H = 28, FOOT_W = 148;
    static const int SLIDER_X = 44, SLIDER_W = 232, SLIDER_CY = 140;
};
