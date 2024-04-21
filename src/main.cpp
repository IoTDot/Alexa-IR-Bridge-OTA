#include <Arduino.h>
#include "fauxmoESP.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <WiFiManager.h>
#include <EEPROM.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <ESPmDNS.h>
#else
#error "Not supported board!"
#endif

#ifndef IRLED
#error "PIN IR LED not defined!"
#endif

#define IRLED_PIN IRLED

const char *ssid = "IrAlexa";
const char *password = "12345678";

IPAddress apIP(4, 4, 4, 4);

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

#define DEVICE_STRUCT_SIZE 64
#define MAX_DEVICES 10

struct __attribute__((packed)) Device {
  char name[32];
  char protocol[16];
  char code[32];
  char bitrate[8];
};

Device devices[MAX_DEVICES];
int numDevices = 0;

volatile int requestedDevice = 0;
volatile boolean receivedState = false;

fauxmoESP fauxmo;

WiFiManager wifiManager;

void handleRoot()
{
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
  html += "<h2>Devices</h2>";
  html += "<form action='/devices' method='post'>";
  html += "<input type='submit' value='Configure Devices'>";
  html += "</form>";
  html += "<br><br>";
  html += "<h2>Wi-Fi Settings</h2>";
  html += "<form action='/wifi' method='post'>";
  html += "<input type='submit' value='Configure Wi-Fi'>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleDevices()
{
  String html = "<html><head><style>";
  html += "body { background-color: #292323; color: white; text-align: center; font-family: Arial, sans-serif; }";
  html += "h1 { margin-top: 50px; }";
  html += "input[type='text'], input[type='password'] { width: 300px; padding: 10px; margin: 10px; }";
  html += "input[type='submit'] { background-color: white; color: black; padding: 10px 20px; border: none; cursor: pointer; font-size: 16px; }";
  html += "button { background-color: white; color: black; padding: 10px 20px; border: none; cursor: pointer; font-size: 16px; }";
  html += "table { margin: 0 auto; }";
  html += "</style></head><body>";
  html += "<h1>Devices Configuration</h1>";
  html += "<form method='post' action='/saveDevices'>";
  html += "<table>";
  for (int i = 0; i < MAX_DEVICES; i++)
  {
    html += "<tr>";
    html += "<td><input type='text' name='device" + String(i) + "' value='" + devices[i].name + "' placeholder='Device name'></td>";
    html += "<td><input type='text' name='protocol" + String(i) + "' value='" + devices[i].protocol + "' placeholder='IR protocol'></td>";
    html += "<td><input type='text' name='code" + String(i) + "' value='" + devices[i].code + "' placeholder='IR code'></td>";
    html += "<td><input type='text' name='bitrate" + String(i) + "' value='" + devices[i].bitrate + "' placeholder='Bitrate'></td>";
    html += "</tr>";
  }
  html += "</table>";
  html += "<input type='submit' value='Save'>";
  html += "<br><br>";
  html += "<a href='/'><button>Back</button></a>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void saveDevicesToEEPROM();
void loadDevicesFromEEPROM();
void handleSaveDevices()
{
  for (int i = 0; i < MAX_DEVICES; i++)
  {
    String name = server.arg("device" + String(i));
    String protocol = server.arg("protocol" + String(i));
    String code = server.arg("code" + String(i));
    String bitrate = server.arg("bitrate" + String(i));

    strncpy(devices[i].name, name.c_str(), 32);
    strncpy(devices[i].protocol, protocol.c_str(), 16);
    strncpy(devices[i].code, code.c_str(), 32);
    strncpy(devices[i].bitrate, bitrate.c_str(), 8);
  }

  saveDevicesToEEPROM();

  server.sendHeader("Location", "/");
  server.send(303);
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

  for (int i = 0; i < MAX_DEVICES; i++) {
    if (devices[i].name[0] != 0) {
      fauxmo.addDevice(devices[i].name);
    }
  }

  fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value) {
    Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
    requestedDevice = device_id + 1;
    receivedState = state;
  });
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

void setup()
{
#if defined(ESP8266) && defined(ESP01_1M)
  pinMode(3, FUNCTION_3);
#endif

#if defined(ESP8266) && defined(ESP01_1M)
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#elif defined(ESP32)
  Serial.begin(115200);
#endif

  IPAddress local_IP(4, 4, 4, 4);
  IPAddress gateway(4, 4, 4, 4);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, password);

  WiFi.softAPConfig(local_IP, gateway, subnet);

  if (!MDNS.begin("iralexa")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started");
    Serial.print("Access Point IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  irsend.begin();

  pinMode(CONNECTED_LED, OUTPUT);
  digitalWrite(CONNECTED_LED, LOW);

  wifiManager.autoConnect(ssid, password);
  WiFi.hostname("IrAlexa");

  Serial.println("Connected to Wi-Fi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  loadDevicesFromEEPROM();

  for (int i = 0; i < MAX_DEVICES; i++)
  {
    if (devices[i].name[0] == 0)
    {
      break;
    }
    numDevices++;
  }

  server.on("/", handleRoot);
  server.on("/wifi", handleWiFi);
  server.on("/save", handleSave);
  server.on("/devices", handleDevices);
  server.on("/saveDevices", handleSaveDevices);
  server.begin();

  httpUpdater.setup(&server);

  setupFauxmo();
}

void loop() {
  fauxmo.handle();
  server.handleClient();

  digitalWrite(CONNECTED_LED, HIGH); // Turn on the LED when connected to the home network

  switch (requestedDevice) {
    case 0:
      break;
    default:
      for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].name[0] != 0 && requestedDevice == i + 1) {
          if (strcmp(devices[i].protocol, "SAMSUNG") == 0) {
            irsend.sendSAMSUNG(atol(devices[i].code), atol(devices[i].bitrate));
          } else if (strcmp(devices[i].protocol, "Epson") == 0) {
            irsend.sendEpson(atol(devices[i].code), atol(devices[i].bitrate));
          } else if (strcmp(devices[i].protocol, "NEC") == 0) {
            irsend.sendNEC(atol(devices[i].code), atol(devices[i].bitrate));
          } else {
            Serial.println("Unknown protocol");
          }
        }
      }
      break;
  }

  requestedDevice = 0;

  static unsigned long last = millis();
  if (millis() - last > 5000) {
    last = millis();
    Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
  }
}

void saveDevicesToEEPROM()
{
  EEPROM.begin(512);
  for (int i = 0; i < MAX_DEVICES; i++)
  {
    EEPROM.put(i * sizeof(Device), devices[i]);
  }
  EEPROM.commit();
  EEPROM.end();
}

void loadDevicesFromEEPROM()
{
  EEPROM.begin(512);
  for (int i = 0; i < MAX_DEVICES; i++)
  {
    Device device;
    EEPROM.get(i * sizeof(Device), device);
    devices[i] = device;
    // Initialize fields to empty strings if not set
    if (devices[i].name[0] == 0 || devices[i].name[0] == 0xff) strcpy(devices[i].name, "");
    if (devices[i].protocol[0] == 0 || devices[i].protocol[0] == 0xff) strcpy(devices[i].protocol, "");
    if (devices[i].code[0] == 0 || devices[i].code[0] == 0xff) strcpy(devices[i].code, "");
    if (devices[i].bitrate[0] == 0 || devices[i].bitrate[0] == 0xff) strcpy(devices[i].bitrate, "");
  }
  EEPROM.end();
}