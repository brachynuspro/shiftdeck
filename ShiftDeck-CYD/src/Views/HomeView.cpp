/*-------------------------------------------------------------------------------------------------
** HomeView.cpp - Player screen implementation (IMGS/principal.png)
**
** Only a track change repaints the whole screen. Progress, play state,
** like, cover and status bar each repaint their own region.
** ------------------------------------------------------------------------------------------------
*/
#include "HomeView.h"
#include "UIViewManager.h"
#include "SettingsView.h"
#include "logTags.h"
#include "SCLogger.h"
#include <esp_task_wdt.h>
#include <WiFi.h>

#define COVER_FILE "/cover.jpg"

HomeView::HomeView(DisplayUI *ui) : UIView(ui) {
    _id = ViewID::Home;
}

void HomeView::onEnter() {
    _dirty = true;
}

String HomeView::statusSignature() {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    return String(WiFi.status() == WL_CONNECTED) + sp.isApiOnline() + sp.getActiveDeviceType() + clockText();
}

void HomeView::draw() {
    esp_task_wdt_reset();
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    PlayingMetadata meta = sp.getCurrentlyPlayingMetadata();
    _trackUri = meta.trackUri;
    _hasMusic = sp.isMusicAvailable();
    _isPlaying = _hasMusic && meta.isPlaying;

    _ui->clear(C_BG);
    drawStatusBar();
    _ui->fillRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, 12, C_CARD, C_BG);
    drawArt();
    esp_task_wdt_reset();
    drawTrackInfo(meta);
    drawHeart();
    _knobX = -1;
    _shownCur = _shownTotal = -1;
    drawProgress(true);
    drawControls();
    _dirty = false;
}

void HomeView::drawStatusBar() {
    _statusSig = statusSignature();
    const int cy = 22;
    _ui->fillRect(0, 0, SCREEN_W, CARD_Y - 1, C_BG);
    drawWifiStatus(10, cy, C_BG);
    drawSpotifyStatus(114, cy, C_BG);
    String dev = SpotifyPlayer::getInstance().getActiveDeviceType();
    drawStatusItem(218, cy, IconID::Phone, dev.length() ? C_WHITE : C_MUTED, dev.length() ? dev.c_str() : "Nenhum",
                   "Dispositivo", C_MUTED, C_BG);
    _ui->fillRect(108, cy - 10, 1, 20, C_DIVIDER);
    _ui->fillRect(212, cy - 10, 1, 20, C_DIVIDER);
}

void HomeView::drawArt() {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    bool drawn = _hasMusic && sp.isCoverArtReady() && _ui->drawJpgScaled(ART_X, ART_Y, ART_SIZE, ART_SIZE, COVER_FILE);
    if (drawn) {
        _ui->roundCorners(ART_X, ART_Y, ART_SIZE, ART_SIZE, 8, C_CARD);
    } else {
        // placeholder tile while the cover downloads (or nothing plays)
        _ui->fillRoundRect(ART_X, ART_Y, ART_SIZE, ART_SIZE, 8, C_BUTTON, C_CARD);
        _ui->drawIcon(_hasMusic ? IconID::Playlist : IconID::Spotify, ART_X + ART_SIZE / 2, ART_Y + ART_SIZE / 2, 34,
                      C_MUTED, C_BUTTON);
    }
}

void HomeView::drawTrackInfo(const PlayingMetadata& meta) {
    int32_t wFull = INFO_R - INFO_X;
    int32_t wHeart = HEART_CX - 16 - INFO_X;
    if (!_hasMusic) {
        _ui->setTextColor(C_WHITE, C_CARD);
        _ui->drawString("Nada tocando", INFO_X, _ui->textY(FONT_LG, 76), FONT_LG);
        _ui->setTextColor(C_SECONDARY, C_CARD);
        _ui->drawString("Abra o Spotify no", INFO_X, _ui->textY(FONT_SM, 101), FONT_SM);
        _ui->drawString("celular e dê play", INFO_X, _ui->textY(FONT_SM, 120), FONT_SM);
        return;
    }
    _ui->setTextColor(C_WHITE, C_CARD);
    _ui->drawString(_ui->fitText(meta.trackName, wFull, FONT_LG).c_str(), INFO_X, _ui->textY(FONT_LG, 76), FONT_LG);
    _ui->setTextColor(C_SECONDARY, C_CARD);
    _ui->drawString(_ui->fitText(meta.artistName, wHeart, FONT_MD).c_str(), INFO_X, _ui->textY(FONT_MD, 101), FONT_MD);
    _ui->drawString(_ui->fitText(meta.albumName, wFull, FONT_SM).c_str(), INFO_X, _ui->textY(FONT_SM, 123), FONT_SM);
}

void HomeView::drawHeart() {
    _ui->fillRect(HEART_CX - 12, HEART_CY - 12, 24, 24, C_CARD);
    if (!_hasMusic) return;
    bool liked = SpotifyPlayer::getInstance().isLiked();
    _ui->drawIcon(liked ? IconID::HeartFilled : IconID::HeartOutline, HEART_CX, HEART_CY, 20,
                  liked ? C_GREEN : C_SECONDARY, C_CARD);
}

// Progress is interpolated locally between polls (every ~3 s).
void HomeView::drawProgress(bool force) {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    PlayingMetadata meta = sp.getCurrentlyPlayingMetadata();
    long dur = _hasMusic ? meta.durationMs : 0;
    long pos = _hasMusic ? meta.progressMs : 0;
    if (_hasMusic && meta.isPlaying && sp.getLastRefreshMs() > 0) pos += (long)(millis() - sp.getLastRefreshMs());
    if (_seekPreviewMs >= 0) pos = _seekPreviewMs;
    pos = constrain(pos, 0L, max(dur, 0L));

    int32_t permille = dur > 0 ? (int32_t)((int64_t)pos * 1000 / dur) : 0;
    _ui->drawSlider(BAR_X, BAR_CY, BAR_W, permille, C_GREEN, C_TRACK, C_CARD, 5, &_knobX);

    int cur = pos / 1000, total = dur / 1000;
    if (!force && cur == _shownCur && total == _shownTotal) return;
    char buf[12];
    int32_t ty = _ui->textY(FONT_SM, TIME_CY);
    _ui->fillRect(BAR_X - 2, TIME_CY - 9, 60, 18, C_CARD);
    snprintf(buf, sizeof(buf), "%d:%02d", cur / 60, cur % 60);
    _ui->setTextColor(C_GREEN, C_CARD);
    _ui->drawString(buf, BAR_X, ty, FONT_SM);
    if (total != _shownTotal || force) {
        _ui->fillRect(BAR_X + BAR_W - 58, TIME_CY - 9, 60, 18, C_CARD);
        snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
        _ui->setTextColor(C_SECONDARY, C_CARD);
        _ui->drawRightString(buf, BAR_X + BAR_W, ty, FONT_SM);
    }
    _shownCur = cur;
    _shownTotal = total;
}

void HomeView::drawPlayButton() {
    _ui->fillCircle(PLAY_CX, PLAY_CY, PLAY_R, C_GREEN, C_CARD);
    if (_busyButton == 1) _ui->drawIcon(IconID::Spinner, PLAY_CX, PLAY_CY, 24, C_WHITE, C_GREEN);
    else _ui->drawIcon(_isPlaying ? IconID::Pause : IconID::Play, PLAY_CX + (_isPlaying ? 0 : 2), PLAY_CY, 22, C_WHITE, C_GREEN);
}

void HomeView::drawControls() {
    const int xs[2] = { PREV_X, NEXT_X };
    const IconID icons[2] = { IconID::Prev, IconID::Next };
    for (int i = 0; i < 2; i++) {
        _ui->fillRoundRect(xs[i], BTN_Y, BTN_W, BTN_H, 14, C_BUTTON, C_CARD);
        bool busy = _busyButton == (i == 0 ? 0 : 2);
        _ui->drawIcon(busy ? IconID::Spinner : icons[i], xs[i] + BTN_W / 2, BTN_Y + BTN_H / 2, 18,
                      busy ? C_GREEN : C_WHITE, C_BUTTON);
    }
    for (int i = 0; i < 2; i++) {
        int32_t x = i ? VOL_UP_X : VOL_DOWN_X, cx = x + VOL_W / 2, cy = BTN_Y + BTN_H / 2;
        _ui->fillRoundRect(x, BTN_Y, VOL_W, BTN_H, 14, C_BUTTON, C_CARD);
        // speaker cone + sign
        const float sx = cx - 8;
        const float cone[8] = { sx, (float)cy - 3, sx + 6, (float)cy - 8, sx + 6, (float)cy + 8, sx, (float)cy + 3 };
        _ui->fillRect(sx - 4, cy - 3, 5, 7, C_WHITE);
        _ui->fillQuad(cone, C_WHITE, C_BUTTON);
        _ui->fillRect(cx + 4, cy - 1, 9, 3, C_WHITE);                  // minus
        if (i) _ui->fillRect(cx + 7, cy - 4, 3, 9, C_WHITE);           // plus
    }
    drawPlayButton();
}

void HomeView::changeVolume(int delta) {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    if (!_volSendAt) _volTarget = sp.getActiveVolume();     // nothing pending: start from the device
    if (_volTarget < 0) {
        UIViewManager::getInstance().showToast("Nenhum dispositivo ativo");
        return;
    }
    _volTarget = constrain(_volTarget + delta, 0, 100);
    _volSendAt = millis() + 400;
    char msg[24];
    snprintf(msg, sizeof(msg), "Volume %d%%", _volTarget);
    UIViewManager::getInstance().showToast(msg);
}

long HomeView::seekTarget(uint16_t x) {
    long dur = SpotifyPlayer::getInstance().getCurrentlyPlayingMetadata().durationMs;
    return (long)((int64_t)dur * constrain((int)x - BAR_X, 0, BAR_W) / BAR_W);
}

void HomeView::onDrag(uint16_t startX, uint16_t startY, int) {
    if (!_hasMusic || !onBar(startX, startY)) return;
    _seekPreviewMs = seekTarget(_curX);
    drawProgress(false);
}

void HomeView::onTouchUp(uint16_t x, uint16_t y) {
    if (_seekPreviewMs >= 0) {
        SpotifyPlayer::getInstance().seek(_seekPreviewMs);
        _seekPreviewMs = -1;
    }
    UIView::onTouchUp(x, y);
}

bool HomeView::onLongPress(uint16_t x, uint16_t y) {
    if (!hitRect(x, y, PLAY_CX - PLAY_R - 8, PLAY_CY - PLAY_R - 4, PLAY_R * 2 + 16, PLAY_R * 2 + 8)) return false;
    UIViewManager::getInstance().gotoView(ViewID::Queue);
    return true;
}

void HomeView::onTap(uint16_t x, uint16_t y) {
    UIViewManager& vm = UIViewManager::getInstance();
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();

    // ---- status bar segments ----
    if (y < CARD_Y) {
        if (x < 108)      vm.gotoView(ViewID::Settings);
        else if (x < 212) vm.gotoView(ViewID::Playlist);
        else {
            SettingsView::openPanelOnEnter(SettingsView::PANEL_DEVICE);
            vm.gotoView(ViewID::Settings);
        }
        return;
    }

    // ---- transport ----
    auto start = [&](int8_t button) {
        if (_busyButton >= 0) return false;   // one action at a time
        _busyButton = button;
        _busySince = millis();
        drawControls();
        return true;
    };
    if (hitRect(x, y, VOL_DOWN_X, BTN_Y - 4, VOL_W, BTN_H + 8)) { changeVolume(-VOL_STEP); return; }
    if (hitRect(x, y, VOL_UP_X, BTN_Y - 4, VOL_W, BTN_H + 8))   { changeVolume(VOL_STEP); return; }
    if (hitRect(x, y, PREV_X, BTN_Y - 4, BTN_W, BTN_H + 8)) { if (start(0)) sp.previousSong(); return; }
    if (hitRect(x, y, NEXT_X, BTN_Y - 4, BTN_W, BTN_H + 8)) { if (start(2)) sp.nextSong(); return; }
    if (hitRect(x, y, PLAY_CX - PLAY_R - 8, PLAY_CY - PLAY_R - 4, PLAY_R * 2 + 16, PLAY_R * 2 + 8)) {
        if (start(1)) sp.pauseSong();
        return;
    }

    if (_hasMusic && onBar(x, y)) {
        SpotifyPlayer::getInstance().seek(seekTarget(x));
        drawProgress(false);
        return;
    }

    // ---- like ----
    if (_hasMusic && hitRect(x, y, HEART_CX - 20, HEART_CY - 20, 40, 40)) {
        sp.toggleLike();
        drawHeart();
        return;
    }

    // ---- navigation ----
    if (hitRect(x, y, ART_X, ART_Y, ART_SIZE, ART_SIZE)) { vm.gotoView(ViewID::Playlist); return; }
    if (hitRect(x, y, INFO_X, ART_Y, INFO_R - INFO_X, ART_SIZE)) { vm.gotoView(ViewID::Search); return; }
}

void HomeView::handleMessage(SCUIMessage *msg) {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    switch (msg->type) {
        case SCUIMessageType::UM_PLAYER_REFRESH: {
            PlayingMetadata meta = sp.getCurrentlyPlayingMetadata();
            if (_trackUri != meta.trackUri || _hasMusic != sp.isMusicAvailable()) { _busyButton = -1; _dirty = true; break; }
            bool playing = _hasMusic && meta.isPlaying;
            if (playing != _isPlaying) { _isPlaying = playing; drawPlayButton(); }
            if (statusSignature() != _statusSig) drawStatusBar();
            drawProgress(false);
            break;
        }
        case SCUIMessageType::UM_MARK_DIRTY:        // cover art landed
            if (!_dirty) drawArt();
            break;
        case SCUIMessageType::UM_LIKE_CHANGED:
            if (!_dirty) drawHeart();
            break;
        case SCUIMessageType::UM_DEVICES_DONE:
            if (!_dirty && statusSignature() != _statusSig) drawStatusBar();
            break;
        case SCUIMessageType::UM_ACTION_RESULT:
            // A skip was accepted: keep spinning until the new track shows (a poll or two later);
            // the 8 s timeout below gives the button back in 4 s if it never does.
            if (msg->num == 1 && (_busyButton == 0 || _busyButton == 2)) { _busySince = millis() - 4000; break; }
            _busyButton = -1;
            if (!_dirty) drawControls();
            break;
        case SCUIMessageType::UM_IDLE: {
            unsigned long now = millis();
            if (_volSendAt && (long)(now - _volSendAt) >= 0) {
                sp.setVolume(_volTarget);
                _volSendAt = 0;
            }
            if (_dirty) break;
            if (now - _lastTickMs >= 250) {
                _lastTickMs = now;
                drawProgress(false);
            }
            if (_busyButton >= 0) {
                if (now - _busySince > 8000) {        // result lost: give the buttons back
                    _busyButton = -1;
                    drawControls();
                } else if (now - _lastSpinMs >= 70) {
                    _lastSpinMs = now;
                    if (_busyButton == 1) drawPlayButton();
                    else {
                        int32_t bx = (_busyButton == 0 ? PREV_X : NEXT_X) + BTN_W / 2;
                        _ui->fillRect(bx - 11, BTN_Y + BTN_H / 2 - 11, 22, 22, C_BUTTON);
                        _ui->drawIcon(IconID::Spinner, bx, BTN_Y + BTN_H / 2, 18, C_GREEN, C_BUTTON);
                    }
                }
            }
            break;
        }
        default:
            break;
    }
}
