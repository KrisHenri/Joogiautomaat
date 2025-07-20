#include "FlowSensor.h"

volatile unsigned long FlowSensor::pulseCount = 0;

FlowSensor::FlowSensor(int pin) : sensorPin(pin), calibrationFactor(4.5) {}

void IRAM_ATTR FlowSensor::pulseCounter() {
    pulseCount++;
}

void FlowSensor::begin(float calibFactor) {
    calibrationFactor = calibFactor;
    pinMode(sensorPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(sensorPin), pulseCounter, FALLING);
    Serial.printf("Flow sensor started (Calib: %.1f pulses/L)\n", calibrationFactor);
}

void FlowSensor::update() {
    static unsigned long lastPulses = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastUpdateTime >= 1000) { // Update every second
        noInterrupts();
        unsigned long newPulses = pulseCount - lastPulseCount;
        lastPulseCount = pulseCount;
        interrupts();
        
        // Calculate instantaneous flow rate (L/min)
        float instantFlow = (newPulses / calibrationFactor) * 60.0f;
        
        // Apply smoothing
        smoothedFlowRate = (smoothingFactor * smoothedFlowRate) + 
                         ((1.0f - smoothingFactor) * instantFlow);
        
        // Update total volume (liters)
        totalVolume += newPulses / calibrationFactor;
        
        lastUpdateTime = currentTime;
        
        // Debug output
        Serial.printf("Flow: %.2f L/min (Raw: %.2f) | Total: %.3f L\n",
                    smoothedFlowRate, instantFlow, totalVolume);
    }
}

float FlowSensor::getFlowRate() const {
    return smoothedFlowRate;
}

float FlowSensor::getTotalVolume() const {
    return totalVolume;
}

void FlowSensor::resetTotalVolume() {
    totalVolume = 0;
}
