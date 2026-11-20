/*-------------------------------------------------------------------------------------------------
** SetupView.cpp - Shared layout of the first-run screens (see SetupView.h)
** ------------------------------------------------------------------------------------------------
*/
#include "SetupView.h"
#include <qrcode.h>

static const int32_t PAD = 14, CARD_Y = 82, QR_S = 108, ROW_H = 39;
static const int32_t LIST_X = PAD + QR_S + 8, LIST_W = SCREEN_W - PAD - LIST_X, CARD_H = ROW_H * 3 + 1;

SetupView::SetupView(DisplayUI *ui, int step, const char* title, const char* subtitle, const char* footer)
    : UIView(ui), _step(step), _title(title), _subtitle(subtitle), _footer(footer) {
    _id = ViewID::Settings;
    for (auto& r : _rows) r = { IconID::Settings, "", "", C_WHITE };
}

void SetupView::setRow(int i, IconID icon, const char* label, const String& value, uint16_t color) {
    _rows[i] = { icon, label, value, color };
    if (!_dirty) drawRow(i);
}

void SetupView::draw() {
    _ui->clear(C_BG);
    drawBrand(PAD, 18, 18, FONT_MD_B);
    char step[16];
    snprintf(step, sizeof(step), "Passo %d de 3", _step);
    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawRightString(step, SCREEN_W - PAD, _ui->textY(FONT_XS, 18), FONT_XS);

    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawString(_title, PAD, _ui->textY(FONT_LG, 46), FONT_LG);
    _ui->setTextColor(C_SECONDARY, C_BG);
    _ui->drawString(_subtitle, PAD, _ui->textY(FONT_XS, 66), FONT_XS);

    _ui->fillRoundRect(PAD, CARD_Y, QR_S, QR_S, 10, C_WHITE, C_BG);
    drawQR(PAD, CARD_Y, QR_S);

    _ui->fillRoundRect(LIST_X, CARD_Y, LIST_W, CARD_H, 10, C_CARD, C_BG);
    for (int i = 0; i < 3; i++) {
        if (i) _ui->fillRect(LIST_X + 10, CARD_Y + i * ROW_H, LIST_W - 20, 1, C_DIVIDER);
        drawRow(i);
    }

    _ui->setTextColor(C_MUTED, C_BG);
    _ui->drawCentreString(_footer, CENTER_X, _ui->textY(FONT_XS, 222), FONT_XS);
    _dirty = false;
}

void SetupView::drawRow(int i) {
    const Row& r = _rows[i];
    int32_t y0 = CARD_Y + 1 + i * ROW_H, cy = y0 + ROW_H / 2;
    // stays inside the card's rounded corners, keeps the dividers
    _ui->fillRect(LIST_X + 6, y0 + 3, LIST_W - 12, ROW_H - 5, C_CARD);
    _ui->drawIcon(r.icon, LIST_X + 16, cy, 16, r.color == C_WHITE ? C_GREEN : r.color, C_CARD);
    _ui->setTextColor(C_MUTED, C_CARD);
    _ui->drawString(r.label, LIST_X + 29, _ui->textY(FONT_XS, cy - 8), FONT_XS);
    const int32_t maxW = LIST_W - 33;
    uint8_t font = _ui->textWidth(r.value.c_str(), FONT_SM_B) <= maxW ? FONT_SM_B : FONT_SM;   // URLs fit in regular
    _ui->setTextColor(r.color, C_CARD);
    _ui->drawString(_ui->fitText(r.value, maxW, font).c_str(), LIST_X + 29, _ui->textY(font, cy + 8), font);
}

void SetupView::drawQR(int32_t x, int32_t y, int32_t box) {
    if (_qr.isEmpty()) return;
    QRCode qr;
    uint8_t buf[384];
    int res = -1;
    for (uint8_t v = 3; v <= 6 && res != 0; v++)
        if (qrcode_getBufferSize(v) <= (int)sizeof(buf)) res = qrcode_initText(&qr, buf, v, ECC_LOW, _qr.c_str());
    if (res != 0) return;
    int32_t m = (box - 16) / qr.size;               // >= 8 px of white quiet zone per side
    int32_t ox = x + (box - m * qr.size) / 2, oy = y + (box - m * qr.size) / 2;
    for (uint8_t r = 0; r < qr.size; r++)
        for (uint8_t c = 0; c < qr.size; c++)
            if (qrcode_getModule(&qr, c, r)) _ui->fillRect(ox + c * m, oy + r * m, m, m, C_BG);
}
