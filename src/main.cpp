#include <Arduino.h>
#include "fauxmoESP.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
#else
  #include <WiFi.h>
  #include <WebServer.h>
#endif

#include "DeviceConfig.h"
#include "webInterface.h"
#include "Protocols.h"

#define IRLED_PIN 4
IRsend irsend(IRLED_PIN);

#define CONNECTED_LED 2
#define BOOT_BUTTON_PIN 0

Device devices[MAX_DEVICES];
uint8_t numDevices = 0;

fauxmoESP fauxmo;
WiFiManager wifiManager;
Ticker irTicker;

#if defined(ESP8266)
ESP8266WebServer configServer(8080);
#else
WebServer configServer(8080);
#endif

bool shouldSaveConfig = false;
bool configMode = false;
const char* CONFIG_FILE = "/config.json";

void loadDevicesConfig() {
  if (!LittleFS.begin()) {
    Serial.println("Błąd montowania LittleFS");
    return;
  }
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println("Plik konfiguracyjny nie istnieje, lista urządzeń pusta");
    numDevices = 0;
    return;
  }
  File configFile = LittleFS.open(CONFIG_FILE, "r");
  if (!configFile) {
    Serial.println("Nie udało się otworzyć pliku konfiguracyjnego");
    return;
  }
  JsonDocument doc;
  auto error = deserializeJson(doc, configFile);
  configFile.close();
  if (error) {
    Serial.println("Błąd odczytu konfiguracji JSON, lista urządzeń pusta");
    numDevices = 0;
    return;
  }
  JsonArray arr = doc.as<JsonArray>();
  numDevices = 0;
  for (JsonObject obj : arr) {
    if (numDevices >= MAX_DEVICES) break;
    devices[numDevices].deviceName = obj["name"].as<String>();
    const char* irStr = obj["ircode"];
    devices[numDevices].irCode = strtoul(irStr, nullptr, 16);
    uint8_t p = obj["protocol"].as<uint8_t>();
    devices[numDevices].protocol = p;
    if (!obj["bits"].isNull()) { // Or alternatively: if (obj["bits"])
      devices[numDevices].bits = obj["bits"].as<uint8_t>();
    } else {
      // Key "bits" doesn't exist or is null, use default
      devices[numDevices].bits = PROTOCOLS[p].bits;
    }
    numDevices++;
  }
  Serial.printf("Wczytano %d urządzeń z konfiguracji\n", numDevices);
}

void saveDevicesConfig() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (uint8_t i = 0; i < numDevices; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["name"]     = devices[i].deviceName;
    char buf[9];
    snprintf(buf, sizeof(buf), "%08X", devices[i].irCode);
    obj["ircode"]   = buf;
    obj["protocol"] = devices[i].protocol;
    obj["bits"]     = devices[i].bits;
  }
  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("Nie udało się otworzyć pliku do zapisu konfiguracji");
    return;
  }
  if (serializeJson(doc, configFile) == 0) {
    Serial.println("Błąd zapisu konfiguracji");
  } else {
    Serial.println("Konfiguracja zapisana w LittleFS");
  }
  configFile.close();
}

void saveConfigCallback() {
  shouldSaveConfig = true;
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  pinMode(CONNECTED_LED, OUTPUT);
  digitalWrite(CONNECTED_LED, HIGH);
  IPAddress apIP(4, 4, 4, 4);
  IPAddress gateway(4, 4, 4, 4);
  IPAddress subnet(255, 255, 255, 0);
  wifiManager.setAPStaticIPConfig(apIP, gateway, subnet);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  if (!wifiManager.autoConnect("IrAlexa")) {
    Serial.println("Nie udało się połączyć, timeout");
    delay(3000);
  } else {
    Serial.println("Połączono z WiFi");
    if (shouldSaveConfig) {
      ESP.restart();
      delay(5000);
    }
  }
  Serial.printf("[WIFI] SSID: %s, IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

void sendIRSignal(const Device &dev) {
  if (dev.protocol < PROTOCOL_COUNT) {
    const auto &p = PROTOCOLS[dev.protocol];
    p.send(dev.irCode, dev.bits);
  } else {
    Serial.println("Nieznany protokół");
  }
}

void setupFauxmo() {
  Serial.println("[fauxmo] setupFauxmo() start");
  fauxmo.createServer(true);
  Serial.println("[fauxmo] createServer(true) done");
  fauxmo.setPort(80);
  Serial.printf("[fauxmo] setPort(%d)\n", 80);
  fauxmo.enable(true);
  Serial.println("[fauxmo] enable(true)");
  for (uint8_t i = 0; i < numDevices; i++) {
    fauxmo.addDevice(devices[i].deviceName.c_str());
    Serial.printf("[fauxmo] addDevice(%s)\n", devices[i].deviceName.c_str());
  }
  fauxmo.onSetState([](unsigned char id, const char* name, bool state, unsigned char val) {
    Serial.printf("[fauxmo] onSetState: id=%d name=%s state=%d\n", id, name, state);
    if (id < numDevices) sendIRSignal(devices[id]);
  });
  Serial.println("[fauxmo] setupFauxmo() end");
}

void processIR() {}

void handleButton() {
  static uint8_t pressCount = 0;
  static bool lastButtonState = HIGH;
  static unsigned long pressStartTime = 0;
  bool current = digitalRead(BOOT_BUTTON_PIN);
  if (current != lastButtonState) {
    delay(50); current = digitalRead(BOOT_BUTTON_PIN);
  }
  if (current == LOW) {
    if (lastButtonState == HIGH) pressStartTime = millis();
    if (millis() - pressStartTime >= 5000) {
      wifiManager.resetSettings();
      WiFi.disconnect(true);
      delay(100);
      ESP.restart();
    }
  }
  if (current == HIGH && lastButtonState == LOW) {
    if (millis() - pressStartTime < 5000) {
      pressCount++;
      if (pressCount >= 3) {
        configMode = !configMode;
        if (configMode) {
          fauxmo.enable(false);
          setupWebInterface(configServer);
          configServer.begin();
        } else {
          configServer.close();
          fauxmo.enable(true);
        }
        pressCount = 0;
      }
    }
    pressStartTime = 0;
  }
  lastButtonState = current;
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CONNECTED_LED, OUTPUT);
  digitalWrite(CONNECTED_LED, LOW);
  irsend.begin();
  setupWiFi();
  if (!LittleFS.begin()) Serial.println("Błąd montowania LittleFS");
  loadDevicesConfig();
  setupFauxmo();
  irTicker.attach_ms(100, processIR);
}

void loop() {
  handleButton();
  if (configMode) configServer.handleClient();
  else fauxmo.handle();
  digitalWrite(CONNECTED_LED, WiFi.status() == WL_CONNECTED);
}
