#include <Arduino.h>
#include "fauxmoESP.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>

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

const char* ssid = "IrAlexa";
const char* password = "12345678";
const char* homeSSID = "test_2.7";
const char* homePassword = "zaq1@WSX";

#if defined(ESP8266)
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
WebServer server(80);
HTTPUpdateServer httpUpdater;
#endif

unsigned long hotspotStartTime = 0;
const unsigned long hotspotDuration = 15000; // 15 seconds
bool clientConnected = false;

#define CONNECTED_LED 2
const uint16_t IrLed = IRLED_PIN;
IRsend irsend(IrLed);

const char* devices[] = {
  "TV",
  "Skip",
  "Mute",
  "Plus",
  "Minus",
  "Speakers",
};

#define numDevices (sizeof(devices)/sizeof(char *))

volatile int requestedDevice = 0;
volatile boolean receivedState = false;

fauxmoESP fauxmo;

void handleRoot() {
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
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  // Create the hotspot
  WiFi.softAP(ssid, password);
  Serial.println("Hotspot created");

  // Start the web server
  server.on("/", handleRoot);
  server.begin();

  // Start the OTA update server
  httpUpdater.setup(&server);

  hotspotStartTime = millis();

  irsend.begin();
  #if defined(ESP8266) && defined(ESP01_1M)
  pinMode(3, FUNCTION_3); // Wykonaj tylko dla ESP01_1M
  #endif

  #if defined(ESP8266) && defined(ESP01_1M)
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  #elif defined(ESP32)
  Serial.begin(115200);
  #endif
}

void loop() {
  // Check if the hotspot is active
  if (WiFi.getMode() == WIFI_AP) {
    server.handleClient();

    // Check if a client is connected
    if (WiFi.softAPgetStationNum() > 0) {
      clientConnected = true;
    }

    // Check if the hotspot duration has elapsed and no client is connected
    if (millis() - hotspotStartTime >= hotspotDuration && !clientConnected) {
      WiFi.softAPdisconnect();
      Serial.println("Hotspot closed");

      // Connect to home network
      WiFi.begin(homeSSID, homePassword);
      Serial.println("Connecting to home network");

      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to home network...");
      }

      Serial.println("Connected to home network");

      // Start your program here
      irsend.begin();

      // Create and enable Fauxmo server
      fauxmo.createServer(true);
      fauxmo.setPort(80);
      fauxmo.enable(true);

      for (unsigned int i = 0; i < numDevices; i++) {
        fauxmo.addDevice(devices[i]);
      }
      fauxmo.onSetState([](unsigned char device_id, const char* device_name, bool state, unsigned char value) {
        Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
        requestedDevice = device_id + 1;
        receivedState = state;
      });
    }
  } else {
    fauxmo.handle();


switch (requestedDevice) {
case 0:
break;
case 1: irsend.sendSAMSUNG(0xE0E040BF, 32); // TV on/off
break;
case 2: irsend.sendSAMSUNG(0xE0E016E9, 32); // TV ok/skip
break;
case 3: irsend.sendEpson(0x8322EE11, 32); // Speakers mute
break;
case 4: irsend.sendEpson(0x8322E21D, 32); // Speakers Vol_Up
break;
case 5: irsend.sendEpson(0x8322E31C, 32); // Speakers Vol_Down
break;
case 6: irsend.sendEpson(0x8322E11E, 32); // Speakers on/off
break;
}

requestedDevice = 0; // Reset requestedDevice before re-entering loop()

    static unsigned long last = millis();     
    if (millis() - last > 5000) {
      last = millis();
      Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
      digitalWrite(CONNECTED_LED,  (WiFi.status() != WL_CONNECTED));
    }
  }
}