#include <M5Core2.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

// Flow sensor pin
#define FLOW_SENSOR_PIN 32  // Port A on M5Stack Core 2

// Flow sensor specifications
// YF-S402 calibration: f = 73×Q formula gives f = 4380 pulses/liter
// This is the precise calibration constant for accurate flow measurement
#define FLOW_SENSOR_PULSES_PER_LITER 4380  // Precise calibration: f = 73×Q
#define FLOW_SENSOR_UPDATE_INTERVAL 200   // Update every 200ms (5 times per second)

// WiFi settings
WiFiMulti wifiMulti;

// Main unit discovery
const char* mainUnitHostname = "joogimasin";  // Main unit's mDNS hostname
String mainUnitIP = "";  // Will be discovered automatically

// MANUAL IP CONFIGURATION - Only use for testing if auto-discovery fails
// Leave commented out for normal operation to allow dynamic discovery
// const String MANUAL_MAIN_UNIT_IP = "192.168.88.188";  // SET YOUR MAIN UNIT IP HERE
unsigned long lastDiscoveryTime = 0;
const unsigned long DISCOVERY_INTERVAL = 30000;  // Try to discover every 30 seconds
int connectionFailureCount = 0;
const int MAX_FAILURE_COUNT = 3;  // Only rediscover after 3 consecutive failures

// Flow sensor variables
volatile long pulseCount = 0;          // Pulse count for current interval (gets reset)
volatile long totalPulseCount = 0;     // Total cumulative pulse count (never reset)
float flowRate = 0.0f;                 // Flow rate in L/min
float totalVolume = 0.0f;              // Total volume in L
bool flowError = false;
unsigned long lastUpdateTime = 0;
unsigned long lastPulseTime = 0;

// Flow rate smoothing variables
float flowRateHistory[5] = {0, 0, 0, 0, 0};  // Store last 5 readings for smoothing
int flowRateIndex = 0;
float smoothedFlowRate = 0.0f;

// Display management
static int currentTab = 0;  // 0: Flow Data, 1: Network Info
static unsigned long lastDisplayUpdate = 0;

// Dispensing tracking variables for auto-reset
bool wasDispensing = false;
float lastTargetVolume = 0.0f;
unsigned long lastDispensingCheck = 0;
const unsigned long DISPENSING_CHECK_INTERVAL = 2000; // Check every 2 seconds

// Interrupt service routine for flow sensor
void IRAM_ATTR pulseCounter() {
    pulseCount++;           // For interval calculations
    totalPulseCount++;      // For total cumulative count
    lastPulseTime = millis();
    // Note: Don't use Serial.print in interrupt handlers as it can cause issues
}

String findMainUnitByBroadcast() {
    WiFiUDP udp;
    
    // Start UDP on port 12345
    if (!udp.begin(12345)) {
        Serial.println("UDP broadcast discovery failed to start");
        return "";
    }
    
    Serial.println("Sending broadcast discovery packets...");
    
    // Calculate broadcast address for current subnet
    IPAddress localIP = WiFi.localIP();
    IPAddress subnetMask = WiFi.subnetMask();
    IPAddress broadcastAddr;
    
    // Calculate broadcast address: IP | (~subnetMask)
    for (int i = 0; i < 4; i++) {
        broadcastAddr[i] = localIP[i] | (~subnetMask[i]);
    }
    
    Serial.println("Broadcast address: " + broadcastAddr.toString());
    
    // Send discovery packet
    String discoveryMsg = "FLOW_SENSOR_DISCOVERY";
    udp.beginPacket(broadcastAddr, 12346); // Send to port 12346
    udp.print(discoveryMsg);
    udp.endPacket();
    
    // Wait for response
    unsigned long startTime = millis();
    while (millis() - startTime < 3000) { // Wait up to 3 seconds
        int packetSize = udp.parsePacket();
        if (packetSize) {
            char responseBuffer[64];
            int len = udp.read(responseBuffer, sizeof(responseBuffer) - 1);
            responseBuffer[len] = '\0';
            
            String response = String(responseBuffer);
            if (response.startsWith("JOOGIMASIN_RESPONSE:")) {
                String foundIP = response.substring(20); // Extract IP after "JOOGIMASIN_RESPONSE:"
                Serial.println("Received response from: " + udp.remoteIP().toString());
                Serial.println("Main unit reports IP: " + foundIP);
                udp.stop();
                return foundIP;
            }
        }
        delay(50);
    }
    
    udp.stop();
    Serial.println("No broadcast response received");
    return "";
}

bool discoverMainUnit() {
    Serial.println("=== DISCOVERING MAIN UNIT ===");
    Serial.println("Flow sensor IP: " + WiFi.localIP().toString());
    
#ifdef MANUAL_MAIN_UNIT_IP
    // Use manual IP if defined
    Serial.println("Using MANUAL IP configuration: " + String(MANUAL_MAIN_UNIT_IP));
    mainUnitIP = MANUAL_MAIN_UNIT_IP;
    
    HTTPClient http;
    http.begin("http://" + mainUnitIP + "/flow/test");
    http.setTimeout(3000);
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        Serial.println("Manual IP connection successful: " + response);
        http.end();
        return true;
    } else {
        Serial.printf("Manual IP connection failed with code: %d\n", httpCode);
        http.end();
        return false;
    }
#endif

    // Try UDP broadcast discovery first (works across different networks)
    Serial.println("Trying UDP broadcast discovery...");
    String broadcastIP = findMainUnitByBroadcast();
    if (broadcastIP.length() > 0) {
        mainUnitIP = broadcastIP;
        Serial.println("*** Found main unit via broadcast at: " + mainUnitIP + " ***");
        return true;
    }
    
    Serial.println("Looking for hostname: " + String(mainUnitHostname) + ".local");
    
    // Multiple attempts at mDNS resolution
    IPAddress serverIP;
    bool mdnsFound = false;
    
    for (int attempt = 1; attempt <= 3; attempt++) {
        Serial.printf("mDNS attempt %d/3...\n", attempt);
        serverIP = MDNS.queryHost(mainUnitHostname);
        
        if (serverIP.toString() != "0.0.0.0") {
            mdnsFound = true;
            break;
        }
        delay(1000); // Wait 1 second between attempts
    }
    
    if (mdnsFound) {
        mainUnitIP = serverIP.toString();
        Serial.println("Found main unit via mDNS at: " + mainUnitIP);
        
        // Test connectivity to the main unit
        HTTPClient http;
        http.begin("http://" + mainUnitIP + "/flow/test");
        http.setTimeout(3000);
        
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String response = http.getString();
            Serial.println("mDNS connection test successful: " + response);
            http.end();
            return true;
        } else {
            Serial.printf("mDNS connection test failed with code: %d\n", httpCode);
            http.end();
            // Don't return false yet, try other methods
        }
    } else {
        Serial.println("Main unit not found via mDNS, trying fallback methods...");
        
        // Try known IP addresses first (faster than scanning)
        String knownIPs[] = {
            "192.168.88.188",  // Your known IP
            "192.168.1.100", 
            "192.168.0.100",
            "192.168.4.1",     // Common ESP32 AP mode IP
            "192.168.1.1",     // Common router IP
            "10.0.0.1"         // Another common router IP
        };
        
        Serial.println("Trying known IP addresses...");
        for (int i = 0; i < 6; i++) {
            Serial.printf("Testing known IP %d/6: %s\n", i+1, knownIPs[i].c_str());
            
            HTTPClient http;
            http.begin("http://" + knownIPs[i] + "/flow/test");
            http.setTimeout(2000);
            
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                String response = http.getString();
                if (response.indexOf("Flow endpoint is working") >= 0) {
                    Serial.println("*** Found main unit at known IP: " + knownIPs[i] + " ***");
                    mainUnitIP = knownIPs[i];
                    http.end();
                    return true;
                }
            }
            http.end();
            delay(200); // Small delay between attempts
        }
        
        // Try subnet scan as last resort (only common IPs to save time)
        Serial.println("Trying subnet scan (limited range)...");
        String baseIP = WiFi.localIP().toString();
        int lastDot = baseIP.lastIndexOf('.');
        if (lastDot > 0) {
            String subnet = baseIP.substring(0, lastDot + 1);
            Serial.println("Scanning subnet: " + subnet + "x");
            
            // Only scan common IP ranges to save time
            int scanIPs[] = {1, 2, 100, 101, 188, 200, 254};
            for (int i = 0; i < 7; i++) {
                String testIP = subnet + String(scanIPs[i]);
                if (testIP != WiFi.localIP().toString()) {
                    Serial.println("Scanning: " + testIP);
                    
                    HTTPClient http;
                    http.begin("http://" + testIP + "/flow/test");
                    http.setTimeout(1500);
                    
                    int httpCode = http.GET();
                    if (httpCode == HTTP_CODE_OK) {
                        String response = http.getString();
                        if (response.indexOf("Flow endpoint is working") >= 0) {
                            Serial.println("*** Found main unit at: " + testIP + " ***");
                            mainUnitIP = testIP;
                            http.end();
                            return true;
                        }
                    }
                    http.end();
                    delay(300);
                }
            }
        }
        
        return false;
    }
}

void connectWiFi() {
    Serial.println("=== FLOW SENSOR WIFI CONNECTION ===");
    
    // Priority 1: Try to connect to main unit's Access Point
    Serial.println("Priority 1: Trying main unit Access Point...");
    wifiMulti.addAP("Joogimasin-AP", "joogimasin123");
    
    // Priority 2: Add known existing networks as fallback
    wifiMulti.addAP("Illuminaty", "S330nm1nuk0du!");
    wifiMulti.addAP("Distillery", "Sommerling");
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
        
        // Check if connected to main unit's AP
        if (WiFi.SSID() == "Joogimasin-AP") {
            Serial.println("*** CONNECTED TO MAIN UNIT ACCESS POINT ***");
            Serial.println("Using direct connection - no discovery needed!");
            // Set main unit IP directly (AP gateway)
            mainUnitIP = "192.168.4.1";
            Serial.println("Main unit IP set to: " + mainUnitIP);
        } else {
            Serial.println("Connected to external network - will use discovery");
        }
        
        // Initialize mDNS
        if (!MDNS.begin("flowsensor")) {
            Serial.println("Error setting up mDNS responder!");
        } else {
            Serial.println("mDNS responder started as 'flowsensor.local'");
        }
        
        // Try to discover main unit (only if not on AP)
        if (WiFi.SSID() != "Joogimasin-AP") {
            discoverMainUnit();
        }
    } else {
        Serial.println("\nWiFi connection failed!");
        Serial.println("Please check:");
        Serial.println("1. Main unit is powered on");
        Serial.println("2. Main unit Access Point is active");
        Serial.println("3. Network credentials are correct");
    }
}

float calculateSmoothedFlowRate(float newReading) {
    // Add new reading to history
    flowRateHistory[flowRateIndex] = newReading;
    flowRateIndex = (flowRateIndex + 1) % 5;
    
    // Calculate average of last 5 readings
    float sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += flowRateHistory[i];
    }
    return sum / 5.0f;
}

void updateFlowSensor() {
    unsigned long currentTime = millis();
    
    // Check for flow sensor error (no pulses for 5 seconds)
    if (currentTime - lastPulseTime > 5000) {
        flowError = true;
        // Clear flow rate history when no flow detected
        for (int i = 0; i < 5; i++) {
            flowRateHistory[i] = 0;
        }
        smoothedFlowRate = 0;
    } else {
        flowError = false;
    }
    
    // Calculate flow rate and volume
    if (currentTime - lastUpdateTime >= FLOW_SENSOR_UPDATE_INTERVAL) {
        float actualTimeInterval = (currentTime - lastUpdateTime) / 1000.0f; // Convert to seconds
        
        // Debug output
        if (pulseCount > 0) {
            Serial.printf("=== FLOW DETECTED ===\n");
            Serial.printf("Pulse count: %ld\n", pulseCount);
            Serial.printf("Time interval: %.3f seconds\n", actualTimeInterval);
            Serial.printf("Calibration: %d pulses/liter (f=73×Q)\n", FLOW_SENSOR_PULSES_PER_LITER);
        }
        
        // Calculate instantaneous flow rate in L/min
        float instantaneousFlowRate = 0.0f;
        if (pulseCount > 0 && actualTimeInterval > 0) {
            // Improved calculation with minimum time threshold
            if (actualTimeInterval >= 0.1f) { // Only calculate if we have at least 100ms of data
                instantaneousFlowRate = (pulseCount * 60.0f) / (FLOW_SENSOR_PULSES_PER_LITER * actualTimeInterval);
                
                // Apply reasonable limits to prevent unrealistic readings
                if (instantaneousFlowRate > 50.0f) { // Cap at 50 L/min (very high but possible)
                    instantaneousFlowRate = 50.0f;
                    Serial.println("WARNING: Flow rate capped at 50 L/min");
                }
            } else {
                // For very short intervals, use a conservative estimate
                instantaneousFlowRate = (pulseCount * 60.0f) / (FLOW_SENSOR_PULSES_PER_LITER * 0.2f); // Assume 200ms minimum
                if (instantaneousFlowRate > 10.0f) { // More conservative cap for short intervals
                    instantaneousFlowRate = 10.0f;
                }
                Serial.printf("Short interval detected, using conservative estimate\n");
            }
        }
        
        // Apply smoothing to reduce erratic readings
        smoothedFlowRate = calculateSmoothedFlowRate(instantaneousFlowRate);
        flowRate = smoothedFlowRate;
        
        // Calculate volume in liters: pulses / pulses_per_liter
        float volumeIncrement = (float)pulseCount / FLOW_SENSOR_PULSES_PER_LITER;
        totalVolume += volumeIncrement;
        
        // Debug output
        if (pulseCount > 0) {
            Serial.printf("Instantaneous flow rate: %.2f L/min\n", instantaneousFlowRate);
            Serial.printf("Smoothed flow rate: %.2f L/min\n", smoothedFlowRate);
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
    StaticJsonDocument<250> doc;
    doc["flowRate"] = flowRate;
    doc["totalVolume"] = totalVolume;
    doc["error"] = flowError;
    doc["pulseCount"] = totalPulseCount;
    
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
                   "&error=" + (flowError ? "true" : "false") +
                   "&pulseCount=" + String(totalPulseCount);
    
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

void checkDispensingStatus() {
    // Only check every 2 seconds to avoid excessive requests
    if (millis() - lastDispensingCheck < DISPENSING_CHECK_INTERVAL) {
        return;
    }
    lastDispensingCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED || mainUnitIP.length() == 0) {
        return; // Can't check if not connected
    }
    
    HTTPClient http;
    String url = "http://" + mainUnitIP + "/flow/dispensing";
    
    http.begin(url);
    http.setTimeout(3000);
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        
        // Parse JSON response
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
            bool isDispensing = doc["isDispensing"] | false;
            float targetVolume = doc["targetVolume"] | 0.0f;
            
            // Check if a new dispensing session started
            if (isDispensing && !wasDispensing) {
                // New dispensing started - reset pulse count
                Serial.println("=== NEW DISPENSING DETECTED - AUTO-RESETTING PULSE COUNT ===");
                Serial.printf("Target volume: %.3f L\n", targetVolume);
                totalPulseCount = 0;
                pulseCount = 0;
                
                // Show notification on display
                M5.Lcd.fillScreen(BLACK);
                M5.Lcd.setTextColor(YELLOW);
                M5.Lcd.setTextSize(2);
                M5.Lcd.setCursor(30, 80);
                M5.Lcd.println("DISPENSING");
                M5.Lcd.setCursor(50, 110);
                M5.Lcd.println("STARTED");
                M5.Lcd.setCursor(20, 140);
                M5.Lcd.printf("%.2fL Target", targetVolume);
                M5.Lcd.setTextColor(GREEN);
                M5.Lcd.setCursor(10, 170);
                M5.Lcd.println("Pulse Count Reset");
                delay(1500); // Show message for 1.5 seconds
            }
            
            // Update tracking variables
            wasDispensing = isDispensing;
            lastTargetVolume = targetVolume;
        }
    }
    
    http.end();
}

void resetVolumeCounter() {
    // Reset local volume counter and pulse count
    totalVolume = 0.0f;
    totalPulseCount = 0;
    pulseCount = 0;
    Serial.println("=== VOLUME AND PULSE COUNT RESET LOCALLY ===");
    
    // Also send reset command to main unit if connected
    if (WiFi.status() == WL_CONNECTED && mainUnitIP.length() > 0) {
        HTTPClient http;
        String resetUrl = "http://" + mainUnitIP + "/flow/reset";
        
        Serial.println("Sending volume reset command to main unit...");
        Serial.println("Reset URL: " + resetUrl);
        
        // Try POST method first
        http.begin(resetUrl);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(3000);
        
        // Send empty JSON object for POST
        String jsonData = "{}";
        int httpCode = http.POST(jsonData);
        
        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK) {
                String response = http.getString();
                Serial.println("Volume reset sent successfully to main unit");
                Serial.println("Response: " + response);
            } else {
                Serial.printf("Volume reset POST failed with code: %d\n", httpCode);
                // Try GET method as fallback
                http.end();
                http.begin(resetUrl);
                httpCode = http.GET();
                if (httpCode == HTTP_CODE_OK) {
                    String response = http.getString();
                    Serial.println("Volume reset sent successfully via GET to main unit");
                    Serial.println("Response: " + response);
                } else {
                    Serial.printf("Volume reset GET also failed with code: %d\n", httpCode);
                }
            }
        } else {
            Serial.printf("Volume reset request failed: %s\n", http.errorToString(httpCode).c_str());
        }
        
        http.end();
    } else {
        Serial.println("Cannot send reset to main unit - not connected");
    }
    
    // Show confirmation message on display
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(50, 100);
    M5.Lcd.println("VOLUME");
    M5.Lcd.setCursor(70, 140);
    M5.Lcd.println("RESET!");
    delay(1000); // Show message for 1 second
}

void drawFlowDataTab() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    
    // Display header with tab indicator
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("Flow Data [1/2]");
    
    // Display flow rate (smoothed)
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.print("Flow: ");
    M5.Lcd.print(flowRate, 2);
    M5.Lcd.println(" L/min");
    
    // Display total volume
    M5.Lcd.setCursor(10, 80);
    M5.Lcd.print("Total: ");
    M5.Lcd.print(totalVolume, 3);
    M5.Lcd.println(" L");
    
    // Display pulse count for debugging
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.print("Pulses: ");
    M5.Lcd.println(totalPulseCount);
    
    // Display sensor status
    M5.Lcd.setCursor(10, 140);
    M5.Lcd.print("Sensor: ");
    M5.Lcd.setTextColor(flowError ? RED : GREEN);
    M5.Lcd.println(flowError ? "ERROR" : "OK");
    
    // Display main unit connection status (simplified)
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(10, 170);
    M5.Lcd.print("Main Unit: ");
    if (mainUnitIP.length() > 0) {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println("Connected");
    } else {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("Searching...");
    }
    
    // Display button instructions at bottom
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 215);
    M5.Lcd.println("A:Reset  B:Network  C:Tabs");
}

void drawNetworkInfoTab() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    
    // Display header with tab indicator
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("Network Info [2/2]");
    
    // Connection status
    M5.Lcd.setCursor(10, 50);
    if (WiFi.status() == WL_CONNECTED) {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println("WiFi: Connected");
    } else {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("WiFi: Disconnected");
    }
    
    // Network name
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 80);
    M5.Lcd.println("SSID: " + WiFi.SSID());
    
    // Connection mode
    M5.Lcd.setCursor(10, 100);
    if (WiFi.SSID() == "Joogimasin-AP") {
        M5.Lcd.setTextColor(YELLOW);
        M5.Lcd.println("Mode: Direct AP Connection");
    } else {
        M5.Lcd.setTextColor(CYAN);
        M5.Lcd.println("Mode: Network Discovery");
    }
    
    // IP addresses
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(10, 120);
    M5.Lcd.println("Flow Sensor IP:");
    M5.Lcd.setCursor(10, 140);
    M5.Lcd.println(WiFi.localIP().toString());
    
    M5.Lcd.setCursor(10, 160);
    M5.Lcd.println("Main Unit IP:");
    M5.Lcd.setCursor(10, 180);
    if (mainUnitIP.length() > 0) {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.println(mainUnitIP);
    } else {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("Not found");
    }
    
    // Signal strength (if available)
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(10, 200);
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        M5.Lcd.printf("Signal: %d dBm", rssi);
        if (rssi > -50) {
            M5.Lcd.setTextColor(GREEN);
            M5.Lcd.println(" (Excellent)");
        } else if (rssi > -70) {
            M5.Lcd.setTextColor(YELLOW);
            M5.Lcd.println(" (Good)");
        } else {
            M5.Lcd.setTextColor(RED);
            M5.Lcd.println(" (Weak)");
        }
    }
    
    // Display button instructions at bottom
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 215);
    M5.Lcd.println("A:Reset  B:Flow Data  C:Tabs");
}

void updateDisplay() {
    // Only update display every 500ms to prevent flicker
    if (millis() - lastDisplayUpdate < 500) {
        return;
    }
    lastDisplayUpdate = millis();
    
    if (currentTab == 0) {
        drawFlowDataTab();
    } else {
        drawNetworkInfoTab();
    }
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
    
    // Check for button presses
    if (M5.BtnA.wasPressed()) {
        Serial.println("Button A pressed - Resetting volume counter");
        resetVolumeCounter();
    }
    
    if (M5.BtnB.wasPressed()) {
        // Toggle between tabs
        currentTab = (currentTab == 0) ? 1 : 0;
        Serial.printf("Switched to tab %d\n", currentTab);
        // Force immediate display update
        lastDisplayUpdate = 0;
        updateDisplay();
    }
    
    if (M5.BtnC.wasPressed()) {
        // Alternative tab switching (same as Button B)
        currentTab = (currentTab == 0) ? 1 : 0;
        Serial.printf("Switched to tab %d\n", currentTab);
        // Force immediate display update
        lastDisplayUpdate = 0;
        updateDisplay();
    }
    
    // Check dispensing status for auto-reset
    checkDispensingStatus();
    
    // Update flow sensor readings
    updateFlowSensor();
    
    // Send data to main unit every 500ms (twice per second)
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime >= 500) {
        sendDataToMainUnit();
        lastSendTime = millis();
    }
    
    // Update display
    updateDisplay();
    
    delay(10);  // Small delay to prevent display flicker
}
