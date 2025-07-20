#include <M5Core2.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
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
WiFiUDP discoveryUDP;

Flashlight flashlight(&pbhub);
RelayController relay(&pbhub);
PIRSensor pir(&pbhub);
DispensingController dispensing(&relay);
WebServerManager webServer(&pir, &relay, &flashlight, &dispensing);
TouchController touch(&flashlight, &relay);

// Flow sensor data from both units
struct FlowSensorData {
    float flowRate = 0.0f;
    float totalVolume = 0.0f;
    bool error = false;
} flowData, flow2Data;

static bool pirStateForWeb = false;
static int currentTab = 0;  // 0: Main, 1: PIR, 2: Flow Sensor, 3: WiFi
static bool lastPirState = false;

void handleUDPDiscovery() {
    int packetSize = discoveryUDP.parsePacket();
    if (packetSize) {
        char incomingPacket[64];
        int len = discoveryUDP.read(incomingPacket, sizeof(incomingPacket) - 1);
        incomingPacket[len] = '\0';
        
        String message = String(incomingPacket);
        Serial.println("Received UDP discovery packet: " + message);
        Serial.println("From IP: " + discoveryUDP.remoteIP().toString());
        
        if (message == "FLOW_SENSOR_DISCOVERY" || message == "FLOW_SENSOR2_DISCOVERY") {
            // Send response with our IP (use AP IP if in AP mode, otherwise local IP)
            String ourIP;
            if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
                ourIP = WiFi.softAPIP().toString();
            } else {
                ourIP = WiFi.localIP().toString();
            }
            
            String response = "JOOGIMASIN_RESPONSE:" + ourIP;
            
            discoveryUDP.beginPacket(discoveryUDP.remoteIP(), 12345); // Respond to port 12345
            discoveryUDP.print(response);
            discoveryUDP.endPacket();
            
            Serial.println("Sent discovery response to " + message + ": " + response);
        }
    }
}

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

void startAccessPoint() {
    Serial.println("Starting Access Point mode...");
    
    // Access Point credentials
    const char* ap_ssid = "Joogimasin-AP";
    const char* ap_password = "joogimasin123";
    
    // Configure AP with fixed IP
    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP(ap_ssid, ap_password);
    
    Serial.println("Access Point started!");
    Serial.println("SSID: " + String(ap_ssid));
    Serial.println("Password: " + String(ap_password));
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
    
    // Initialize mDNS for AP mode
    if (!MDNS.begin("joogimasin")) {
        Serial.println("Error setting up mDNS responder!");
    } else {
        Serial.println("mDNS responder started as 'joogimasin.local'");
        MDNS.addService("http", "tcp", 80);
    }
    
    // Start UDP discovery listener
    if (discoveryUDP.begin(12346)) {
        Serial.println("UDP discovery listener started on port 12346");
    } else {
        Serial.println("Failed to start UDP discovery listener");
    }
    
    // Display AP info
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 30);
    M5.Lcd.println("Access Point Mode");
    
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.setCursor(10, 70);
    M5.Lcd.println("SSID: " + String(ap_ssid));
    
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.println("Password:");
    M5.Lcd.setCursor(10, 140);
    M5.Lcd.println(ap_password);
    
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.setCursor(10, 180);
    M5.Lcd.println("IP: " + WiFi.softAPIP().toString());
    
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(10, 210);
    M5.Lcd.println("Flow sensor will auto-connect");
    
    delay(3000);
}

void connectWiFi() {
    Serial.println("=== MAIN UNIT WIFI SETUP ===");
    
    // Always start Access Point for flow sensor connection
    Serial.println("Starting Access Point for flow sensor...");
    startAccessPoint();
    
    // Optional: Also try to connect to existing networks for internet access
    Serial.println("Also trying to connect to existing networks...");
    
    WiFi.mode(WIFI_AP_STA);  // Enable both Access Point and Station mode
    loadSavedWiFiCredentials();
    
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.println("Starting WiFi...");
    
    int attempts = 0;
    while (wifiMulti.run() != WL_CONNECTED && attempts < 10) { // Reduced attempts for faster AP start
        delay(500);
        Serial.print(".");
        M5.Lcd.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nAlso connected to existing network: " + WiFi.SSID());
        Serial.println("Station IP: " + WiFi.localIP().toString());
        
        // Initialize mDNS for auto-discovery
        if (!MDNS.begin("joogimasin")) {
            Serial.println("Error setting up mDNS responder!");
        } else {
            Serial.println("mDNS responder started as 'joogimasin.local'");
            MDNS.addService("http", "tcp", 80);
        }
        
        // Start UDP discovery listener
        if (discoveryUDP.begin(12346)) {
            Serial.println("UDP discovery listener started on port 12346");
        } else {
            Serial.println("Failed to start UDP discovery listener");
        }
        
        M5.Lcd.setCursor(10, 90);
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println("Hybrid Mode!");
        M5.Lcd.setCursor(10, 110);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setTextSize(1);
        M5.Lcd.println("AP: " + WiFi.softAPIP().toString());
        M5.Lcd.setCursor(10, 130);
        M5.Lcd.println("STA: " + WiFi.localIP().toString());
        M5.Lcd.setCursor(10, 150);
        M5.Lcd.println("Network: " + WiFi.SSID());
        M5.Lcd.setCursor(10, 170);
        M5.Lcd.println("Flow sensor connects to AP");
        delay(3000);
    } else {
        Serial.println("\nExternal WiFi connection failed");
        Serial.println("Running in Access Point only mode");
        
        M5.Lcd.setCursor(10, 90);
        M5.Lcd.setTextColor(YELLOW);
        M5.Lcd.println("AP Mode Only");
        M5.Lcd.setCursor(10, 130);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.println("AP IP: " + WiFi.softAPIP().toString());
        M5.Lcd.setCursor(10, 170);
        M5.Lcd.setTextSize(1);
        M5.Lcd.println("Flow sensor will connect to AP");
        delay(3000);
    }
    
    Serial.println("Access Point IP: " + WiFi.softAPIP().toString());
    Serial.println("Flow sensor should connect to: Joogimasin-AP");
}

void drawWiFiTab() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    
    // Draw header
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("WiFi Information");
    
    // Check if in AP mode or connected to existing network
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        // Access Point mode
        M5.Lcd.setCursor(10, 50);
        M5.Lcd.setTextColor(YELLOW);
        M5.Lcd.println("Mode: Access Point");
        M5.Lcd.setTextColor(WHITE);
        
        M5.Lcd.setCursor(10, 90);
        M5.Lcd.println("SSID: Joogimasin-AP");
        
        M5.Lcd.setCursor(10, 130);
        M5.Lcd.println("IP: " + WiFi.softAPIP().toString());
        
        M5.Lcd.setCursor(10, 170);
        M5.Lcd.printf("Clients: %d", WiFi.softAPgetStationNum());
        
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(10, 200);
        M5.Lcd.println("Password: joogimasin123");
        
    } else if (WiFi.status() == WL_CONNECTED) {
        // Connected to existing network
        M5.Lcd.setCursor(10, 50);
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
        // Disconnected
        M5.Lcd.setCursor(10, 50);
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
    M5.Lcd.println("Web interface available on shown IP");
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
    
    // Set flow data callbacks to sync with global flow data
    webServer.setFlowDataCallback([](float rate, float volume, bool error) {
        flowData.flowRate = rate;
        flowData.totalVolume = volume;
        flowData.error = error;
        Serial.printf("Flow 1 data updated: Rate=%.2f L/min, Volume=%.3f L, Error=%s\n", 
                     rate, volume, error ? "true" : "false");
    });
    
    webServer.setFlow2DataCallback([](float rate, float volume, bool error) {
        flow2Data.flowRate = rate;
        flow2Data.totalVolume = volume;
        flow2Data.error = error;
        Serial.printf("Flow 2 data updated: Rate=%.2f L/min, Volume=%.3f L, Error=%s\n", 
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
    
    // Handle UDP discovery requests
    if (WiFi.status() == WL_CONNECTED) {
        handleUDPDiscovery();
    }
    
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
        webServer.updateFlow2Data(flow2Data.flowRate, flow2Data.totalVolume, flow2Data.error);
        // Update dispensing controller with flow data from both sensors
        dispensing.updateFlowData(flowData.totalVolume);    // Pump 1 uses Flow Sensor 1
        dispensing.updateFlow2Data(flow2Data.totalVolume);  // Pump 2 uses Flow Sensor 2
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
    M5.Lcd.println("Flow Sensors Info");
    
    // Draw labels for Sensor 1
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 40);
    M5.Lcd.println("Sensor 1:");
    M5.Lcd.setCursor(10, 55);
    M5.Lcd.println("Rate:");
    M5.Lcd.setCursor(10, 70);
    M5.Lcd.println("Volume:");
    M5.Lcd.setCursor(10, 85);
    M5.Lcd.println("Status:");
    
    // Draw labels for Sensor 2
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.println("Sensor 2:");
    M5.Lcd.setCursor(10, 125);
    M5.Lcd.println("Rate:");
    M5.Lcd.setCursor(10, 140);
    M5.Lcd.println("Volume:");
    M5.Lcd.setCursor(10, 155);
    M5.Lcd.println("Status:");
    
    // Initial update of values
    updateFlowSensorTab();
}

void updateFlowSensorTab() {
    // Clear the value areas first
    M5.Lcd.fillRect(100, 40, 220, 130, BLACK);
    
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(1);
    
    // Update Sensor 1 data
    M5.Lcd.setCursor(100, 55);
    M5.Lcd.printf("%.2f L/min", flowData.flowRate);
    
    M5.Lcd.setCursor(100, 70);
    M5.Lcd.printf("%.3f L", flowData.totalVolume);
    
    M5.Lcd.setCursor(100, 85);
    if (flowData.error) {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("ERROR");
    } else {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println("OK");
    }
    
    // Update Sensor 2 data
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(100, 125);
    M5.Lcd.printf("%.2f L/min", flow2Data.flowRate);
    
    M5.Lcd.setCursor(100, 140);
    M5.Lcd.printf("%.3f L", flow2Data.totalVolume);
    
    M5.Lcd.setCursor(100, 155);
    if (flow2Data.error) {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("ERROR");
    } else {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println("OK");
    }
    M5.Lcd.setTextColor(WHITE);
    
    // Debug output
    Serial.printf("Display updated: S1=%.2f L/min %.3f L %s, S2=%.2f L/min %.3f L %s\n", 
                  flowData.flowRate, flowData.totalVolume, flowData.error ? "ERROR" : "OK",
                  flow2Data.flowRate, flow2Data.totalVolume, flow2Data.error ? "ERROR" : "OK");
}
