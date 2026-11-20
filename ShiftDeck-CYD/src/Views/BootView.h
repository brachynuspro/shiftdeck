/*-------------------------------------------------------------------------------------------------
** BootView.h - Splash screen with the real hardware/service checklist
**
** setup() drives it directly (the UI task is not running yet): setStep()
** and setProgress() repaint only what changed, animate() spins the
** spinners while setup() waits on WiFi / Spotify.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include "UIView.h"

class BootView : public UIView {
public:
    enum Step : uint8_t { STEP_DISPLAY, STEP_TOUCH, STEP_WIFI, STEP_BACKEND, STEP_SPOTIFY, STEP_COUNT };
    enum State : uint8_t { PENDING, LOADING, OK, FAILED, TIMEOUT };

    BootView(DisplayUI *ui);

    void draw() override;
    void onTouchDown(uint16_t x, uint16_t y) override {}

    void setStep(Step step, State state, const char* label = nullptr);
    void setProgress(int pct);
    void animate();                 // call while blocking; cheap when nothing spins
    // Replaces the board name in the footer, e.g. why the last boot was a reset.
    void setNotice(const char* text, uint16_t color) { _notice = text; _noticeColor = color; }

private:
    struct Row { const char* name; IconID icon; State state; const char* label; };
    Row _rows[STEP_COUNT] = {
        {"Tela",    IconID::Display, PENDING, nullptr},
        {"Touch",   IconID::Touch,   PENDING, nullptr},
        {"Wi-Fi",   IconID::Wifi,    PENDING, nullptr},
        {"Backend", IconID::Cloud,   PENDING, nullptr},
        {"Spotify", IconID::Spotify, PENDING, nullptr},
    };
    int _progress = 0;
    int16_t _knobX = -1;
    unsigned long _lastSpin = 0;
    const char* _notice = nullptr;
    uint16_t _noticeColor = C_MUTED;

    void drawRow(int i, bool statusOnly);

    static const int PANEL_X = 40, PANEL_Y = 90, PANEL_W = 240, ROW_H = 24;
    static const int BAR_X = 56, BAR_W = 208, BAR_CY = 76;
};
