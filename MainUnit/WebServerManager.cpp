#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "WebServerManager.h"

// HTML page template
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>Joogimasin</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; text-align: center; margin: 20px; }
        .button { 
            background-color: #4CAF50;
            border: none;
            color: white;
            padding: 15px 32px;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: 16px;
            margin: 4px 2px;
            cursor: pointer;
            border-radius: 4px;
        }
        .button:disabled {
            background-color: #cccccc;
            cursor: not-allowed;
        }
        .status {
            margin: 20px;
            padding: 10px;
            border-radius: 4px;
        }
        .active { background-color: #dff0d8; }
        .inactive { background-color: #f2dede; }
    </style>
</head>
<body>
    <h1>Joogimasin</h1>
    
    <div class="status" id="pirStatus">
        PIR: <span id="pirValue">Checking...</span>
    </div>
    
    <div class="status" id="flowStatus">
        Flow Rate: <span id="flowRate">0.00</span> L/min<br>
        Total Volume: <span id="totalVolume">0.00</span> L
    </div>
    
    <button class="button" onclick="toggleDevice('relay1')" id="relay1Btn">Relay 1</button>
    <button class="button" onclick="toggleDevice('relay2')" id="relay2Btn">Relay 2</button>
    <button class="button" onclick="toggleDevice('flashlight')" id="flashlightBtn">Flashlight</button>
    <button class="button" onclick="resetFlow()">Reset Flow</button>

    <script>
        function updateStatus() {
            fetch('/pir')
                .then(response => response.text())
                .then(state => {
                    document.getElementById('pirValue').textContent = state;
                    document.getElementById('pirStatus').className = 
                        'status ' + (state === 'ACTIVE' ? 'active' : 'inactive');
                });
            
            fetch('/flow')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('flowRate').textContent = data.flowRate.toFixed(2);
                    document.getElementById('totalVolume').textContent = data.totalVolume.toFixed(2);
                    document.getElementById('flowStatus').className = 
                        'status ' + (data.error ? 'inactive' : 'active');
                });
        }

        function toggleDevice(device) {
            fetch('/toggle?device=' + device, { method: 'POST' })
                .then(response => response.text())
                .then(state => {
                    const btn = document.getElementById(device + 'Btn');
                    btn.style.backgroundColor = state === 'true' ? '#4CAF50' : '#f44336';
                });
        }

        function resetFlow() {
            fetch('/flow/reset', { method: 'POST' });
        }

        // Update status every second
        setInterval(updateStatus, 1000);
        updateStatus();
    </script>
</body>
</html>
)rawliteral";

WebServerManager::WebServerManager(PIRSensor* pir, RelayController* relay)
    : server(80), pirSensor(pir), relayController(relay), flowRate(0), totalVolume(0), flowError(false) {
}

void WebServerManager::begin() {
    setupRoutes();
    server.begin();
}

void WebServerManager::update() {
    // Nothing to update in the main loop
}

void WebServerManager::setupRoutes() {
    // Serve the main page
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });

    // PIR sensor status endpoint
    server.on("/pir", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", pirSensor->isTriggered() ? "ACTIVE" : "INACTIVE");
    });

    // Flow sensor data endpoint
    server.on("/flow", HTTP_GET, [this](AsyncWebServerRequest *request) {
        StaticJsonDocument<200> doc;
        doc["flowRate"] = flowRate;
        doc["totalVolume"] = totalVolume;
        doc["error"] = flowError;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Flow sensor update endpoint (for FlowSensorUnit)
    server.on("/flow/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("data", true)) {
            String data = request->getParam("data", true)->value();
            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, data);
            
            if (!error) {
                flowRate = doc["flowRate"] | 0.0f;
                totalVolume = doc["totalVolume"] | 0.0f;
                flowError = doc["error"] | false;
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Invalid JSON");
            }
        } else {
            request->send(400, "text/plain", "No data");
        }
    });

    // Toggle device endpoint
    server.on("/toggle", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("device")) {
            String device = request->getParam("device")->value();
            bool newState = false;
            
            if (device == "relay1") {
                newState = !relayController->getRelay1();
                relayController->setRelay1(newState);
            } else if (device == "relay2") {
                newState = !relayController->getRelay2();
                relayController->setRelay2(newState);
            }
            
            request->send(200, "text/plain", newState ? "true" : "false");
        } else {
            request->send(400, "text/plain", "No device specified");
        }
    });

    // Reset flow volume endpoint
    server.on("/flow/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        totalVolume = 0;
        request->send(200, "text/plain", "OK");
    });
}
