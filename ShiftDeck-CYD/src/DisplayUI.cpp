/*-------------------------------------------------------------------------------------------------
** DisplayUI.cpp - UI rendering on ILI9341 via TFT_eSPI
** ------------------------------------------------------------------------------------------------
*/
#include "DisplayUI.h"
#include "fonts/InterFonts.h"
#include <esp_task_wdt.h>
#include <LittleFS.h>
#include <math.h>
#include "iTunesProxy.h"

static const uint8_t FONT_COUNT = sizeof(FONT_TABLE) / sizeof(FONT_TABLE[0]);

static const FontInfo& fontInfo(uint8_t id) {
    if (id < 1 || id > FONT_COUNT) id = FONT_MD;
    return FONT_TABLE[id - 1];
}

// The fonts cover Latin-1. Map the punctuation Spotify metadata uses most
// (curly quotes, dashes, bullets, ellipsis) to Latin-1 look-alikes and drop
// anything else (emoji, CJK) so it never renders as an empty box.
static String normalize(const char* s) {
    String out;
    if (!s) return out;
    out.reserve(strlen(s));
    const uint8_t* p = (const uint8_t*)s;
    while (*p) {
        uint32_t cp;
        int n;
        if (p[0] < 0x80)                                       { cp = p[0]; n = 1; }
        else if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); n = 2; }
        else if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); n = 3;
        }
        else if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            cp = 0x10000; n = 4;
        }
        else { p++; continue; }   // stray byte

        if (cp >= 0x20 && (cp < 0x7F || (cp >= 0xA0 && cp <= 0xFF))) {
            for (int i = 0; i < n; i++) out += (char)p[i];
        } else {
            switch (cp) {
                case 0x2018: case 0x2019: case 0x201B: case 0x2032: out += '\''; break;
                case 0x201C: case 0x201D: case 0x2033:              out += '"'; break;
                case 0x2010: case 0x2011: case 0x2012:
                case 0x2013: case 0x2014: case 0x2212:              out += '-'; break;
                case 0x2026:                                        out += "..."; break;
                case 0x2022: case 0x2027: case 0x30FB:              out += "\xC2\xB7"; break;  // middle dot
                case 0x00A0: case 0x2002: case 0x2003: case 0x2009: case 0x202F: out += ' '; break;
                default: break;
            }
        }
        p += n;
    }
    return out;
}

// TJpg_Decoder state (static callback -> instance bridge).
static TFT_eSPI *s_tft_for_jpg = nullptr;
static int32_t s_dstX = 0, s_dstY = 0;
static uint16_t s_srcW = 0, s_srcH = 0;
static uint16_t *s_outBuf = nullptr;
static uint32_t s_blockCount = 0;

DisplayUI::DisplayUI(TFT_eSPI *tft) {
    _tft = tft;
    s_tft_for_jpg = tft;
    TJpgDec.setCallback(DisplayUI::jpgCallback);
}

void DisplayUI::init() {
    // TJpg_Decoder emits little-endian RGB565; pushImage expects big-endian.
    TJpgDec.setSwapBytes(true);
    // Never wrap: a label touching the right edge would otherwise continue at x = 0.
    _tft->setTextWrap(false, false);
}

void DisplayUI::useFont(uint8_t id) {
    if (id < 1 || id > FONT_COUNT) id = FONT_MD;
    if (_font == id) return;
    if (_font) _tft->unloadFont();
    _tft->loadFont(FONT_TABLE[id - 1].data);
    _font = id;
}

#define LOCK_OR_RETURN(...) if (!xSemaphoreTake(_xSemaphoreDisplay, portMAX_DELAY)) return __VA_ARGS__
#define UNLOCK() xSemaphoreGive(_xSemaphoreDisplay)

/* ---------------------------------------------------------------- primitives */

void DisplayUI::clear(uint16_t color) {
    LOCK_OR_RETURN();
    _tft->fillScreen(color);
    UNLOCK();
}

void DisplayUI::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color) {
    LOCK_OR_RETURN();
    _tft->fillRect(x, y, w, h, color);
    UNLOCK();
}

void DisplayUI::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t color, uint16_t bg) {
    if (w <= 0 || h <= 0) return;
    r = min(r, min(w, h) / 2);
    LOCK_OR_RETURN();
    if (r <= 0) _tft->fillRect(x, y, w, h, color);
    else        _tft->fillSmoothRoundRect(x, y, w, h, r, color, bg);
    UNLOCK();
}

void DisplayUI::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t color, uint16_t bg, int32_t thickness) {
    if (w <= 0 || h <= 0) return;
    r = max((int32_t)1, min(r, min(w, h) / 2));
    LOCK_OR_RETURN();
    _tft->drawSmoothRoundRect(x, y, r, max((int32_t)0, r - thickness + 1), w, h, color, bg);
    UNLOCK();
}

void DisplayUI::fillCircle(int32_t x, int32_t y, int32_t r, uint16_t color, uint16_t bg) {
    LOCK_OR_RETURN();
    _tft->fillSmoothCircle(x, y, r, color, bg);
    UNLOCK();
}

void DisplayUI::drawLine(float ax, float ay, float bx, float by, float width, uint16_t color, uint16_t bg) {
    LOCK_OR_RETURN();
    _tft->drawWideLine(ax, ay, bx, by, width, color, bg);
    UNLOCK();
}

void DisplayUI::fillQuad(const float* p, uint16_t color, uint16_t bg) {
    LOCK_OR_RETURN();
    for (int i = 0; i < 4; i++) {   // AA edges first, solid fill on top
        int j = (i + 1) % 4;
        _tft->drawWideLine(p[2 * i], p[2 * i + 1], p[2 * j], p[2 * j + 1], 1.2f, color, bg);
    }
    auto I = [](float v) { return (int32_t)lroundf(v); };
    _tft->fillTriangle(I(p[0]), I(p[1]), I(p[2]), I(p[3]), I(p[4]), I(p[5]), color);
    _tft->fillTriangle(I(p[0]), I(p[1]), I(p[4]), I(p[5]), I(p[6]), I(p[7]), color);
    UNLOCK();
}

void DisplayUI::drawCrosshair(int32_t cx, int32_t cy, int32_t size, uint16_t color) {
    LOCK_OR_RETURN();
    _tft->drawFastHLine(cx - size, cy, size * 2, color);
    _tft->drawFastVLine(cx, cy - size, size * 2, color);
    _tft->drawCircle(cx, cy, size / 2, color);
    UNLOCK();
}

void DisplayUI::fillGradientV(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t cTop, uint16_t cBottom) {
    if (h <= 0 || w <= 0) return;
    LOCK_OR_RETURN();
    for (int32_t row = 0; row < h; row++) {
        uint8_t a = (h > 1) ? (uint8_t)(255 - (row * 255) / (h - 1)) : 255;
        _tft->drawFastHLine(x, y + row, w, _tft->alphaBlend(a, cTop, cBottom));
    }
    UNLOCK();
}

void DisplayUI::sliderLocked(int32_t x, int32_t cy, int32_t w, int32_t kx, uint16_t fill, uint16_t track,
                             uint16_t bg, int32_t knobR) {
    const int32_t th = 4;
    const int32_t y = cy - th / 2;
    // Played and remaining parts never overlap, so repainting them on
    // every tick does not flicker.
    if (kx - x >= th)          _tft->fillSmoothRoundRect(x, y, kx - x, th, th / 2, fill, bg);
    else if (kx > x)           _tft->fillRect(x, y, kx - x, th, fill);
    if (x + w - kx >= th)      _tft->fillSmoothRoundRect(kx, y, x + w - kx, th, th / 2, track, bg);
    else if (x + w > kx)       _tft->fillRect(kx, y, x + w - kx, th, track);
    if (knobR > 0)             _tft->fillSmoothCircle(kx, cy, knobR, fill, bg);
}

void DisplayUI::drawSlider(int32_t x, int32_t cy, int32_t w, int32_t permille, uint16_t fill, uint16_t track,
                           uint16_t bg, int32_t knobR, int16_t *lastKnobX) {
    permille = constrain(permille, 0, 1000);
    int32_t kx = x + (int32_t)((int64_t)w * permille / 1000);
    int32_t pad = max(knobR, (int32_t)2) + 1;
    LOCK_OR_RETURN();
    if (lastKnobX && *lastKnobX >= 0) {
        if (*lastKnobX == kx) { UNLOCK(); return; }
        int32_t a = max(min((int32_t)*lastKnobX, kx) - pad, x - pad);
        int32_t b = min(max((int32_t)*lastKnobX, kx) + pad, x + w + pad);
        _tft->fillRect(a, cy - pad, b - a + 1, 2 * pad + 1, bg);
    } else {
        _tft->fillRect(x - pad, cy - pad, w + 2 * pad + 1, 2 * pad + 1, bg);
    }
    sliderLocked(x, cy, w, kx, fill, track, bg, knobR);
    if (lastKnobX) *lastKnobX = (int16_t)kx;
    UNLOCK();
}

void DisplayUI::drawProgressBar(int32_t x, int32_t y, int32_t w, int32_t h,
                                uint8_t percentage, uint16_t trackColor, uint16_t barColor) {
    percentage = min(percentage, (uint8_t)100);
    int32_t r = h / 2;
    LOCK_OR_RETURN();
    _tft->fillSmoothRoundRect(x, y, w, h, r, trackColor, C_BG);
    int32_t bw = w * percentage / 100;
    if (bw >= h) _tft->fillSmoothRoundRect(x, y, bw, h, r, barColor, trackColor);
    UNLOCK();
}

void DisplayUI::drawButton(int32_t x, int32_t y, int32_t w, int32_t h, const char* label, IconID icon, bool hasIcon,
                           uint16_t fill, uint16_t fg, uint16_t bg, uint8_t font, int32_t r) {
    String s = normalize(label);
    const FontInfo& fi = fontInfo(font);
    LOCK_OR_RETURN();
    _tft->fillSmoothRoundRect(x, y, w, h, min(r, min(w, h) / 2), fill, bg);
    useFont(font);
    int32_t tw = s.length() ? _tft->textWidth(s) : 0;
    int32_t isz = fi.cap * 17 / 10 + 2;
    int32_t gap = (hasIcon && tw) ? 8 : 0;
    int32_t total = (hasIcon ? isz : 0) + gap + tw;
    int32_t sx = x + (w - total) / 2;
    int32_t cy = y + h / 2;
    if (hasIcon) iconLocked(icon, sx + isz / 2, cy, isz, fg, fill);
    if (tw) {
        _tft->setTextColor(fg, fill);
        _tft->setCursor(sx + (hasIcon ? isz + gap : 0), cy - fi.ascent + (fi.cap + 1) / 2);
        _tft->print(s);
    }
    UNLOCK();
}

/* ---------------------------------------------------------------- icons */

void DisplayUI::drawIcon(IconID icon, int32_t cx, int32_t cy, int32_t size, uint16_t color, uint16_t bg) {
    LOCK_OR_RETURN();
    iconLocked(icon, cx, cy, size, color, bg);
    UNLOCK();
}

void DisplayUI::iconLocked(IconID icon, int32_t cx, int32_t cy, int32_t size, uint16_t color, uint16_t bg) {
    TFT_eSPI* t = _tft;
    const float s = (float)size;
    const float sw = max(1.6f, s * 0.09f);   // stroke width
    auto X = [&](float f) { return cx + f * s; };
    auto Y = [&](float f) { return cy + f * s; };
    auto I = [](float v) { return (int32_t)lroundf(v); };
    auto line = [&](float ax, float ay, float bx, float by, float w, uint16_t c, uint16_t b) {
        t->drawWideLine(X(ax), Y(ay), X(bx), Y(by), w, c, b);
    };
    auto L = [&](float ax, float ay, float bx, float by) { line(ax, ay, bx, by, sw, color, bg); };
    auto dot = [&](float x, float y, float r, uint16_t c, uint16_t b) {
        t->fillSmoothCircle(I(X(x)), I(Y(y)), max((int32_t)1, I(r * s)), c, b);
    };
    // Arc ring of radius r (fraction of size); angles: 0 = 6 o'clock, clockwise.
    auto arc = [&](float x, float y, float r, float thick, int a0, int a1, uint16_t c, uint16_t b, bool round) {
        int32_t R = max((int32_t)2, I(r * s));
        int32_t ir = max((int32_t)0, R - max((int32_t)1, (int32_t)thick));
        t->drawSmoothArc(I(X(x)), I(Y(y)), R, ir, a0, a1, c, b, round);
    };
    auto rrect = [&](float x, float y, float w, float h, float r, bool filled) {
        int32_t ix = I(X(x)), iy = I(Y(y)), iw = I(w * s), ih = I(h * s), ir = max((int32_t)1, I(r * s));
        if (filled) t->fillSmoothRoundRect(ix, iy, iw, ih, ir, color, bg);
        else        t->drawSmoothRoundRect(ix, iy, ir, max((int32_t)0, ir - I(sw) + 1), iw, ih, color, bg);
    };
    // Anti-aliased triangle: AA edges first (they blend against bg on both
    // sides), then the solid fill on top restores the inner half.
    auto tri = [&](float ax, float ay, float bx, float by, float qx, float qy) {
        line(ax, ay, bx, by, 1.2f, color, bg);
        line(bx, by, qx, qy, 1.2f, color, bg);
        line(qx, qy, ax, ay, 1.2f, color, bg);
        t->fillTriangle(I(X(ax)), I(Y(ay)), I(X(bx)), I(Y(by)), I(X(qx)), I(Y(qy)), color);
    };
    auto heart = [&](float k, float oy, uint16_t c, uint16_t b) {
        float lr = 0.25f * k, lx = 0.235f * k, ly = -0.12f * k + oy;
        float yT = ly + 0.06f * k, xT = 0.47f * k, yB = 0.40f * k + oy;
        line(-xT, yT, 0, yB, 1.2f, c, b);
        line(xT, yT, 0, yB, 1.2f, c, b);
        dot(-lx, ly, lr, c, b);
        dot(lx, ly, lr, c, b);
        t->fillTriangle(I(X(-xT)), I(Y(yT)), I(X(xT)), I(Y(yT)), cx, I(Y(yB)), c);
    };
    auto arrowHead = [&](float px, float py, float dx, float dy, float len) {
        float n = sqrtf(dx * dx + dy * dy);
        dx /= n; dy /= n;
        float tx = px + dx * len, ty = py + dy * len;
        float bx = px - dx * len * 0.2f, by = py - dy * len * 0.2f;
        tri(tx, ty, bx - dy * len * 0.7f, by + dx * len * 0.7f, bx + dy * len * 0.7f, by - dx * len * 0.7f);
    };

    switch (icon) {
        case IconID::Wifi:
        case IconID::WifiOff: {
            const float by = 0.34f;
            dot(0, by, 0.075f, color, bg);
            for (int i = 0; i < 3; i++) arc(0, by, 0.26f + i * 0.21f, sw * 1.3f, 135, 225, color, bg, true);
            if (icon == IconID::WifiOff) {
                line(-0.42f, -0.36f, 0.42f, 0.44f, sw + 2.0f, bg, bg);
                L(-0.42f, -0.36f, 0.42f, 0.44f);
            }
            break;
        }
        case IconID::Spotify: {
            dot(0, 0, 0.5f, color, bg);
            // three curved bars cut out of the disc
            const float bars[3][3] = { {-0.21f, 0.31f, 0.085f}, {-0.03f, 0.26f, 0.07f}, {0.13f, 0.20f, 0.06f} };
            const float radii[3] = { 0.62f, 0.52f, 0.42f };
            for (int i = 0; i < 3; i++) {
                float R = radii[i];
                int ha = (int)(asinf(bars[i][1] / R) * 180.0f / (float)PI);
                arc(0, bars[i][0] + R, R, bars[i][2] * s, 180 - ha, 180 + ha, bg, color, true);
            }
            break;
        }
        case IconID::Phone:
            rrect(-0.27f, -0.47f, 0.54f, 0.94f, 0.12f, false);
            dot(0, 0.32f, 0.055f, color, bg);
            break;
        case IconID::Display:
            rrect(-0.47f, -0.42f, 0.94f, 0.64f, 0.08f, false);
            L(0, 0.22f, 0, 0.38f);
            L(-0.22f, 0.40f, 0.22f, 0.40f);
            break;
        case IconID::Speaker:
            rrect(-0.3f, -0.47f, 0.6f, 0.94f, 0.1f, false);
            arc(0, 0.12f, 0.17f, sw, 0, 360, color, bg, false);
            dot(0, -0.26f, 0.06f, color, bg);
            break;
        case IconID::Touch: {
            int32_t fx = I(X(-0.17f)), fy = I(Y(-0.48f));
            t->fillSmoothRoundRect(fx, fy, I(0.2f * s), I(0.62f * s), I(0.1f * s), color, bg);
            t->fillSmoothRoundRect(I(X(-0.17f)), I(Y(-0.06f)), I(0.58f * s), I(0.52f * s), I(0.16f * s), color, bg);
            line(-0.14f, 0.08f, -0.36f, -0.08f, 0.16f * s, color, bg);
            break;
        }
        case IconID::Cloud: {
            dot(-0.2f, 0.08f, 0.2f, color, bg);
            dot(0.03f, -0.07f, 0.27f, color, bg);
            dot(0.26f, 0.1f, 0.18f, color, bg);
            rrect(-0.4f, 0.06f, 0.84f, 0.22f, 0.1f, true);
            break;
        }
        case IconID::Check:
            line(-0.34f, 0.02f, -0.1f, 0.26f, sw * 1.2f, color, bg);
            line(-0.1f, 0.26f, 0.36f, -0.24f, sw * 1.2f, color, bg);
            break;
        case IconID::CheckCircle:
            arc(0, 0, 0.47f, max(1.2f, s * 0.08f), 0, 360, color, bg, false);
            line(-0.19f, 0.01f, -0.05f, 0.14f, 1.4f, color, bg);
            line(-0.05f, 0.14f, 0.2f, -0.12f, 1.4f, color, bg);
            break;
        case IconID::Spinner: {
            uint16_t dim = t->alphaBlend(60, color, bg);
            float th = max(1.2f, s * 0.08f);
            arc(0, 0, 0.47f, th, 0, 360, dim, bg, false);
            int a = (int)((millis() / 3) % 360);
            arc(0, 0, 0.47f, th, a, (a + 110) % 360, color, bg, false);
            break;
        }
        case IconID::ChevronRight:
            L(-0.14f, -0.3f, 0.14f, 0);
            L(0.14f, 0, -0.14f, 0.3f);
            break;
        case IconID::Back:
            L(0.14f, -0.3f, -0.14f, 0);
            L(-0.14f, 0, 0.14f, 0.3f);
            break;
        case IconID::ArrowLeft:
            L(-0.38f, 0, 0.4f, 0);
            L(-0.38f, 0, -0.1f, -0.28f);
            L(-0.38f, 0, -0.1f, 0.28f);
            break;
        case IconID::Search:
            arc(-0.08f, -0.08f, 0.32f, sw, 0, 360, color, bg, false);
            line(0.16f, 0.16f, 0.42f, 0.42f, sw * 1.25f, color, bg);
            break;
        case IconID::HeartFilled:
            heart(1.0f, 0, color, bg);
            break;
        case IconID::HeartOutline:
            heart(1.0f, 0, color, bg);
            heart(0.66f, 0.01f, bg, color);
            break;
        case IconID::Play:
            tri(-0.28f, -0.4f, -0.28f, 0.4f, 0.42f, 0);
            break;
        case IconID::Pause:
            rrect(-0.34f, -0.4f, 0.24f, 0.8f, 0.06f, true);
            rrect(0.10f, -0.4f, 0.24f, 0.8f, 0.06f, true);
            break;
        case IconID::Prev:
            rrect(-0.42f, -0.38f, 0.14f, 0.76f, 0.05f, true);
            tri(0.38f, -0.38f, 0.38f, 0.38f, -0.26f, 0);
            break;
        case IconID::Next:
            rrect(0.28f, -0.38f, 0.14f, 0.76f, 0.05f, true);
            tri(-0.38f, -0.38f, -0.38f, 0.38f, 0.26f, 0);
            break;
        case IconID::Close:
            L(-0.28f, -0.28f, 0.28f, 0.28f);
            L(-0.28f, 0.28f, 0.28f, -0.28f);
            break;
        case IconID::CloseCircle:
            dot(0, 0, 0.5f, color, bg);
            line(-0.17f, -0.17f, 0.17f, 0.17f, sw, bg, color);
            line(-0.17f, 0.17f, 0.17f, -0.17f, sw, bg, color);
            break;
        case IconID::Brightness:
            arc(0, 0, 0.2f, sw, 0, 360, color, bg, false);
            for (int i = 0; i < 8; i++) {
                float a = i * (float)PI / 4.0f, c = cosf(a), n = sinf(a);
                L(c * 0.33f, n * 0.33f, c * 0.46f, n * 0.46f);
            }
            break;
        case IconID::Update: {
            // two arcs chasing each other clockwise, arrow heads at their ends
            const float R = 0.34f;
            arc(0, 0, R, sw, 60, 160, color, bg, true);
            arc(0, 0, R, sw, 240, 340, color, bg, true);
            for (int deg : { 160, 340 }) {
                float a = deg * (float)PI / 180;
                arrowHead(-R * sinf(a), R * cosf(a), -cosf(a), -sinf(a), 0.2f);
            }
            break;
        }
        case IconID::Diagnostics: {
            const float p[6][2] = { {-0.47f, 0.04f}, {-0.22f, 0.04f}, {-0.1f, -0.32f}, {0.06f, 0.36f}, {0.18f, 0.04f}, {0.47f, 0.04f} };
            for (int i = 0; i < 5; i++) L(p[i][0], p[i][1], p[i + 1][0], p[i + 1][1]);
            break;
        }
        case IconID::Save:
            rrect(-0.42f, -0.42f, 0.84f, 0.84f, 0.1f, false);
            L(-0.2f, -0.4f, -0.2f, -0.14f);
            L(-0.2f, -0.14f, 0.18f, -0.14f);
            L(0.18f, -0.14f, 0.18f, -0.4f);
            rrect(-0.24f, 0.06f, 0.48f, 0.32f, 0.04f, false);
            break;
        case IconID::Undo:
            L(-0.36f, -0.2f, 0.08f, -0.2f);
            arc(0.08f, 0.03f, 0.23f, sw, 180, 360, color, bg, false);
            L(0.08f, 0.26f, -0.2f, 0.26f);
            L(-0.36f, -0.2f, -0.2f, -0.35f);
            L(-0.36f, -0.2f, -0.2f, -0.05f);
            break;
        case IconID::Keyboard: {
            rrect(-0.48f, -0.32f, 0.96f, 0.64f, 0.1f, false);
            for (int r = 0; r < 2; r++)
                for (int c = 0; c < 5; c++)
                    dot(-0.28f + c * 0.14f, -0.13f + r * 0.13f, 0.035f, color, bg);
            L(-0.18f, 0.17f, 0.18f, 0.17f);
            break;
        }
        case IconID::Playlist:
            L(-0.42f, -0.28f, 0.08f, -0.28f);
            L(-0.42f, -0.06f, 0.08f, -0.06f);
            L(-0.42f, 0.16f, -0.12f, 0.16f);
            L(0.3f, -0.38f, 0.3f, 0.22f);
            L(0.3f, -0.38f, 0.46f, -0.28f);
            dot(0.18f, 0.25f, 0.13f, color, bg);
            break;
        case IconID::Alert:
            L(0, -0.42f, 0.46f, 0.38f);
            L(0.46f, 0.38f, -0.46f, 0.38f);
            L(-0.46f, 0.38f, 0, -0.42f);
            L(0, -0.12f, 0, 0.1f);
            dot(0, 0.24f, 0.05f, color, bg);
            break;
        case IconID::Backspace:
            L(-0.2f, -0.3f, 0.42f, -0.3f);
            L(0.42f, -0.3f, 0.42f, 0.3f);
            L(0.42f, 0.3f, -0.2f, 0.3f);
            L(-0.2f, 0.3f, -0.46f, 0);
            L(-0.46f, 0, -0.2f, -0.3f);
            L(0.0f, -0.12f, 0.22f, 0.12f);
            L(0.0f, 0.12f, 0.22f, -0.12f);
            break;
        case IconID::Settings: {
            arc(0, 0, 0.3f, 0.13f * s, 0, 360, color, bg, false);
            for (int i = 0; i < 8; i++) {
                float a = i * (float)PI / 4.0f, c = cosf(a), n = sinf(a);
                line(c * 0.3f, n * 0.3f, c * 0.45f, n * 0.45f, 0.16f * s, color, bg);
            }
            break;
        }
    }
}

/* ---------------------------------------------------------------- text */

void DisplayUI::setTextColor(uint16_t fg, uint16_t bg) {
    LOCK_OR_RETURN();
    _tft->setTextColor(fg, bg);
    UNLOCK();
}

void DisplayUI::setTextSize(uint8_t size) {
    LOCK_OR_RETURN();
    _tft->setTextSize(size);
    UNLOCK();
}

void DisplayUI::drawString(const char* str, int32_t x, int32_t y, uint8_t font) {
    String s = normalize(str);
    LOCK_OR_RETURN();
    useFont(font);
    _tft->setCursor(x, y);
    _tft->print(s);
    UNLOCK();
}

void DisplayUI::drawCentreString(const char* str, int32_t x, int32_t y, uint8_t font) {
    String s = normalize(str);
    LOCK_OR_RETURN();
    useFont(font);
    _tft->setCursor(x - _tft->textWidth(s) / 2, y);
    _tft->print(s);
    UNLOCK();
}

void DisplayUI::drawRightString(const char* str, int32_t xRight, int32_t y, uint8_t font) {
    String s = normalize(str);
    LOCK_OR_RETURN();
    useFont(font);
    _tft->setCursor(xRight - _tft->textWidth(s), y);
    _tft->print(s);
    UNLOCK();
}

int32_t DisplayUI::textWidth(const char* str) {
    return textWidth(str, _font ? _font : FONT_MD);
}

int32_t DisplayUI::textWidth(const char* str, uint8_t font) {
    String s = normalize(str);
    LOCK_OR_RETURN(0);
    useFont(font);
    int32_t w = _tft->textWidth(s);
    UNLOCK();
    return w;
}

int16_t DisplayUI::fontHeight(uint8_t font) {
    const FontInfo& fi = fontInfo(font);
    return fi.ascent + fi.descent;
}

int16_t DisplayUI::textY(uint8_t font, int32_t cy) {
    const FontInfo& fi = fontInfo(font);
    return cy - fi.ascent + (fi.cap + 1) / 2;
}

String DisplayUI::fitText(const String& in, int32_t maxW, uint8_t font) {
    String s = normalize(in.c_str());
    LOCK_OR_RETURN(s);
    useFont(font);
    if (_tft->textWidth(s) > maxW) {
        while (s.length() > 0) {
            int i = s.length() - 1;
            while (i > 0 && ((uint8_t)s[i] & 0xC0) == 0x80) i--;   // UTF-8 lead byte
            s.remove(i);
            while (s.endsWith(" ")) s.remove(s.length() - 1);
            if (_tft->textWidth(s + "...") <= maxW) break;
        }
        s += "...";
    }
    UNLOCK();
    return s;
}

/* ---------------------------------------------------------------- images */

bool DisplayUI::jpgCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    if ((++s_blockCount & 31) == 0) esp_task_wdt_reset();
    if (!s_outBuf || s_srcW == 0 || s_srcH == 0) return true;
    // The decoder adds the draw offset; strip it to index the buffer.
    x -= s_dstX;
    y -= s_dstY;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= s_srcW || y >= s_srcH) return true;
    if (x + w > s_srcW) w = s_srcW - x;
    if (y + h > s_srcH) h = s_srcH - y;
    for (uint16_t row = 0; row < h; row++) {
        memcpy(s_outBuf + (size_t)(y + row) * s_srcW + x, bitmap + (size_t)row * w, (size_t)w * sizeof(uint16_t));
    }
    return true;
}

void DisplayUI::drawJpg(int32_t x, int32_t y, const char* filename) {
    FsLock fs;
    if (!fs.ok || !LittleFS.exists(filename)) return;
    uint16_t jpgW = 0, jpgH = 0;
    TJpgDec.getFsJpgSize(&jpgW, &jpgH, filename, LittleFS);
    if (jpgW == 0 || jpgH == 0) return;
    drawJpgScaled(x, y, jpgW, jpgH, filename);
}

// Decodes into a RAM buffer at the coarsest scale that still covers the
// destination, nearest-neighbour scales to w x h, then pushes once (no
// per-MCU flicker). Returns false if the file is missing or undecodable.
bool DisplayUI::drawJpgScaled(int32_t x, int32_t y, int32_t w, int32_t h, const char* filename) {
    FsLock fs;   // before the display lock; the download task never takes that one
    if (!fs.ok || w <= 0 || h <= 0 || !LittleFS.exists(filename)) return false;
    LOCK_OR_RETURN(false);
    uint16_t jpgW = 0, jpgH = 0;
    TJpgDec.getFsJpgSize(&jpgW, &jpgH, filename, LittleFS);
    if (jpgW == 0 || jpgH == 0) { UNLOCK(); return false; }

    uint8_t scale = 1;
    while (scale < 8 && jpgW / (scale * 2) >= w && jpgH / (scale * 2) >= h) scale *= 2;
    TJpgDec.setJpgScale(scale);
    s_srcW = (jpgW + scale - 1) / scale;
    s_srcH = (jpgH + scale - 1) / scale;
    s_dstX = x;
    s_dstY = y;

    bool ok = false;
    size_t bufBytes = (size_t)s_srcW * s_srcH * sizeof(uint16_t);
    s_outBuf = (uint16_t *)malloc(bufBytes);
    if (s_outBuf) {
        memset(s_outBuf, 0, bufBytes);
        JRESULT r = TJpgDec.drawFsJpg(x, y, filename, LittleFS);
        uint16_t sw = s_srcW, sh = s_srcH;
        s_srcW = s_srcH = 0;
        if (r == JDR_OK) {
            uint16_t *scaled = (uint16_t *)malloc((size_t)w * h * sizeof(uint16_t));
            if (scaled) {
                for (int dy = 0; dy < h; dy++) {
                    const uint16_t *srcRow = s_outBuf + (size_t)min((int)(dy * sh / h), sh - 1) * sw;
                    uint16_t *dstRow = scaled + (size_t)dy * w;
                    for (int dx = 0; dx < w; dx++) dstRow[dx] = srcRow[min((int)(dx * sw / w), sw - 1)];
                }
                _tft->pushImage(x, y, w, h, scaled);
                free(scaled);
            } else {
                _tft->pushImage(x, y, min((int32_t)sw, w), min((int32_t)sh, h), s_outBuf);
            }
            ok = true;
        }
        free(s_outBuf);
        s_outBuf = nullptr;
    }
    TJpgDec.setJpgScale(1);
    UNLOCK();
    return ok;
}

// Masks the four corners of an already drawn image so it reads as a
// rounded tile (no anti-aliasing: the panel cannot be read back).
void DisplayUI::roundCorners(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t bg) {
    LOCK_OR_RETURN();
    for (int32_t dy = 0; dy < r; dy++) {
        float fy = r - dy - 0.5f;
        int32_t cut = r - (int32_t)ceilf(sqrtf(max(0.0f, (float)(r * r) - fy * fy)) - 0.5f);
        if (cut <= 0) continue;
        _tft->drawFastHLine(x, y + dy, cut, bg);
        _tft->drawFastHLine(x + w - cut, y + dy, cut, bg);
        _tft->drawFastHLine(x, y + h - 1 - dy, cut, bg);
        _tft->drawFastHLine(x + w - cut, y + h - 1 - dy, cut, bg);
    }
    UNLOCK();
}

bool DisplayUI::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data) {
    LOCK_OR_RETURN(false);
    _tft->pushImage(x, y, w, h, data);
    UNLOCK();
    return true;
}

int16_t DisplayUI::width() { return SCREEN_W; }
int16_t DisplayUI::height() { return SCREEN_H; }

void DisplayUI::setBacklight(uint8_t pct) {
    pct = min(pct, (uint8_t)100);
    ledcWrite(0, 8 + pct * 247 / 100);   // never fully dark
}
