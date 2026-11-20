/*-------------------------------------------------------------------------------------------------
** XPT2046Touch.cpp - XPT2046 resistive touch driver for CYD
**
** Implementation copied from SPOTIFY_ESP32BR/firmware/src/main.cpp
** (readAxis, readTouch). The protocol is the well-tested 24-bit
** transfer (command byte + 2 transfer bytes for the 12-bit ADC
** result). The touch shares the HSPI bus with the display; TFT_eSPI
** manages the display's CS while this code controls the touch's CS.
**
** Mapped axes follow the same convention as SPOTIFY_ESP32BR's
** handleTouch(): raw Y -> screen X, raw X -> screen Y.
** ------------------------------------------------------------------------------------------------
*/
#include "XPT2046Touch.h"
#include "config.h"
#include "logTags.h"
#include "SCLogger.h"
#include <algorithm>

// HSPI is shared with the display (which uses the same SPI bus via
// TFT_eSPI's USE_HSPI_PORT). Both controllers coexist because only
// one CS line is low at a time.
static SPIClass s_touchSpi(HSPI);

static uint16_t readAxis(uint8_t cmd) {
    s_touchSpi.transfer(cmd);
    return ((static_cast<uint16_t>(s_touchSpi.transfer(0)) << 8) | s_touchSpi.transfer(0)) >> 3;
}

// The first conversion after the ADC switches axis is noisy and single
// samples spike (taps landed ~40 px off and read as drags): drop one, keep
// the median of three.
static uint16_t readAxisMedian(uint8_t cmd) {
    readAxis(cmd);
    uint16_t a = readAxis(cmd), b = readAxis(cmd), c = readAxis(cmd);
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

static bool readTouchRaw(uint16_t &rx, uint16_t &ry) {
    if (digitalRead(XPT2046_IRQ) != LOW) return false;
    s_touchSpi.beginTransaction(SPISettings(2500000, MSBFIRST, SPI_MODE0));
    digitalWrite(XPT2046_CS, LOW);
    // Y first (0xD0), then X (0x90). This matches the protocol used
    // by SPOTIFY_ESP32BR (which works on this hardware) and many
    // other CYD projects.
    rx = readAxisMedian(0xD0);
    ry = readAxisMedian(0x90);
    digitalWrite(XPT2046_CS, HIGH);
    s_touchSpi.endTransaction();
    return rx > 100 && rx < 4000 && ry > 100 && ry < 4000;
}

void XPT2046Touch::begin(uint16_t width, uint16_t height) {
    _width = width;
    _height = height;
    s_touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    pinMode(XPT2046_CS, OUTPUT);
    digitalWrite(XPT2046_CS, HIGH);
    if (XPT2046_IRQ > 0) pinMode(XPT2046_IRQ, INPUT);
    spLogI(LOGTAG_INPUT, "XPT2046 touch ready on CS=%d IRQ=%d (HSPI shared)",
           XPT2046_CS, XPT2046_IRQ);
}

bool XPT2046Touch::touched() {
    if (XPT2046_IRQ > 0) {
        if (digitalRead(XPT2046_IRQ) != LOW) return false;
    }
    uint16_t rx, ry;
    return readTouchRaw(rx, ry);
}

TS_Point XPT2046Touch::getPoint() {
    TS_Point p{0, 0, 0};
    uint16_t rx, ry;
    if (!readTouchRaw(rx, ry)) return p;

    // SPOTIFY_ESP32BR mapping: raw Y -> screen X, raw X -> screen Y.
    // This handles the axis swap for the landscape (rotation 1) panel.
    long mx, my;
    if (_calibrated) {
        mx = map(ry, _calRawY1, _calRawY2, TOUCH_CAL_MARGIN, (long)_width - TOUCH_CAL_MARGIN);
        my = map(rx, _calRawX1, _calRawX2, TOUCH_CAL_MARGIN, (long)_height - TOUCH_CAL_MARGIN);
    } else {
        // Standard CYD range, derived from SPOTIFY_ESP32BR's working values.
        mx = map(ry, 200, 3800, 0, (long)_width);
        my = map(rx, 200, 3800, 0, (long)_height);
    }

    mx = constrain(mx, 0, (long)(_width - 1));
    my = constrain(my, 0, (long)(_height - 1));

    p.x = (uint16_t)mx;
    p.y = (uint16_t)my;
    p.z = 1000; // pressure not used by callers
    return p;
}

void XPT2046Touch::setRotation(uint8_t r) {
    _rotation = r;
}

void XPT2046Touch::getRawPoint(int16_t &rawX, int16_t &rawY, uint8_t samples) {
    long sumX = 0, sumY = 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < samples; i++) {
        uint16_t rx, ry;
        if (readTouchRaw(rx, ry)) {
            sumX += rx;
            sumY += ry;
            count++;
        }
        delay(3);
    }
    if (count > 0) {
        rawX = (int16_t)(sumX / count);
        rawY = (int16_t)(sumY / count);
    } else {
        rawX = 0;
        rawY = 0;
    }
}

void XPT2046Touch::setCalibration(int16_t rawX1, int16_t rawY1,
                                   int16_t rawX2, int16_t rawY2) {
    _calRawX1 = rawX1;
    _calRawY1 = rawY1;
    _calRawX2 = rawX2;
    _calRawY2 = rawY2;
    _calibrated = true;
    spLogI(LOGTAG_INPUT, "Touch calibration set: raw1=(%d,%d) raw2=(%d,%d)",
           rawX1, rawY1, rawX2, rawY2);
}
