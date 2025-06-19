#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>
#include "WebServerManager.h"

// Minimal HTML page to prevent memory issues
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head><title>Joogimasin</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{font-family:Arial;text-align:center;margin:20px;background:#f0f0f0}
.container{max-width:600px;margin:0 auto;background:white;padding:15px;border-radius:5px}
.button{background:#4CAF50;border:none;color:white;padding:10px 20px;margin:5px;cursor:pointer;border-radius:3px}
.wifi-button{background:#2196F3}
.status{margin:10px;padding:10px;border-radius:5px}
.active{background:#dff0d8;color:#3c763d}
.inactive{background:#f2dede;color:#a94442}
.section{margin:15px 0;padding:15px;border:1px solid #ddd;border-radius:5px;background:#f9f9f9}
</style></head><body>
<div class="container">
<h1>Joogimasin Control Panel</h1>
<div class="section">
<h3>WiFi Management</h3>
<button class="button wifi-button" onclick="location.href='/wifi'">WiFi Settings</button>
<div id="currentWiFi"></div>
</div>
<div class="section">
<h3>Device Control</h3>
<button class="button" onclick="toggleDevice('relay1')">Relay 1</button>
<button class="button" onclick="toggleDevice('relay2')">Relay 2</button>
<button class="button" onclick="toggleDevice('flashlight')">Flashlight</button>
</div>
<div class="section">
<h3>PIR Sensor</h3>
<div class="status" id="pirStatus">Motion: <span id="pirValue">Checking...</span></div>
</div>
<div class="section">
<h3>Flow Sensor</h3>
<div class="status" id="flowStatus">
<div>Flow Rate: <span id="flowRate">0.00</span> L/min</div>
<div>Total Volume: <span id="totalVolume">0.00</span> L</div>
</div>
<button class="button" onclick="resetFlow()">Reset Volume</button>
</div>
</div>
<script>
function updateStatus(){
fetch('/wifi/status').then(r=>r.json()).then(d=>{
document.getElementById('currentWiFi').textContent=d.connected?'Connected: '+d.ssid:'Not connected';
});
fetch('/pir').then(r=>r.text()).then(s=>{
document.getElementById('pirValue').textContent=s;
document.getElementById('pirStatus').className='status '+(s==='ACTIVE'?'active':'inactive');
});
fetch('/flow').then(r=>r.json()).then(d=>{
document.getElementById('flowRate').textContent=d.flowRate.toFixed(2);
document.getElementById('totalVolume').textContent=d.totalVolume.toFixed(2);
document.getElementById('flowStatus').className='status '+(d.error?'inactive':'active');
});
}
function toggleDevice(device){
fetch('/toggle?device='+device).then(r=>r.text()).then(result=>alert(result));
}
function resetFlow(){
fetch('/flow/reset').then(r=>r.text()).then(result=>alert('Success: '+result));
}
setInterval(updateStatus,10000);updateStatus();
</script></body></html>
)rawliteral";

WebServerManager::WebServerManager(PIRSensor* pir, RelayController* relay)
    : server(80), pirSensor(pir), relayController(relay), wifiMultiPtr(nullptr), flowRate(0), totalVolume(0), flowError(false) {
}

void WebServerManager::updateFlowData(float newFlowRate, float newTotalVolume, bool newError) {
    flowRate = newFlowRate;
    totalVolume = newTotalVolume;
    flowError = newError;
}

void WebServerManager::updatePIRStatus(bool isTriggered) {
    // PIR status is read directly from pirSensor, so this is for future use
}



void WebServerManager::begin() {
    setupRoutes();
    server.begin();
}

void WebServerManager::update() {
    // Nothing to update in the main loop
}

void WebServerManager::setWiFiMulti(WiFiMulti* wifiMulti) {
    wifiMultiPtr = wifiMulti;
}

// Removed scanNetworks() function to prevent memory crashes

bool WebServerManager::connectToNetwork(String ssid, String password) {
    // Store credentials in preferences
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("password", password);
    prefs.end();
    
    // Disconnect current connection
    WiFi.disconnect();
    yield(); // Prevent watchdog reset
    delay(500); // Reduced delay
    
    // Try to connect to new network
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // Wait up to 8 seconds for connection with yield calls
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 16) {
        yield(); // Prevent watchdog reset
        delay(500);
        attempts++;
    }
    
    return WiFi.status() == WL_CONNECTED;
}

void WebServerManager::setupRoutes() {
    // Serve the main page with chunked response to prevent memory issues
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        // Check available memory before serving large HTML
        size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 15000) { // Less than 15KB free
            request->send(503, "text/plain", "Service temporarily unavailable - low memory");
            return;
        }
        
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html);
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
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

    // Flow sensor update endpoint is handled in main Arduino file to avoid conflicts

    // Toggle device endpoint - will be handled externally
    // This endpoint is handled in the main Arduino file

    // Reset flow volume endpoint
    server.on("/flow/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        totalVolume = 0;
        request->send(200, "text/plain", "OK");
    });

    // Simple WiFi settings page
    server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String html = "<!DOCTYPE HTML><html><head><title>WiFi Settings</title>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>body{font-family:Arial;margin:20px;background:#f0f0f0}";
        html += ".container{max-width:400px;margin:0 auto;background:white;padding:20px;border-radius:5px}";
        html += ".button{background:#4CAF50;border:none;color:white;padding:10px 20px;margin:5px;cursor:pointer;border-radius:3px;width:100%}";
        html += "input{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:3px;box-sizing:border-box}";
        html += "</style></head><body><div class='container'>";
        html += "<h2>WiFi Configuration</h2>";
        
        if (WiFi.status() == WL_CONNECTED) {
            html += "<p><strong>Current Network:</strong> " + WiFi.SSID() + "</p>";
            html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
            html += "<p><strong>Signal:</strong> " + String(WiFi.RSSI()) + " dBm</p>";
        } else {
            html += "<p><strong>Status:</strong> Not connected</p>";
        }
        
        html += "<form action='/wifi/connect' method='post'>";
        html += "<h3>Connect to Network</h3>";
        html += "<input type='text' name='ssid' placeholder='Network Name (SSID)' required>";
        html += "<input type='password' name='password' placeholder='Password'>";
        html += "<button type='submit' class='button'>Connect</button>";
        html += "</form>";
        html += "<button class='button' onclick='location.href=\"/\"' style='background:#666;margin-top:20px'>Back to Main</button>";
        html += "</div></body></html>";
        
        request->send(200, "text/html", html);
    });

    server.on("/wifi/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        try {
            StaticJsonDocument<128> doc; // Smaller buffer
            doc["connected"] = WiFi.status() == WL_CONNECTED;
            
            if (WiFi.status() == WL_CONNECTED) {
                String ssid = WiFi.SSID();
                String ip = WiFi.localIP().toString();
                
                if (ssid.length() > 0) doc["ssid"] = ssid;
                if (ip.length() > 0) doc["ip"] = ip;
                doc["rssi"] = WiFi.RSSI();
            }
            
            String response;
            response.reserve(100); // Pre-allocate
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        } catch (...) {
            request->send(500, "application/json", "{\"connected\":false}");
        }
    });

    server.on("/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String password = request->getParam("password", true)->value();
            
            bool success = connectToNetwork(ssid, password);
            
            String html = "<!DOCTYPE HTML><html><head><title>WiFi Connection</title>";
            html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
            html += "<style>body{font-family:Arial;margin:20px;text-align:center;background:#f0f0f0}";
            html += ".container{max-width:400px;margin:0 auto;background:white;padding:20px;border-radius:5px}";
            html += ".button{background:#4CAF50;border:none;color:white;padding:10px 20px;margin:10px;cursor:pointer;border-radius:3px;text-decoration:none;display:inline-block}";
            html += ".success{background:#dff0d8;color:#3c763d;padding:15px;border-radius:5px;margin:20px 0}";
            html += ".error{background:#f2dede;color:#a94442;padding:15px;border-radius:5px;margin:20px 0}";
            html += "</style></head><body><div class='container'>";
            html += "<h2>WiFi Connection Result</h2>";
            
            if (success) {
                html += "<div class='success'>";
                html += "<h3>Connected Successfully!</h3>";
                html += "<p>Network: " + ssid + "</p>";
                html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
                html += "</div>";
            } else {
                html += "<div class='error'>";
                html += "<h3>Connection Failed</h3>";
                html += "<p>Could not connect to: " + ssid + "</p>";
                html += "<p>Please check your credentials and try again.</p>";
                html += "</div>";
            }
            
            html += "<a href='/wifi' class='button'>Back to WiFi Settings</a>";
            html += "<a href='/' class='button' style='background:#666'>Main Menu</a>";
            html += "</div></body></html>";
            
            request->send(200, "text/html", html);
        } else {
            request->send(400, "text/html", "<html><body><h2>Error: Missing parameters</h2><a href='/wifi'>Back</a></body></html>");
        }
    });
}
