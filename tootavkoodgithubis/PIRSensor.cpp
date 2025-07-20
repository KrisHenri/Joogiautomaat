#include "PIRSensor.h"
#include <Arduino.h>

PIRSensor::PIRSensor(M5UnitPbHub* hub) : pbhub(hub) {}

void PIRSensor::init() {
    // Ei ole vaja midagi määrata – PBHUB käsitleb I/O
}

void PIRSensor::check(bool& flashlightState, void (*flashlightControl)(bool)) {
    unsigned long currentMillis = millis();
    int pirValue = pbhub->digitalRead(PIR_CHANNEL, PIR_INDEX);

    if (pirValue == HIGH) {
        pirTriggered = true;
        pirTriggeredTime = currentMillis;

        if (!flashlightState) {
            flashlightState = true;
            flashlightControl(true);
        }
    } else if (pirTriggered && (currentMillis - pirTriggeredTime > pirTimeout)) {
        pirTriggered = false;

        if (flashlightState) {
            flashlightState = false;
            flashlightControl(false);
        }
    }
}

bool PIRSensor::isTriggered() const {
    return pirTriggered;
}

unsigned long PIRSensor::getTriggeredTime() const {
    return pirTriggeredTime;
}
