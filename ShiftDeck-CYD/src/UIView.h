/*-------------------------------------------------------------------------------------------------
** UIView.h - Abstract base class for all views
**
** Touch model: a press that moves at most TAP_SLOP px is a tap, fired on
** release at the press position (resistive panels are noisy on release).
** Anything longer is a drag: onDrag() gets the vertical delta per frame.
** Views may still override onTouchDown() directly (setup screens do).
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "DisplayUI.h"
#include "config.h"
#include "scui.h"

struct MediaItem;

class UIView {
public:
    UIView(DisplayUI *ui) : _ui(ui) {}
    virtual ~UIView() {}

    virtual void draw() = 0;
    virtual void onTouchDown(uint16_t x, uint16_t y) { _downX = _curX = x; _downY = y; _lastY = y; _dragging = false; }
    virtual void onTouchMove(uint16_t x, uint16_t y);
    virtual void onTouchUp(uint16_t x = 0, uint16_t y = 0);
    virtual void onTap(uint16_t x, uint16_t y) {}
    virtual void onDrag(uint16_t startX, uint16_t startY, int dy) {}
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void handleMessage(SCUIMessage *msg);

    ViewID getID() { return _id; }
    bool isDirty() { return _dirty; }
    void markDirty() { _dirty = true; }
    void markClean() { _dirty = false; }

protected:
    static const int TAP_SLOP = 14;   // resistive panel jitter

    DisplayUI *_ui;
    ViewID _id;
    bool _dirty = true;
    uint16_t _downX = 0, _downY = 0, _lastY = 0, _curX = 0;
    bool _dragging = false;

    bool hitRect(uint16_t tx, uint16_t ty, int32_t x, int32_t y, int32_t w, int32_t h) {
        return (tx >= x && tx <= x + w && ty >= y && ty <= y + h);
    }

    // Brand mark (two green slabs, IMGS/inicializacao.png) + wordmark, from x; markH = mark height.
    void drawBrand(int32_t x, int32_t cy, float markH, uint8_t font);
    static int32_t brandMarkW(float markH) { return (int32_t)(markH * 34 / 30 + markH / 3); }   // mark + gap

    // QR code of text centred in a box x box area (paint the white card first).
    void drawQR(int32_t x, int32_t y, int32_t box, const String& text);

    // Two-line status item as in the mockups: icon, white title, coloured value.
    void drawStatusItem(int32_t x, int32_t cy, IconID icon, uint16_t iconColor, const char* title,
                        const char* value, uint16_t valueColor, uint16_t bg, int32_t maxW = 70);
    void drawWifiStatus(int32_t x, int32_t cy, uint16_t bg);
    void drawSpotifyStatus(int32_t x, int32_t cy, uint16_t bg);
    // Cover tile for a list row: cached JPEG, "Curtidas" gradient or a placeholder.
    void drawThumb(const MediaItem& m, int32_t x, int32_t y, int32_t size, uint16_t bg, bool circle);
    // Screen header: back arrow + title on the left.
    void drawBackHeader(const char* title, int32_t h);
};
