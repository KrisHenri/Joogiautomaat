#include <M5Core2.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "Flashlight.h"
#include "RelayController.h"
#include "PIRSensor.h"
#include "FlowSensor.h"
#include "WebServerManager.h"
#include "TouchController.h"

M5UnitPbHub pbhub;
WiFiMulti wifiMulti;

Flashlight flashlight(&pbhub);
RelayController relay(&pbhub);
PIRSensor pir(&pbhub);
FlowSensor flow;
WebServerManager webServer;
TouchController touch(&flashlight, &relay);

static bool pirStateForWeb = false;

void connectWiFi() {
    wifiMulti.addAP("Illuminaty", "S330nm1nuk0du!");
    Serial.println("Connecting to WiFi...");
    while (wifiMulti.run() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    Serial.println(WiFi.localIP());
}

void setup() {
    M5.begin();
    Serial.begin(115200);

    if (!pbhub.begin(&Wire, UNIT_PBHUB_I2C_ADDR, 32, 33, 400000U)) {
        Serial.println("Pbhub initialization failed");
        while (1) delay(1);
    }

    flashlight.init();
    relay.setRelay1(false);
    relay.setRelay2(false);
    pir.init();
    flow.begin();
    touch.init();

    connectWiFi();

    webServer.setStatePointers(
        &flashlight.state,
        &relay.relay1,
        &relay.relay2,
        &pirStateForWeb
    );

    webServer.setControllers(&flashlight, &relay);
    webServer.setFlowSensor(&flow);
    webServer.begin();
}

void loop() {
    M5.update();

    pir.check(flashlight.state, [](bool on) {
        flashlight.set(on);
    });

    pirStateForWeb = pir.isTriggered();
    flow.update();
    touch.update();

    static bool lastPirState = false;
    bool currentPirState = pir.isTriggered();
    if (currentPirState != lastPirState) {
        lastPirState = currentPirState;
        touch.drawUI(currentPirState);
    }

    delay(100);
}
