/*-------------------------------------------------------------------------------------------------
** WelcomeView.h - First boot after a fresh install: what ShiftDeck is, then what it needs
**
** Page 1 presents the product, page 2 lists the requirements with a QR to the full guide
** (same layout as the setup steps). "Começar" hands over to the Wi-Fi setup (step 1 of 3).
** ------------------------------------------------------------------------------------------------
*/
#pragma once
#include "SetupView.h"

class WelcomeView : public SetupView {
public:
    WelcomeView(DisplayUI *ui);
    void draw() override;
    void onTouchDown(uint16_t x, uint16_t y) override { UIView::onTouchDown(x, y); }   // SetupView ignores touch
    void onTap(uint16_t x, uint16_t y) override;
    void onEnter() override { _page = 0; _done = false; _dirty = true; }
    bool done() const { return _done; }
    void showPage(int page) { _page = page; _dirty = true; }

private:
    int _page = 0;
    bool _done = false;
    void drawIntro();
};
