#ifndef WEBSERVERMANAGER_H
#define WEBSERVERMANAGER_H

#include <ESPAsyncWebServer.h>

class WebServerManager {
public:
    WebServerManager();
    void begin();
    void setStatePointers(bool* flashlight, bool* relay1, bool* relay2, bool* pir);

private:
    AsyncWebServer server;
    bool* flashlightState;
    bool* relay1State;
    bool* relay2State;
    bool* pirTriggered;
};

#endif