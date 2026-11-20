#pragma once
#include "LittleFS.h"
enum JRESULT { JDR_OK = 0, JDR_INP = 2 };
typedef bool (*SketchCallback)(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* data);
class TJpg_Decoder {
public:
    void setCallback(SketchCallback cb) { _cb = cb; }
    void setSwapBytes(bool s) { _swap = s; }
    void setJpgScale(uint8_t s) { _scale = s ? s : 1; }
    JRESULT getFsJpgSize(uint16_t* w, uint16_t* h, const char* file, LittleFSStub&);
    JRESULT drawFsJpg(int32_t x, int32_t y, const char* file, LittleFSStub&);
private:
    SketchCallback _cb = nullptr;
    bool _swap = false;
    uint8_t _scale = 1;
};
extern TJpg_Decoder TJpgDec;
