#include "WebServerManager.h"

WebServerManager::WebServerManager() : server(80) {}

void WebServerManager::setStatePointers(bool* flashlight, bool* relay1, bool* relay2, bool* pir) {
    flashlightState = flashlight;
    relay1State = relay1;
    relay2State = relay2;
    pirTriggered = pir;
}

void WebServerManager::begin() {
    server.on("/flashlightState", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(200, "text/plain", *flashlightState ? "ON" : "OFF");
    });

    server.on("/relay1State", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(200, "text/plain", *relay1State ? "ON" : "OFF");
    });

    server.on("/relay2State", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(200, "text/plain", *relay2State ? "ON" : "OFF");
    });

    server.on("/pirState", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(200, "text/plain", *pirTriggered ? "ON" : "OFF");
    });

    server.on("/syncFlashlightState", HTTP_POST, [this](AsyncWebServerRequest *request){
        String state = request->arg("state");
        *flashlightState = (state == "ON");
        request->send(200);
    });

    server.on("/syncRelay1State", HTTP_POST, [this](AsyncWebServerRequest *request){
        String state = request->arg("state");
        *relay1State = (state == "ON");
        request->send(200);
    });

    server.on("/syncRelay2State", HTTP_POST, [this](AsyncWebServerRequest *request){
        String state = request->arg("state");
        *relay2State = (state == "ON");
        request->send(200);
    });

    server.on("/syncPirState", HTTP_POST, [this](AsyncWebServerRequest *request){
        String state = request->arg("state");
        *pirTriggered = (state == "ON");
        request->send(200);
    });

    server.begin();
}