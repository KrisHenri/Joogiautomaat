#include <M5Core2.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include "M5UnitPbHub.h"
#include "screen.h"
#include "flashlight.h"
#include "relay.h"
#include "pir.h"
#include "webserver.h"
#include "vooluandur.h"

// Define ports for devices
#define FLASHLIGHT_CHANNEL 0
#define FLASHLIGHT_INDEX 1
#define RELAY_CHANNEL 1
#define RELAY_INDEX1 0
#define RELAY_INDEX2 1
#define PIR_CHANNEL 2
#define PIR_INDEX 0

M5UnitPbHub pbhub;

bool flashlightState = false;
bool relay1State = false;
bool relay2State = false;
bool pirTriggered = false;
unsigned long pirTriggeredTime = 0;
unsigned long lastPirCheckTime = 0;
unsigned long pirTimeout = 10000;  // 10 seconds PIR timeout
volatile unsigned long pulseCount = 0;

// Debouncing variables for buttons
unsigned long lastButtonPressTime = 0;
const unsigned long buttonDebounceDelay = 50;  // 50 ms debounce delay for buttons

WiFiMulti wifiMulti;

void updateDisplay();
void syncWebState();
void fetchWebState();

void updateDisplay() {
    M5.Lcd.clear();
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    drawButtons(flashlightState, relay1State, relay2State, pirTimeout, pirTriggered);
}

void syncWebState() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        http.begin(String("http://") + WiFi.localIP().toString() + "/syncFlashlightState");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String postData = String("state=") + (flashlightState ? "ON" : "OFF");
        http.POST(postData);
        http.end();

        http.begin(String("http://") + WiFi.localIP().toString() + "/syncRelay1State");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        postData = String("state=") + (relay1State ? "ON" : "OFF");
        http.POST(postData);
        http.end();

        http.begin(String("http://") + WiFi.localIP().toString() + "/syncRelay2State");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        postData = String("state=") + (relay2State ? "ON" : "OFF");
        http.POST(postData);
        http.end();

        http.begin(String("http://") + WiFi.localIP().toString() + "/syncPirState");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        postData = String("state=") + (pirTriggered ? "ON" : "OFF");
        http.POST(postData);
        http.end();
    }
}

void fetchWebState() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(String("http://") + WiFi.localIP().toString() + "/flashlightState");
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            flashlightState = (payload == "ON");
        }
        http.end();

        http.begin(String("http://") + WiFi.localIP().toString() + "/relay1State");
        httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            relay1State = (payload == "ON");
        }
        http.end();

        http.begin(String("http://") + WiFi.localIP().toString() + "/relay2State");
        httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            relay2State = (payload == "ON");
        }
        http.end();

        http.begin(String("http://") + WiFi.localIP().toString() + "/pirState");
        httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            pirTriggered = (payload == "ON");
        }
        http.end();

        updateDisplay();
    }
}

void setup() {
    M5.begin();
    Serial.begin(115200);

    wifiMulti.addAP("Illuminaty", "S330nm1nuk0du!");
    M5.Lcd.print("\nConnecting Wifi...\n");

    if (!pbhub.begin(&Wire, UNIT_PBHUB_I2C_ADDR, 32, 33, 400000U)) {
        Serial.println("Couldn't find Pbhub");
        while (1) delay(1);
    }

    initScreen();
    unit_flash_init();
    updateDisplay();

    setupWiFi();
    setupWebServer();

    initFlowSensor();

    Serial.println("Setup complete.");
    fetchWebState();  // Fetch initial states from the web
}

void loop() {
    M5.update();
    checkPIR();
    readFlowSensor();

    unsigned long currentMillis = millis();
    if (M5.Touch.ispressed() && (currentMillis - lastButtonPressTime > buttonDebounceDelay)) {
        lastButtonPressTime = currentMillis;
        int x = M5.Touch.getPressPoint().x;
        int y = M5.Touch.getPressPoint().y;

        if (x > 30 && x < 80 && y > 150 && y < 250) {
            flashlightState = !flashlightState;
            unit_flash_set_brightness(flashlightState);
            pirTriggered = false;
            updateDisplay();
            syncWebState();  // Sync with web server
        }

        if (x > 120 && x < 200 && y > 130 && y < 250) {
            relay1State = !relay1State;
            toggleRelay(RELAY_CHANNEL, RELAY_INDEX1, relay1State);
            updateDisplay();
            syncWebState();  // Sync with web server
        }

        if (x > 230 && x < 270 && y > 150 && y < 250) {
            relay2State = !relay2State;
            toggleRelay(RELAY_CHANNEL, RELAY_INDEX2, relay2State);
            updateDisplay();
            syncWebState();  // Sync with web server
        }
    }

    // Add a delay to prevent rapid looping
    delay(100);  // Adjust the delay as needed
}
