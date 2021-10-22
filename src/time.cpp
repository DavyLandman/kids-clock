#include "time.hpp"
#include <ezTime.h>

bool Time::process() {
    events();
    if (timeStatus() == timeSet && secondChanged()) {
        if (now == nullptr) {
            now = new Timezone();
            now->setLocation(this->zone);
            Serial.println(now->dateTime());
        }
        return true;
    }
    return false;
}

bool Time::didMinuteChanged() {
    return minuteChanged();
}

uint8_t Time::hour() {
    if (now == nullptr) {
        return 0;
    }
    return now->hour();
}
uint8_t Time::minute() {
    if (now == nullptr) {
        return 0;
    }
    return now->minute();
}