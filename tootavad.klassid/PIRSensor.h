#ifndef PIRSENSOR_H
#define PIRSENSOR_H

#include "M5UnitPbHub.h"

#define PIR_CHANNEL 2
#define PIR_INDEX 0
#define PIR_TIMEOUT 10000  // ms

class PIRSensor {
public:
    PIRSensor(M5UnitPbHub* hub);
    void init();

    // Parandatud funktsioon – saab juhtida flashlight'i
    void check(bool& flashlightState, void (*flashlightControl)(bool));

    bool isTriggered() const;
    unsigned long getTriggeredTime() const;

private:
    M5UnitPbHub* pbhub;
    bool pirTriggered = false;
    unsigned long pirTriggeredTime = 0;
    unsigned long lastCheckTime = 0;
    const unsigned long pirTimeout = PIR_TIMEOUT;
};

#endif
