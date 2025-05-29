#include "WebServerManager.h"
#include "Flashlight.h"
#include "RelayController.h"
#include "FlowSensor.h"  // lisa FlowSensor

#include <ArduinoJson.h> // JSON raamatukogu

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>TARK-KRAAN</title>
    <style>
        body { font-family: Arial; text-align: center; padding: 50px; }
        button { padding: 15px 30px; font-size: 18px; margin: 10px; }
        .flow-info { font-size: 20px; margin-top: 20px; }
    </style>
</head>
<body>
    <h1>TARK-KRAAN</h1>
    <button onclick="toggle('flashlight')">Flashlight</button>
    <button onclick="toggle('relay1')">Relay 1</button>
    <button onclick="toggle('relay2')">Relay 2</button>
    <p>PIR Status: <span id="pir-status">Laadimine...</span></p>
    
    <div class="flow-info">
        <p>Vooluhulk (L/min): <span id="flow-rate">0.00</span></p>
        <p>Kogus (L): <span id="total-volume">0.000</span></p>
        <button onclick="resetVolume()">Reset kogus</button>
    </div>

<script>
setInterval(() => {
  fetch('/pir')
    .then(response => response.text())
    .then(state => {
      document.getElementById('pir-status').textContent = state;
    });

  fetch('/flow')
    .then(response => response.json())
    .then(data => {
      document.getElementById('flow-rate').textContent = data.flowRate.toFixed(2);
      document.getElementById('total-volume').textContent = data.totalVolume.toFixed(3);
    });
}, 1000); // uuendab iga sekund

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

WebServerManager::WebServerManager() 
    : flashlightState(nullptr), relay1State(nullptr), relay2State(nullptr), pirState(nullptr),
      flashlight(nullptr), relay(nullptr), flowSensor(nullptr)
{}

void WebServerManager::setStatePointers(bool* f, bool* r1, bool* r2, bool* pir) {
    flashlightState = f;
    relay1State = r1;
    relay2State = r2;
    pirState = pir;
}

void WebServerManager::setControllers(Flashlight* f, RelayController* r) {
    flashlight = f;
    relay = r;
}

void WebServerManager::setFlowSensor(FlowSensor* fs) {
    flowSensor = fs;
}

void WebServerManager::begin() {
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html, std::bind(&WebServerManager::processor, this, std::placeholders::_1));
    });

    // AJAX endpoint PIR oleku jaoks
    server.on("/pir", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String state = (pirState && *pirState) ? "ACTIVE" : "INACTIVE";
        request->send(200, "text/plain", state);
    });

    // AJAX vooluhulga info JSONina
    server.on("/flow", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (flowSensor == nullptr) {
            request->send(500, "text/plain", "Flow sensor not set");
            return;
        }

        StaticJsonDocument<200> doc;
        doc["flowRate"] = flowSensor->getFlowRate();
        doc["totalVolume"] = flowSensor->getTotalVolume();

        String jsonResponse;
        serializeJson(doc, jsonResponse);

        request->send(200, "application/json", jsonResponse);
    });

    // Reset vooluhulga kogus
    server.on("/flow/reset", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (flowSensor) {
            flowSensor->resetTotalVolume();
            request->send(200, "text/plain", "Total volume reset");
        } else {
            request->send(500, "text/plain", "Flow sensor not set");
        }
    });

    // Kontrollide toggle käsud
    server.on("/toggle", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (!request->hasParam("device")) {
            request->send(400, "text/plain", "Missing device parameter");
            return;
        }

        String device = request->getParam("device")->value();
        String response = "Unknown device";

        if (device == "flashlight" && flashlightState && flashlight) {
            *flashlightState = !(*flashlightState);
            flashlight->set(*flashlightState);
            response = *flashlightState ? "Flashlight ON" : "Flashlight OFF";
        }
        else if (device == "relay1" && relay1State && relay) {
            *relay1State = !(*relay1State);
            relay->setRelay1(*relay1State);
            response = *relay1State ? "Relay 1 ON" : "Relay 1 OFF";
        }
        else if (device == "relay2" && relay2State && relay) {
            *relay2State = !(*relay2State);
            relay->setRelay2(*relay2State);
            response = *relay2State ? "Relay 2 ON" : "Relay 2 OFF";
        }

        request->send(200, "text/plain", response);
    });

    server.begin();
}

String WebServerManager::processor(const String& var) {
    if (var == "FLASHLIGHT_STATE" && flashlightState) {
        return *flashlightState ? "ON" : "OFF";
    } else if (var == "RELAY1_STATE" && relay1State) {
        return *relay1State ? "ON" : "OFF";
    } else if (var == "RELAY2_STATE" && relay2State) {
        return *relay2State ? "ON" : "OFF";
    } else if (var == "PIR_STATE" && pirState) {
        return *pirState ? "ACTIVE" : "INACTIVE";
    }
    return String();
}
