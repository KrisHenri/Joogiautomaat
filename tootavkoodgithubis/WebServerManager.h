#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

extern const char htmlPage[];

class FlowSensor;
class Flashlight;
class RelayController;

class WebServerManager {
public:
    WebServerManager();

    void setStatePointers(bool* flashlightState, bool* relay1State, bool* relay2State, bool* pirState);
    void setControllers(Flashlight* flashlight, RelayController* relay);
    void setFlowSensor(FlowSensor* fs);  // ✅ LISATUD
    void begin();

private:
    AsyncWebServer server = AsyncWebServer(80);

    bool* flashlightState = nullptr;
    bool* relay1State = nullptr;
    bool* relay2State = nullptr;
    bool* pirState = nullptr;

    Flashlight* flashlight = nullptr;
    RelayController* relay = nullptr;
    FlowSensor* flowSensor = nullptr;    // ✅ LISATUD

    String processor(const String& var);
};
