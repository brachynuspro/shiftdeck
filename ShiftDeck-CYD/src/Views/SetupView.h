/*-------------------------------------------------------------------------------------------------
** SetupView.h - Shared layout of the first-run screens (Wi-Fi, Spotify app, authorization)
**
** Same identity as the boot screen (IMGS/inicializacao.png):
**
**   y=18   [mark] ShiftDeck                    Passo 1 de 3
**   y=48   Title (FONT_LG)
**   y=68   subtitle (secondary)
**   y=84   ┌ white QR card ┐   ┌ card: 3 rows icon / label / value ┐
**   y=226  footer hint (muted)
**
** The QR is dark-on-white: inverted QRs are not read by every phone camera.
** ------------------------------------------------------------------------------------------------
*/
#pragma once
#include "UIView.h"

class SetupView : public UIView {
public:
    SetupView(DisplayUI *ui, int step, const char* title, const char* subtitle, const char* footer);

    void draw() override;
    void onTouchDown(uint16_t, uint16_t) override {}
    void onEnter() override { _dirty = true; }

protected:
    struct Row { IconID icon; const char* label; String value; uint16_t color; };
    Row _rows[3];
    String _qr;   // text encoded in the QR

    // Replaces one row and repaints only that row (no flicker of the QR).
    void setRow(int i, IconID icon, const char* label, const String& value, uint16_t color);

private:
    int _step;
    const char *_title, *_subtitle, *_footer;
    void drawRow(int i);
    void drawQR(int32_t x, int32_t y, int32_t box);
};
