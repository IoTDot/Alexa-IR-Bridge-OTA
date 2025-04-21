#include "webInterface.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// static pointer to whichever WebServerType you have
static WebServerType* webServer = nullptr;

// Protocol names for display
static const char* PROTOCOL_NAMES[] = {
  "SAMSUNG",
  "EPSON",
  "SYMPHONY"
};

// English HTML UI, with separate Save and Restart buttons
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Device Configuration</title>
    <link rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css">
    <style>
      body { background-color: #000; color: #fff; min-height: 100vh; }
      .custom-card { background-color: #1a1a1a; border-radius: 15px;
                     padding: 20px; margin-bottom: 20px; }
      .form-control { background-color: #333; color: #fff; border: 1px solid #444; }
      .form-control:focus { background-color: #444; color: #ffffff !important; border-color: #666; box-shadow: none; }
      .btn-primary { background-color: #0069d9; border-color: #0062cc; }
      .btn-danger  { background-color: #dc3545; border-color: #dc3545; }
      .btn-success { background-color: #28a745; border-color: #28a745; }
      .btn:focus, .btn:active { outline: none !important; box-shadow: none !important; }
      .btn:focus-visible { outline: 2px solid #ffffff; outline-offset: 2px;}
      .list-group-item { background-color: #222; color: #fff; border: 1px solid #333; }
    </style>
  </head>
  <body class="p-4">
    <div class="container-lg">
      <div class="custom-card">
        <h1 class="mb-4 text-center">Device Configuration</h1>
        <div class="row">
          <div class="col-md-6 mb-4">
            <div class="custom-card">
              <h4 class="mb-3">Add New Device</h4>
              <form id="deviceForm">
                <div class="mb-3">
                  <label for="name" class="form-label">Device Name</label>
                  <input type="text" id="name" class="form-control" required>
                </div>
                <div class="mb-3">
                  <label for="ircode" class="form-label">IR Code (hex)</label>
                  <input type="text" id="ircode" class="form-control" required>
                </div>
                <div class="mb-3">
                  <label for="protocol" class="form-label">Protocol</label>
                  <select id="protocol" class="form-select">
                    <option value="0">SAMSUNG</option>
                    <option value="1">EPSON</option>
                    <option value="2">SYMPHONY</option>
                  </select>
                </div>
                <button type="button" class="btn btn-primary w-100" onclick="addDevice()">
                  <i class="bi bi-plus-circle"></i> Add Device
                </button>
              </form>
            </div>
          </div>
          <div class="col-md-6">
            <div class="custom-card">
              <h4 class="mb-3">Device List</h4>
              <div id="deviceList" class="list-group mb-3"></div>
              <button class="btn btn-success w-100 mb-2" onclick="saveConfig()">
                <i class="bi bi-save"></i> Save Configuration
              </button>
              <button class="btn btn-danger w-100" onclick="restartDevice()">
                <i class="bi bi-arrow-clockwise"></i> Restart ESP
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
    <script>
      function addDevice() {
        const name     = encodeURIComponent(document.getElementById("name").value);
        const ircode   = encodeURIComponent(document.getElementById("ircode").value);
        const protocol = document.getElementById("protocol").value;
        fetch(`/add?name=${name}&ircode=${ircode}&protocol=${protocol}`)
          .then(_=>loadDevices());
      }
      function loadDevices() {
        fetch("/list")
          .then(r=>r.text())
          .then(html=>{ document.getElementById("deviceList").innerHTML = html; });
      }
      function removeDevice(idx) {
        fetch(`/remove?index=${idx}`)
          .then(_=>loadDevices());
      }
      function saveConfig() {
        fetch("/save")
          .then(_=>alert("Configuration saved."));
      }
      function restartDevice() {
        fetch("/restart")
          .then(_=>alert("Restarting ESP..."));
      }
      window.onload = loadDevices;
    </script>
    <!-- Bootstrap Icons -->
    <link rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.8.1/font/bootstrap-icons.css">
  </body>
</html>
)rawliteral";

// ——— Handlers ———

void handleAdd() {
  if (!webServer->hasArg("name") ||
      !webServer->hasArg("ircode") ||
      !webServer->hasArg("protocol")) {
    return webServer->send(400, "text/plain", "Missing parameters");
  }
  String name    = webServer->arg("name");
  String irStr   = webServer->arg("ircode");
  uint32_t code  = strtoul(irStr.c_str(), NULL, 16);
  uint8_t proto  = webServer->arg("protocol").toInt();
  if (numDevices < MAX_DEVICES) {
    devices[numDevices++] = { name, code, proto };
    fauxmo.addDevice(name.c_str());
    webServer->send(200, "text/plain", "Device added");
  } else {
    webServer->send(200, "text/plain", "Max devices reached");
  }
}

void handleRemove() {
  if (!webServer->hasArg("index"))
    return webServer->send(400, "text/plain", "Missing index");
  uint8_t idx = webServer->arg("index").toInt();
  if (idx >= numDevices)
    return webServer->send(400, "text/plain", "Invalid index");
  for (uint8_t i = idx; i < numDevices-1; i++)
    devices[i] = devices[i+1];
  numDevices--;
  // rebuild fauxmo device list
  fauxmo.enable(false);
  for (uint8_t i=0; i<numDevices; i++) {
    fauxmo.removeDevice(devices[i].deviceName.c_str());
  }
  fauxmo.enable(true);
  for (uint8_t i=0; i<numDevices; i++) {
    fauxmo.addDevice(devices[i].deviceName.c_str());
  }
  webServer->send(200, "text/plain", "Device removed");
}

void handleList() {
  String html;
  for (uint8_t i = 0; i < numDevices; i++) {
    String hexCode = String(devices[i].irCode, HEX);
    hexCode.toUpperCase();
    html += "<div class='list-group-item d-flex justify-content-between align-items-center'>";
    html += "<div><strong>" + devices[i].deviceName + "</strong><br>";
    html += "<small class='text-muted'>IR: 0x" + hexCode + "</small><br>";
    html += "<small class='text-muted'>Protocol: " +
            String(PROTOCOL_NAMES[devices[i].protocol]) + "</small></div>";
    html += "<button class='btn btn-danger btn-sm' "
            "onclick='removeDevice(" + String(i) + ")'>"
            "<i class='bi bi-trash'></i></button>";
    html += "</div>";
  }
  webServer->send(200, "text/html", html);
}

void handleSave() {
  extern void saveDevicesConfig();
  saveDevicesConfig();
  webServer->send(200, "text/plain", "Configuration saved");
}

void handleRestart() {
  webServer->send(200, "text/plain", "Rebooting...");
  delay(100);
  ESP.restart();
}

void handleRoot() {
  webServer->send_P(200, "text/html", index_html);
}

void setupWebInterface(WebServerType &server) {
  webServer = &server;
  webServer->on("/",      handleRoot);
  webServer->on("/add",   handleAdd);
  webServer->on("/remove",handleRemove);
  webServer->on("/list",  handleList);
  webServer->on("/save",  handleSave);
  webServer->on("/restart",handleRestart);
  webServer->begin();
  Serial.printf("Web interface: http://%s:8080\n", WiFi.localIP().toString().c_str());
}

void stopWebInterface(WebServerType &server) {
  server.stop();
  Serial.println("Web interface stopped");
}
