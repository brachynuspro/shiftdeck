/*-------------------------------------------------------------------------------------------------
** PlaylistView.cpp - Library screen implementation (IMGS/Playlists.png)
** ------------------------------------------------------------------------------------------------
*/
#include "PlaylistView.h"
#include "UIViewManager.h"
#include <WiFi.h>

PlaylistView::PlaylistView(DisplayUI *ui) : UIView(ui) {
    _id = ViewID::Playlist;
}

void PlaylistView::onEnter() {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    _items = sp.getPlaylists();              // last result: shows instantly, refreshed below
    _state = _items.empty() ? 0 : 1;
    _selected = -1;
    _scroll = 0;
    _dragAccum = 0;
    sp.fetchPlaylists();
    _dirty = true;
}

void PlaylistView::draw() {
    _ui->clear(C_BG);
    drawBackHeader("Playlists", HEADER_H);
    drawHeaderStatus();
    _ui->fillRoundRect(LIST_X, LIST_Y, LIST_W, LIST_HEIGHT, 12, C_CARD, C_BG);
    drawList();
    drawFooter();
    _dirty = false;
}

void PlaylistView::drawHeaderStatus() {
    _statusSig = String(WiFi.status() == WL_CONNECTED) + SpotifyPlayer::getInstance().isApiOnline();
    drawWifiStatus(134, HEADER_H / 2, C_BG);
    _ui->fillRect(222, HEADER_H / 2 - 9, 1, 18, C_DIVIDER);
    drawSpotifyStatus(228, HEADER_H / 2, C_BG);
}

void PlaylistView::drawList() {
    for (int i = 0; i < ROWS; i++) drawRow(i);
    drawScrollbar();
    if (!_items.empty()) return;
    // Empty / failed state: message over the (blank or skeleton) card.
    if (_state == 0) return;
    const char* l1 = _state < 0 ? "Não foi possível carregar" : "Nenhuma playlist";
    const char* l2 = _state < 0 ? "Toque para tentar de novo" : "Crie uma no app do Spotify";
    _ui->setTextColor(C_WHITE, C_CARD);
    _ui->drawCentreString(l1, CENTER_X, _ui->textY(FONT_MD_B, LIST_Y + LIST_HEIGHT / 2 - 10), FONT_MD_B);
    _ui->setTextColor(C_MUTED, C_CARD);
    _ui->drawCentreString(l2, CENTER_X, _ui->textY(FONT_SM, LIST_Y + LIST_HEIGHT / 2 + 12), FONT_SM);
}

void PlaylistView::drawRow(int slot) {
    const int32_t y = LIST_Y + 2 + slot * ROW_H;
    const int32_t rowW = LIST_W - 14;             // leaves room for the scrollbar
    const int32_t thumb = ROW_H - 8;
    const int32_t tx = LIST_X + 8 + thumb + 10;
    const int idx = _scroll + slot;
    _ui->fillRect(LIST_X + 2, y, rowW + 2, ROW_H, C_CARD);

    if (idx >= (int)_items.size()) {
        if (_state == 0) {                        // skeleton while loading
            _ui->fillRoundRect(LIST_X + 8, y + 4, thumb, thumb, 5, C_BUTTON, C_CARD);
            _ui->fillRoundRect(tx, y + 8, 120 - slot * 12, 8, 4, C_BUTTON, C_CARD);
            _ui->fillRoundRect(tx, y + 21, 64, 7, 3, C_BUTTON, C_CARD);
        }
        return;
    }
    const MediaItem& m = _items[idx];
    const bool sel = idx == _selected;
    const uint16_t bg = sel ? C_GREEN : C_CARD;
    if (sel) _ui->fillRoundRect(LIST_X + 3, y + 1, rowW, ROW_H - 2, 8, C_GREEN, C_CARD);
    drawThumb(m, LIST_X + 8, y + 4, thumb, bg, false);

    const int32_t textW = LIST_X + rowW - 6 - tx;
    _ui->setTextColor(sel ? C_ON_GREEN : C_WHITE, bg);
    _ui->drawString(_ui->fitText(m.name, textW, FONT_SM_B).c_str(), tx, _ui->textY(FONT_SM_B, y + 11), FONT_SM_B);
    _ui->setTextColor(sel ? C_ON_GREEN : C_SECONDARY, bg);
    _ui->drawString(_ui->fitText(m.subtitle, textW, FONT_XS).c_str(), tx, _ui->textY(FONT_XS, y + 25), FONT_XS);

    bool nextSel = (idx + 1) == _selected;
    if (!sel && !nextSel && slot < ROWS - 1) _ui->fillRect(tx, y + ROW_H - 1, LIST_X + rowW - tx, 1, C_DIVIDER);
}

void PlaylistView::drawScrollbar() {
    const int32_t x = LIST_X + LIST_W - 7, y = LIST_Y + 10, h = LIST_HEIGHT - 20;
    _ui->fillRect(x - 1, LIST_Y + 4, 6, LIST_HEIGHT - 8, C_CARD);
    int n = _items.size();
    if (n <= ROWS) return;
    _ui->fillRoundRect(x, y, 4, h, 2, C_TRACK, C_CARD);
    int32_t th = max((int32_t)16, h * ROWS / n);
    int32_t ty = y + (h - th) * _scroll / (n - ROWS);
    _ui->fillRoundRect(x, ty, 4, th, 2, C_GREEN, C_TRACK);
}

void PlaylistView::drawFooter() {
    bool can = _selected >= 0;
    uint16_t fg = can ? C_GREEN : C_MUTED;
    _ui->fillRoundRect(LIST_X, FOOT_Y, LIST_W, FOOT_H, 12, C_CARD, C_BG);
    int32_t tw = _ui->textWidth("Tocar", FONT_MD_B);
    int32_t x = CENTER_X - (tw + 26) / 2, cy = FOOT_Y + FOOT_H / 2;
    _ui->fillCircle(x + 9, cy, 9, fg, C_CARD);
    _ui->drawIcon(IconID::Play, x + 10, cy, 9, C_CARD, fg);
    _ui->setTextColor(fg, C_CARD);
    _ui->drawString("Tocar", x + 26, _ui->textY(FONT_MD_B, cy), FONT_MD_B);
}

void PlaylistView::playSelected() {
    if (_selected < 0 || _selected >= (int)_items.size()) {
        UIViewManager::getInstance().showToast("Selecione uma playlist");
        return;
    }
    SpotifyPlayer::getInstance().playItem(_items[_selected]);
    UIViewManager::getInstance().home();
}

void PlaylistView::onTap(uint16_t x, uint16_t y) {
    if (y < HEADER_H && x < 120) { UIViewManager::getInstance().back(); return; }
    if (hitRect(x, y, LIST_X, FOOT_Y - 2, LIST_W, FOOT_H + 6)) { playSelected(); return; }
    if (!hitRect(x, y, LIST_X, LIST_Y, LIST_W, LIST_HEIGHT)) return;

    if (_items.empty()) {
        if (_state < 0) { _state = 0; SpotifyPlayer::getInstance().fetchPlaylists(); drawList(); }
        return;
    }
    int slot = constrain((int)(y - LIST_Y - 2) / ROW_H, 0, ROWS - 1);
    int idx = _scroll + slot;
    if (idx >= (int)_items.size()) return;
    if (idx == _selected) { playSelected(); return; }
    int old = _selected;
    _selected = idx;
    // repaint the rows whose look changed (and their divider neighbours)
    for (int s = 0; s < ROWS; s++) {
        int i = _scroll + s;
        if (i == old || i == idx || i + 1 == old || i + 1 == idx) drawRow(s);
    }
    if (old < 0) drawFooter();
}

void PlaylistView::onDrag(uint16_t, uint16_t, int dy) {
    int maxScroll = max(0, (int)_items.size() - ROWS);
    _dragAccum += dy;
    int before = _scroll;
    while (_dragAccum <= -ROW_H * 6 / 10 && _scroll < maxScroll) { _scroll++; _dragAccum += ROW_H * 6 / 10; }
    while (_dragAccum >=  ROW_H * 6 / 10 && _scroll > 0)         { _scroll--; _dragAccum -= ROW_H * 6 / 10; }
    if (_scroll == 0 || _scroll == maxScroll) _dragAccum = constrain(_dragAccum, -ROW_H, ROW_H);
    if (_scroll != before) {
        for (int i = 0; i < ROWS; i++) drawRow(i);
        drawScrollbar();
    }
}

void PlaylistView::handleMessage(SCUIMessage *msg) {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    switch (msg->type) {
        case SCUIMessageType::UM_PLAYLISTS_DONE: {
            _items = sp.getPlaylists();
            if (msg->num < 0 && _items.empty()) _state = -1;
            else _state = 1;
            _scroll = constrain(_scroll, 0, max(0, (int)_items.size() - ROWS));
            if (_selected >= (int)_items.size()) _selected = -1;
            if (!_dirty) { _ui->fillRoundRect(LIST_X, LIST_Y, LIST_W, LIST_HEIGHT, 12, C_CARD, C_BG); drawList(); }
            break;
        }
        case SCUIMessageType::UM_THUMB_READY: {
            std::vector<MediaItem> fresh = sp.getPlaylists();
            if (fresh.size() != _items.size()) break;
            for (int s = 0; s < ROWS; s++) {
                int i = _scroll + s;
                if (i >= (int)_items.size()) break;
                if (fresh[i].thumb != _items[i].thumb) { _items[i].thumb = fresh[i].thumb; if (!_dirty) drawRow(s); }
            }
            for (size_t i = 0; i < _items.size(); i++) _items[i].thumb = fresh[i].thumb;
            break;
        }
        case SCUIMessageType::UM_PLAYER_REFRESH:
            if (!_dirty && _statusSig != String(WiFi.status() == WL_CONNECTED) + sp.isApiOnline()) drawHeaderStatus();
            break;
        default:
            UIView::handleMessage(msg);
            break;
    }
}
