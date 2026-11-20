/*-------------------------------------------------------------------------------------------------
** XPT2046Touch.h - XPT2046 resistive touch driver for CYD
**
** Custom driver based on the SPI routine used in SPOTIFY_ESP32BR
** (which works reliably on the same hardware). Uses HSPI shared
** with the display, 24-bit transfer protocol (cmd byte + 2 transfer
** bytes), and IRQ-driven detection. The previous attempts at using
** PaulStoffregen's XPT2046_Touchscreen library produced inconsistent
** results on this user's panel.
**
** Public API (begin / touched / getPoint / setRotation / setCalibration)
** is preserved so existing views and main.cpp do not need to change.
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>
#include <SPI.h>

#define XPT2046_CS   33
#define XPT2046_IRQ  36
#define XPT2046_CLK  25
#define XPT2046_MOSI 32
#define XPT2046_MISO 39

// The project code uses TS_Point as a simple (x, y, z) struct. We
// define it locally to avoid pulling in the XPT2046_Touchscreen
// library's TS_Point (which we're no longer using).
struct TS_Point {
    uint16_t x;
    uint16_t y;
    uint16_t z;
};

class XPT2046Touch {
public:
    void begin(uint16_t width, uint16_t height);
    bool touched();
    TS_Point getPoint();

    void setRotation(uint8_t r);

    // Reads averaged raw ADC values for the calibration wizard.
    // Implements a simple averaging loop over multiple SPI reads.
    void getRawPoint(int16_t &rawX, int16_t &rawY, uint8_t samples = 10);

    // Stores a 2-point calibration. Currently a no-op at runtime
    // because the raw -> screen mapping is hard-coded to the typical
    // CYD range. Kept for API compatibility.
    void setCalibration(int16_t rawX1, int16_t rawY1, int16_t rawX2, int16_t rawY2);
    bool isCalibrated() const { return _calibrated; }

private:
    uint16_t _width = 320;
    uint16_t _height = 240;
    uint8_t _rotation = 1;
    bool _calibrated = false;
    int16_t _calRawX1 = 0;
    int16_t _calRawY1 = 0;
    int16_t _calRawX2 = 0;
    int16_t _calRawY2 = 0;
};
