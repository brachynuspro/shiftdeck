/*-------------------------------------------------------------------------------------------------
** SearchView.h - Search screen (IMGS/busca.png)
**
** Two modes share the header + input field: the result list (with the
** Voltar / teclado / Buscar footer) and an on-screen QWERTY keyboard that
** covers the list. Tapping a result plays it and returns Home.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include "UIView.h"
#include "SpotifyPlayer.h"
#include <vector>

class SearchView : public UIView {
public:
    SearchView(DisplayUI *ui);

    void draw() override;
    void onTap(uint16_t x, uint16_t y) override;
    void onDrag(uint16_t startX, uint16_t startY, int dy) override;
    bool onLongPress(uint16_t x, uint16_t y) override;   // hold a song: play it next
    void onEnter() override;
    void handleMessage(SCUIMessage *msg) override;

private:
    void drawInput();
    void drawBody();               // results or keyboard
    void drawResults();
    void drawResultRow(int slot);
    void drawFooter();
    void drawKeyboard();
    void drawKey(int row, int col, bool pressed);
    bool keyRect(int row, int col, int32_t& x, int32_t& y, int32_t& w);
    void pressKey(int row, int col);
    void runSearch();

    String _query;
    bool _keyboard = true;
    bool _numbers = false;
    std::vector<MediaItem> _results;
    int _state = 0;               // 0 idle, 1 searching, 2 done, -1 failed
    int _scroll = 0, _dragAccum = 0;
    int8_t _pressedRow = -1, _pressedCol = -1;
    unsigned long _pressedAt = 0;

    static const int TITLE_CY = 13;
    static const int INPUT_X = 8, INPUT_Y = 26, INPUT_W = 304, INPUT_H = 30;
    static const int LIST_Y = 60, ROW_H = 34, ROW_PITCH = 37, ROWS = 4;
    static const int FOOT_Y = 210, FOOT_H = 28;
    static const int KB_Y = 62, KEY_H = 38, KEY_GAP = 4;
};
