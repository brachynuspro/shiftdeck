/*-------------------------------------------------------------------------------------------------
** SearchView.cpp - Search screen implementation (IMGS/busca.png)
** ------------------------------------------------------------------------------------------------
*/
#include "SearchView.h"
#include "UIViewManager.h"

// Keyboard pages: rows 0-2 are character keys (row 2 ends with backspace),
// row 3 is [123/ABC] [espaço] [Buscar].
static const char* const KEYS[2][3] = {
    { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" },
    { "1234567890", "-/:;()&@'", ".,?!#\"+" },
};
static const int KEY_W = 28, KEY_PITCH = 31, WIDE_W = 58, GO_W = 90;
static const size_t MAX_QUERY = 40;

SearchView::SearchView(DisplayUI *ui) : UIView(ui) {
    _id = ViewID::Search;
}

void SearchView::onEnter() {
    // Keep the last query and results: coming back to tweak a search is common.
    _keyboard = _results.empty();
    _state = _results.empty() ? 0 : 2;
    _scroll = 0;
    _dragAccum = 0;
    _pressedRow = -1;
    _dirty = true;
}

void SearchView::draw() {
    _ui->clear(C_BG);
    _ui->drawIcon(IconID::ArrowLeft, 20, TITLE_CY, 18, C_WHITE, C_BG);
    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawString("Buscar", 38, _ui->textY(FONT_MD_B, TITLE_CY), FONT_MD_B);
    drawInput();
    drawBody();
    _dirty = false;
}

void SearchView::drawInput() {
    _ui->fillRoundRect(INPUT_X, INPUT_Y, INPUT_W, INPUT_H, INPUT_H / 2, C_GREEN, C_BG);
    _ui->fillRoundRect(INPUT_X + 2, INPUT_Y + 2, INPUT_W - 4, INPUT_H - 4, INPUT_H / 2 - 2, C_BG, C_GREEN);
    const int32_t cy = INPUT_Y + INPUT_H / 2;
    _ui->drawIcon(IconID::Search, INPUT_X + 18, cy, 16, C_GREEN, C_BG);

    const int32_t tx = INPUT_X + 34;
    const int32_t maxW = INPUT_W - 34 - 34;
    if (_query.isEmpty()) {
        _ui->setTextColor(C_MUTED, C_BG);
        _ui->drawString("Música, artista ou álbum", tx, _ui->textY(FONT_MD, cy), FONT_MD);
        if (_keyboard) _ui->fillRect(tx - 3, cy - 8, 2, 16, C_GREEN);
        return;
    }
    // Show the end of long queries (that is where the user is typing).
    String shown = _query;
    while (shown.length() > 1 && _ui->textWidth(shown.c_str(), FONT_MD) > maxW) {
        int n = ((uint8_t)shown[0] >= 0xC0) ? 2 : 1;
        shown.remove(0, n);
    }
    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawString(shown.c_str(), tx, _ui->textY(FONT_MD, cy), FONT_MD);
    if (_keyboard) _ui->fillRect(tx + _ui->textWidth(shown.c_str(), FONT_MD) + 2, cy - 8, 2, 16, C_GREEN);
    _ui->drawIcon(IconID::CloseCircle, INPUT_X + INPUT_W - 18, cy, 16, C_MUTED, C_BG);
}

void SearchView::drawBody() {
    _ui->fillRect(0, LIST_Y - 2, SCREEN_W, SCREEN_H - LIST_Y + 2, C_BG);
    if (_keyboard) drawKeyboard();
    else { drawResults(); drawFooter(); }
}

/* ---------------------------------------------------------------- results */

void SearchView::drawResults() {
    for (int s = 0; s < ROWS; s++) drawResultRow(s);
    if (!_results.empty() || _state == 1) return;
    const char* l1;
    String l2;
    switch (_state) {
        case -1: l1 = "Não foi possível buscar"; l2 = "Verifique a conexão"; break;
        case 2:  l1 = "Nada encontrado"; l2 = "para \"" + _query + "\""; break;
        default: l1 = "O que você quer ouvir?"; l2 = "Músicas, artistas, álbuns e playlists"; break;
    }
    int32_t cy = LIST_Y + (FOOT_Y - LIST_Y) / 2;
    _ui->drawIcon(IconID::Search, CENTER_X, cy - 34, 28, C_MUTED, C_BG);
    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawCentreString(l1, CENTER_X, _ui->textY(FONT_MD_B, cy + 2), FONT_MD_B);
    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawCentreString(_ui->fitText(l2, 290, FONT_SM).c_str(), CENTER_X, _ui->textY(FONT_SM, cy + 24), FONT_SM);
}

void SearchView::drawResultRow(int slot) {
    const int32_t y = LIST_Y + slot * ROW_PITCH;
    const int idx = _scroll + slot;
    _ui->fillRect(INPUT_X, y, INPUT_W, ROW_H, C_BG);
    const int32_t thumb = ROW_H - 8, tx = INPUT_X + 8 + thumb + 10;
    if (idx >= (int)_results.size()) {
        if (_state == 1) {   // skeleton while searching
            _ui->fillRoundRect(INPUT_X, y, INPUT_W, ROW_H, 8, C_CARD, C_BG);
            _ui->fillRoundRect(INPUT_X + 8, y + 4, thumb, thumb, 5, C_BUTTON, C_CARD);
            _ui->fillRoundRect(tx, y + 8, 130 - slot * 15, 8, 4, C_BUTTON, C_CARD);
            _ui->fillRoundRect(tx, y + 21, 70, 7, 3, C_BUTTON, C_CARD);
        }
        return;
    }
    const MediaItem& m = _results[idx];
    _ui->fillRoundRect(INPUT_X, y, INPUT_W, ROW_H, 8, C_CARD, C_BG);
    drawThumb(m, INPUT_X + 8, y + 4, thumb, C_CARD, m.kind == MediaItem::Artist);
    const int32_t iconCx = INPUT_X + INPUT_W - 16;
    const int32_t textW = iconCx - 14 - tx;
    _ui->setTextColor(C_WHITE, C_CARD);
    _ui->drawString(_ui->fitText(m.name, textW, FONT_SM_B).c_str(), tx, _ui->textY(FONT_SM_B, y + 11), FONT_SM_B);
    _ui->setTextColor(C_SECONDARY, C_CARD);
    _ui->drawString(_ui->fitText(m.subtitle, textW, FONT_XS).c_str(), tx, _ui->textY(FONT_XS, y + 25), FONT_XS);
    if (m.kind == MediaItem::Track) _ui->drawIcon(IconID::Play, iconCx, y + ROW_H / 2, 14, C_GREEN, C_CARD);
    else                            _ui->drawIcon(IconID::ChevronRight, iconCx, y + ROW_H / 2, 13, C_SECONDARY, C_CARD);
}

void SearchView::drawFooter() {
    _ui->drawButton(INPUT_X, FOOT_Y, 104, FOOT_H, "Voltar", IconID::ArrowLeft, true, C_CARD, C_GREEN, C_BG, FONT_SM_B, 10);
    _ui->drawButton(124, FOOT_Y, 72, FOOT_H, "", IconID::Keyboard, true, C_CARD, C_WHITE, C_BG, FONT_LG, 10);
    _ui->drawButton(208, FOOT_Y, 104, FOOT_H, "Buscar", IconID::Search, true, C_CARD, C_GREEN, C_BG, FONT_SM_B, 10);
}

/* ---------------------------------------------------------------- keyboard */

bool SearchView::keyRect(int row, int col, int32_t& x, int32_t& y, int32_t& w) {
    y = KB_Y + row * (KEY_H + KEY_GAP);
    if (row < 3) {
        int n = strlen(KEYS[_numbers][row]);
        int extra = (row == 2) ? KEY_PITCH - KEY_W + WIDE_W : 0;                 // backspace
        int total = n * KEY_PITCH - (KEY_PITCH - KEY_W) + extra;
        int x0 = (SCREEN_W - total) / 2;
        if (col < n)                 { x = x0 + col * KEY_PITCH; w = KEY_W; return true; }
        if (row == 2 && col == n)    { x = x0 + n * KEY_PITCH;   w = WIDE_W; return true; }
        return false;
    }
    const int total = 10 * KEY_PITCH - (KEY_PITCH - KEY_W);
    const int x0 = (SCREEN_W - total) / 2;
    const int spaceW = total - WIDE_W - GO_W - 2 * (KEY_PITCH - KEY_W);
    switch (col) {
        case 0: x = x0; w = WIDE_W; return true;
        case 1: x = x0 + WIDE_W + (KEY_PITCH - KEY_W); w = spaceW; return true;
        case 2: x = x0 + total - GO_W; w = GO_W; return true;
    }
    return false;
}

void SearchView::drawKey(int row, int col, bool pressed) {
    int32_t x, y, w;
    if (!keyRect(row, col, x, y, w)) return;
    bool isGo = (row == 3 && col == 2);
    uint16_t fill = pressed ? C_GREEN : (isGo ? C_GREEN_DARK : C_BUTTON);
    uint16_t fg = pressed ? C_ON_GREEN : C_WHITE;
    _ui->fillRoundRect(x, y, w, KEY_H, 6, fill, C_BG);
    int32_t cx = x + w / 2, cy = y + KEY_H / 2;
    _ui->setTextColor(fg, fill);
    if (row < 3 && col < (int)strlen(KEYS[_numbers][row])) {
        char label[2] = { KEYS[_numbers][row][col], 0 };
        _ui->drawCentreString(label, cx, _ui->textY(FONT_MD_B, cy), FONT_MD_B);
    } else if (row == 2) {
        _ui->drawIcon(IconID::Backspace, cx, cy, 20, fg, fill);
    } else if (col == 0) {
        _ui->drawCentreString(_numbers ? "ABC" : "123", cx, _ui->textY(FONT_SM, cy), FONT_SM);
    } else if (col == 1) {
        _ui->setTextColor(pressed ? C_ON_GREEN : C_MUTED, fill);
        _ui->drawCentreString("espaço", cx, _ui->textY(FONT_SM, cy), FONT_SM);
    } else {
        _ui->drawCentreString("Buscar", cx, _ui->textY(FONT_MD_B, cy), FONT_MD_B);
    }
}

void SearchView::drawKeyboard() {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c <= 10; c++) drawKey(r, c, false);
}

void SearchView::pressKey(int row, int col) {
    if (row < 3 && col < (int)strlen(KEYS[_numbers][row])) {
        if (_query.length() < MAX_QUERY) {
            char ch = KEYS[_numbers][row][col];
            if (!_numbers && !_query.isEmpty()) ch = tolower(ch);   // "Xamã", not "XAMÃ"
            _query += ch;
        }
    } else if (row == 2) {                                           // backspace (UTF-8 safe)
        int i = (int)_query.length() - 1;
        while (i > 0 && ((uint8_t)_query[i] & 0xC0) == 0x80) i--;
        if (i >= 0) _query.remove(i);
    } else if (row == 3 && col == 0) {
        _numbers = !_numbers;
        drawKeyboard();
        return;
    } else if (row == 3 && col == 1) {
        if (!_query.isEmpty() && !_query.endsWith(" ") && _query.length() < MAX_QUERY) _query += ' ';
    } else if (row == 3 && col == 2) {
        runSearch();
        return;
    }
    drawInput();
}

void SearchView::runSearch() {
    _query.trim();
    if (_query.isEmpty()) {
        UIViewManager::getInstance().showToast("Digite algo para buscar");
        return;
    }
    _keyboard = false;
    _state = 1;
    _scroll = 0;
    _results.clear();
    SpotifyPlayer::getInstance().search(_query);
    drawInput();
    drawBody();
}

/* ---------------------------------------------------------------- input */

void SearchView::onTap(uint16_t x, uint16_t y) {
    if (y < INPUT_Y && x < 120) { UIViewManager::getInstance().back(); return; }

    if (hitRect(x, y, INPUT_X, INPUT_Y - 2, INPUT_W, INPUT_H + 4)) {
        if (!_query.isEmpty() && x > INPUT_X + INPUT_W - 40) _query = "";
        bool wasKeyboard = _keyboard;
        _keyboard = true;
        drawInput();
        if (!wasKeyboard) drawBody();
        return;
    }

    if (_keyboard) {
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c <= 10; c++) {
                int32_t kx, ky, kw;
                if (!keyRect(r, c, kx, ky, kw)) break;
                // hit boxes include half the gap so there are no dead zones
                if (hitRect(x, y, kx - 2, ky - 2, kw + 3, KEY_H + KEY_GAP)) {
                    if (_pressedRow >= 0) drawKey(_pressedRow, _pressedCol, false);
                    bool keepsKeyboard = !(r == 3 && c == 2);
                    if (keepsKeyboard && !(r == 3 && c == 0)) {
                        drawKey(r, c, true);
                        _pressedRow = r;
                        _pressedCol = c;
                        _pressedAt = millis();
                    }
                    pressKey(r, c);
                    return;
                }
            }
        }
        return;
    }

    if (y >= FOOT_Y - 2) {
        if (x < 116)       UIViewManager::getInstance().back();
        else if (x < 202) { _keyboard = true; drawInput(); drawBody(); }
        else if (_query.isEmpty()) { _keyboard = true; drawInput(); drawBody(); }
        else               runSearch();
        return;
    }
    int slot = (int)(y - LIST_Y) / ROW_PITCH;
    int idx = _scroll + slot;
    if (y < LIST_Y || slot >= ROWS || idx >= (int)_results.size()) return;
    SpotifyPlayer::getInstance().playItem(_results[idx]);
    UIViewManager::getInstance().home();
}

bool SearchView::onLongPress(uint16_t x, uint16_t y) {
    int slot = (int)(y - LIST_Y) / ROW_PITCH;
    int idx = _scroll + slot;
    if (_keyboard || y < LIST_Y || y >= FOOT_Y - 2 || slot >= ROWS || idx >= (int)_results.size()) return false;
    const MediaItem& m = _results[idx];
    if (m.kind == MediaItem::Track) SpotifyPlayer::getInstance().addToQueue(m.uri);   // the player toasts the result
    else UIViewManager::getInstance().showToast("Só músicas vão para a fila");
    return true;
}

void SearchView::onDrag(uint16_t, uint16_t startY, int dy) {
    if (_keyboard || startY < LIST_Y || startY >= FOOT_Y) return;
    int maxScroll = max(0, (int)_results.size() - ROWS);
    _dragAccum += dy;
    int before = _scroll;
    const int step = ROW_PITCH * 6 / 10;
    while (_dragAccum <= -step && _scroll < maxScroll) { _scroll++; _dragAccum += step; }
    while (_dragAccum >=  step && _scroll > 0)         { _scroll--; _dragAccum -= step; }
    if (_scroll == 0 || _scroll == maxScroll) _dragAccum = constrain(_dragAccum, -step, step);
    if (_scroll != before) for (int s = 0; s < ROWS; s++) drawResultRow(s);
}

void SearchView::handleMessage(SCUIMessage *msg) {
    switch (msg->type) {
        case SCUIMessageType::UM_SEARCH_DONE:
            _results = SpotifyPlayer::getInstance().getSearchResults();
            _state = msg->num < 0 ? -1 : 2;
            _scroll = 0;
            if (!_dirty && !_keyboard) drawBody();
            break;
        case SCUIMessageType::UM_THUMB_READY: {
            std::vector<MediaItem> fresh = SpotifyPlayer::getInstance().getSearchResults();
            if (fresh.size() != _results.size()) break;
            for (size_t i = 0; i < _results.size(); i++) {
                if (fresh[i].thumb == _results[i].thumb) continue;
                _results[i].thumb = fresh[i].thumb;
                int slot = (int)i - _scroll;
                if (!_dirty && !_keyboard && slot >= 0 && slot < ROWS) drawResultRow(slot);
            }
            break;
        }
        case SCUIMessageType::UM_IDLE:
            if (_pressedRow >= 0 && millis() - _pressedAt > 120) {
                if (_keyboard && !_dirty) drawKey(_pressedRow, _pressedCol, false);
                _pressedRow = -1;
            }
            break;
        default:
            UIView::handleMessage(msg);
            break;
    }
}
