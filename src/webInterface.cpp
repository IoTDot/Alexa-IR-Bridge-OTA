#include "webInterface.h"
#include "DeviceConfig.h"
#include "Protocols.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

extern void saveDevicesConfig();
extern void sendIRSignal(const Device &dev);

// statyczny wskaźnik na aktywny serwer
static WebServerType* webServer = nullptr;

// HTML interfejsu (niezmienione)
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
      .form-control:focus { background-color: #444; color: #ffffff !important;
                             border-color: #666; box-shadow: none; }
      .form-select { background-color: #333; color: #fff; }
      .form-select:focus { background-color: #444; color: #ffffff !important;
                            border-color: #666; box-shadow: none; }
      .btn-primary { background-color: #0069d9; border-color: #0062cc; }
      .btn-success { background-color: #28a745; border-color: #28a745; }
      .btn-danger  { background-color: #dc3545; border-color: #dc3545; }
      .btn:focus, .btn:active { outline: none !important; box-shadow: none !important; }
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
                  <select id="protocol" class="form-select"></select>
                </div>
                <div class="mb-3">
                  <label for="bits" class="form-label">Bits</label>
                  <select id="bits" class="form-select" required></select>
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
      const allowedBits = [8, 12, 13, 14, 15, 16, 20, 24, 28, 32, 36, 38, 48];
      function populateBits(defaultBits) {
        const bitsSel = document.getElementById('bits');
        bitsSel.innerHTML = '';
        allowedBits.forEach(b => {
          const opt = document.createElement('option');
          opt.value = b;
          opt.text  = `${b}-bit`;
          bitsSel.add(opt);
        });
        if (allowedBits.includes(defaultBits)) bitsSel.value = defaultBits;
      }
      let protocolData = [];
      function loadProtocols() {
        fetch("/protocols")
          .then(r => r.json())
          .then(arr => {
            protocolData = arr;
            const sel = document.getElementById("protocol");
            sel.innerHTML = "";
            sel.onchange = () => {
              const p = sel.value;
              const entry = protocolData.find(x => x.value == p);
              populateBits(entry.bits);
            };
            arr.forEach(p => {
              const opt = document.createElement("option");
              opt.value = p.value;
              opt.text  = `${p.name} (${p.bits}‑bit)`;
              sel.add(opt);
            });
            sel.onchange();
          });
      }
      function addDevice() {
        const nameInput = document.getElementById("name");
        const ircodeInput = document.getElementById("ircode");
        const protocolSel = document.getElementById("protocol");
        const bitsSel = document.getElementById("bits");
        const name     = encodeURIComponent(nameInput.value.trim());
        const ircode   = encodeURIComponent(ircodeInput.value.trim());
        const protocol = protocolSel.value;
        const bits     = bitsSel.value;
        if (!nameInput.value.trim() || !ircodeInput.value.trim()) {
            alert("Device Name and IR Code cannot be empty.");
            return;
        }
        fetch(`/add?name=${name}&ircode=${ircode}&protocol=${protocol}&bits=${bits}`)
          .then(response => {
            if (response.ok) {
              nameInput.value = '';
              loadDevices();
            } else {
              return response.text().then(text => { throw new Error(text); });
            }
          })
          .catch(e => alert(`Error adding device: ${e.message}`));
      }
      function loadDevices() {
        fetch("/list")
          .then(r => r.text())
          .then(html => {
            document.getElementById("deviceList").innerHTML = html;
          });
      }
      function removeDevice(idx) {
        fetch(`/remove?index=${idx}`)
          .then(_ => loadDevices());
      }
      function testDevice(idx) {
        fetch(`/test?index=${idx}`)
          .then(r => {
            if (r.ok) {
              alert("Sygnał IR wysłany do urządzenia #" + idx);
            } else {
              return r.text().then(text => { throw new Error(text); });
            }
          })
          .catch(e => alert("Błąd testu IR: " + e.message));
      }
      function saveConfig() {
        fetch("/save");
        alert("Configuration saved.");
      }
      function restartDevice() {
        fetch("/restart");
        alert("Restarting ESP...");
      }
      window.onload = () => {
        loadProtocols();
        loadDevices();
      };
    </script>
    <link rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.8.1/font/bootstrap-icons.css">
  </body>
</html>
)rawliteral";

// Handler – add device (bez zmian)
void handleAdd() {
  if (!webServer->hasArg("name") ||
      !webServer->hasArg("ircode") ||
      !webServer->hasArg("protocol") ||
      !webServer->hasArg("bits")) {
    return webServer->send(400, "text/plain", "Missing parameters");
  }

  String name    = webServer->arg("name");
  String irStr   = webServer->arg("ircode");
  uint32_t code  = strtoul(irStr.c_str(), nullptr, 16);
  uint8_t proto  = webServer->arg("protocol").toInt();
  uint8_t bits   = webServer->arg("bits").toInt();

  for (uint8_t i = 0; i < numDevices; i++) {
    if (devices[i].deviceName == name) {
      return webServer->send(400, "text/plain", "Device name already exists");
    }
  }

  bool bitsOk = false;
  for (size_t i = 0; i < sizeof(ALLOWED_BITS)/sizeof(ALLOWED_BITS[0]); i++) {
    if (bits == ALLOWED_BITS[i]) { bitsOk = true; break; }
  }
  if (!bitsOk) {
    return webServer->send(400, "text/plain", "Invalid bits value");
  }

  if (numDevices < MAX_DEVICES) {
    devices[numDevices] = { name, code, bits, proto };
    fauxmo.addDevice(name.c_str());
    numDevices++;
    webServer->send(200, "text/plain", "Device added");
  } else {
    webServer->send(200, "text/plain", "Max devices reached");
  }
}

// Handler – remove device (bez zmian)
void handleRemove() {
  if (!webServer->hasArg("index"))
    return webServer->send(400, "text/plain", "Missing index");
  uint8_t idx = webServer->arg("index").toInt();
  if (idx >= numDevices)
    return webServer->send(400, "text/plain", "Invalid index");
  for (uint8_t i = idx; i < numDevices - 1; i++) {
    devices[i] = devices[i + 1];
  }
  numDevices--;
  // rebuild fauxmo list
  fauxmo.enable(false);
  for (uint8_t i = 0; i < numDevices; i++) {
    fauxmo.removeDevice(devices[i].deviceName.c_str());
  }
  fauxmo.enable(true);
  for (uint8_t i = 0; i < numDevices; i++) {
    fauxmo.addDevice(devices[i].deviceName.c_str());
  }
  webServer->send(200, "text/plain", "Device removed");
}

// Handler – list devices (zaktualizowane)
void handleList() {
  String html;
  for (uint8_t i = 0; i < numDevices; i++) {
    String hexCode = String(devices[i].irCode, HEX);
    hexCode.toUpperCase();
    html += "<div class='list-group-item d-flex justify-content-between align-items-center'>";
    html += "<div>";
    html += "<strong>" + devices[i].deviceName + "</strong><br>";
    html += "<small class='text-muted'>IR: 0x" + hexCode + "</small><br>";
    html += "<small class='text-muted'>Protocol: " 
         + String(PROTOCOLS[devices[i].protocol].name) + "</small><br>";
    html += "<small class='text-muted'>Bits: " 
         + String(devices[i].bits) + "</small>";
    html += "</div>";
    // przycisk Test IR
    html += "<button "
         "class='btn btn-secondary btn-sm me-1' "
          "onclick='testDevice(" + String(i) + ")' "
          "title='Test IR'>"
          "<i class='bi bi-play-circle me-1'></i>"
          "Test IR"
          "</button>";
    // przycisk usuń
    html += "<button class='btn btn-danger btn-sm' "
         "onclick='removeDevice(" + String(i) + ")'>"
        "<i class='bi bi-trash'></i></button>";
    html += "</div>";
  }
  webServer->send(200, "text/html", html);
}

// Handler – save configuration (bez zmian)
void handleSave() {
  extern void saveDevicesConfig();
  saveDevicesConfig();
  webServer->send(200, "text/plain", "Configuration saved");
}

// Handler – restart ESP (bez zmian)
void handleRestart() {
  webServer->send(200, "text/plain", "Rebooting...");
  delay(100);
  ESP.restart();
}

// Handler – list available protocols jako JSON (nowy)
void handleProtocols() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (uint8_t i = 0; i < PROTOCOL_COUNT; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["value"] = i;
    o["name"]  = PROTOCOLS[i].name;
    o["bits"]  = PROTOCOLS[i].bits;
  }
  String out;
  serializeJson(arr, out);
  webServer->send(200, "application/json", out);
}

// +++ nowy handler – test wysyłki IR +++
void handleTest() {
  if (!webServer->hasArg("index")) {
    return webServer->send(400, "text/plain", "Missing index");
  }
  uint8_t idx = webServer->arg("index").toInt();
  if (idx >= numDevices) {
    return webServer->send(400, "text/plain", "Invalid index");
  }
  // Wyślij sygnał IR
  sendIRSignal(devices[idx]);
  webServer->send(200, "text/plain", "IR code sent");
}

// Handler – root page (bez zmian)
void handleRoot() {
  webServer->send_P(200, "text/html", index_html);
}

void setupWebInterface(WebServerType &server) {
  webServer = &server;
  webServer->on("/",        handleRoot);
  webServer->on("/add",     handleAdd);
  webServer->on("/remove",  handleRemove);
  webServer->on("/list",    handleList);
  webServer->on("/save",    handleSave);
  webServer->on("/restart", handleRestart);
  webServer->on("/protocols",handleProtocols);
  webServer->on("/test",      handleTest);
  webServer->begin();
  Serial.printf("Web interface: http://%s:8080\n", WiFi.localIP().toString().c_str());
}

void stopWebInterface(WebServerType &server) {
  server.stop();
  Serial.println("Web interface stopped");
}
