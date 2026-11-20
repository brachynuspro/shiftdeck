/*-------------------------------------------------------------------------------------------------
** SCLogger.h - Tag-based logging wrapper
** Ported from original project, simplified for ShiftDeck
** ------------------------------------------------------------------------------------------------
*/
#pragma once

#include <Arduino.h>
#include <map>

class SCLogger {
public:
    static SCLogger& getInstance();

    void setLogLevel(const char* tag, uint8_t level);
    uint8_t getLogLevel(const char* tag);

    void log(const char* tag, uint8_t level, const char* format, ...);

private:
    SCLogger() {}
    std::map<String, uint8_t> _levels;
    uint8_t _defaultLevel = 2;
};

#define spLogV(tag, format, ...) SCLogger::getInstance().log(tag, 5, format, ##__VA_ARGS__)
#define spLogD(tag, format, ...) SCLogger::getInstance().log(tag, 4, format, ##__VA_ARGS__)
#define spLogI(tag, format, ...) SCLogger::getInstance().log(tag, 3, format, ##__VA_ARGS__)
#define spLogW(tag, format, ...) SCLogger::getInstance().log(tag, 2, format, ##__VA_ARGS__)
#define spLogE(tag, format, ...) SCLogger::getInstance().log(tag, 1, format, ##__VA_ARGS__)
