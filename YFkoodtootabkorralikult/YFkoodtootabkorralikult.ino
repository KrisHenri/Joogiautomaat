#include <M5Core2.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

// Flow sensor pin
#define FLOW_SENSOR_PIN 32  // Port A on M5Stack Core 2

// Flow sensor specifications
#define FLOW_SENSOR_PULSES_PER_LITER 450  // YF-S402 typically has 450 pulses per liter
#define FLOW_SENSOR_UPDATE_INTERVAL 1000  // Update every second

// WiFi settings
WiFiMulti wifiMulti;

// Main unit discovery
const char* mainUnitHostname = "joogimasin";  // Main unit's mDNS hostname
String mainUnitIP = "";  // Will be discovered automatically
// Fallback IP - set this manually if mDNS discovery fails
// const String fallbackIP = "192.168.1.100";  // Uncomment and set your main unit's IP
unsigned long lastDiscoveryTime = 0;
const unsigned long DISCOVERY_INTERVAL = 30000;  // Try to discover every 30 seconds
int connectionFailureCount = 0;
const int MAX_FAILURE_COUNT = 3;  // Only rediscover after 3 consecutive failures

// Flow sensor variables
volatile long pulseCount = 0;
float flowRate = 0.0f;      // Flow rate in L/min
float totalVolume = 0.0f;   // Total volume in L
bool flowError = false;
unsigned long lastUpdateTime = 0;
unsigned long lastPulseTime = 0;

// Interrupt service routine for flow sensor
void IRAM_ATTR pulseCounter() {
    pulseCount++;
    lastPulseTime = millis();
    // Note: Don't use Serial.print in interrupt handlers as it can cause issues
}

bool discoverMainUnit() {
    Serial.println("Discovering main unit...");
    Serial.println("Looking for hostname: " + String(mainUnitHostname) + ".local");
    
    // Try to resolve the mDNS hostname
    IPAddress serverIP = MDNS.queryHost(mainUnitHostname);
    
    if (serverIP.toString() != "0.0.0.0") {
        mainUnitIP = serverIP.toString();
        Serial.println("Found main unit at: " + mainUnitIP);
        
        // Test connectivity to the main unit
        HTTPClient http;
        http.begin("http://" + mainUnitIP + "/flow/test");
        http.setTimeout(3000);
        
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String response = http.getString();
            Serial.println("Connection test successful: " + response);
            http.end();
            return true;
        } else {
            Serial.printf("Connection test failed with code: %d\n", httpCode);
            http.end();
            return false;
        }
    } else {
        Serial.println("Main unit not found via mDNS");
        Serial.println("Trying to ping common IP addresses...");
        
        // Try some common local IP addresses as fallback
        String baseIP = WiFi.localIP().toString();
        int lastDot = baseIP.lastIndexOf('.');
        if (lastDot > 0) {
            String subnet = baseIP.substring(0, lastDot + 1);
            
            // Try a few common addresses in the same subnet
            for (int i = 1; i <= 254; i++) {
                String testIP = subnet + String(i);
                if (testIP != WiFi.localIP().toString()) {
                    Serial.println("Testing IP: " + testIP);
                    
                    HTTPClient http;
                    http.begin("http://" + testIP + "/flow/test");
                    http.setTimeout(1000); // 1 second timeout
                    
                    int httpCode = http.GET();
                    if (httpCode == HTTP_CODE_OK) {
                        String response = http.getString();
                        if (response.indexOf("Flow endpoint is working") >= 0) {
                            Serial.println("Found main unit at: " + testIP);
                            mainUnitIP = testIP;
                            http.end();
                            return true;
                        } else {
                            Serial.println("Found web server at " + testIP + " but not main unit");
                        }
                    }
                    http.end();
                }
            }
        }
        
        Serial.println("No main unit found in network scan");
        
        // Try known IP addresses as last resort
        String knownIPs[] = {"192.168.88.188", "192.168.1.100", "192.168.0.100"};
        for (int i = 0; i < 3; i++) {
            Serial.println("Trying known IP: " + knownIPs[i]);
            
            HTTPClient http;
            http.begin("http://" + knownIPs[i] + "/flow/test");
            http.setTimeout(2000);
            
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                String response = http.getString();
                if (response.indexOf("Flow endpoint is working") >= 0) {
                    Serial.println("Found main unit at known IP: " + knownIPs[i]);
                    mainUnitIP = knownIPs[i];
                    http.end();
                    return true;
                }
            }
            http.end();
        }
        
        return false;
    }
}

void connectWiFi() {
    // Add known networks
    wifiMulti.addAP("Illuminaty", "S330nm1nuk0du!");
    // Add second WiFi network - replace with your actual credentials
    wifiMulti.addAP("Distillery", "Sommerling");
    // Add third WiFi network if needed
    wifiMulti.addAP("Distillery-Office", "Sommerling");
    // Add more networks as needed
    // wifiMulti.addAP("YourNetwork", "YourPassword");
    
    Serial.print("Connecting to WiFi");
    
    int attempts = 0;
    while (wifiMulti.run() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi");
        Serial.println("SSID: " + WiFi.SSID());
        Serial.println("IP Address: " + WiFi.localIP().toString());
        
        // Initialize mDNS
        if (!MDNS.begin("flowsensor")) {
            Serial.println("Error setting up mDNS responder!");
        } else {
            Serial.println("mDNS responder started as 'flowsensor.local'");
        }
        
        // Try to discover main unit
        discoverMainUnit();
    } else {
        Serial.println("\nWiFi connection failed!");
        Serial.println("Please check network credentials");
    }
}

void updateFlowSensor() {
    unsigned long currentTime = millis();
    
    // Check for flow sensor error (no pulses for 5 seconds)
    if (currentTime - lastPulseTime > 5000) {
        flowError = true;
    } else {
        flowError = false;
    }
    
    // Calculate flow rate and volume
    if (currentTime - lastUpdateTime >= FLOW_SENSOR_UPDATE_INTERVAL) {
        // Debug output
        if (pulseCount > 0) {
            Serial.printf("=== FLOW DETECTED ===\n");
            Serial.printf("Pulse count: %ld\n", pulseCount);
            Serial.printf("Time interval: %lu ms\n", currentTime - lastUpdateTime);
        }
        
        // Calculate flow rate in L/min
        flowRate = (pulseCount * 60.0f) / (FLOW_SENSOR_PULSES_PER_LITER * (currentTime - lastUpdateTime) / 1000.0f);
        
        // Calculate volume in liters: pulses / pulses_per_liter
        // Example: 450 pulses / 450 pulses_per_liter = 1 liter
        float volumeIncrement = (float)pulseCount / FLOW_SENSOR_PULSES_PER_LITER;
        totalVolume += volumeIncrement;
        
        // Debug output
        if (pulseCount > 0) {
            Serial.printf("Flow rate: %.2f L/min\n", flowRate);
            Serial.printf("Volume increment: %.4f L\n", volumeIncrement);
            Serial.printf("Total volume: %.3f L\n", totalVolume);
            Serial.printf("Error status: %s\n", flowError ? "true" : "false");
            Serial.println("=== FLOW UPDATE COMPLETE ===");
        }
        
        // Reset pulse count
        pulseCount = 0;
        lastUpdateTime = currentTime;
    }
}

void sendDataToMainUnit() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        connectWiFi();
        return;
    }

    // Check if we need to discover main unit IP
    if (mainUnitIP.length() == 0 || (millis() - lastDiscoveryTime > DISCOVERY_INTERVAL)) {
        Serial.println("Attempting to discover main unit...");
        if (discoverMainUnit()) {
            lastDiscoveryTime = millis();
        } else {
            Serial.println("Cannot find main unit, skipping data send");
            return;
        }
    }

    HTTPClient http;
    String url = "http://" + mainUnitIP + "/flow/update";
    
    // Try POST method first (JSON)
    Serial.println("Trying POST method with JSON data...");
    
    // Create JSON document
    StaticJsonDocument<200> doc;
    doc["flowRate"] = flowRate;
    doc["totalVolume"] = totalVolume;
    doc["error"] = flowError;
    
    String jsonData;
    serializeJson(doc, jsonData);
    
    Serial.println("Sending data to: " + url);
    Serial.println("JSON data: " + jsonData);
    
    // Send data to main unit via POST
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000); // 5 second timeout
    
    int httpCode = http.POST(jsonData);
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String response = http.getString();
            Serial.println("POST data sent successfully to " + mainUnitIP);
            Serial.println("Response: " + response);
            connectionFailureCount = 0;  // Reset failure counter on success
            http.end();
            return; // Success, exit function
        } else {
            Serial.printf("POST HTTP error: %d\n", httpCode);
            String response = http.getString();
            Serial.println("POST Error response: " + response);
        }
    } else {
        Serial.printf("POST HTTP request failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    
    // If POST failed, try GET method as fallback
    Serial.println("POST failed, trying GET method as fallback...");
    
    String getUrl = url + "?flowRate=" + String(flowRate, 2) + 
                   "&totalVolume=" + String(totalVolume, 3) + 
                   "&error=" + (flowError ? "true" : "false");
    
    Serial.println("GET URL: " + getUrl);
    
    http.begin(getUrl);
    http.setTimeout(5000);
    
    httpCode = http.GET();
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String response = http.getString();
            Serial.println("GET data sent successfully to " + mainUnitIP);
            Serial.println("Response: " + response);
            connectionFailureCount = 0;  // Reset failure counter on success
        } else {
            Serial.printf("GET HTTP error: %d\n", httpCode);
            String response = http.getString();
            Serial.println("GET Error response: " + response);
            connectionFailureCount++;
            
            // Only clear IP after multiple failures or specific error codes
            if (connectionFailureCount >= MAX_FAILURE_COUNT || httpCode == -1 || httpCode == 404) {
                Serial.printf("Too many failures (%d) or server unreachable, will rediscover main unit\n", connectionFailureCount);
                mainUnitIP = "";  // Force rediscovery
                connectionFailureCount = 0;  // Reset counter
            } else {
                Serial.printf("Failure %d/%d, keeping current IP for next attempt\n", connectionFailureCount, MAX_FAILURE_COUNT);
            }
        }
    } else {
        Serial.printf("GET HTTP request failed: %s\n", http.errorToString(httpCode).c_str());
        connectionFailureCount++;
        
        if (connectionFailureCount >= MAX_FAILURE_COUNT) {
            Serial.printf("Too many connection failures (%d), will rediscover main unit\n", connectionFailureCount);
            mainUnitIP = "";  // Force rediscovery
            connectionFailureCount = 0;  // Reset counter
        } else {
            Serial.printf("Connection failure %d/%d, keeping current IP for next attempt\n", connectionFailureCount, MAX_FAILURE_COUNT);
        }
    }
    
    http.end();
}

void updateDisplay() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    
    // Display header
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("Flow Sensor Unit");
    
    // Display flow rate
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.print("Flow: ");
    M5.Lcd.print(flowRate, 2);
    M5.Lcd.println(" L/min");
    
    // Display total volume
    M5.Lcd.setCursor(10, 80);
    M5.Lcd.print("Total: ");
    M5.Lcd.print(totalVolume, 2);
    M5.Lcd.println(" L");
    
    // Display pulse count for debugging
    M5.Lcd.setCursor(10, 100);
    M5.Lcd.print("Pulses: ");
    M5.Lcd.println(pulseCount);
    
    // Display sensor status
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.print("Sensor: ");
    M5.Lcd.setTextColor(flowError ? RED : GREEN);
    M5.Lcd.println(flowError ? "ERROR" : "OK");
    
    // Display main unit connection status
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(10, 140);
    M5.Lcd.print("Main Unit: ");
    if (mainUnitIP.length() > 0) {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println("Connected");
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(10, 170);
        M5.Lcd.println("IP: " + mainUnitIP);
    } else {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("Searching...");
    }
    
    // Display own IP address
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 190);
    M5.Lcd.print("Flow Unit IP: ");
    M5.Lcd.println(WiFi.localIP());
    
    // Display WiFi network
    M5.Lcd.setCursor(10, 210);
    M5.Lcd.print("Network: ");
    M5.Lcd.println(WiFi.SSID());
}

void setup() {
    M5.begin();
    Serial.begin(115200);
    Serial.println("Flow Sensor Unit initializing...");
    
    // Initialize flow sensor
    pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
    
    // Connect to WiFi
    connectWiFi();
    
    // Initialize display
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 30);
    M5.Lcd.println("Flow Sensor Unit");
    M5.Lcd.setCursor(10, 70);
    M5.Lcd.println("Connecting to WiFi...");
    
    lastUpdateTime = millis();
    lastPulseTime = millis();
}

void loop() {
    M5.update();
    
    // Update flow sensor readings
    updateFlowSensor();
    
    // Send data to main unit every second
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime >= 1000) {
        sendDataToMainUnit();
        lastSendTime = millis();
    }
    
    // Update display
    updateDisplay();
    
    delay(10);  // Small delay to prevent display flicker
}
