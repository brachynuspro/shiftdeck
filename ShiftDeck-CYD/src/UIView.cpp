/*-------------------------------------------------------------------------------------------------
** UIView.cpp - Abstract view base implementation
** ------------------------------------------------------------------------------------------------
*/
#include "UIView.h"
#include "SpotifyPlayer.h"
#include <WiFi.h>

void UIView::handleMessage(SCUIMessage *msg) {
    if (msg->type == SCUIMessageType::UM_MARK_DIRTY) _dirty = true;
}

void UIView::onTouchMove(uint16_t x, uint16_t y) {
    if (!_dragging && (abs((int)x - _downX) > TAP_SLOP || abs((int)y - _downY) > TAP_SLOP)) _dragging = true;
    _curX = x;
    if (_dragging) {
        onDrag(_downX, _downY, (int)y - (int)_lastY);
        _lastY = y;
    }
}

void UIView::onTouchUp(uint16_t, uint16_t) {
    if (!_dragging) onTap(_downX, _downY);
    _dragging = false;
}

void UIView::drawBrand(int32_t x0, int32_t cy, float h, uint8_t font) {
    const float w = h * 34 / 30, x = x0, y = cy - h / 2;
    const float upper[8] = { x + .42f * w, y,            x + .94f * w, y,            x + .60f * w, y + .44f * h, x + .06f * w, y + .44f * h };
    const float lower[8] = { x + .42f * w, y + .56f * h, x + .94f * w, y + .56f * h, x + .60f * w, y + h,        x + .06f * w, y + h };
    _ui->fillQuad(upper, C_GREEN, C_BG);
    _ui->fillQuad(lower, C_GREEN, C_BG);
    _ui->fillRect((int32_t)(x + .42f * w), (int32_t)(y + .40f * h), (int32_t)(.12f * w), (int32_t)(.2f * h), C_GREEN);
    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawString(APP_NAME, x0 + brandMarkW(h), _ui->textY(font, cy), font);
}

void UIView::drawStatusItem(int32_t x, int32_t cy, IconID icon, uint16_t iconColor, const char* title,
                            const char* value, uint16_t valueColor, uint16_t bg, int32_t maxW) {
    _ui->fillRect(x, cy - 15, 24 + maxW, 30, bg);
    _ui->drawIcon(icon, x + 10, cy, 20, iconColor, bg);
    _ui->setTextColor(C_WHITE, bg);
    _ui->drawString(_ui->fitText(title, maxW, FONT_XS_B).c_str(), x + 26, _ui->textY(FONT_XS_B, cy - 7), FONT_XS_B);
    _ui->setTextColor(valueColor, bg);
    _ui->drawString(_ui->fitText(value, maxW, FONT_XS).c_str(), x + 26, _ui->textY(FONT_XS, cy + 8), FONT_XS);
}

void UIView::drawWifiStatus(int32_t x, int32_t cy, uint16_t bg) {
    bool ok = WiFi.status() == WL_CONNECTED;
    drawStatusItem(x, cy, ok ? IconID::Wifi : IconID::WifiOff, ok ? C_WHITE : C_ERROR, "Wi-Fi",
                   ok ? "Conectado" : "Sem rede", ok ? C_GREEN : C_ERROR, bg);
}

void UIView::drawSpotifyStatus(int32_t x, int32_t cy, uint16_t bg) {
    bool ok = SpotifyPlayer::getInstance().isApiOnline();
    drawStatusItem(x, cy, IconID::Spotify, ok ? C_GREEN : C_MUTED, "Spotify",
                   ok ? "Conectado" : "Offline", ok ? C_GREEN : C_MUTED, bg);
}

void UIView::drawThumb(const MediaItem& m, int32_t x, int32_t y, int32_t s, uint16_t bg, bool circle) {
    int32_t r = circle ? s / 2 : 5;
    if (m.kind == MediaItem::Liked) {
        _ui->fillGradientV(x, y, s, s, C_LIKED_TOP, C_LIKED_BOT);
        _ui->roundCorners(x, y, s, s, r, bg);
        _ui->drawIcon(IconID::HeartFilled, x + s / 2, y + s / 2 + 1, s / 2, C_WHITE, 0x4979);
        return;
    }
    if (m.thumb == 1 && _ui->drawJpgScaled(x, y, s, s, SpotifyPlayer::thumbPath(m.imageUrl).c_str())) {
        _ui->roundCorners(x, y, s, s, r, bg);
        return;
    }
    _ui->fillRoundRect(x, y, s, s, r, C_BUTTON, bg);
    _ui->drawIcon(IconID::Playlist, x + s / 2, y + s / 2, s / 2, m.kind == MediaItem::Playlist ? C_GREEN : C_MUTED, C_BUTTON);
}

void UIView::drawBackHeader(const char* title, int32_t h) {
    _ui->fillRect(0, 0, SCREEN_W, h, C_BG);
    _ui->drawIcon(IconID::ArrowLeft, 22, h / 2, 20, C_WHITE, C_BG);
    _ui->setTextColor(C_WHITE, C_BG);
    _ui->drawString(title, 42, _ui->textY(FONT_LG, h / 2), FONT_LG);
}
