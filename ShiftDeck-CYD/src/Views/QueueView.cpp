/*-------------------------------------------------------------------------------------------------
** QueueView.cpp - "A seguir" screen (see QueueView.h)
** ------------------------------------------------------------------------------------------------
*/
#include "QueueView.h"
#include "UIViewManager.h"

QueueView::QueueView(DisplayUI *ui) : UIView(ui) {
    _id = ViewID::Queue;
}

void QueueView::onEnter() {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    _items = sp.getQueue();
    _state = 0;
    _trackUri = sp.getCurrentlyPlayingMetadata().trackUri;
    sp.fetchQueue();
    _dirty = true;
}

void QueueView::draw() {
    _ui->clear(C_BG);
    drawBackHeader("A seguir", HEADER_H);
    drawToggles();
    drawList();
    _dirty = false;
}

void QueueView::drawToggles() {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    bool shuffle = sp.getShuffle();
    String r = sp.getRepeat();
    const char* repeat = r == "track" ? "Repetir: música" : r == "context" ? "Repetir: tudo" : "Repetir: não";
    _ui->drawButton(LIST_X, TOG_Y, TOG_W, TOG_H, shuffle ? "Aleatório: sim" : "Aleatório: não", IconID::Playlist, true,
                    shuffle ? C_GREEN_DARK : C_BUTTON, C_WHITE, C_BG, FONT_SM_B, 10);
    _ui->drawButton(LIST_X + LIST_W - TOG_W, TOG_Y, TOG_W, TOG_H, repeat, IconID::Update, true,
                    r != "off" && r.length() ? C_GREEN_DARK : C_BUTTON, C_WHITE, C_BG, FONT_SM_B, 10);
}

void QueueView::drawList() {
    _ui->fillRoundRect(LIST_X, LIST_Y, LIST_W, LIST_HEIGHT, 12, C_CARD, C_BG);
    if (_items.empty()) {
        const char* l1 = _state == 0 ? "Carregando..." : _state < 0 ? "Não foi possível carregar" : "Nada na fila";
        const char* l2 = _state > 0 ? "Na busca, segure uma música" : "";
        const char* l3 = _state > 0 ? "para ela tocar a seguir" : "";
        int32_t cy = LIST_Y + LIST_HEIGHT / 2;
        _ui->setTextColor(C_WHITE, C_CARD);
        _ui->drawCentreString(l1, CENTER_X, _ui->textY(FONT_MD_B, cy - 18), FONT_MD_B);
        _ui->setTextColor(C_MUTED, C_CARD);
        _ui->drawCentreString(l2, CENTER_X, _ui->textY(FONT_SM, cy + 6), FONT_SM);
        _ui->drawCentreString(l3, CENTER_X, _ui->textY(FONT_SM, cy + 24), FONT_SM);
        return;
    }
    const int32_t thumb = ROW_H - 8, tx = LIST_X + 8 + thumb + 10, textW = LIST_X + LIST_W - 10 - tx;
    for (int i = 0; i < ROWS && i < (int)_items.size(); i++) {
        const MediaItem& m = _items[i];
        int32_t y = LIST_Y + 2 + i * ROW_H;
        drawThumb(m, LIST_X + 8, y + 4, thumb, C_CARD, false);
        _ui->setTextColor(C_WHITE, C_CARD);
        _ui->drawString(_ui->fitText(m.name, textW, FONT_SM_B).c_str(), tx, _ui->textY(FONT_SM_B, y + 10), FONT_SM_B);
        _ui->setTextColor(C_SECONDARY, C_CARD);
        _ui->drawString(_ui->fitText(m.subtitle, textW, FONT_XS).c_str(), tx, _ui->textY(FONT_XS, y + 23), FONT_XS);
        if (i < ROWS - 1 && i + 1 < (int)_items.size()) _ui->fillRect(tx, y + ROW_H - 1, LIST_X + LIST_W - 10 - tx, 1, C_DIVIDER);
    }
}

void QueueView::onTap(uint16_t x, uint16_t y) {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    if (y < HEADER_H && x < 140) { UIViewManager::getInstance().back(); return; }
    if (y < TOG_Y - 4 || y > TOG_Y + TOG_H + 4) return;
    if (x < CENTER_X) {
        sp.setShuffle(!sp.getShuffle());
    } else {
        String r = sp.getRepeat();   // off -> tudo -> música -> off, like the Spotify app
        sp.setRepeat(r == "off" ? "context" : r == "context" ? "track" : "off");
    }
    drawToggles();
}

void QueueView::handleMessage(SCUIMessage *msg) {
    SpotifyPlayer& sp = SpotifyPlayer::getInstance();
    switch (msg->type) {
        case SCUIMessageType::UM_QUEUE_DONE:
            _items = sp.getQueue();
            _state = msg->num < 0 ? -1 : 1;
            if (!_dirty) { drawToggles(); drawList(); }
            break;
        case SCUIMessageType::UM_THUMB_READY: {
            std::vector<MediaItem> fresh = sp.getQueue();
            bool changed = fresh.size() == _items.size();
            for (size_t i = 0; changed && i < fresh.size(); i++) changed = fresh[i].uri == _items[i].uri;
            if (!changed) break;                       // another list's thumbnail, or a stale copy
            bool any = false;
            for (size_t i = 0; i < fresh.size(); i++) any |= fresh[i].thumb != _items[i].thumb;
            _items = fresh;
            if (any && !_dirty) drawList();
            break;
        }
        case SCUIMessageType::UM_PLAYER_REFRESH: {     // next track started: the queue moved
            String uri = sp.getCurrentlyPlayingMetadata().trackUri;
            if (uri != _trackUri) { _trackUri = uri; sp.fetchQueue(); }
            break;
        }
        default:
            UIView::handleMessage(msg);
            break;
    }
}
