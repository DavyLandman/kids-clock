#ifndef __TIME_H
#define __TIME_H
#include <Arduino.h>
#include <ezTime.h>

class Time {
private:
    Timezone *now;
    const String zone;
public:
    Time(const String zone): zone(zone) {
        now = nullptr;
        setDebug(INFO); // if this is off, it crashes?
    };
    bool process();
    bool didMinuteChanged();
    uint8_t hour();
    uint8_t minute();
};
#endif