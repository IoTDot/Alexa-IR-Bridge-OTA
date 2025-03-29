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

// -----------------------
// Definicje sprzętowe
// -----------------------
#define IRLED_PIN 4         // Dostosuj pin dla IR LED
const uint16_t IrLed = IRLED_PIN;
IRsend irsend(IrLed);

#define CONNECTED_LED 2
#define BOOT_BUTTON_PIN 0

// -----------------------
// Zmienne globalne – lista urządzeń
// -----------------------
Device devices[MAX_DEVICES];
uint8_t numDevices = 0;  // Początkowo pusta lista

// -----------------------
// Globalne obiekty
// -----------------------
fauxmoESP fauxmo;
WiFiManager wifiManager;
Ticker irTicker;

#if defined(ESP8266)
ESP8266WebServer server(80);
#else
WebServer server(80);
#endif

bool shouldSaveConfig = false;
bool configMode = false;  // Tryb konfiguracji – gdy true, uruchamiamy webowy interfejs

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
  // Wyciszamy ostrzeżenia o deprecjacji dla StaticJsonDocument
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<1024> doc;
  #pragma GCC diagnostic pop

  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();
  if (error) {
    Serial.println("Błąd odczytu konfiguracji JSON, lista urządzeń pusta");
    numDevices = 0;
    return;
  }
  JsonArray arr = doc["devices"].as<JsonArray>();
  numDevices = 0;
  for (JsonObject obj : arr) {
    if (numDevices >= MAX_DEVICES) break;
    devices[numDevices].deviceName = obj["name"].as<String>();
    const char* irStr = obj["ircode"];
    devices[numDevices].irCode = strtoul(irStr, NULL, 16);
    devices[numDevices].protocol = obj["protocol"];
    numDevices++;
  }
  Serial.printf("Wczytano %d urządzeń z konfiguracji\n", numDevices);
}

void saveDevicesConfig() {
  // Wyciszamy ostrzeżenia o deprecjacji dla StaticJsonDocument
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  StaticJsonDocument<1024> doc;
  #pragma GCC diagnostic pop

  JsonArray arr = doc["devices"].to<JsonArray>();
  if (arr.isNull()) {
    doc["devices"] = JsonArray();
    arr = doc["devices"].to<JsonArray>();
  }
  for (uint8_t i = 0; i < numDevices; i++) {
    // Używamy metody add<JsonObject>() zamiast createNestedObject()
    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = devices[i].deviceName;
    char buffer[9];
    sprintf(buffer, "%X", devices[i].irCode);
    obj["ircode"] = buffer;
    obj["protocol"] = devices[i].protocol;
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
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  pinMode(CONNECTED_LED, OUTPUT);
  digitalWrite(CONNECTED_LED, HIGH);

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  if (!wifiManager.autoConnect("IrAlexa", "iralexa123")) {
    Serial.println("Nie udało się połączyć, timeout");
    delay(3000);
  } else {
    Serial.println("Połączono z WiFi");
    if (shouldSaveConfig) {
      Serial.println("Konfiguracja została zapisana");
      ESP.restart();
      delay(5000);
    }
  }
  Serial.printf("[WIFI] SSID: %s, IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

void setupFauxmo() {
  fauxmo.createServer(true);
  fauxmo.setPort(80);
  fauxmo.enable(true);
  for (uint8_t i = 0; i < numDevices; i++) {
    fauxmo.addDevice(devices[i].deviceName.c_str());
    Serial.printf("Fauxmo: Dodano urządzenie: %s\n", devices[i].deviceName.c_str());
  }
  fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value) {
    Serial.printf("[FAUXMO] Alexa wysłała komendę do urządzenia #%d (%s): %s, wartość: %d\n",
                  device_id, device_name, state ? "ON" : "OFF", value);
    // Wywołaj wysłanie sygnału IR lub inne działania
  });
}

void sendIRSignal(const Device &device) {
  switch (device.protocol) {
    case 0:
      irsend.sendSAMSUNG(device.irCode, 32);
      break;
    case 1:
      irsend.sendEpson(device.irCode, 32);
      break;
    case 2:
      irsend.sendSymphony(device.irCode, 12);
      break;
    default:
      Serial.println("Nieznany protokół");
      break;
  }
}

void processIR() {
  // Przykładowa funkcja przetwarzająca wysłanie sygnału IR – do uzupełnienia wg potrzeb
}

void handleButton() {
  static uint8_t pressCount = 0;
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BOOT_BUTTON_PIN);
  if (currentButtonState != lastButtonState) {
    delay(50);
    currentButtonState = digitalRead(BOOT_BUTTON_PIN);
  }
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    pressCount++;
    if (pressCount >= 3) {
      configMode = !configMode;
      if (configMode) {
        Serial.println("Włączono tryb konfiguracji");
        // Wyłączamy fauxmo, aby zwolnić port 80
        fauxmo.enable(false);
        setupWebInterface(server);
      } else {
        Serial.println("Wyłączono tryb konfiguracji");
        stopWebInterface(server);
        // Przywracamy działanie fauxmo
        fauxmo.enable(true);
      }
      pressCount = 0;
    }
  }
  lastButtonState = currentButtonState;
}


void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CONNECTED_LED, OUTPUT);
  digitalWrite(CONNECTED_LED, LOW);
  irsend.begin();
  setupWiFi();
  if (!LittleFS.begin()) {
    Serial.println("Błąd montowania LittleFS");
  }
  loadDevicesConfig();
  setupFauxmo();
  irTicker.attach_ms(100, processIR);
}

void loop() {
  handleButton();
  if (!configMode) {
    fauxmo.handle();
  } else {
    server.handleClient();
  }
  digitalWrite(CONNECTED_LED, (WiFi.status() == WL_CONNECTED));
}