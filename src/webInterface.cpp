//this is webInterface.cpp
#include "webInterface.h"
#include "DeviceConfig.h"
#include "Protocols.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// static pointer to the active server
static WebServerType* webServer = nullptr;

// HTML interface stored in flash, with dynamic protocol dropdown
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
      // lista dostępnych bitrate'ów
      const allowedBits = [8, 12, 13, 14, 15, 16, 20, 24, 32, 36, 38];

      // funkcja wypełniająca select#bits odpowiednimi opcjami
      function populateBits(defaultBits) {
        const bitsSel = document.getElementById('bits');
        bitsSel.innerHTML = '';
        allowedBits.forEach(b => {
          const opt = document.createElement('option');
          opt.value = b;
          opt.text  = `${b}-bit`;
          bitsSel.add(opt);
        });
        if (allowedBits.includes(defaultBits)) {
          bitsSel.value = defaultBits;
        }
      }

      let protocolData = [];

      function loadProtocols() {
        fetch("/protocols")
          .then(r => r.json())
          .then(arr => {
            protocolData = arr;
            const sel = document.getElementById("protocol");
            sel.innerHTML = "";

            // przy zmianie protokołu uaktualniamy listę bits
            sel.onchange = () => {
              const p = sel.value;
              const entry = protocolData.find(x => x.value == p);
              populateBits(entry.bits);
            };

            // tworzymy opcje protokołów
            arr.forEach(p => {
              const opt = document.createElement("option");
              opt.value = p.value;
              opt.text  = `${p.name} (${p.bits}‑bit)`;
              sel.add(opt);
            });

            // ustawiamy pierwszy protokół jako domyślny
            sel.onchange();
          });
      }

      function addDevice() {
        const name     = encodeURIComponent(document.getElementById("name").value);
        const ircode   = encodeURIComponent(document.getElementById("ircode").value);
        const protocol = document.getElementById("protocol").value;
        const bits     = document.getElementById("bits").value;
        fetch(`/add?name=${name}&ircode=${ircode}&protocol=${protocol}&bits=${bits}`)
          .then(_ => loadDevices());
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

static const uint8_t ALLOWED_BITS[] = {8, 12, 13, 14, 15, 16, 20, 24, 32, 36, 38};
static const size_t NUM_ALLOWED_BITS = sizeof(ALLOWED_BITS) / sizeof(ALLOWED_BITS[0]);

// Handler – add device
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

  bool bitsOk = false;
  for (size_t i = 0; i < NUM_ALLOWED_BITS; i++) {
    if (bits == ALLOWED_BITS[i]) {
      bitsOk = true;
      break;
    }
  }
  if (!bitsOk) {
    // odrzucamy nieprawidłową wartość
    return webServer->send(400, "text/plain", "Invalid bits value");
    // lub: bits = PROTOCOL_BITS[proto];  // fallback do domyślnej długości
  }

  if (numDevices < MAX_DEVICES) {
    // zamiast devices[numDevices++] = { ... };
    devices[numDevices].deviceName = name;
    devices[numDevices].irCode      = code;
    devices[numDevices].protocol    = (Protocol)proto;
    devices[numDevices].bits        = bits;

    fauxmo.addDevice(name.c_str());
    numDevices++;

    webServer->send(200, "text/plain", "Device added");
  } else {
    webServer->send(200, "text/plain", "Max devices reached");
  }
}

// Handler – remove device
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

// Handler – list devices
void handleList() {
  String html;

  for (uint8_t i = 0; i < numDevices; i++) {
    // Kod IR w hex
    String hexCode = String(devices[i].irCode, HEX);
    hexCode.toUpperCase();

    html += "<div class='list-group-item d-flex justify-content-between align-items-center'>";
    html += "<div>";
    html += "<strong>" + devices[i].deviceName + "</strong><br>";
    html += "<small class='text-muted'>IR: 0x" + hexCode + "</small><br>";
    html += "<small class='text-muted'>Protocol: "
           + String(PROTOCOL_NAMES[devices[i].protocol]) + "</small><br>";
    // Tutaj wyświetlamy bits:
    html += "<small class='text-muted'>Bits: "
           + String(devices[i].bits) + "</small>";
    html += "</div>";

    html += "<button class='btn btn-danger btn-sm' onclick='removeDevice(" 
         + String(i) + ")'><i class='bi bi-trash'></i></button>";
    html += "</div>";
  }

  webServer->send(200, "text/html", html);
}

// Handler – save configuration (writes to LittleFS)
void handleSave() {
  extern void saveDevicesConfig();
  saveDevicesConfig();
  webServer->send(200, "text/plain", "Configuration saved");
}

// Handler – restart ESP
void handleRestart() {
  webServer->send(200, "text/plain", "Rebooting...");
  delay(100);
  ESP.restart();
}

// Handler – list available protocols as JSON
void handleProtocols() {
  // Use the recommended JsonDocument class
  JsonDocument doc; // Replaced DynamicJsonDocument doc(256);

  // Note: No need for .to<JsonArray>() here, directly assign to JsonArray
  JsonArray arr = doc.to<JsonArray>(); // Or JsonArray arr = doc.as<JsonArray>(); (both work, .to<> is slightly more modern C++)


  for (uint8_t i = 0; i < PROTOCOL_COUNT; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["value"] = i;
    obj["name"]  = PROTOCOL_NAMES[i];
    obj["bits"]  = PROTOCOL_BITS[i];
  }

  String out;
  serializeJson(doc, out);

  webServer->send(200, "application/json", out);
}

// Handler – root page
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
  webServer->begin();
  Serial.printf("Web interface: http://%s:8080\n", WiFi.localIP().toString().c_str());
}

void stopWebInterface(WebServerType &server) {
  server.stop();
  Serial.println("Web interface stopped");
}
