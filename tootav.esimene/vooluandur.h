#ifndef VOOLUANDUR_H
#define VOOLUANDUR_H

#include <Arduino.h>

#define FLOW_SENSOR_PIN 36

extern volatile unsigned long pulseCount;

void IRAM_ATTR pulseCounter() {
    pulseCount++;
}

void initFlowSensor() {
    pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
}

void readFlowSensor() {
    static unsigned long lastTime = 0;
    unsigned long currentTime = millis();
    static unsigned long lastPulseCount = 0;

    if (currentTime - lastTime >= 1000) {
        lastTime = currentTime;
        unsigned long pulses = pulseCount - lastPulseCount;
        lastPulseCount = pulseCount;
        Serial.printf("Flow rate: %lu pulses/second\n", pulses);

        M5.Lcd.setCursor(0, 100);
        M5.Lcd.printf("Flow rate: %lu pulses/sec\n", pulses);
    }
}

#endif // VOOLUANDUR_H
