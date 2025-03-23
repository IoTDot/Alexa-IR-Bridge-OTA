#include <Arduino.h>
#include "fauxmoESP.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <WiFiManager.h>
#include <Ticker.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

#define IRLED_PIN IRLED
const uint16_t IrLed = IRLED_PIN;
IRsend irsend(IrLed);

#define CONNECTED_LED 2
#define BOOT_BUTTON_PIN 0

struct Device {
  const char *deviceName;
  uint32_t irCode;
  uint8_t protocol; // 0: SAMSUNG, 1: EPSON, 2: Symphony
};

Device devices[] = {
  {"TV", 0xE0E040BF, 0},
  {"Skip", 0xE0E016E9, 0},
  {"Mute", 0x8322EE11, 1},
  {"Speaker Plus", 0x8322E21D, 1},
  {"Speaker Minus", 0x8322E31C, 1},
  {"Speakers", 0x8322E11E, 1},
  {"Fan", 0xD82, 2},
  {"Fan 1", 0xD81, 2}
};

#define numDevices (sizeof(devices) / sizeof(Device))

volatile unsigned int requestedDevice = 0;
volatile boolean receivedState = false;

fauxmoESP fauxmo;
WiFiManager wifiManager;

Ticker irTicker;

bool shouldSaveConfig = false;
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  pinMode(CONNECTED_LED, OUTPUT);
  digitalWrite(CONNECTED_LED, HIGH);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Ustawienie portalu konfiguracji z hasłem "iralexa123"
  if (!wifiManager.autoConnect("IrAlexa", "iralexa123")) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
  } else {
    Serial.println("Connected...");
    if (shouldSaveConfig) {
      Serial.println("Config saved");
      ESP.restart();
      delay(5000);
    }
  }

  digitalWrite(CONNECTED_LED, HIGH);
  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

void setupFauxmo() {
  fauxmo.createServer(true);
  fauxmo.setPort(80);
  fauxmo.enable(true);

  for (unsigned int i = 0; i < numDevices; i++) {
    fauxmo.addDevice(devices[i].deviceName);
  }

  fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value) {
    Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
    requestedDevice = device_id + 1;
    receivedState = state;
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

// Funkcja wywoływana asynchronicznie przez Ticker
void processIR() {
  if (requestedDevice > 0 && requestedDevice <= numDevices) {
    const Device &device = devices[requestedDevice - 1];
    sendIRSignal(device);
    requestedDevice = 0;
  }
}

void handleButton() {
  static unsigned long buttonPressStart = 0;
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BOOT_BUTTON_PIN);

  // Debounce: sprawdzamy zmianę stanu z niewielkim opóźnieniem
  if (currentButtonState != lastButtonState) {
    delay(50);
    currentButtonState = digitalRead(BOOT_BUTTON_PIN);
  }

  if (currentButtonState == LOW) {
    if (buttonPressStart == 0) {
      buttonPressStart = millis();
    } else if (millis() - buttonPressStart >= 3000) {
      Serial.println("Przycisk przytrzymany 3 sekundy, uruchamiam konfigurator...");
      wifiManager.resetSettings();
      WiFi.disconnect(true);
      WiFi.mode(WIFI_AP);
      // Portal konfiguracji zabezpieczony hasłem "iralexa"
      wifiManager.startConfigPortal("IrAlexa", "iralexa");
      buttonPressStart = 0;
    }
  } else {
    buttonPressStart = 0;
  }
  lastButtonState = currentButtonState;
}

void setup() {
#if defined(ESP01_1M)
  pinMode(3, FUNCTION_3);
#endif

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  irsend.begin();

#if defined(ESP01_1M)
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#elif defined(ESP8266) || defined(ESP32)
  Serial.begin(115200);
#endif

  setupWiFi();
  setupFauxmo();

  // Ustawiamy Ticker do asynchronicznego przetwarzania komend IR co 100 ms
  irTicker.attach_ms(100, processIR);
}

void loop() {
  handleButton();
  fauxmo.handle();
  // Inne zadania mogą być wykonywane tutaj - Ticker działa asynchronicznie
  digitalWrite(CONNECTED_LED, (WiFi.status() == WL_CONNECTED));
}
