#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiMulti.h>

extern bool flashlightState;
extern bool relay1State;
extern bool relay2State;
extern bool pirTriggered;
extern unsigned long pirTimeout;
extern volatile unsigned long pulseCount;

extern WiFiMulti wifiMulti;

AsyncWebServer server(80);

const char* ssid = "Illuminaty";
const char* password = "S330nm1nuk0du!";

const char* PARAM_INPUT_1 = "state";
const char* PARAM_INPUT_2 = "timeout";

void setupWiFi() {
    wifiMulti.addAP(ssid, password);
    while (wifiMulti.run() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nConnected to the WiFi network");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

String processor(const String& var) {
    if (var == "FLASHLIGHT_STATE") {
        return flashlightState ? "ON" : "OFF";
    } else if (var == "RELAY1_STATE") {
        return relay1State ? "ON" : "OFF";
    } else if (var == "RELAY2_STATE") {
        return relay2State ? "ON" : "OFF";
    } else if (var == "PIR_STATE") {
        return pirTriggered ? "ON" : "OFF";
    } else if (var == "PIR_TIMEOUT") {
        return String(pirTimeout);
    } else if (var == "FLOW_RATE") {
        return String(pulseCount) + " pulses/sec";
    }
    return String();
}

void setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>TARK-KRAAN</title>
    <style>
        body {
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100vh;
            margin: 0;
            background-color: #f4f4f9;
            font-family: Arial, sans-serif;
        }
        h1 {
            margin-bottom: 20px;
        }
        .button {
            width: 200px;
            padding: 10px;
            margin: 10px;
            border: none;
            border-radius: 25px;
            color: white;
            font-size: 18px;
            cursor: pointer;
        }
        .button-on {
            background-color: red;
        }
        .button-off {
            background-color: green;
        }
    </style>
</head>
<body>
    <h1>TARK-KRAAN</h1>
    <button id="flashlightBtn" class="button" onclick="toggleFlashlight()">Flashlight</button>
    <button id="relay1Btn" class="button" onclick="toggleRelay1()">Relay 1</button>
    <button id="relay2Btn" class="button" onclick="toggleRelay2()">Relay 2</button>
    <p>PIR Timeout: <span id="pirTimeout">%PIR_TIMEOUT%</span> ms</p>
    <input type="range" min="5000" max="30000" step="1000" value="%PIR_TIMEOUT%" onchange="updatePIRTimeout(this.value)">
    <p>Flow Rate: <span id="flowRate">%FLOW_RATE%</span></p>
    <p>PIR State: <span id="pirState">%PIR_STATE%</span></p>
    <script>
        async function toggleFlashlight() {
            const response = await fetch("/toggleFlashlight");
            const newState = await response.text();
            updateButtonState('flashlightBtn', newState);
            // Update M5Core2 display
            await fetch("/syncFlashlightState", {
                method: "POST",
                headers: {
                    "Content-Type": "application/x-www-form-urlencoded"
                },
                body: "state=" + newState
            });
        }

        async function toggleRelay1() {
            const response = await fetch("/toggleRelay1");
            const newState = await response.text();
            updateButtonState('relay1Btn', newState);
            // Update M5Core2 display
            await fetch("/syncRelay1State", {
                method: "POST",
                headers: {
                    "Content-Type": "application/x-www-form-urlencoded"
                },
                body: "state=" + newState
            });
        }

        async function toggleRelay2() {
            const response = await fetch("/toggleRelay2");
            const newState = await response.text();
            updateButtonState('relay2Btn', newState);
            // Update M5Core2 display
            await fetch("/syncRelay2State", {
                method: "POST",
                headers: {
                    "Content-Type": "application/x-www-form-urlencoded"
                },
                body: "state=" + newState
            });
        }

        function updateButtonState(buttonId, state) {
            const button = document.getElementById(buttonId);
            if (state === "ON") {
                button.classList.remove('button-off');
                button.classList.add('button-on');
                button.innerText = "ON";
            } else {
                button.classList.remove('button-on');
                button.classList.add('button-off');
                button.innerText = "OFF";
            }
        }

        async function updatePIRTimeout(value) {
            const response = await fetch("/setPIRTimeout?timeout=" + value);
            const newTimeout = await response.text();
            document.getElementById("pirTimeout").innerText = newTimeout;
        }

        setInterval(async () => {
            const response = await fetch("/flowRate");
            const flowRate = await response.text();
            document.getElementById("flowRate").innerText = flowRate;

            const pirState = await fetch("/pirState");
            const pirStatus = await pirState.text();
            document.getElementById("pirState").innerText = pirStatus;
        }, 1000);

        // Initial button state update
        document.addEventListener('DOMContentLoaded', async () => {
            const flashlightState = await fetch("/flashlightState").then(response => response.text());
            updateButtonState('flashlightBtn', flashlightState);

            const relay1State = await fetch("/relay1State").then(response => response.text());
            updateButtonState('relay1Btn', relay1State);

            const relay2State = await fetch("/relay2State").then(response => response.text());
            updateButtonState('relay2Btn', relay2State);

            const pirState = await fetch("/pirState").then(response => response.text());
            document.getElementById("pirState").innerText = pirState;
        });
    </script>
</body>
</html>
)rawliteral", processor);
    });

    server.on("/toggleFlashlight", HTTP_GET, [](AsyncWebServerRequest *request) {
        flashlightState = !flashlightState;
        unit_flash_set_brightness(flashlightState);
        request->send(200, "text/plain", flashlightState ? "ON" : "OFF");
    });

    server.on("/toggleRelay1", HTTP_GET, [](AsyncWebServerRequest *request) {
        relay1State = !relay1State;
        toggleRelay(RELAY_CHANNEL, RELAY_INDEX1, relay1State);
        request->send(200, "text/plain", relay1State ? "ON" : "OFF");
    });

    server.on("/toggleRelay2", HTTP_GET, [](AsyncWebServerRequest *request) {
        relay2State = !relay2State;
        toggleRelay(RELAY_CHANNEL, RELAY_INDEX2, relay2State);
        request->send(200, "text/plain", relay2State ? "ON" : "OFF");
    });

    server.on("/setPIRTimeout", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam(PARAM_INPUT_2)) {
            pirTimeout = request->getParam(PARAM_INPUT_2)->value().toInt();
        }
        request->send(200, "text/plain", String(pirTimeout));
    });

    server.on("/flowRate", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", String(pulseCount) + " pulses/sec");
    });

    server.on("/pirState", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", pirTriggered ? "ON" : "OFF");
    });

    // Sync state from M5Core2
    server.on("/syncFlashlightState", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam(PARAM_INPUT_1, true)) {
            String state = request->getParam(PARAM_INPUT_1, true)->value();
            flashlightState = (state == "ON");
            unit_flash_set_brightness(flashlightState);
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/syncRelay1State", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam(PARAM_INPUT_1, true)) {
            String state = request->getParam(PARAM_INPUT_1, true)->value();
            relay1State = (state == "ON");
            toggleRelay(RELAY_CHANNEL, RELAY_INDEX1, relay1State);
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/syncRelay2State", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam(PARAM_INPUT_1, true)) {
            String state = request->getParam(PARAM_INPUT_1, true)->value();
            relay2State = (state == "ON");
            toggleRelay(RELAY_CHANNEL, RELAY_INDEX2, relay2State);
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/syncPirState", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam(PARAM_INPUT_1, true)) {
            String state = request->getParam(PARAM_INPUT_1, true)->value();
            pirTriggered = (state == "ON");
        }
        request->send(200, "text/plain", "OK");
    });

    server.begin();
}

#endif // WEBSERVER_H
