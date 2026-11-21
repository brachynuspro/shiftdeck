/*-------------------------------------------------------------------------------------------------
** QueueView.h - "A seguir": shuffle, repeat and the next tracks
**
** Opened by holding the play button on Home. The two toggles act right away
** (optimistic) and the list is re-read from Spotify after each change and
** whenever the track changes. Songs get here from Busca (hold a song).
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include "UIView.h"
#include "SpotifyPlayer.h"
#include <vector>

class QueueView : public UIView {
public:
    QueueView(DisplayUI *ui);

    void draw() override;
    void onTap(uint16_t x, uint16_t y) override;
    void onEnter() override;
    void handleMessage(SCUIMessage *msg) override;

private:
    void drawToggles();
    void drawList();

    std::vector<MediaItem> _items;
    int _state = 0;             // 0 loading, 1 loaded, -1 failed
    String _trackUri;

    static const int HEADER_H = 34, TOG_Y = 38, TOG_H = 30, TOG_W = 150;
    static const int LIST_X = 8, LIST_Y = 74, LIST_W = 304, ROW_H = 31, ROWS = 5;
    static const int LIST_HEIGHT = ROW_H * ROWS + 4;
};
