#ifndef FLOWSENSOR_H
#define FLOWSENSOR_H

#include <Arduino.h>
#include <M5Core2.h>

class FlowSensor {
public:
    FlowSensor(int pin = 36); // Default pin 36
    
    void begin(float calibFactor = 4.5);  // Initialize with calibration
    void update();                       // Update calculations
    float getFlowRate() const;           // L/min
    float getTotalVolume() const;        // Liters
    void resetTotalVolume();
    void setCalibration(float factor);
    void setSmoothing(float factor);     // 0.0-1.0 smoothing factor

private:
    const int sensorPin;
    volatile static unsigned long pulseCount;
    unsigned long lastPulseCount = 0;
    unsigned long lastUpdateTime = 0;
    float calibrationFactor;
    float smoothedFlowRate = 0;
    float totalVolume = 0;
    float smoothingFactor = 0.5; // Default moderate smoothing
    
    static void IRAM_ATTR pulseCounter();
};

#endif