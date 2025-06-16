#include <M5Core2.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Flow sensor pin
#define FLOW_SENSOR_PIN 32  // Port A on M5Stack Core 2

// Flow sensor specifications
#define FLOW_SENSOR_PULSES_PER_LITER 450  // YF-S402 typically has 450 pulses per liter
#define FLOW_SENSOR_UPDATE_INTERVAL 1000  // Update every second

// WiFi settings
const char* ssid = "Illuminaty";
const char* password = "S330nm1nuk0du!";

// Main unit's IP address
const char* mainUnitIP = "192.168.88.188";  // Main unit's IP address

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
}

void connectWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
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
        // Calculate flow rate in L/min
        flowRate = (pulseCount * 60.0f) / (FLOW_SENSOR_PULSES_PER_LITER * (currentTime - lastUpdateTime) / 1000.0f);
        
        // Calculate volume in liters: pulses / pulses_per_liter
        // Example: 450 pulses / 450 pulses_per_liter = 1 liter
        totalVolume += (float)pulseCount / FLOW_SENSOR_PULSES_PER_LITER;
        
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

    HTTPClient http;
    String url = "http://" + String(mainUnitIP) + "/flow/update";
    
    // Create JSON document
    StaticJsonDocument<200> doc;
    doc["flowRate"] = flowRate;
    doc["totalVolume"] = totalVolume;
    doc["error"] = flowError;
    
    String jsonData;
    serializeJson(doc, jsonData);
    
    // Send data to main unit
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(jsonData);
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            Serial.println("Data sent successfully");
        } else {
            Serial.printf("HTTP error: %d\n", httpCode);
        }
    } else {
        Serial.printf("HTTP request failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
}

void updateDisplay() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    
    // Display flow rate
    M5.Lcd.setCursor(10, 30);
    M5.Lcd.print("Flow Rate: ");
    M5.Lcd.print(flowRate, 2);
    M5.Lcd.println(" L/min");
    
    // Display total volume
    M5.Lcd.setCursor(10, 70);
    M5.Lcd.print("Total: ");
    M5.Lcd.print(totalVolume, 2);
    M5.Lcd.println(" L");
    
    // Display status
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.print("Status: ");
    M5.Lcd.setTextColor(flowError ? RED : GREEN);
    M5.Lcd.println(flowError ? "ERROR" : "OK");
    
    // Display IP address
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(10, 150);
    M5.Lcd.print("IP: ");
    M5.Lcd.println(WiFi.localIP());
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
