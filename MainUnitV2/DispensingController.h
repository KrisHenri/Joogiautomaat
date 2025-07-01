#ifndef DISPENSING_CONTROLLER_H
#define DISPENSING_CONTROLLER_H

#include <Arduino.h>
#include "RelayController.h"

enum DispensingState {
    READY,
    DISPENSING,
    PAUSED,
    COMPLETED,
    ERROR_STATE
};

struct DispensingSession {
    int pumpId;                    // 1 or 2
    float targetVolume;            // Target volume in liters
    float currentVolume;           // Current dispensed volume in liters
    float startVolume;             // Volume when dispensing started
    unsigned long startTime;       // When dispensing started
    unsigned long pauseTime;       // When paused (if paused)
    DispensingState state;
    bool autoStop;                 // Whether to auto-stop at target
    float maxFlowRate;            // Maximum expected flow rate (for error detection)
    unsigned long timeoutMs;       // Maximum time allowed for dispensing
};

class DispensingController {
public:
    DispensingController(RelayController* relayController);
    
    // Main control methods
    bool startDispensing(int pumpId, float targetVolume, bool autoStop = true);
    void stopDispensing(int pumpId);
    void pauseDispensing(int pumpId);
    void resumeDispensing(int pumpId);
    void emergencyStopAll();
    
    // Status methods
    bool isDispensing(int pumpId) const;
    bool isPaused(int pumpId) const;
    bool isCompleted(int pumpId) const;
    float getProgress(int pumpId) const;  // Returns 0.0 to 1.0
    float getCurrentVolume(int pumpId) const;
    float getTargetVolume(int pumpId) const;
    DispensingState getState(int pumpId) const;
    unsigned long getElapsedTime(int pumpId) const;
    float getEstimatedTimeRemaining(int pumpId) const;
    
    // Update method - call this regularly with flow sensor data
    void updateFlowData(float totalVolume);
    
    // Configuration
    void setMaxFlowRate(float maxRate) { maxFlowRate = maxRate; }
    void setTimeout(unsigned long timeoutMs) { defaultTimeoutMs = timeoutMs; }
    void setVolumeThreshold(float threshold) { volumeThreshold = threshold; }
    
    // Safety and error handling
    bool hasError(int pumpId) const;
    String getErrorMessage(int pumpId) const;
    void clearError(int pumpId);
    
private:
    RelayController* relay;
    DispensingSession sessions[2];  // For pump 1 and pump 2
    
    // Configuration
    float maxFlowRate;              // L/min - for error detection
    unsigned long defaultTimeoutMs; // Default timeout in milliseconds
    float volumeThreshold;          // Minimum volume difference to consider "reached target"
    
    // Helper methods
    int getPumpIndex(int pumpId) const { return pumpId - 1; }  // Convert 1,2 to 0,1
    bool isValidPumpId(int pumpId) const { return pumpId == 1 || pumpId == 2; }
    void activatePump(int pumpId);
    void deactivatePump(int pumpId);
    void checkForErrors(int pumpId);
    void checkForCompletion(int pumpId);
    void resetSession(int pumpId);
};

#endif 