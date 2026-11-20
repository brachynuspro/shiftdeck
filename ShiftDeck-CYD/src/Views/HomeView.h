/*-------------------------------------------------------------------------------------------------
** HomeView.h - Player screen (IMGS/principal.png)
**
** Navigation (the mockup has no nav buttons, so the existing elements are
** the targets): Wi-Fi segment -> Configurações, Spotify segment or the
** cover -> Playlists, device segment -> Configurações/Dispositivo,
** track title/artist -> Busca.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include "UIView.h"
#include "SpotifyPlayer.h"

class HomeView : public UIView {
public:
    HomeView(DisplayUI *ui);

    void draw() override;
    void onTap(uint16_t x, uint16_t y) override;
    void handleMessage(SCUIMessage *msg) override;
    void onEnter() override;

private:
    void drawStatusBar();
    void drawArt();
    void drawTrackInfo(const PlayingMetadata& meta);
    void drawHeart();
    void drawProgress(bool force);
    void drawControls();
    void drawPlayButton();
    String statusSignature();

    // Layout (320x240)
    static const int CARD_X = 8, CARD_Y = 44, CARD_W = 304, CARD_H = 192;
    static const int ART_X = 16, ART_Y = 52, ART_SIZE = 100;
    static const int INFO_X = 128, INFO_R = 300;
    static const int HEART_CX = 290, HEART_CY = 101;
    static const int BAR_X = 22, BAR_W = 276, BAR_CY = 164, TIME_CY = 179;
    static const int BTN_Y = 192, BTN_H = 36, BTN_W = 100, PREV_X = 16, NEXT_X = 204;
    static const int PLAY_CX = 160, PLAY_CY = 210, PLAY_R = 24;

    String _trackUri;
    String _statusSig;
    bool _isPlaying = false;
    bool _hasMusic = false;
    int16_t _knobX = -1;
    int _shownCur = -1, _shownTotal = -1;
    unsigned long _lastTickMs = 0;

    // A queued transport action shows a spinner on its button and ignores
    // further taps until UM_ACTION_RESULT (or an 8 s safety timeout).
    int8_t _busyButton = -1;   // 0 prev, 1 play, 2 next
    unsigned long _busySince = 0;
    unsigned long _lastSpinMs = 0;
};
