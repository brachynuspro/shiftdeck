/*-------------------------------------------------------------------------------------------------
** Monitor.cpp - Performance monitoring implementation
** ------------------------------------------------------------------------------------------------
*/
#include "Monitor.h"

Monitor::Metric Monitor::_metrics[10] = {};
unsigned long Monitor::_bootTime = millis();

void Monitor::registerDescription(int id, const char* desc) {
    if (id >= 0 && id < 10) _metrics[id].description = desc;
}

void Monitor::start(int id, const char* tag, const char* msg) {
    if (id >= 0 && id < 10) _metrics[id].startTime = micros();
    spLogD(tag, "%s", msg);
}

void Monitor::stop(int id) {
    if (id >= 0 && id < 10) {
        unsigned long elapsed = micros() - _metrics[id].startTime;
        _metrics[id].totalTime += elapsed;
        _metrics[id].count++;
        if ((int)elapsed > _metrics[id].maxTime) _metrics[id].maxTime = (int)elapsed;
    }
}

uint32_t Monitor::getFreeHeap() {
    return ESP.getFreeHeap();
}

bool Monitor::watchHeap(uint32_t threshold) {
    return ESP.getFreeHeap() > threshold;
}

void Monitor::dumpStats(const char* tag) {
    unsigned long uptime = (millis() - _bootTime) / 1000;
    spLogI(tag, "--- Metrics (uptime %lus, heap %u) ---", uptime, ESP.getFreeHeap());
    for (int i = 0; i < 10; i++) {
        if (_metrics[i].description && _metrics[i].count > 0) {
            int avg = (int)(_metrics[i].totalTime / _metrics[i].count);
            spLogI(tag, "  %s: cnt=%d avg=%dus max=%dus",
                   _metrics[i].description, _metrics[i].count, avg, _metrics[i].maxTime);
        }
    }
}

void Monitor::logUptime(const char* tag) {
    unsigned long uptime = (millis() - _bootTime) / 1000;
    spLogI(tag, "Uptime: %lus", uptime);
}

void Monitor::watchQueue(QueueHandle_t queue, int threshold) {
    UBaseType_t count = uxQueueMessagesWaiting(queue);
    if (count > (UBaseType_t)threshold && count > 0) {
        spLogW("SCMtrc", "SCUI Queue depth: %u", (unsigned int)count);
    }
}
