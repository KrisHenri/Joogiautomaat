#include <M5Core2.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "Flashlight.h"
#include "RelayController.h"
#include "PIRSensor.h"
#include "WebServerManager.h"
#include "TouchController.h"
#include "DispensingController.h"
#include "M5_PbHub.h"

// First M5Stack Core 2 (Main unit with PbHub)
M5UnitPbHub pbhub;
WiFiMulti wifiMulti;
// Removed duplicate server - using WebServerManager's server instead
Preferences preferences;

Flashlight flashlight(&pbhub);
RelayController relay(&pbhub);
PIRSensor pir(&pbhub);
DispensingController dispensing(&relay);
WebServerManager webServer(&pir, &relay, &flashlight, &dispensing);
TouchController touch(&flashlight, &relay);

// Flow sensor data from second unit
struct FlowSensorData {
    float flowRate = 0.0f;
    float totalVolume = 0.0f;
    bool error = false;
} flowData;

static bool pirStateForWeb = false;
static int currentTab = 0;  // 0: Main, 1: PIR, 2: Flow Sensor, 3: WiFi
static bool lastPirState = false;

void loadSavedWiFiCredentials() {
    preferences.begin("wifi", true);
    String savedSSID = preferences.getString("ssid", "");
    String savedPassword = preferences.getString("password", "");
    preferences.end();
    
    // Add default network
    wifiMulti.addAP("Illuminaty", "S330nm1nuk0du!");
    // Add second WiFi network - replace with your actual credentials
    wifiMulti.addAP("Distillery", "Sommerling");
    // Add third WiFi network if needed
    wifiMulti.addAP("Distillery-Office", "Sommerling");
    
    // Add saved network if exists
    if (savedSSID.length() > 0) {
        Serial.println("Found saved WiFi credentials: " + savedSSID);
        wifiMulti.addAP(savedSSID.c_str(), savedPassword.c_str());
    }
}

void connectWiFi() {
    Serial.println("Loading WiFi credentials...");
    loadSavedWiFiCredentials();
    
    Serial.println("Connecting to WiFi...");
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.println("Connecting to WiFi...");
    
    int attempts = 0;
    while (wifiMulti.run() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        M5.Lcd.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.println("SSID: " + WiFi.SSID());
        Serial.println("IP: " + WiFi.localIP().toString());
        
        // Initialize mDNS for auto-discovery
        if (!MDNS.begin("joogimasin")) {
            Serial.println("Error setting up mDNS responder!");
        } else {
            Serial.println("mDNS responder started as 'joogimasin.local'");
            MDNS.addService("http", "tcp", 80);
        }
        
        M5.Lcd.setCursor(10, 90);
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println("Connected!");
        M5.Lcd.setCursor(10, 130);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.println("SSID: " + WiFi.SSID());
        M5.Lcd.setCursor(10, 170);
        M5.Lcd.println("IP: " + WiFi.localIP().toString());
        M5.Lcd.setCursor(10, 190);
        M5.Lcd.setTextSize(1);
        M5.Lcd.println("Discoverable as: joogimasin.local");
        delay(3000);
    } else {
        Serial.println("\nWiFi connection failed!");
        M5.Lcd.setCursor(10, 90);
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("Connection Failed!");
        M5.Lcd.setCursor(10, 130);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.println("Check credentials or");
        M5.Lcd.setCursor(10, 170);
        M5.Lcd.println("use web interface");
        delay(3000);
    }
}

void drawWiFiTab() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    
    // Draw header
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("WiFi Information");
    
    // Draw current connection status
    M5.Lcd.setCursor(10, 50);
    if (WiFi.status() == WL_CONNECTED) {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println("Status: Connected");
        M5.Lcd.setTextColor(WHITE);
        
        M5.Lcd.setCursor(10, 90);
        M5.Lcd.println("SSID: " + WiFi.SSID());
        
        M5.Lcd.setCursor(10, 130);
        M5.Lcd.println("IP: " + WiFi.localIP().toString());
        
        M5.Lcd.setCursor(10, 170);
        M5.Lcd.printf("Signal: %d dBm", WiFi.RSSI());
    } else {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("Status: Disconnected");
        M5.Lcd.setTextColor(WHITE);
        
        M5.Lcd.setCursor(10, 90);
        M5.Lcd.println("Use web interface");
        M5.Lcd.setCursor(10, 130);
        M5.Lcd.println("to configure WiFi");
    }
    
    // Instructions
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 210);
    M5.Lcd.println("Use web interface for WiFi config");
}

void updateWiFiTab() {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {  // Update every 5 seconds
        drawWiFiTab();
        lastUpdate = millis();
    }
}

void setup() {
    M5.begin();
    Serial.begin(115200);
    Serial.println("M5Core2 initializing...");

    // Initialize preferences
    preferences.begin("system", false);
    preferences.end();

    // Initialize I2C for PbHub on Port A
    Wire.begin(32, 33);  // SDA: GPIO32, SCL: GPIO33 for Port A
    Serial.println("I2C initialized on Port A (pins 32, 33)");
    
    // Try to initialize PbHub with retry
    int retryCount = 0;
    bool pbhubInitialized = false;
    
    while (!pbhubInitialized && retryCount < 3) {
        Serial.printf("Attempting PbHub initialization (attempt %d)...\n", retryCount + 1);
        pbhubInitialized = pbhub.begin(&Wire, UNIT_PBHUB_I2C_ADDR, 32, 33, 400000U);
        
        if (!pbhubInitialized) {
            Serial.println("PbHub initialization failed, retrying...");
            delay(1000);
            retryCount++;
        }
    }
    
    if (!pbhubInitialized) {
        Serial.println("PbHub initialization failed after multiple attempts");
        Serial.println("Please check:");
        Serial.println("1. PbHub is properly connected to Port A");
        Serial.println("2. All connections are secure");
        Serial.println("3. No bent pins");
        while (1) {
            delay(1000);
            Serial.println("Please check PbHub connections and restart");
        }
    }
    
    Serial.println("PbHub initialized successfully");

    // Test components
    Serial.println("Testing components...");
    
    // Test flashlight
    Serial.println("Testing flashlight...");
    flashlight.init();
    flashlight.set(true);
    delay(1000);
    flashlight.set(false);
    Serial.println("Flashlight test complete");
    
    // Test relays
    Serial.println("Testing relays...");
    relay.setRelay1(true);
    delay(1000);
    relay.setRelay1(false);
    delay(1000);
    relay.setRelay2(true);
    delay(1000);
    relay.setRelay2(false);
    Serial.println("Relay test complete");
    
    // Initialize PIR
    Serial.println("Initializing PIR sensor...");
    pir.init();
    Serial.println("PIR sensor initialized");
    
    // Initialize touch controller
    Serial.println("Initializing touch controller...");
    touch.init();
    Serial.println("Touch controller initialized");

    // Connect to WiFi
    connectWiFi();

    // Initialize web server with WiFi management
    webServer.setWiFiMulti(&wifiMulti);
    
    // Set flow data callback to sync with global flow data
    webServer.setFlowDataCallback([](float rate, float volume, bool error) {
        flowData.flowRate = rate;
        flowData.totalVolume = volume;
        flowData.error = error;
        Serial.printf("Flow data updated: Rate=%.2f L/min, Volume=%.3f L, Error=%s\n", 
                     rate, volume, error ? "true" : "false");
    });
    
    // Device control endpoints moved to WebServerManager

    // Flow endpoints moved to WebServerManager

    // Start the web server manager (single server)
    webServer.begin();
    Serial.println("Web server with WiFi management started");

    // Draw initial UI
    touch.drawUI(false);
}

void loop() {
    M5.update();
    
    // Prevent watchdog reset
    yield();
    
    // Check free heap memory periodically
    static unsigned long lastMemoryCheck = 0;
    if (millis() - lastMemoryCheck > 10000) { // Every 10 seconds
        size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 10000) { // Less than 10KB free
            Serial.printf("WARNING: Low memory! Free heap: %d bytes\n", freeHeap);
        }
        lastMemoryCheck = millis();
    }

    // Check PIR sensor and handle flashlight
    pir.check(flashlight.state, [](bool on) {
        flashlight.set(on);
    });

    pirStateForWeb = pir.isTriggered();
    
    // Update components
    touch.update();
    webServer.update();
    
    // Update web server with current flow data (less frequently)
    static unsigned long lastFlowUpdate = 0;
    if (millis() - lastFlowUpdate > 1000) { // Only every second
        webServer.updateFlowData(flowData.flowRate, flowData.totalVolume, flowData.error);
        // Also update dispensing controller with flow data
        dispensing.updateFlowData(flowData.totalVolume);
        lastFlowUpdate = millis();
    }
    
    // Additional yield to prevent watchdog issues
    yield();

    // Handle tab switching with buttons
    if (M5.BtnA.wasPressed()) {
        currentTab = 0;  // Main tab
        touch.drawUI(pir.isTriggered());
    }
    if (M5.BtnB.wasPressed()) {
        currentTab = 1;  // PIR tab
        pir.drawTab();
    }
    if (M5.BtnC.wasPressed()) {
        currentTab = (currentTab == 2) ? 3 : 2;  // Toggle between Flow Sensor and WiFi tabs
        if (currentTab == 2) {
            drawFlowSensorTab();
        } else {
            drawWiFiTab();
        }
    }

    // Get current PIR state
    bool currentPirState = pir.isTriggered();

    // Update current tab
    switch (currentTab) {
        case 0:  // Main tab
            // Update UI if PIR state changed
            if (currentPirState != lastPirState) {
                lastPirState = currentPirState;
                touch.drawUI(currentPirState);
            }
            break;
            
        case 1:  // PIR tab
            pir.updateTab();
            break;
            
        case 2:  // Flow Sensor tab
            static unsigned long lastFlowDisplayUpdate = 0;
            if (millis() - lastFlowDisplayUpdate > 500) { // Update display every 500ms
                updateFlowSensorTab();
                lastFlowDisplayUpdate = millis();
            }
            break;
            
        case 3:  // WiFi tab
            updateWiFiTab();
            break;
    }

    delay(100);
}

void drawFlowSensorTab() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    
    // Draw header
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("Flow Sensor Info");
    
    // Draw labels
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.println("Flow Rate:");
    
    M5.Lcd.setCursor(10, 90);
    M5.Lcd.println("Total Volume:");
    
    M5.Lcd.setCursor(10, 130);
    M5.Lcd.println("Status:");
    
    // Initial update of values
    updateFlowSensorTab();
}

void updateFlowSensorTab() {
    // Clear the value areas first
    M5.Lcd.fillRect(150, 50, 170, 20, BLACK);
    M5.Lcd.fillRect(150, 90, 170, 20, BLACK);
    M5.Lcd.fillRect(150, 130, 170, 20, BLACK);
    
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    
    // Update flow rate
    M5.Lcd.setCursor(150, 50);
    M5.Lcd.printf("%.2f L/min", flowData.flowRate);
    
    // Update total volume
    M5.Lcd.setCursor(150, 90);
    M5.Lcd.printf("%.3f L", flowData.totalVolume);
    
    // Update status
    M5.Lcd.setCursor(150, 130);
    if (flowData.error) {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("ERROR");
    } else {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println("OK   ");  // Extra spaces to clear old text
    }
    M5.Lcd.setTextColor(WHITE);
    
    // Debug output
    Serial.printf("Display updated: %.2f L/min, %.3f L, %s\n", 
                  flowData.flowRate, flowData.totalVolume, flowData.error ? "ERROR" : "OK");
}
