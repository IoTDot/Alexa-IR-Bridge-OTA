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
    <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css">
  </head>
  <body class="container mt-4">
    <h1 class="mb-4">Konfiguracja urządzeń</h1>
    <form id="deviceForm" class="mb-4">
      <div class="form-group">
        <label for="name">Nazwa urządzenia:</label>
        <input type="text" id="name" class="form-control" required>
      </div>
      <div class="form-group">
        <label for="ircode">Kod IR (hex):</label>
        <input type="text" id="ircode" class="form-control" required>
      </div>
      <div class="form-group">
        <label for="protocol">Protokół:</label>
        <select id="protocol" class="form-control">
          <option value="0">SAMSUNG</option>
          <option value="1">EPSON</option>
          <option value="2">Symphony</option>
        </select>
      </div>
      <button type="button" class="btn btn-primary" onclick="addDevice()">Dodaj urządzenie</button>
    </form>
    <h2>Lista urządzeń</h2>
    <ul id="deviceList" class="list-group mb-4"></ul>
    <button class="btn btn-success" onclick="saveConfig()">Zapisz konfigurację</button>
    <script>
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
    listHTML += "<li>" + devices[i].deviceName + " (IR: " + String(devices[i].irCode, HEX) +
                ", protokół: " + String(devices[i].protocol) + ") " +
                "<button onclick='removeDevice(" + String(i) + ")'>Usuń</button></li>";
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
