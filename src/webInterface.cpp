//this is webinteface.cpp
#include "webInterface.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Utwórz statyczny wskaźnik do serwera – ustawiony w setupWebInterface
static WebServerType* webServer = nullptr;

// HTML interfejsu zapisany w pamięci flash
const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>
    <head>
      <meta charset="utf-8">
      <title>Konfiguracja urządzeń</title>
      <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css">
      <style>
        body {
          background-color: #000000;
          color: #ffffff;
          min-height: 100vh;
        }
        .custom-card {
          background-color: #1a1a1a;
          border-radius: 15px;
          padding: 20px;
          margin-bottom: 20px;
        }
        .form-control {
          background-color: #333333;
          color: #ffffff;
          border: 1px solid #444444;
        }
        .form-control:focus {
          background-color: #444444;
          color: #ffffff;
          border-color: #666666;
          box-shadow: none;
        }
        .btn-primary {
          background-color: #0069d9;
          border-color: #0062cc;
        }
        .btn-danger {
          background-color: #dc3545;
          border-color: #dc3545;
        }
        .list-group-item {
          background-color: #222222;
          color: #ffffff;
          border: 1px solid #333333;
        }
      </style>
    </head>
    <body class="p-4">
      <div class="container-lg">
        <div class="custom-card">
          <h1 class="mb-4 text-center">Konfiguracja urządzeń</h1>
          
          <div class="row">
            <div class="col-md-6 mb-4">
              <div class="custom-card">
                <h4 class="mb-3">Dodaj nowe urządzenie</h4>
                <form id="deviceForm">
                  <div class="mb-3">
                    <label for="name" class="form-label">Nazwa urządzenia</label>
                    <input type="text" id="name" class="form-control" required>
                  </div>
                  <div class="mb-3">
                    <label for="ircode" class="form-label">Kod IR (hex)</label>
                    <input type="text" id="ircode" class="form-control" required>
                  </div>
                  <div class="mb-3">
                    <label for="protocol" class="form-label">Protokół</label>
                    <select id="protocol" class="form-select">
                      <option value="0">SAMSUNG</option>
                      <option value="1">EPSON</option>
                      <option value="2">Symphony</option>
                    </select>
                  </div>
                  <button type="button" class="btn btn-primary w-100" onclick="addDevice()">
                    <i class="bi bi-plus-circle"></i> Dodaj urządzenie
                  </button>
                </form>
              </div>
            </div>
  
            <div class="col-md-6">
              <div class="custom-card">
                <h4 class="mb-3">Lista urządzeń</h4>
                <div id="deviceList" class="list-group mb-3"></div>
                <button class="btn btn-success w-100" onclick="saveConfig()">
                  <i class="bi bi-save"></i> Zapisz konfigurację
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
  
      <script>
        // Funkcje JavaScript pozostają bez zmian
        function addDevice() {
          var name = document.getElementById("name").value;
          var ircode = document.getElementById("ircode").value;
          var protocol = document.getElementById("protocol").value;
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "/add?name=" + encodeURIComponent(name) + "&ircode=" + encodeURIComponent(ircode) + "&protocol=" + protocol, true);
          xhr.send();
          setTimeout(loadDevices, 500);
        }
        
        function loadDevices() {
          var xhr = new XMLHttpRequest();
          xhr.onreadystatechange = function() {
            if (this.readyState == 4 && this.status == 200) {
              document.getElementById("deviceList").innerHTML = this.responseText;
            }
          };
          xhr.open("GET", "/list", true);
          xhr.send();
        }
        
        function removeDevice(index) {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "/remove?index=" + index, true);
          xhr.send();
          setTimeout(loadDevices, 500);
        }
        
        function saveConfig() {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "/save", true);
          xhr.send();
          alert("Konfiguracja zapisana. Urządzenie zostanie zrestartowane.");
        }
        
        window.onload = loadDevices;
      </script>
      
      <!-- Bootstrap Icons -->
      <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.8.1/font/bootstrap-icons.css">
    </body>
  </html>
  )rawliteral";

// Handler – dodanie urządzenia
void handleAdd() {
  if (webServer->hasArg("name") && webServer->hasArg("ircode") && webServer->hasArg("protocol")) {
    String name = webServer->arg("name");
    String ircodeStr = webServer->arg("ircode");
    uint32_t ircode = strtoul(ircodeStr.c_str(), NULL, 16);
    uint8_t protocol = webServer->arg("protocol").toInt();
    if (numDevices < MAX_DEVICES) {
      devices[numDevices].deviceName = name;
      devices[numDevices].irCode = ircode;
      devices[numDevices].protocol = protocol;
      numDevices++;
      // Dodaj urządzenie do emulacji fauxmo
      fauxmo.addDevice(name.c_str());
      webServer->send(200, "text/plain", "Urządzenie dodane");
    } else {
      webServer->send(200, "text/plain", "Osiągnięto maksymalną liczbę urządzeń");
    }
  } else {
    webServer->send(400, "text/plain", "Brak parametrów");
  }
}

// Handler – usuwanie urządzenia
void handleRemove() {
  if (webServer->hasArg("index")) {
    uint8_t index = webServer->arg("index").toInt();
    if (index < numDevices) {
      for (uint8_t i = index; i < numDevices - 1; i++) {
        devices[i] = devices[i + 1];
      }
      numDevices--;
        // Wyłącz fauxmo, usuń urządzenia i ponownie dodaj
        fauxmo.enable(false);  // Wyłączenie emulacji
        for (uint8_t i = 0; i < numDevices; i++) {
            fauxmo.removeDevice(devices[i].deviceName.c_str());  // Usunięcie urządzenia
        }
        fauxmo.enable(true);   // Włączenie emulacji
        for (uint8_t i = 0; i < numDevices; i++) {
            fauxmo.addDevice(devices[i].deviceName.c_str());  // Dodanie ponownie
        }
      webServer->send(200, "text/plain", "Urządzenie usunięte");
    } else {
      webServer->send(400, "text/plain", "Nieprawidłowy indeks");
    }
  } else {
    webServer->send(400, "text/plain", "Brak parametru index");
  }
}

// Handler – lista urządzeń
void handleList() {
  String listHTML = "";
  for (uint8_t i = 0; i < numDevices; i++) {
    listHTML += "<div class='list-group-item d-flex justify-content-between align-items-center'>";
    listHTML += "<div><strong>" + devices[i].deviceName + "</strong><br>";
    listHTML += "<small class='text-muted'>IR: " + String(devices[i].irCode, HEX) + "</small><br>";
    listHTML += "<small class='text-muted'>Protokół: " + String(devices[i].protocol) + "</small></div>";
    listHTML += "<button class='btn btn-danger btn-sm' onclick='removeDevice(" + String(i) + ")'><i class='bi bi-trash'></i></button>";
    listHTML += "</div>";
  }
  webServer->send(200, "text/html", listHTML);
}

// Handler – zapis konfiguracji
void handleSave() {
  extern void saveDevicesConfig();
  saveDevicesConfig();
  webServer->send(200, "text/plain", "Konfiguracja zapisana");
  ESP.restart();
}

// Handler – strona główna
void handleRoot() {
    Serial.println("handleRoot() wywołany");
    webServer->send_P(200, "text/html", index_html);
  }
void setupWebInterface(WebServerType &server) {
  webServer = &server; // ustawiamy wskaźnik globalny
  webServer->on("/", handleRoot);
  webServer->on("/add", handleAdd);
  webServer->on("/remove", handleRemove);
  webServer->on("/list", handleList);
  webServer->on("/save", handleSave);
  webServer->begin(8080);
  Serial.println("Web interfejs dostępny pod adresem: http://" + WiFi.localIP().toString() + ":8080");
}

void stopWebInterface(WebServerType &server) {
  server.stop();
  Serial.println("Web interfejs wyłączony");
}
