#include "PIRSensor.h"
#include <Arduino.h>
#include <M5Core2.h>

PIRSensor::PIRSensor(M5UnitPbHub* hub) : pbhub(hub) {}

void PIRSensor::init() {
    // No initialization needed - PBHUB handles I/O
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

void PIRSensor::drawTab() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    
    // Draw header
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("PIR Sensor Info");
    
    // Draw labels
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.println("Status:");
    
    M5.Lcd.setCursor(10, 90);
    M5.Lcd.println("Last Trigger:");
    
    // Initial update of values
    updateTab();
}

void PIRSensor::updateTab() {
    // Update status
    M5.Lcd.setCursor(150, 50);
    if (pirTriggered) {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println("ACTIVE");
    } else {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("INACTIVE");
    }
    
    // Update last trigger time
    M5.Lcd.setCursor(150, 90);
    M5.Lcd.setTextColor(WHITE);
    if (pirTriggered) {
        M5.Lcd.printf("%lu ms", millis() - pirTriggeredTime);
    } else {
        M5.Lcd.println("N/A");
    }
}
