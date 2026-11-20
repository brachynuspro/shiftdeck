#pragma once

// User_Setup.h - TFT_eSPI configuration for the ESP32-2432S028 (CYD)
// Copied from SPOTIFY_ESP32BR/firmware/src/display_setups/ESP32_2432S028_ILI9341_NO_RST.h
// which is the configuration that works reliably on this hardware.

#define USER_SETUP_ID 2432028
#define ILI9341_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1
#define TFT_BL   21

#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY       20000000
#define SPI_READ_FREQUENCY  10000000
#define SPI_TOUCH_FREQUENCY 2500000
