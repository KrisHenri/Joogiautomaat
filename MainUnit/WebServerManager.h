#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "PIRSensor.h"
#include "RelayController.h"

class WebServerManager {
public:
    WebServerManager(PIRSensor* pir, RelayController* relay);
    void begin();
    void update();

private:
    AsyncWebServer server;
    PIRSensor* pirSensor;
    RelayController* relayController;
    
    // Flow sensor data (received from FlowSensorUnit)
    float flowRate;
    float totalVolume;
    bool flowError;
    
    void setupRoutes();
    void handleRoot(AsyncWebServerRequest *request);
    void handlePIRStatus(AsyncWebServerRequest *request);
    void handleFlowData(AsyncWebServerRequest *request);
    void handleFlowUpdate(AsyncWebServerRequest *request);
    void handleToggle(AsyncWebServerRequest *request);
    void handleFlowReset(AsyncWebServerRequest *request);
};

#endif
