#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "PIRSensor.h"
#include "RelayController.h"
#include "Flashlight.h"
#include "DispensingController.h"

class WebServerManager {
public:
    WebServerManager(PIRSensor* pir, RelayController* relay, Flashlight* flashlight, DispensingController* dispensing = nullptr);
    void begin();
    void update();
    
    // WiFi management functions
    void setWiFiMulti(WiFiMulti* wifiMulti);
    bool connectToNetwork(String ssid, String password);
    
    // Data update functions
    void updateFlowData(float flowRate, float totalVolume, bool error, long pulseCount = 0);
    void updateFlow2Data(float flowRate, float totalVolume, bool error, long pulseCount = 0);
    void updatePIRStatus(bool isTriggered);
    
    // Callback for flow data updates from external source
    void setFlowDataCallback(void (*callback)(float, float, bool));
    void setFlow2DataCallback(void (*callback)(float, float, bool));
    
    // Get current flow data
    float getFlowRate() const { return flowRate; }
    float getTotalVolume() const { return totalVolume; }
    bool getFlowError() const { return flowError; }
    
    // Get current flow2 data
    float getFlow2Rate() const { return flow2Rate; }
    float getFlow2TotalVolume() const { return flow2TotalVolume; }
    bool getFlow2Error() const { return flow2Error; }

private:
    AsyncWebServer server;
    PIRSensor* pirSensor;
    RelayController* relayController;
    Flashlight* flashlightController;
    DispensingController* dispensingController;
    WiFiMulti* wifiMultiPtr;
    
    // Flow sensor data (received from FlowSensorUnit)
    float flowRate;
    float totalVolume;
    bool flowError;
    long pulseCount;
    
    // Flow sensor 2 data (received from FlowSensorUnit2)
    float flow2Rate;
    float flow2TotalVolume;
    bool flow2Error;
    long flow2PulseCount;
    
    // Callback for external flow data updates
    void (*flowDataCallback)(float, float, bool);
    void (*flow2DataCallback)(float, float, bool);
    
    void setupRoutes();
    void handleRoot(AsyncWebServerRequest *request);
    void handlePIRStatus(AsyncWebServerRequest *request);
    void handleFlowData(AsyncWebServerRequest *request);
    void handleFlowUpdate(AsyncWebServerRequest *request);
    void handleToggle(AsyncWebServerRequest *request);
    void handleFlowReset(AsyncWebServerRequest *request);
    
    // WiFi management handlers (now handled inline)
};

#endif
