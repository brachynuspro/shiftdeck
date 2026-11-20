/*-------------------------------------------------------------------------------------------------
** DisplayUI.h - UI rendering on ILI9341 via TFT_eSPI
**
** Text uses anti-aliased Inter (fonts/InterFonts.h, Latin-1 with accents).
** Shapes use TFT_eSPI's anti-aliased primitives, which blend against the
** `bg` colour you pass -- always pass the colour that is really behind the
** shape, or the edges get a halo.
**
** Text `y` is the TOP of the line box. Use textY(font, cy) to centre the
** cap height of a line on cy.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "config.h"

enum class IconID : uint8_t {
    Wifi, WifiOff, Spotify, Phone, Brightness, Update, Diagnostics,
    ChevronRight, Back, Search, HeartOutline, HeartFilled,
    Play, Pause, Prev, Next, Check, Spinner, Save, Close,
    Display, Touch, Cloud, Settings,
    ArrowLeft, CheckCircle, CloseCircle, Undo, Keyboard, Playlist,
    Speaker, Alert, Backspace
};

class DisplayUI {
public:
    DisplayUI(TFT_eSPI *tft);
    void init();

    // ---- primitives ----
    void clear(uint16_t color = C_BG);
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color);
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t color, uint16_t bg = C_BG);
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t color, uint16_t bg = C_BG, int32_t thickness = 1);
    void fillCircle(int32_t x, int32_t y, int32_t r, uint16_t color, uint16_t bg = C_BG);
    void drawLine(float ax, float ay, float bx, float by, float width, uint16_t color, uint16_t bg = C_BG);
    // Filled quad (convex, clockwise) with anti-aliased edges.
    void fillQuad(const float* xy, uint16_t color, uint16_t bg = C_BG);
    void fillGradientV(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t cTop, uint16_t cBottom);
    void drawCrosshair(int32_t cx, int32_t cy, int32_t size, uint16_t color);

    // Thin progress track centred on cy (4 px) with an optional knob.
    // permille: 0..1000. Pass lastKnobX (init to -1) to update in place
    // without clearing the whole band (no flicker on periodic updates).
    void drawSlider(int32_t x, int32_t cy, int32_t w, int32_t permille, uint16_t fill, uint16_t track,
                    uint16_t bg, int32_t knobR, int16_t *lastKnobX = nullptr);
    // Legacy bar used by boot / factory-reset overlays.
    void drawProgressBar(int32_t x, int32_t y, int32_t w, int32_t h,
                         uint8_t percentage, uint16_t trackColor, uint16_t barColor);

    // Vector icon centred on (cx, cy) inside a size x size box.
    void drawIcon(IconID icon, int32_t cx, int32_t cy, int32_t size, uint16_t color, uint16_t bg = C_BG);

    // Rounded button with optional leading icon; label+icon centred as a group.
    void drawButton(int32_t x, int32_t y, int32_t w, int32_t h, const char* label, IconID icon, bool hasIcon,
                    uint16_t fill, uint16_t fg, uint16_t bg = C_BG, uint8_t font = FONT_MD, int32_t r = 10);

    // ---- text ----
    void setTextColor(uint16_t fg, uint16_t bg = C_BG);
    void setTextSize(uint8_t size);
    void drawString(const char* str, int32_t x, int32_t y, uint8_t font = FONT_MD);
    void drawCentreString(const char* str, int32_t x, int32_t y, uint8_t font = FONT_MD);
    void drawRightString(const char* str, int32_t xRight, int32_t y, uint8_t font = FONT_MD);
    int32_t textWidth(const char* str);
    int32_t textWidth(const char* str, uint8_t font);
    int16_t fontHeight(uint8_t font);
    int16_t textY(uint8_t font, int32_t cy);     // top y that centres the cap height on cy
    // Shortens to fit maxW pixels, appending "..." (UTF-8 safe).
    String fitText(const String& s, int32_t maxW, uint8_t font);

    // ---- images ----
    void drawJpg(int32_t x, int32_t y, const char* filename);
    bool drawJpgScaled(int32_t x, int32_t y, int32_t w, int32_t h, const char* filename);
    void roundCorners(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t bg);
    bool pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data);

    int16_t width();
    int16_t height();
    void setBacklight(uint8_t pct);

private:
    TFT_eSPI *_tft;
    uint8_t _font = 0;   // currently loaded smooth font id (0 = none)
    static bool jpgCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
    void useFont(uint8_t id);
    void iconLocked(IconID icon, int32_t cx, int32_t cy, int32_t size, uint16_t color, uint16_t bg);
    void sliderLocked(int32_t x, int32_t cy, int32_t w, int32_t kx, uint16_t fill, uint16_t track, uint16_t bg, int32_t knobR);

    SemaphoreHandle_t _xSemaphoreDisplay = xSemaphoreCreateMutex();
};
