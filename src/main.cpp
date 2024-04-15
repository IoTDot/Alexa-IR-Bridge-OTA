#include <Arduino.h>
#include "fauxmoESP.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <WiFiManager.h>
#include <vector>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#else
#error "Not supported board!"
#endif

#ifndef IRLED
#error "PIN IR LED not defined!"
#endif

#define IRLED_PIN IRLED

const char *ssid = "IrAlexa";
const char *password = "12345678";

#if defined(ESP8266)
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
WebServer server(80);
HTTPUpdateServer httpUpdater;
#endif

#define CONNECTED_LED 2
const uint16_t IrLed = IRLED_PIN;
IRsend irsend(IrLed);

struct Device {
  String name;
  String irCode;
  uint16_t irBits;
  String irProtocol;
};

std::vector<Device> devices;

volatile int requestedDevice = 0;
volatile boolean receivedState = false;

fauxmoESP fauxmo;

WiFiManager wifiManager;

void handleRoot() {
  String html = "<html><head><style>";
  html += "body { background-color: #292323; color: white; text-align: center; font-family: Arial, sans-serif; }";
  html += "h1 { margin-top: 50px; }";
  html += ".custom-file-input { display: none; }";
  html += ".custom-file-label, input[type='submit'] { margin-top: 20px; background-color: white; color: black; padding: 10px 20px; border: none; cursor: pointer; font-size: 16px; }";
  html += "input[type='text'], input[type='number'], select { margin-top: 10px; padding: 5px; font-size: 16px; }";
  html += "table { margin: 0 auto; border-collapse: collapse; }";
  html += "th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }";
  html += "tr:hover { background-color: #f5f5f5; }";
  html += ".delete-button { background-color: #f44336; color: white; border: none; padding: 5px 10px; text-align: center; text-decoration: none; display: inline-block; font-size: 14px; margin: 4px 2px; cursor: pointer; }";
  html += "</style></head><body>";
  html += "<h1>IrAlexa</h1>";
  html += "<h2>FIRMWARE UPDATE</h2>";
  html += "<br>";
  html += "<p>First choose the new firmware .bin file</p>";
  html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
  html += "<input type='file' name='update' class='custom-file-input' id='fileInput'>";
  html += "<label for='fileInput' class='custom-file-label'>Choose file</label>";
  html += "<div id='fileName' style='margin-top: 10px;'></div>";
  html += "<br>";
  html += "<br>";
  html += "<br>";
  html += "<br>";
  html += "<p>next use Update button</p>";
  html += "<br>";
  html += "<br>";
  html += "<input type='submit' value='Update'>";
  html += "</form>";
  html += "<script>";
  html += "const fileInput = document.getElementById('fileInput');";
  html += "const fileName = document.getElementById('fileName');";
  html += "fileInput.addEventListener('change', (event) => {";
  html += "fileName.textContent = event.target.files[0].name || 'No file chosen';";
  html += "});";
  html += "</script>";
  html += "<br><br>";
  html += "<h2>Wi-Fi Settings</h2>";
  html += "<form action='/wifi' method='post'>";
  html += "<input type='submit' value='Configure Wi-Fi'>";
  html += "</form>";
  html += "<br><br>";
  html += "<h2>Device List</h2>";
  html += "<table>";
  html += "<tr><th>Name</th><th>IR Code</th><th>IR Bits</th><th>IR Protocol</th><th>Action</th></tr>";
  for (size_t i = 0; i < devices.size(); i++) {
    html += "<tr>";
    html += "<td>" + devices[i].name + "</td>";
    html += "<td>" + devices[i].irCode + "</td>";
    html += "<td>" + String(devices[i].irBits) + "</td>";
    html += "<td>" + devices[i].irProtocol + "</td>";
    html += "<td>";
    html += "<form method='POST' action='/devices'>";
html += "<input type='hidden' name='index' value='" + String(i) + "'>";
html += "<input type='submit' name='action' value='Edit'>";
html += "<input type='submit' name='action' value='Delete' class='delete-button'>";
html += "</form>";
html += "</td>";
html += "</tr>";
}
html += "</table>";
html += "<br><br>";
html += "<h2>Add New Device</h2>";
html += "<form method='POST' action='/devices'>";
html += "<input type='text' name='name' placeholder='Device Name'><br>";
html += "<input type='text' name='irCode' placeholder='IR Code'><br>";
html += "<input type='number' name='irBits' placeholder='IR Bits'><br>";
html += "<select name='irProtocol'>";
html += "<option value='SAMSUNG'>SAMSUNG</option>";
html += "<option value='EPSON'>EPSON</option>";
// Add more options for other IR protocols as needed
html += "</select><br>";
html += "<input type='submit' value='Add Device'>";
html += "</form>";
html += "</body></html>";

server.send(200, "text/html", html);
}

void handleWiFi()
{
  String html = "<html><head><style>";
  html += "body { background-color: #292323; color: white; text-align: center; font-family: Arial, sans-serif; }";
  html += "h1 { margin-top: 50px; }";
  html += "input[type='text'], input[type='password'] { width: 300px; padding: 10px; margin: 10px; }";
  html += "input[type='submit'] { background-color: white; color: black; padding: 10px 20px; border: none; cursor: pointer; font-size: 16px; }";
  html += "</style></head><body>";
  html += "<h1>Wi-Fi Configuration</h1>";
  html += "<form method='post' action='/save'>";
  html += "<select name='ssid'>";

  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++)
  {
    html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
  }

  html += "</select><br>";
  html += "<input type='password' name='pass' placeholder='Wi-Fi Password'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleSave()
{
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  if (ssid.length() > 0 && pass.length() > 0)
  {
    String html = "<html><head><style>";
    html += "body { background-color: #292323; color: white; text-align: center; font-family: Arial, sans-serif; }";
    html += "h1 { margin-top: 50px; }";
    html += "</style></head><body>";
    html += "<h1>Wi-Fi Settings Saved</h1>";
    html += "<p>The device will now attempt to connect to the new network</p>";
    html += "<p>You can close this page</p>";
    html += "</body></html>";

    server.send(200, "text/html", html);

    delay(1000);

    WiFi.begin(ssid.c_str(), pass.c_str());

    int timeout = 10; // 10 seconds
    while (WiFi.status() != WL_CONNECTED && timeout > 0)
    {
      delay(1000);
      timeout--;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("Connected to Wi-Fi");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
    else
    {
      Serial.println("Failed to connect to Wi-Fi");
    }
  }
  else
  {
    server.sendHeader("Location", "/wifi");
    server.send(303);
  }
}

void setupFauxmo() {
fauxmo.createServer(true);
fauxmo.setPort(80);
fauxmo.enable(true);

for (const auto& device : devices) {
fauxmo.addDevice(device.name.c_str());
}

fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value) {
Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
requestedDevice = device_id + 1;
receivedState = state;
});
}

void handleDevices() {
  if (server.method() == HTTP_GET) {
    // Return the list of devices as JSON
    String json = "[";
    for (const auto& device : devices) {
      json += "{\"name\":\"" + device.name + "\",\"irCode\":\"" + device.irCode + "\",\"irBits\":" + String(device.irBits) + ",\"irProtocol\":\"" + device.irProtocol + "\"},";
    }
    json.remove(json.length() - 1);
    json += "]";
    server.send(200, "application/json", json);
  } else if (server.method() == HTTP_POST) {
    // Add a new device
    String name = server.arg("name");
    String irCode = server.arg("irCode");
    uint16_t irBits = server.arg("irBits").toInt();
    String irProtocol = server.arg("irProtocol");
    devices.push_back({name, irCode, irBits, irProtocol});
    setupFauxmo();
    server.send(200, "text/plain", "Device added");
  } else if (server.method() == HTTP_PUT) {
    // Update an existing device
    int index = server.arg("index").toInt();
    if (index >= 0 && index < static_cast<int>(devices.size())) {
      devices[index].name = server.arg("name");
      devices[index].irCode = server.arg("irCode");
      devices[index].irBits = server.arg("irBits").toInt();
      devices[index].irProtocol = server.arg("irProtocol");
      setupFauxmo();
      server.send(200, "text/plain", "Device updated");
    } else {
      server.send(404, "text/plain", "Device not found");
    }
  } else if (server.method() == HTTP_DELETE) {
    // Delete a device
    int index = server.arg("index").toInt();
    if (index >= 0 && index < static_cast<int>(devices.size())) {
      devices.erase(devices.begin() + index);
      setupFauxmo();
      server.send(200, "text/plain", "Device deleted");
    } else {
      server.send(404, "text/plain", "Device not found");
    }
  }
}

void blinkLED() {
static unsigned long lastBlinkTime = 0;
const unsigned long blinkInterval = 500;

if (millis() - lastBlinkTime >= blinkInterval) {
lastBlinkTime = millis();
digitalWrite(CONNECTED_LED, !digitalRead(CONNECTED_LED));
}
}

void setup() {
#if defined(ESP8266) && defined(ESP01_1M)
pinMode(3, FUNCTION_3);
#endif

#if defined(ESP8266) && defined(ESP01_1M)
Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#elif defined(ESP32)
Serial.begin(115200);
#endif

irsend.begin();

pinMode(CONNECTED_LED, OUTPUT);
digitalWrite(CONNECTED_LED, LOW);

wifiManager.autoConnect(ssid, password);

Serial.println("Connected to Wi-Fi");
Serial.print("IP address: ");
Serial.println(WiFi.localIP());

server.on("/", handleRoot);
server.on("/wifi", handleWiFi);
server.on("/save", handleSave);
server.on("/devices", handleDevices);
server.begin();

httpUpdater.setup(&server);

setupFauxmo();
}

void loop() {
  fauxmo.handle();
  server.handleClient();

  digitalWrite(CONNECTED_LED, HIGH); // Turn on the LED when connected to the home network

 if (requestedDevice > 0 && requestedDevice <= static_cast<int>(devices.size())) {
    const auto& device = devices[requestedDevice - 1];
    if (device.irProtocol == "SAMSUNG") {
      irsend.sendSAMSUNG(strtoul(device.irCode.c_str(), nullptr, 16), device.irBits);
    } else if (device.irProtocol == "EPSON") {
      irsend.sendEpson(strtoul(device.irCode.c_str(), nullptr, 16), device.irBits);
    }
    // Add more conditions for other IR protocols as needed
  }

  requestedDevice = 0;

  static unsigned long last = millis();
  if (millis() - last > 5000) {
    last = millis();
    Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
  }
}