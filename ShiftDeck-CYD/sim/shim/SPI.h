#pragma once
#include "Arduino.h"
#define MSBFIRST 1
#define SPI_MODE0 0
#define SPI_MODE3 3
struct SPISettings { SPISettings(uint32_t = 0, uint8_t = 0, uint8_t = 0) {} };
// Feeds an ILI9341 emulator (sim_tft.cpp) instead of a bus.
class SPIClass {
public:
    void begin(int = -1, int = -1, int = -1, int = -1) {}
    void end() {}
    void beginTransaction(SPISettings) {}
    void endTransaction() {}
    void setFrequency(uint32_t) {}
    uint8_t transfer(uint8_t b);
    uint16_t transfer16(uint16_t w) { transfer(w >> 8); transfer(w & 0xFF); return 0; }
    void transfer(void* buf, size_t n) { for (size_t i = 0; i < n; i++) transfer(((uint8_t*)buf)[i]); }
    void writeBytes(const uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) transfer(b[i]); }
};
extern SPIClass SPI;
