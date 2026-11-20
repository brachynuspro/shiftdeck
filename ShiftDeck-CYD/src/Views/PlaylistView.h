/*-------------------------------------------------------------------------------------------------
** PlaylistView.h - Library screen (IMGS/Playlists.png)
**
** "Curtidas" + the user's playlists. Tap selects (green row), tapping the
** selected row or "Tocar" plays it and returns Home. Drag scrolls by rows.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include "UIView.h"
#include "SpotifyPlayer.h"
#include <vector>

class PlaylistView : public UIView {
public:
    PlaylistView(DisplayUI *ui);

    void draw() override;
    void onTap(uint16_t x, uint16_t y) override;
    void onDrag(uint16_t startX, uint16_t startY, int dy) override;
    void onEnter() override;
    void handleMessage(SCUIMessage *msg) override;

private:
    void drawHeaderStatus();
    void drawList();
    void drawRow(int slot);
    void drawScrollbar();
    void drawFooter();
    void playSelected();

    std::vector<MediaItem> _items;
    int _selected = -1;
    int _scroll = 0;
    int _dragAccum = 0;
    int _state = 0;             // 0 loading, 1 loaded, -1 failed
    String _statusSig;

    static const int HEADER_H = 34;
    static const int LIST_X = 8, LIST_Y = 34, LIST_W = 304, ROW_H = 34, ROWS = 5;
    static const int LIST_HEIGHT = ROW_H * ROWS + 4;
    static const int FOOT_Y = LIST_Y + LIST_HEIGHT + 4, FOOT_H = 28;
};
