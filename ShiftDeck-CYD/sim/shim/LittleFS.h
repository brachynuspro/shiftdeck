#pragma once
#include "Arduino.h"
// Files map to sim/assets/<name>.rgb (16-bit header w,h then RGB565 pixels).
struct LittleFSStub { bool exists(const char* path); bool exists(const String& p) { return exists(p.c_str()); } };
extern LittleFSStub LittleFS;
