#include "DispensingController.h"

DispensingController::DispensingController(RelayController* relayController)
    : relay(relayController), maxFlowRate(5.0), defaultTimeoutMs(300000), volumeThreshold(0.01) {
    
    // Initialize both sessions
    for (int i = 0; i < 2; i++) {
        resetSession(i + 1);
    }
}

bool DispensingController::startDispensing(int pumpId, float targetVolume, bool autoStop) {
    if (!isValidPumpId(pumpId) || targetVolume <= 0) {
        return false;
    }
    
    int index = getPumpIndex(pumpId);
    DispensingSession& session = sessions[index];
    
    // Don't start if already dispensing
    if (session.state == DISPENSING) {
        return false;
    }
    
    // Initialize session
    session.pumpId = pumpId;
    session.targetVolume = targetVolume;
    session.currentVolume = 0.0;
    session.startVolume = 0.0;  // Will be set when we get first flow data
    session.startTime = millis();
    session.pauseTime = 0;
    session.state = DISPENSING;
    session.autoStop = autoStop;
    session.maxFlowRate = maxFlowRate;
    session.timeoutMs = defaultTimeoutMs;
    
    // Activate the pump
    activatePump(pumpId);
    
    Serial.printf("Started dispensing on pump %d: target %.3f L\n", pumpId, targetVolume);
    return true;
}

void DispensingController::stopDispensing(int pumpId) {
    if (!isValidPumpId(pumpId)) return;
    
    int index = getPumpIndex(pumpId);
    DispensingSession& session = sessions[index];
    
    if (session.state == DISPENSING || session.state == PAUSED) {
        deactivatePump(pumpId);
        session.state = READY;
        Serial.printf("Stopped dispensing on pump %d\n", pumpId);
    }
}

void DispensingController::pauseDispensing(int pumpId) {
    if (!isValidPumpId(pumpId)) return;
    
    int index = getPumpIndex(pumpId);
    DispensingSession& session = sessions[index];
    
    if (session.state == DISPENSING) {
        deactivatePump(pumpId);
        session.state = PAUSED;
        session.pauseTime = millis();
        Serial.printf("Paused dispensing on pump %d\n", pumpId);
    }
}

void DispensingController::resumeDispensing(int pumpId) {
    if (!isValidPumpId(pumpId)) return;
    
    int index = getPumpIndex(pumpId);
    DispensingSession& session = sessions[index];
    
    if (session.state == PAUSED) {
        activatePump(pumpId);
        session.state = DISPENSING;
        // Adjust start time to account for pause duration
        if (session.pauseTime > 0) {
            session.startTime += (millis() - session.pauseTime);
        }
        session.pauseTime = 0;
        Serial.printf("Resumed dispensing on pump %d\n", pumpId);
    }
}

void DispensingController::emergencyStopAll() {
    Serial.println("EMERGENCY STOP - All dispensing stopped");
    for (int pumpId = 1; pumpId <= 2; pumpId++) {
        stopDispensing(pumpId);
        int index = getPumpIndex(pumpId);
        sessions[index].state = ERROR_STATE;
    }
}

bool DispensingController::isDispensing(int pumpId) const {
    if (!isValidPumpId(pumpId)) return false;
    return sessions[getPumpIndex(pumpId)].state == DISPENSING;
}

bool DispensingController::isPaused(int pumpId) const {
    if (!isValidPumpId(pumpId)) return false;
    return sessions[getPumpIndex(pumpId)].state == PAUSED;
}

bool DispensingController::isCompleted(int pumpId) const {
    if (!isValidPumpId(pumpId)) return false;
    return sessions[getPumpIndex(pumpId)].state == COMPLETED;
}

float DispensingController::getProgress(int pumpId) const {
    if (!isValidPumpId(pumpId)) return 0.0;
    
    const DispensingSession& session = sessions[getPumpIndex(pumpId)];
    if (session.targetVolume <= 0) return 0.0;
    
    float progress = session.currentVolume / session.targetVolume;
    return min(1.0f, max(0.0f, progress));
}

float DispensingController::getCurrentVolume(int pumpId) const {
    if (!isValidPumpId(pumpId)) return 0.0;
    return sessions[getPumpIndex(pumpId)].currentVolume;
}

float DispensingController::getTargetVolume(int pumpId) const {
    if (!isValidPumpId(pumpId)) return 0.0;
    return sessions[getPumpIndex(pumpId)].targetVolume;
}

DispensingState DispensingController::getState(int pumpId) const {
    if (!isValidPumpId(pumpId)) return ERROR_STATE;
    return sessions[getPumpIndex(pumpId)].state;
}

unsigned long DispensingController::getElapsedTime(int pumpId) const {
    if (!isValidPumpId(pumpId)) return 0;
    
    const DispensingSession& session = sessions[getPumpIndex(pumpId)];
    if (session.startTime == 0) return 0;
    
    unsigned long currentTime = millis();
    if (session.state == PAUSED && session.pauseTime > 0) {
        return session.pauseTime - session.startTime;
    }
    
    return currentTime - session.startTime;
}

float DispensingController::getEstimatedTimeRemaining(int pumpId) const {
    if (!isValidPumpId(pumpId)) return 0.0;
    
    const DispensingSession& session = sessions[getPumpIndex(pumpId)];
    if (session.state != DISPENSING || session.currentVolume <= 0) return 0.0;
    
    float remainingVolume = session.targetVolume - session.currentVolume;
    if (remainingVolume <= 0) return 0.0;
    
    unsigned long elapsedMs = getElapsedTime(pumpId);
    if (elapsedMs == 0) return 0.0;
    
    float currentRate = (session.currentVolume * 60000.0) / elapsedMs; // L/min
    if (currentRate <= 0) return 0.0;
    
    return (remainingVolume / currentRate) * 60.0; // seconds
}

void DispensingController::updateFlowData(float totalVolume) {
    // Update both sessions with flow data
    for (int pumpId = 1; pumpId <= 2; pumpId++) {
        int index = getPumpIndex(pumpId);
        DispensingSession& session = sessions[index];
        
        if (session.state == DISPENSING) {
            // Set start volume on first update
            if (session.startVolume == 0.0 && totalVolume > 0) {
                session.startVolume = totalVolume;
                Serial.printf("Pump %d: Set start volume to %.3f L\n", pumpId, totalVolume);
            }
            
            // Calculate current dispensed volume
            if (session.startVolume > 0) {
                session.currentVolume = totalVolume - session.startVolume;
                if (session.currentVolume < 0) session.currentVolume = 0;
            }
            
            // Check for completion and errors
            checkForCompletion(pumpId);
            checkForErrors(pumpId);
        }
    }
}

bool DispensingController::hasError(int pumpId) const {
    if (!isValidPumpId(pumpId)) return true;
    return sessions[getPumpIndex(pumpId)].state == ERROR_STATE;
}

String DispensingController::getErrorMessage(int pumpId) const {
    if (!isValidPumpId(pumpId)) return "Invalid pump ID";
    
    const DispensingSession& session = sessions[getPumpIndex(pumpId)];
    if (session.state != ERROR_STATE) return "";
    
    // Check different error conditions
    unsigned long elapsed = getElapsedTime(pumpId);
    if (elapsed > session.timeoutMs) {
        return "Timeout - dispensing took too long";
    }
    
    return "Unknown error";
}

void DispensingController::clearError(int pumpId) {
    if (!isValidPumpId(pumpId)) return;
    
    int index = getPumpIndex(pumpId);
    if (sessions[index].state == ERROR_STATE) {
        resetSession(pumpId);
        Serial.printf("Cleared error for pump %d\n", pumpId);
    }
}

void DispensingController::activatePump(int pumpId) {
    if (pumpId == 1) {
        relay->setRelay1(true);
    } else if (pumpId == 2) {
        relay->setRelay2(true);
    }
}

void DispensingController::deactivatePump(int pumpId) {
    if (pumpId == 1) {
        relay->setRelay1(false);
    } else if (pumpId == 2) {
        relay->setRelay2(false);
    }
}

void DispensingController::checkForErrors(int pumpId) {
    int index = getPumpIndex(pumpId);
    DispensingSession& session = sessions[index];
    
    // Check for timeout
    unsigned long elapsed = getElapsedTime(pumpId);
    if (elapsed > session.timeoutMs) {
        Serial.printf("Pump %d: Timeout error after %lu ms\n", pumpId, elapsed);
        deactivatePump(pumpId);
        session.state = ERROR_STATE;
        return;
    }
    
    // Could add more error checks here:
    // - No flow detected for too long
    // - Flow rate too high/low
    // - Hardware errors
}

void DispensingController::checkForCompletion(int pumpId) {
    int index = getPumpIndex(pumpId);
    DispensingSession& session = sessions[index];
    
    if (session.autoStop && session.currentVolume >= (session.targetVolume - volumeThreshold)) {
        Serial.printf("Pump %d: Target reached! %.3f/%.3f L\n", 
                     pumpId, session.currentVolume, session.targetVolume);
        deactivatePump(pumpId);
        session.state = COMPLETED;
    }
}

void DispensingController::resetSession(int pumpId) {
    if (!isValidPumpId(pumpId)) return;
    
    int index = getPumpIndex(pumpId);
    DispensingSession& session = sessions[index];
    
    session.pumpId = pumpId;
    session.targetVolume = 0.0;
    session.currentVolume = 0.0;
    session.startVolume = 0.0;
    session.startTime = 0;
    session.pauseTime = 0;
    session.state = READY;
    session.autoStop = true;
    session.maxFlowRate = maxFlowRate;
    session.timeoutMs = defaultTimeoutMs;
} 