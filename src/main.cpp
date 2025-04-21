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

// Serwer fauxmo na porcie 80
#if defined(ESP8266)
ESP8266WebServer server(80);
#else
WebServer server(80);
#endif

// Serwer konfiguracji na porcie 8080
#if defined(ESP8266)
ESP8266WebServer configServer(8080);
#else
WebServer configServer(8080);
#endif

bool shouldSaveConfig = false;
bool configMode = false;  // Tryb konfiguracji – gdy true, uruchamiamy webowy interfejs

const char* CONFIG_FILE = "/config.json";

// -----------------------
// Funkcje obsługi konfiguracji urządzeń
// -----------------------
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

// -----------------------
// Funkcja połączenia WiFi i hotspot
// -----------------------
void setupWiFi() {
    WiFi.mode(WIFI_STA);
    pinMode(CONNECTED_LED, OUTPUT);
    digitalWrite(CONNECTED_LED, HIGH);

    // Ustawienia AP (hotspot) dostęp z przeglądarki po adresie 4.4.4.4
    IPAddress apIP(4, 4, 4, 4);
    IPAddress gateway(4, 4, 4, 4);
    IPAddress subnet(255, 255, 255, 0);
    wifiManager.setAPStaticIPConfig(apIP, gateway, subnet);

    // Hotspot bez hasła
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    if (!wifiManager.autoConnect("IrAlexa")) {
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

// -----------------------
// Funkcja wysyłająca sygnał IR
// -----------------------
void sendIRSignal(const Device &device) {
    switch (device.protocol) {
        case 0: irsend.sendSAMSUNG(device.irCode, 32);   break;
        case 1: irsend.sendEpson(device.irCode, 32);    break;
        case 2: irsend.sendSymphony(device.irCode, 12); break;
        default: Serial.println("Nieznany protokół");      break;
    }
}

// -----------------------
// Inicjalizacja Fauxmo (Alexa emulation)
// -----------------------
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

        if (device_id < numDevices) {
            sendIRSignal(devices[device_id]);
        } else {
            Serial.println("Nieprawidłowy numer urządzenia");
        }
    });
}

// -----------------------
// Przetwarzanie IR (placeholder)
// -----------------------
void processIR() {
    // Placeholder: przetwarzanie sygnałów IR, jeżeli potrzebne w przyszłości
}

// -----------------------
// Obsługa przycisku BOOT
// -----------------------
// 3 razy naciśnij przycisk, aby przełączyć tryb konfiguracji
// Interfejs jest dostępny po wejściu na adres IP ESP w przeglądarce plus port 8080
// np. http://192.168.50.245:8080
void handleButton() {
    static uint8_t  pressCount       = 0;
    static bool     lastButtonState  = HIGH;
    static unsigned long pressStartTime = 0;

    bool currentButtonState = digitalRead(BOOT_BUTTON_PIN);

    if (currentButtonState != lastButtonState) {
        delay(50);
        currentButtonState = digitalRead(BOOT_BUTTON_PIN);
    }

    // Długie przytrzymanie = reset WiFi
    if (currentButtonState == LOW) {
        if (lastButtonState == HIGH) {
            pressStartTime = millis();
        }
        if (millis() - pressStartTime >= 5000) {
            Serial.println("Przytrzymanie 5 sekund - reset Wi-Fi");
            wifiManager.resetSettings();
            WiFi.disconnect(true);
            delay(100);
            ESP.restart();
        }
    }

    // Krótkie naciśnięcie = zmiana trybu
    if (currentButtonState == HIGH && lastButtonState == LOW) {
        if (millis() - pressStartTime < 5000) {
            pressCount++;
            if (pressCount >= 3) {
                configMode = !configMode;

                if (configMode) {
                    Serial.println("Włączono tryb konfiguracji");
                    fauxmo.enable(false);
                    setupWebInterface(configServer);
                    configServer.begin();

                    if (WiFi.status() == WL_CONNECTED) {
                        WiFi.hostname("iralexa");
                    }
                } else {
                    Serial.println("Wyłączono tryb konfiguracji");
                    configServer.close();
                    fauxmo.enable(true);
                }

                pressCount = 0;
            }
        }
        pressStartTime = 0;
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

    if (configMode) {
        configServer.handleClient();
    } else {
        fauxmo.handle();
    }

    digitalWrite(CONNECTED_LED, (WiFi.status() == WL_CONNECTED));
}