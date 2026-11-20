/*-------------------------------------------------------------------------------------------------
** SCLogger.cpp - Tag-based logging wrapper implementation
** ------------------------------------------------------------------------------------------------
*/
#include "SCLogger.h"
#include <stdarg.h>

SCLogger& SCLogger::getInstance() {
    static SCLogger instance;
    return instance;
}

void SCLogger::setLogLevel(const char* tag, uint8_t level) {
    _levels[String(tag)] = level;
}

uint8_t SCLogger::getLogLevel(const char* tag) {
    auto it = _levels.find(String(tag));
    if (it != _levels.end()) return it->second;
    return _defaultLevel;
}

void SCLogger::log(const char* tag, uint8_t level, const char* format, ...) {
    uint8_t currentLevel = _defaultLevel;
    if (strcmp(tag, "*") != 0) {
        auto it = _levels.find(String(tag));
        if (it != _levels.end()) currentLevel = it->second;
    }
    if (level > (uint8_t)currentLevel) return;

    const char* levelStr[] = {"", "E", "W", "I", "D", "V"};
    Serial.printf("[%s] [%s] ", tag, levelStr[level]);

    va_list args;
    va_start(args, format);
    char buf[256];
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial.println(buf);
}
