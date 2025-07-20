#include <M5Core2.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "Flashlight.h"
#include "RelayController.h"
#include "PIRSensor.h"
#include "WebServerManager.h"
#include "TouchController.h"

// First M5Stack Core 2 (Main unit with PbHub)
M5UnitPbHub pbhub;
WiFiMulti wifiMulti;
AsyncWebServer server(80);

Flashlight flashlight(&pbhub);
RelayController relay(&pbhub);
PIRSensor pir(&pbhub);
WebServerManager webServer(&pir, &relay);
TouchController touch(&flashlight, &relay);

// Flow sensor data from second unit
struct FlowSensorData {
    float flowRate = 0.0f;
    float totalVolume = 0.0f;
    bool error = false;
} flowData;

static bool pirStateForWeb = false;
static int currentTab = 0;  // 0: Main, 1: PIR, 2: Flow Sensor
static bool lastPirState = false;

// HTML page with combined information
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>TARK-KRAAN</title>
    <style>
        body { font-family: Arial; text-align: center; padding: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        .section { margin: 20px 0; padding: 20px; border: 1px solid #ccc; border-radius: 5px; }
        button { padding: 15px 30px; font-size: 18px; margin: 10px; }
        .status { font-size: 20px; margin: 10px 0; }
        .flow-info { font-size: 20px; margin-top: 20px; }
        .error { color: red; }
        .success { color: green; }
    </style>
</head>
<body>
    <div class="container">
        <h1>TARK-KRAAN</h1>
        
        <div class="section">
            <h2>Control Panel</h2>
            <button onclick="toggle('flashlight')">Flashlight</button>
            <button onclick="toggle('relay1')">Relay 1</button>
            <button onclick="toggle('relay2')">Relay 2</button>
        </div>

        <div class="section">
            <h2>PIR Sensor</h2>
            <p>Status: <span id="pir-status" class="status">Loading...</span></p>
        </div>

        <div class="section">
            <h2>Flow Sensor</h2>
            <p>Flow Rate: <span id="flow-rate" class="status">0.00</span> L/min</p>
            <p>Total Volume: <span id="total-volume" class="status">0.000</span> L</p>
            <p>Status: <span id="flow-status" class="status">Loading...</span></p>
            <button onclick="resetVolume()">Reset Volume</button>
        </div>
    </div>

<script>
// Update all data every second
setInterval(() => {
    // Update PIR status
    fetch('/pir')
        .then(response => response.text())
        .then(state => {
            document.getElementById('pir-status').textContent = state;
            document.getElementById('pir-status').className = 
                state === 'ACTIVE' ? 'status success' : 'status';
        });

    // Update flow sensor data
    fetch('/flow')
        .then(response => response.json())
        .then(data => {
            document.getElementById('flow-rate').textContent = 
                data.flowRate.toFixed(2);
            document.getElementById('total-volume').textContent = 
                data.totalVolume.toFixed(3);
            document.getElementById('flow-status').textContent = 
                data.error ? 'ERROR' : 'OK';
            document.getElementById('flow-status').className = 
                data.error ? 'status error' : 'status success';
        });
}, 1000);

function toggle(device) {
    fetch('/toggle?device=' + device)
        .then(res => res.text())
        .then(alert);
}

function resetVolume() {
    fetch('/flow/reset')
        .then(res => res.text())
        .then(alert);
}
</script>
</body>
</html>
)rawliteral";

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
    Serial.println("M5Core2 initializing...");

    // Initialize I2C for PbHub on Port A
    Wire.begin(32, 33);  // SDA: GPIO32, SCL: GPIO33 for Port A
    
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

    // Initialize components
    flashlight.init();
    relay.setRelay1(false);
    relay.setRelay2(false);
    pir.init();
    touch.init();

    connectWiFi();

    // Set up web server routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });

    // PIR status endpoint
    server.on("/pir", HTTP_GET, [](AsyncWebServerRequest *request) {
        String state = pirStateForWeb ? "ACTIVE" : "INACTIVE";
        request->send(200, "text/plain", state);
    });

    // Flow sensor data endpoint
    server.on("/flow", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<200> doc;
        doc["flowRate"] = flowData.flowRate;
        doc["totalVolume"] = flowData.totalVolume;
        doc["error"] = flowData.error;
        
        String jsonResponse;
        serializeJson(doc, jsonResponse);
        request->send(200, "application/json", jsonResponse);
    });

    // Endpoint to receive data from flow sensor unit
    server.on("/flow/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "OK");
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, (char*)data);
        
        if (!error) {
            flowData.flowRate = doc["flowRate"] | 0.0f;
            flowData.totalVolume = doc["totalVolume"] | 0.0f;
            flowData.error = doc["error"] | false;
        }
    });

    // Reset volume endpoint
    server.on("/flow/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        flowData.totalVolume = 0.0f;
        request->send(200, "text/plain", "Volume reset");
    });

    // Control endpoints
    server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("device")) {
            request->send(400, "text/plain", "Missing device parameter");
            return;
        }

        String device = request->getParam("device")->value();
        String response = "Unknown device";

        if (device == "flashlight") {
            flashlight.state = !flashlight.state;
            flashlight.set(flashlight.state);
            response = flashlight.state ? "Flashlight ON" : "Flashlight OFF";
        }
        else if (device == "relay1") {
            relay.relay1 = !relay.relay1;
            relay.setRelay1(relay.relay1);
            response = relay.relay1 ? "Relay 1 ON" : "Relay 1 OFF";
        }
        else if (device == "relay2") {
            relay.relay2 = !relay.relay2;
            relay.setRelay2(relay.relay2);
            response = relay.relay2 ? "Relay 2 ON" : "Relay 2 OFF";
        }

        request->send(200, "text/plain", response);
    });

    server.begin();
    Serial.println("Web server started");

    // Draw initial UI
    touch.drawUI(false);
}

void loop() {
    M5.update();

    // Check PIR sensor and handle flashlight
    pir.check(flashlight.state, [](bool on) {
        flashlight.set(on);
    });

    pirStateForWeb = pir.isTriggered();
    
    // Update components
    touch.update();

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
        currentTab = 2;  // Flow Sensor tab
        drawFlowSensorTab();
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
            updateFlowSensorTab();
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
        M5.Lcd.println("OK");
    }
    M5.Lcd.setTextColor(WHITE);
}
