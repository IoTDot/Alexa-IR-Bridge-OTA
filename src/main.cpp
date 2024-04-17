#include <Arduino.h>
#include "fauxmoESP.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <WiFiManager.h>
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
#include <vector>
=======
#include <ArduinoJson.h>
#include <FS.h>
>>>>>>> 2d680f7 (web interface upgrade test)
=======
>>>>>>> d2815e6 (test)
=======
>>>>>>> 134c5fe (test)

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

<<<<<<< HEAD
<<<<<<< HEAD
struct Device {
  String name;
<<<<<<< HEAD
  String irCode;
  uint16_t irBits;
  String irProtocol;
=======
  uint64_t code;
  uint16_t bits;
  decode_type_t protocol;
>>>>>>> 2d680f7 (web interface upgrade test)
=======
=======
>>>>>>> 134c5fe (test)
const char *devices[] = {
    "TV",
    "Skip",
    "Mute",
    "Plus",
    "Minus",
    "Speakers",
<<<<<<< HEAD
>>>>>>> d2815e6 (test)
=======
>>>>>>> 134c5fe (test)
};

#define numDevices (sizeof(devices) / sizeof(char *))

volatile int requestedDevice = 0;
volatile boolean receivedState = false;

fauxmoESP fauxmo;

WiFiManager wifiManager;

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
=======
void loadDevices() {
  if (SPIFFS.begin()) {
    if (SPIFFS.exists("/devices.json")) {
      File file = SPIFFS.open("/devices.json", "r");
      if (file) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, file);
        if (error) {
          Serial.println("Failed to parse devices file");
        } else {
          JsonArray array = doc.as<JsonArray>();
          for (JsonVariant v : array) {
            Device device;
            device.name = v["name"].as<String>();
            device.code = v["code"].as<uint64_t>();
            device.bits = v["bits"].as<uint16_t>();
            device.protocol = (decode_type_t)v["protocol"].as<int>();
            devices.push_back(device);
          }
        }
        file.close();
      }
    }
    SPIFFS.end();
  }
}

void saveDevices() {
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  for (Device device : devices) {
    JsonObject obj = array.createNestedObject();
    obj["name"] = device.name;
    obj["code"] = device.code;
    obj["bits"] = device.bits;
    obj["protocol"] = device.protocol;
  }

  if (SPIFFS.begin()) {
    File file = SPIFFS.open("/devices.json", "w");
    if (file) {
      serializeJson(doc, file);
      file.close();
    }
    SPIFFS.end();
  }
}

void setupFauxmo() {
  fauxmo.createServer(true);
  fauxmo.setPort(80);
  fauxmo.enable(true);

  for (Device device : devices) {
    fauxmo.addDevice(device.name.c_str());
  }

fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value) {
  Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
  requestedDevice = device_id;
  receivedState = state;
});
}

>>>>>>> 2d680f7 (web interface upgrade test)
void handleRoot() {
=======
void handleRoot()
{
>>>>>>> d2815e6 (test)
=======
void handleRoot()
{
>>>>>>> 134c5fe (test)
  String html = "<html><head><style>";
  html += "body { background-color: #292323; color: white; text-align: center; font-family: Arial, sans-serif; }";
  html += "h1 { margin-top: 50px; }";
  html += ".custom-file-input { display: none; }";
  html += ".custom-file-label, input[type='submit'] { margin-top: 20px; background-color: white; color: black; padding: 10px 20px; border: none; cursor: pointer; font-size: 16px; }";
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
<<<<<<< HEAD
<<<<<<< HEAD
  html += "<br><br>";
<<<<<<< HEAD
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
=======
  html += "<h2>Device Settings</h2>";
  html += "<form action='/devices' method='post'>";
  html += "<input type='submit' value='Configure Devices'>";
  html += "</form>";
=======
>>>>>>> d2815e6 (test)
  html += "</body></html>";
>>>>>>> 2d680f7 (web interface upgrade test)
=======
  html += "</body></html>";
>>>>>>> 134c5fe (test)

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

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
void setupFauxmo() {
=======

void setupFauxmo()
{
>>>>>>> 134c5fe (test)
fauxmo.createServer(true);
fauxmo.setPort(80);
fauxmo.enable(true);

for (unsigned int i = 0; i < numDevices; i++)
{
fauxmo.addDevice(devices[i]);
}

fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value)
{
Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
requestedDevice = device_id + 1;
receivedState = state; });
}

void blinkLED()
{
static unsigned long lastBlinkTime = 0;
const unsigned long blinkInterval = 500;

if (millis() - lastBlinkTime >= blinkInterval)
{
lastBlinkTime = millis();
digitalWrite(CONNECTED_LED, !digitalRead(CONNECTED_LED));
}
}

<<<<<<< HEAD
void setup() {
=======
void handleDevices() {
  String html = "<html><head><style>";
  html += "body { background-color: #292323; color: white; text-align: center; font-family: Arial, sans-serif; }";
  html += "h1 { margin-top: 50px; }";
  html += "input[type='text'], input[type='number'], select { width: 300px; padding: 10px; margin: 10px; }";
  html += "input[type='submit'] { background-color: white; color: black; padding: 10px 20px; border: none; cursor: pointer; font-size: 16px; }";
  html += "</style></head><body>";
  html += "<h1>Device Configuration</h1>";
  html += "<form method='post' action='/saveDevices'>";
  
  for (size_t i = 0; i < devices.size(); i++) {
    html += "<h2>Device " + String(i + 1) + "</h2>";
    html += "<input type='text' name='name" + String(i) + "' value='" + devices[i].name + "' placeholder='Device Name'><br>";
    html += "<input type='text' name='code" + String(i) + "' value='" + String(devices[i].code, HEX) + "' placeholder='IR Code (hex)'><br>";
    html += "<input type='number' name='bits" + String(i) + "' value='" + String(devices[i].bits) + "' placeholder='Bits'><br>";
    html += "<select name='protocol" + String(i) + "'>";
    html += "<option value='" + String(decode_type_t::NEC) + "'" + (devices[i].protocol == decode_type_t::NEC ? " selected" : "") + ">NEC</option>";
    html += "<option value='" + String(decode_type_t::SONY) + "'" + (devices[i].protocol == decode_type_t::SONY ? " selected" : "") + ">SONY</option>";
    html += "<option value='" + String(decode_type_t::RC5) + "'" + (devices[i].protocol == decode_type_t::RC5 ? " selected" : "") + ">RC5</option>";
    html += "<option value='" + String(decode_type_t::RC6) + "'" + (devices[i].protocol == decode_type_t::RC6 ? " selected" : "") + ">RC6</option>";
    html += "<option value='" + String(decode_type_t::DISH) + "'" + (devices[i].protocol == decode_type_t::DISH ? " selected" : "") + ">DISH</option>";
    html += "<option value='" + String(decode_type_t::SHARP) + "'" + (devices[i].protocol == decode_type_t::SHARP ? " selected" : "") + ">SHARP</option>";
    html += "<option value='" + String(decode_type_t::SAMSUNG) + "'" + (devices[i].protocol == decode_type_t::SAMSUNG ? " selected" : "") + ">SAMSUNG</option>";
    html += "</select><br>";
  }
=======
>>>>>>> d2815e6 (test)

void setupFauxmo()
{
fauxmo.createServer(true);
fauxmo.setPort(80);
fauxmo.enable(true);

for (unsigned int i = 0; i < numDevices; i++)
{
fauxmo.addDevice(devices[i]);
}

fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value)
{
Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
requestedDevice = device_id + 1;
receivedState = state; });
}

void blinkLED()
{
static unsigned long lastBlinkTime = 0;
const unsigned long blinkInterval = 500;

if (millis() - lastBlinkTime >= blinkInterval)
{
lastBlinkTime = millis();
digitalWrite(CONNECTED_LED, !digitalRead(CONNECTED_LED));
}
}

<<<<<<< HEAD
saveDevices();

server.sendHeader("Location", "/devices");
server.send(303);
}

void handleAddDevice() {
Device device;
device.name = "New Device";
device.code = 0;
device.bits = 0;
device.protocol = decode_type_t::NEC;
devices.push_back(device);

server.sendHeader("Location", "/devices");
server.send(303);
}

void setup() {
// ... (previous setup code remains the same)
>>>>>>> 2d680f7 (web interface upgrade test)
=======
void setup()
{
>>>>>>> d2815e6 (test)
=======
void setup()
{
>>>>>>> 134c5fe (test)
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

<<<<<<< HEAD
<<<<<<< HEAD
server.on("/", handleRoot);
server.on("/wifi", handleWiFi);
server.on("/save", handleSave);
server.on("/devices", handleDevices);
<<<<<<< HEAD
=======
server.on("/saveDevices", handleSaveDevices);
server.on("/addDevice", handleAddDevice);
>>>>>>> 2d680f7 (web interface upgrade test)
server.begin();
=======
=======
>>>>>>> 134c5fe (test)
  server.on("/", handleRoot);
  server.on("/wifi", handleWiFi);
  server.on("/save", handleSave);
  server.begin();
<<<<<<< HEAD
>>>>>>> d2815e6 (test)
=======
>>>>>>> 134c5fe (test)

httpUpdater.setup(&server);

setupFauxmo();
}

<<<<<<< HEAD
<<<<<<< HEAD
void loop() {
<<<<<<< HEAD
  fauxmo.handle();
  server.handleClient();
=======
=======
void loop()
{
>>>>>>> d2815e6 (test)
fauxmo.handle();
server.handleClient();
>>>>>>> 2d680f7 (web interface upgrade test)
=======
void loop()
{
fauxmo.handle();
server.handleClient();
>>>>>>> 134c5fe (test)

digitalWrite(CONNECTED_LED, HIGH); // Turn on the LED when connected to the home network

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
 if (requestedDevice > 0 && requestedDevice <= static_cast<int>(devices.size())) {
    const auto& device = devices[requestedDevice - 1];
    if (device.irProtocol == "SAMSUNG") {
      irsend.sendSAMSUNG(strtoul(device.irCode.c_str(), nullptr, 16), device.irBits);
    } else if (device.irProtocol == "EPSON") {
      irsend.sendEpson(strtoul(device.irCode.c_str(), nullptr, 16), device.irBits);
    }
    // Add more conditions for other IR protocols as needed
  }
=======
switch (requestedDevice)
{
case 0:
break;
case 1:
irsend.sendSAMSUNG(0xE0E040BF, 32);
break;
case 2:
irsend.sendSAMSUNG(0xE0E016E9, 32);
break;
case 3:
irsend.sendEpson(0x8322EE11, 32);
break;
case 4:
irsend.sendEpson(0x8322E21D, 32);
break;
case 5:
irsend.sendEpson(0x8322E31C, 32);
break;
case 6:
irsend.sendEpson(0x8322E11E, 32);
break;
}
>>>>>>> 134c5fe (test)

requestedDevice = 0;

<<<<<<< HEAD
  static unsigned long last = millis();
  if (millis() - last > 5000) {
    last = millis();
    Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
  }
=======
if (requestedDevice >= 0 && requestedDevice < devices.size()) {
Device device = devices[requestedDevice];
switch (device.protocol) {
case decode_type_t::NEC:
irsend.sendNEC(device.code, device.bits);
=======
switch (requestedDevice)
{
case 0:
>>>>>>> d2815e6 (test)
break;
case 1:
irsend.sendSAMSUNG(0xE0E040BF, 32);
break;
case 2:
irsend.sendSAMSUNG(0xE0E016E9, 32);
break;
case 3:
irsend.sendEpson(0x8322EE11, 32);
break;
case 4:
irsend.sendEpson(0x8322E21D, 32);
break;
case 5:
irsend.sendEpson(0x8322E31C, 32);
break;
case 6:
irsend.sendEpson(0x8322E11E, 32);
break;
}

requestedDevice = 0;

=======
>>>>>>> 134c5fe (test)
static unsigned long last = millis();
if (millis() - last > 5000)
{
last = millis();
Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
}
<<<<<<< HEAD
>>>>>>> 2d680f7 (web interface upgrade test)
=======
>>>>>>> 134c5fe (test)
}