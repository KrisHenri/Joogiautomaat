#ifndef PIR_H
#define PIR_H

#include "M5UnitPbHub.h"
#include "flashlight.h"

#define PIR_CHANNEL 2
#define PIR_INDEX 0

extern M5UnitPbHub pbhub;
extern bool pirTriggered;
extern unsigned long pirTriggeredTime;
extern unsigned long lastPirCheckTime;
extern unsigned long pirTimeout;
extern bool flashlightState;
extern void updateDisplay();

void checkPIR() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastPirCheckTime > 100) {
        lastPirCheckTime = currentMillis;
        int pirValue = pbhub.digitalRead(PIR_CHANNEL, PIR_INDEX);
        Serial.print("PIR Value: ");
        Serial.println(pirValue);

        if (pirValue == HIGH) {
            pirTriggered = true;
            pirTriggeredTime = currentMillis;
            flashlightState = true;
            unit_flash_set_brightness(true);
            updateDisplay();
        }
    }

    if (pirTriggered && currentMillis - pirTriggeredTime > pirTimeout) {
        pirTriggered = false;
        flashlightState = false;
        unit_flash_set_brightness(false);
        updateDisplay();
    }
}

#endif
