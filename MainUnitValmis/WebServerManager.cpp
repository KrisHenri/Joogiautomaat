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
.container{max-width:800px;margin:0 auto;background:white;padding:15px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
.button{background:#4CAF50;border:none;color:white;padding:10px 20px;margin:5px;cursor:pointer;border-radius:5px;font-size:14px;transition:all 0.3s;box-shadow:0 2px 4px rgba(0,0,0,0.2)}
.button:hover{background:#45a049;transform:translateY(-1px);box-shadow:0 4px 8px rgba(0,0,0,0.3)}
.button:disabled{background:#ccc;cursor:not-allowed;transform:none;box-shadow:none}
.button.stop{background:#f44336}
.button.stop:hover{background:#da190b}
.button.preset{background:#2196F3;font-size:12px;padding:8px 12px}
.button.preset:hover{background:#1976D2}
.wifi-button{background:#2196F3}
.status{margin:10px;padding:10px;border-radius:5px}
.active{background:#dff0d8;color:#3c763d}
.inactive{background:#f2dede;color:#a94442}
.section{margin:15px 0;padding:20px;border:1px solid #ddd;border-radius:8px;background:#f9f9f9;box-shadow:0 1px 3px rgba(0,0,0,0.1)}
.pump-container{background:white;padding:15px;border-radius:8px;border:1px solid #e0e0e0;box-shadow:0 2px 4px rgba(0,0,0,0.05)}
.progress-container{background:#e0e0e0;height:25px;margin:10px 0;border-radius:12px;overflow:hidden;box-shadow:inset 0 2px 4px rgba(0,0,0,0.1)}
.progress-bar{height:100%;background:linear-gradient(90deg,#4CAF50,#45a049);border-radius:12px;transition:width 0.2s ease-out;position:relative}
.progress-bar.completed{background:linear-gradient(90deg,#2196F3,#1976D2)}
.progress-bar.error{background:linear-gradient(90deg,#f44336,#da190b)}
.volume-input{width:100%;padding:10px;margin:8px 0;border:2px solid #ddd;border-radius:5px;font-size:16px;text-align:center}
.volume-input:focus{border-color:#4CAF50;outline:none}
.status-text{font-size:14px;margin:8px 0;padding:8px;background:#f5f5f5;border-radius:5px;border-left:4px solid #4CAF50}
.status-text.dispensing{border-left-color:#FF9800;background:#fff3e0}
.status-text.completed{border-left-color:#2196F3;background:#e3f2fd}
.status-text.error{border-left-color:#f44336;background:#ffebee}
.pump-title{color:#333;margin-bottom:15px;font-size:18px;font-weight:bold}
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
<button class="button" onclick="toggleDevice('relay1')">Jook 1</button>
<button class="button" onclick="toggleDevice('relay2')">Jook 2</button>
<button class="button" onclick="toggleDevice('flashlight')">Flashlight</button>
</div>
<div class="section">
<h3>Volume Dispensing System</h3>
<div style="display:flex;gap:20px;flex-wrap:wrap;justify-content:center">
<div class="pump-container" style="flex:1;min-width:300px;max-width:400px">
<h4 class="pump-title">Jook 1</h4>
<input type="number" id="volume1" class="volume-input" placeholder="Volume (L)" step="0.01" min="0.01" max="10" value="0.5">
<div style="display:flex;gap:8px;margin:10px 0">
<button class="button preset" onclick="setVolume(1,0.25)" style="flex:1">250ml</button>
<button class="button preset" onclick="setVolume(1,0.5)" style="flex:1">500ml</button>
<button class="button preset" onclick="setVolume(1,1.0)" style="flex:1">1L</button>
<button class="button preset" onclick="setVolume(1,2.0)" style="flex:1">2L</button>
</div>
<div class="progress-container">
<div id="progress1" class="progress-bar" style="width:0%">
<span id="progressText1" style="position:absolute;width:100%;text-align:center;line-height:25px;color:white;font-weight:bold;text-shadow:1px 1px 2px rgba(0,0,0,0.5)"></span>
</div>
</div>
<div id="status1" class="status-text">Ready: 0.000L / 0.000L</div>
<div style="display:flex;gap:8px;margin-top:15px">
<button class="button" onclick="startDispensing(1)" id="start1" style="flex:2">Start Dispensing</button>
<button class="button stop" onclick="stopDispensing(1)" id="stop1" style="flex:1">Stop</button>
<button class="button" onclick="pauseDispensing(1)" id="pause1" style="flex:1;background:#FF9800">Pause</button>
</div>
<div id="eta1" style="font-size:12px;color:#666;margin-top:8px;text-align:center"></div>
</div>
<div class="pump-container" style="flex:1;min-width:300px;max-width:400px">
<h4 class="pump-title">Jook 2</h4>
<input type="number" id="volume2" class="volume-input" placeholder="Volume (L)" step="0.01" min="0.01" max="10" value="0.5">
<div style="display:flex;gap:8px;margin:10px 0">
<button class="button preset" onclick="setVolume(2,0.25)" style="flex:1">250ml</button>
<button class="button preset" onclick="setVolume(2,0.5)" style="flex:1">500ml</button>
<button class="button preset" onclick="setVolume(2,1.0)" style="flex:1">1L</button>
<button class="button preset" onclick="setVolume(2,2.0)" style="flex:1">2L</button>
</div>
<div class="progress-container">
<div id="progress2" class="progress-bar" style="width:0%">
<span id="progressText2" style="position:absolute;width:100%;text-align:center;line-height:25px;color:white;font-weight:bold;text-shadow:1px 1px 2px rgba(0,0,0,0.5)"></span>
</div>
</div>
<div id="status2" class="status-text">Ready: 0.000L / 0.000L</div>
<div style="display:flex;gap:8px;margin-top:15px">
<button class="button" onclick="startDispensing(2)" id="start2" style="flex:2">Start Dispensing</button>
<button class="button stop" onclick="stopDispensing(2)" id="stop2" style="flex:1">Stop</button>
<button class="button" onclick="pauseDispensing(2)" id="pause2" style="flex:1;background:#FF9800">Pause</button>
</div>
<div id="eta2" style="font-size:12px;color:#666;margin-top:8px;text-align:center"></div>
</div>
</div>
<div style="text-align:center;margin-top:20px">
<button class="button stop" onclick="emergencyStop()" style="background:#d32f2f;font-size:16px;padding:12px 24px">EMERGENCY STOP ALL</button>
</div>
</div>
<div class="section">
<h3>PIR Sensor</h3>
<div class="status" id="pirStatus">Motion: <span id="pirValue">Checking...</span></div>
</div>
<div class="section">
<h3>Flow Sensors</h3>
<div style="display:flex;gap:20px;flex-wrap:wrap;justify-content:center">
<div class="pump-container" style="flex:1;min-width:300px;max-width:400px">
<h4 class="pump-title">Flow Sensor 1</h4>
<div class="status" id="flowStatus">
<div>Flow Rate: <span id="flowRate">0.00</span> L/min</div>
<div>Total Volume: <span id="totalVolume">0.00</span> L</div>
<div>Pulse Count: <span id="pulseCount">0</span></div>
</div>
<button class="button" onclick="resetFlow()">Reset Volume</button>
</div>
<div class="pump-container" style="flex:1;min-width:300px;max-width:400px">
<h4 class="pump-title">Flow Sensor 2</h4>
<div class="status" id="flow2Status">
<div>Flow Rate: <span id="flow2Rate">0.00</span> L/min</div>
<div>Total Volume: <span id="flow2TotalVolume">0.00</span> L</div>
<div>Pulse Count: <span id="flow2PulseCount">0</span></div>
</div>
<button class="button" onclick="resetFlow2()">Reset Volume</button>
</div>
</div>
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
document.getElementById('pulseCount').textContent=d.pulseCount||0;
document.getElementById('flowStatus').className='status '+(d.error?'inactive':'active');
});
fetch('/flow2').then(r=>r.json()).then(d=>{
document.getElementById('flow2Rate').textContent=d.flowRate.toFixed(2);
document.getElementById('flow2TotalVolume').textContent=d.totalVolume.toFixed(2);
document.getElementById('flow2PulseCount').textContent=d.pulseCount||0;
document.getElementById('flow2Status').className='status '+(d.error?'inactive':'active');
});
}
function toggleDevice(device){
fetch('/toggle?device='+device).then(r=>r.text()).then(result=>alert(result));
}
function resetFlow(){
fetch('/flow/reset').then(r=>r.text()).then(result=>alert('Success: '+result));
}
function resetFlow2(){
fetch('/flow2/reset').then(r=>r.text()).then(result=>alert('Success: '+result));
}
function setVolume(pump,volume){
document.getElementById('volume'+pump).value=volume;
}
function playSound(type){
var ctx=new(window.AudioContext||window.webkitAudioContext)();
var freq=type==='start'?800:type==='complete'?1000:type==='error'?400:600;
var osc=ctx.createOscillator();
var gain=ctx.createGain();
osc.connect(gain);
gain.connect(ctx.destination);
osc.frequency.setValueAtTime(freq,ctx.currentTime);
gain.gain.setValueAtTime(0.1,ctx.currentTime);
gain.gain.exponentialRampToValueAtTime(0.001,ctx.currentTime+0.3);
osc.start();
osc.stop(ctx.currentTime+0.3);
}
function showNotification(message,type='info'){
var notification=document.createElement('div');
notification.style.cssText='position:fixed;top:20px;right:20px;background:'+(type==='success'?'#4CAF50':type==='error'?'#f44336':'#2196F3')+';color:white;padding:15px 20px;border-radius:5px;box-shadow:0 4px 8px rgba(0,0,0,0.3);z-index:1000;font-weight:bold;max-width:300px';
notification.textContent=message;
document.body.appendChild(notification);
setTimeout(()=>document.body.removeChild(notification),4000);
}
function startDispensing(pump){
var volume=parseFloat(document.getElementById('volume'+pump).value);
if(!volume||volume<=0){
showNotification('Please enter valid volume (0.01-10L)','error');
playSound('error');
return;
}
var formData=new FormData();
formData.append('pump',pump);
formData.append('volume',volume);
fetch('/dispense/start',{method:'POST',body:formData})
.then(r=>r.text()).then(result=>{
if(result==='OK'){
document.getElementById('start'+pump).disabled=true;
showNotification('Started dispensing '+volume+'L on Jook '+pump,'success');
playSound('start');
updateDispensingStatus();
}else{
showNotification('Failed to start: '+result,'error');
playSound('error');
}
});
}
function stopDispensing(pump){
var formData=new FormData();
formData.append('pump',pump);
fetch('/dispense/stop',{method:'POST',body:formData})
.then(r=>r.text()).then(result=>{
document.getElementById('start'+pump).disabled=false;
updateDispensingStatus();
});
}
function pauseDispensing(pump){
var formData=new FormData();
formData.append('pump',pump);
fetch('/dispense/pause',{method:'POST',body:formData})
.then(r=>r.text()).then(result=>{
updateDispensingStatus();
});
}
function emergencyStop(){
if(confirm('Emergency stop all dispensing operations?')){
fetch('/dispense/emergency',{method:'POST'})
.then(r=>r.text()).then(result=>{
updateDispensingStatus();
alert('All operations stopped!');
});
}
}
function formatTime(seconds){
if(seconds<60)return Math.round(seconds)+'s';
var mins=Math.floor(seconds/60);
var secs=Math.round(seconds%60);
return mins+'m '+secs+'s';
}
function updateDispensingStatus(){
fetch('/dispense/status').then(r=>r.json()).then(data=>{
for(let pump=1;pump<=2;pump++){
var pumpData=data['pump'+pump];
if(pumpData){
var progress=Math.round(pumpData.progress*100);
var current=pumpData.currentVolume.toFixed(3);
var target=pumpData.targetVolume.toFixed(3);
var progressBar=document.getElementById('progress'+pump);
var progressText=document.getElementById('progressText'+pump);
var statusDiv=document.getElementById('status'+pump);
var etaDiv=document.getElementById('eta'+pump);
var startBtn=document.getElementById('start'+pump);
var stopBtn=document.getElementById('stop'+pump);
var pauseBtn=document.getElementById('pause'+pump);
progressBar.style.width=progress+'%';
progressText.textContent=progress+'%';
var statusText='Ready';
var statusClass='';
if(pumpData.isDispensing){
statusText='Dispensing';
statusClass='dispensing';
startBtn.disabled=true;
pauseBtn.disabled=false;
stopBtn.disabled=false;
}else if(pumpData.isPaused){
statusText='Paused';
statusClass='dispensing';
startBtn.disabled=false;
startBtn.textContent='Resume';
pauseBtn.disabled=true;
stopBtn.disabled=false;
}else if(pumpData.isCompleted){
statusText='Completed';
statusClass='completed';
progressBar.className='progress-bar completed';
startBtn.disabled=false;
startBtn.textContent='Start Dispensing';
pauseBtn.disabled=true;
stopBtn.disabled=true;
if(!progressBar.hasAttribute('data-completed')){
progressBar.setAttribute('data-completed','true');
showNotification('Jook '+pump+' dispensing completed! '+current+'L dispensed','success');
playSound('complete');
}
setTimeout(()=>{
progressBar.style.width='0%';
progressBar.className='progress-bar';
progressText.textContent='';
progressBar.removeAttribute('data-completed');
},5000);
}else if(pumpData.hasError){
statusText='Error: '+pumpData.errorMessage;
statusClass='error';
progressBar.className='progress-bar error';
startBtn.disabled=false;
startBtn.textContent='Start Dispensing';
pauseBtn.disabled=true;
stopBtn.disabled=true;
}else{
startBtn.disabled=false;
startBtn.textContent='Start Dispensing';
pauseBtn.disabled=true;
stopBtn.disabled=true;
progressBar.className='progress-bar';
}
statusDiv.textContent=statusText+': '+current+'L / '+target+'L';
statusDiv.className='status-text '+statusClass;
if(pumpData.isDispensing&&pumpData.estimatedTimeRemaining>0){
etaDiv.textContent='ETA: '+formatTime(pumpData.estimatedTimeRemaining);
}else{
etaDiv.textContent='';
}
}
}
}).catch(err=>{
console.error('Status update failed:',err);
});
}
setInterval(updateStatus,10000);setInterval(updateDispensingStatus,200);updateStatus();updateDispensingStatus();
</script></body></html>
)rawliteral";

WebServerManager::WebServerManager(PIRSensor* pir, RelayController* relay, Flashlight* flashlight, DispensingController* dispensing)
    : server(80), pirSensor(pir), relayController(relay), flashlightController(flashlight), dispensingController(dispensing), wifiMultiPtr(nullptr), flowRate(0), totalVolume(0), flowError(false), pulseCount(0), flow2Rate(0), flow2TotalVolume(0), flow2Error(false), flow2PulseCount(0), flowDataCallback(nullptr), flow2DataCallback(nullptr) {
}

void WebServerManager::updateFlowData(float newFlowRate, float newTotalVolume, bool newError, long newPulseCount) {
    flowRate = newFlowRate;
    totalVolume = newTotalVolume;
    flowError = newError;
    pulseCount = newPulseCount;
    
    // Call external callback if set
    if (flowDataCallback) {
        flowDataCallback(flowRate, totalVolume, flowError);
    }
}

void WebServerManager::setFlowDataCallback(void (*callback)(float, float, bool)) {
    flowDataCallback = callback;
}

void WebServerManager::updateFlow2Data(float newFlowRate, float newTotalVolume, bool newError, long newPulseCount) {
    flow2Rate = newFlowRate;
    flow2TotalVolume = newTotalVolume;
    flow2Error = newError;
    flow2PulseCount = newPulseCount;
    
    // Call external callback if set
    if (flow2DataCallback) {
        flow2DataCallback(flow2Rate, flow2TotalVolume, flow2Error);
    }
}

void WebServerManager::setFlow2DataCallback(void (*callback)(float, float, bool)) {
    flow2DataCallback = callback;
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
        StaticJsonDocument<250> doc;
        doc["flowRate"] = flowRate;
        doc["totalVolume"] = totalVolume;
        doc["error"] = flowError;
        doc["pulseCount"] = pulseCount;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Flow sensor update endpoint for receiving data from YF-S402 unit
    server.on("/flow/update", HTTP_ANY, [this](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_GET) {
            if (request->hasParam("flowRate") && request->hasParam("totalVolume") && request->hasParam("error")) {
                float newFlowRate = request->getParam("flowRate")->value().toFloat();
                float newTotalVolume = request->getParam("totalVolume")->value().toFloat();
                bool newError = request->getParam("error")->value() == "true";
                long newPulseCount = request->hasParam("pulseCount") ? request->getParam("pulseCount")->value().toInt() : 0;
                
                updateFlowData(newFlowRate, newTotalVolume, newError, newPulseCount);
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Missing parameters");
            }
        } else if (request->method() == HTTP_POST) {
            // POST body will be handled in the body callback below
        } else {
            request->send(405, "text/plain", "Method not allowed");
        }
    }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // Handle POST body data for JSON flow updates
        if (request->method() != HTTP_POST) return;
        
        if (index + len == total && total > 0) {
            char* jsonStr = new char[total + 1];
            memcpy(jsonStr, data, len);
            jsonStr[total] = '\0';
            
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, jsonStr);
            
            if (!error) {
                float newFlowRate = doc["flowRate"] | 0.0f;
                float newTotalVolume = doc["totalVolume"] | 0.0f;
                bool newError = doc["error"] | false;
                long newPulseCount = doc["pulseCount"] | 0;
                
                updateFlowData(newFlowRate, newTotalVolume, newError, newPulseCount);
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "JSON parse error");
            }
            
            delete[] jsonStr;
        }
    });

    // Flow test endpoint for connectivity testing
    server.on("/flow/test", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Flow endpoint is working");
    });

    // Flow sensor dispensing status endpoint - tells flow sensor 1 if pump 1 is dispensing
    server.on("/flow/dispensing", HTTP_GET, [this](AsyncWebServerRequest *request) {
        StaticJsonDocument<200> doc;
        bool isDispensing = false;
        float targetVolume = 0.0f;
        
        if (dispensingController) {
            isDispensing = dispensingController->isDispensing(1);  // Check pump 1 for flow sensor 1
            targetVolume = dispensingController->getTargetVolume(1);
        }
        
        doc["isDispensing"] = isDispensing;
        doc["activePump"] = 1;
        doc["targetVolume"] = targetVolume;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Toggle device endpoint
    server.on("/toggle", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!request->hasParam("device")) {
            request->send(400, "text/plain", "Missing device parameter");
            return;
        }

        String device = request->getParam("device")->value();
        String response = "Unknown device";

        if (device == "flashlight") {
            bool newState = !flashlightController->state;
            flashlightController->state = newState;
            flashlightController->set(newState);
            response = newState ? "Flashlight ON" : "Flashlight OFF";
        }
        else if (device == "relay1") {
            bool newState = !relayController->getRelay1();
            relayController->setRelay1(newState);
            response = newState ? "Jook 1 ON" : "Jook 1 OFF";
        }
        else if (device == "relay2") {
            bool newState = !relayController->getRelay2();
            relayController->setRelay2(newState);
            response = newState ? "Jook 2 ON" : "Jook 2 OFF";
        }

        request->send(200, "text/plain", response);
    });

    // Reset flow volume endpoint
    server.on("/flow/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        totalVolume = 0;
        request->send(200, "text/plain", "OK");
    });

    // FLOW SENSOR 2 ENDPOINTS
    // Flow sensor 2 data endpoint
    server.on("/flow2", HTTP_GET, [this](AsyncWebServerRequest *request) {
        StaticJsonDocument<250> doc;
        doc["flowRate"] = flow2Rate;
        doc["totalVolume"] = flow2TotalVolume;
        doc["error"] = flow2Error;
        doc["pulseCount"] = flow2PulseCount;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Flow sensor 2 update endpoint for receiving data from YF-S402 unit 2
    server.on("/flow2/update", HTTP_ANY, [this](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_GET) {
            if (request->hasParam("flowRate") && request->hasParam("totalVolume") && request->hasParam("error")) {
                float newFlowRate = request->getParam("flowRate")->value().toFloat();
                float newTotalVolume = request->getParam("totalVolume")->value().toFloat();
                bool newError = request->getParam("error")->value() == "true";
                long newPulseCount = request->hasParam("pulseCount") ? request->getParam("pulseCount")->value().toInt() : 0;
                
                updateFlow2Data(newFlowRate, newTotalVolume, newError, newPulseCount);
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Missing parameters");
            }
        } else if (request->method() == HTTP_POST) {
            // POST body will be handled in the body callback below
        } else {
            request->send(405, "text/plain", "Method not allowed");
        }
    }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // Handle POST body data for JSON flow2 updates
        if (request->method() != HTTP_POST) return;
        
        if (index + len == total && total > 0) {
            char* jsonStr = new char[total + 1];
            memcpy(jsonStr, data, len);
            jsonStr[total] = '\0';
            
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, jsonStr);
            
            if (!error) {
                float newFlowRate = doc["flowRate"] | 0.0f;
                float newTotalVolume = doc["totalVolume"] | 0.0f;
                bool newError = doc["error"] | false;
                long newPulseCount = doc["pulseCount"] | 0;
                
                updateFlow2Data(newFlowRate, newTotalVolume, newError, newPulseCount);
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "JSON parse error");
            }
            
            delete[] jsonStr;
        }
    });

    // Flow 2 test endpoint for connectivity testing
    server.on("/flow2/test", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Flow endpoint is working");
    });

    // Flow sensor 2 dispensing status endpoint - tells flow sensor 2 if pump 2 is dispensing
    server.on("/flow2/dispensing", HTTP_GET, [this](AsyncWebServerRequest *request) {
        StaticJsonDocument<200> doc;
        bool isDispensing = false;
        float targetVolume = 0.0f;
        
        if (dispensingController) {
            isDispensing = dispensingController->isDispensing(2);  // Check pump 2 for flow sensor 2
            targetVolume = dispensingController->getTargetVolume(2);
        }
        
        doc["isDispensing"] = isDispensing;
        doc["activePump"] = 2;
        doc["targetVolume"] = targetVolume;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Reset flow2 volume endpoint
    server.on("/flow2/reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
        flow2TotalVolume = 0;
        request->send(200, "text/plain", "OK");
    });

    // NEW DISPENSING ENDPOINTS - Added for volume-controlled dispensing
    if (dispensingController) {
        // Start dispensing endpoint
        server.on("/dispense/start", HTTP_POST, [this](AsyncWebServerRequest *request) {
            if (!request->hasParam("pump", true) || !request->hasParam("volume", true)) {
                request->send(400, "text/plain", "Missing pump or volume parameter");
                return;
            }

            int pumpId = request->getParam("pump", true)->value().toInt();
            float targetVolume = request->getParam("volume", true)->value().toFloat();

            if (dispensingController->startDispensing(pumpId, targetVolume)) {
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Failed to start dispensing");
            }
        });

        // Stop dispensing endpoint
        server.on("/dispense/stop", HTTP_POST, [this](AsyncWebServerRequest *request) {
            if (!request->hasParam("pump", true)) {
                request->send(400, "text/plain", "Missing pump parameter");
                return;
            }

            int pumpId = request->getParam("pump", true)->value().toInt();
            dispensingController->stopDispensing(pumpId);
            request->send(200, "text/plain", "OK");
        });

        // Pause dispensing endpoint
        server.on("/dispense/pause", HTTP_POST, [this](AsyncWebServerRequest *request) {
            if (!request->hasParam("pump", true)) {
                request->send(400, "text/plain", "Missing pump parameter");
                return;
            }

            int pumpId = request->getParam("pump", true)->value().toInt();
            if (dispensingController->isDispensing(pumpId)) {
                dispensingController->pauseDispensing(pumpId);
            } else if (dispensingController->isPaused(pumpId)) {
                dispensingController->resumeDispensing(pumpId);
            }
            request->send(200, "text/plain", "OK");
        });

        // Emergency stop endpoint
        server.on("/dispense/emergency", HTTP_POST, [this](AsyncWebServerRequest *request) {
            dispensingController->emergencyStopAll();
            request->send(200, "text/plain", "Emergency stop activated");
        });

        // Dispensing status endpoint
        server.on("/dispense/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
            StaticJsonDocument<512> doc;
            
            for (int pumpId = 1; pumpId <= 2; pumpId++) {
                String pumpKey = "pump" + String(pumpId);
                JsonObject pump = doc.createNestedObject(pumpKey);
                
                pump["state"] = (int)dispensingController->getState(pumpId);
                pump["isDispensing"] = dispensingController->isDispensing(pumpId);
                pump["isPaused"] = dispensingController->isPaused(pumpId);
                pump["isCompleted"] = dispensingController->isCompleted(pumpId);
                pump["currentVolume"] = dispensingController->getCurrentVolume(pumpId);
                pump["targetVolume"] = dispensingController->getTargetVolume(pumpId);
                pump["progress"] = dispensingController->getProgress(pumpId);
                pump["elapsedTime"] = dispensingController->getElapsedTime(pumpId);
                pump["estimatedTimeRemaining"] = dispensingController->getEstimatedTimeRemaining(pumpId);
                pump["hasError"] = dispensingController->hasError(pumpId);
                if (dispensingController->hasError(pumpId)) {
                    pump["errorMessage"] = dispensingController->getErrorMessage(pumpId);
                }
            }
            
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        });
    }

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
