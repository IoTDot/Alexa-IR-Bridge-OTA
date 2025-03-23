#include <Arduino.h>
#include "fauxmoESP.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <WiFiManager.h>

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

const struct Device
{
  const char *deviceName;
  uint32_t irCode;
  uint8_t protocol; // 0 for SAMSUNG, 1 for EPSON, 2 for Symphony
} devices[] = {
    {"TV", 0xE0E040BF, 0}, // TV Turn ON or OFF
    {"Skip", 0xE0E016E9, 0}, // TV OK button
    {"Mute", 0x8322EE11, 1}, // TV Mute
    {"Speaker Plus", 0x8322E21D, 1}, //Speakers Volume UP
    {"Speaker Minus", 0x8322E31C, 1}, //Speakers Volume Down
    {"Speakers", 0x8322E11E, 1}, // Speakers Turn ON or OFF
    {"Fan", 0xD82, 2}, // Fan ON
    {"Fan 1", 0xD81, 2}}; // Fan OFF

#define numDevices (sizeof(devices) / sizeof(Device))

volatile unsigned int requestedDevice = 0;
volatile boolean receivedState = false;

fauxmoESP fauxmo;

WiFiManager wifiManager;

bool shouldSaveConfig = false;

void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupWiFi()
{
  WiFi.mode(WIFI_STA);
  pinMode(CONNECTED_LED, OUTPUT);
  digitalWrite(CONNECTED_LED, HIGH);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  if (!wifiManager.autoConnect("IrAlexa"))
  {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
  }
  else
  {
    Serial.println("Connected...");
    if (shouldSaveConfig)
    {
      Serial.println("Config saved");
      ESP.restart();
      delay(5000);
    }
  }

  digitalWrite(CONNECTED_LED, HIGH);

  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

void setupFauxmo()
{
  fauxmo.createServer(true);
  fauxmo.setPort(80);
  fauxmo.enable(true);

  for (unsigned int i = 0; i < numDevices; i++)
  {
    fauxmo.addDevice(devices[i].deviceName);
  }

  fauxmo.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value)
                    {
    Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
      requestedDevice = device_id + 1;
      receivedState = state; });
}

void setup()
{
#if defined(ESP01_1M)
  pinMode(3, FUNCTION_3);
#endif

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  irsend.begin();

#if defined(ESP01_1M)
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#elif defined(ESP8266) && defined(ESP32)
  Serial.begin(115200);
#endif

  setupWiFi();
  setupFauxmo();
}

void loop()
{
  static unsigned long buttonPressStart = 0;
  // Sprawdzamy stan przycisku (zakładamy, że przycisk aktywny jest stan niski)
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    // Jeśli przycisk został właśnie wciśnięty, zapisz czas
    if (buttonPressStart == 0) {
      buttonPressStart = millis();
    }
    // Jeśli przycisk jest już przytrzymany dłużej niż 5 sekund
    else if (millis() - buttonPressStart >= 3000) {
      Serial.println("Przycisk przytrzymany 3 sekund, uruchamiam konfigurator...");
      // Usuń zapisane dane WiFi
      wifiManager.resetSettings();
      // Rozłącz z bieżącą siecią i przełącz na tryb wyłącznie AP
      WiFi.disconnect(true);
      WiFi.mode(WIFI_AP);
      // Uruchom konfigurator, który domyślnie pojawi się na 192.168.4.1
      wifiManager.startConfigPortal("IrAlexa");
      // Zresetuj licznik przycisku
      buttonPressStart = 0;
    }
  } else {
    // Jeśli przycisk nie jest wciśnięty, resetujemy licznik
    buttonPressStart = 0;
  }

  fauxmo.handle();

  if (requestedDevice > 0 && requestedDevice <= numDevices)
  {
    const Device *device = &devices[requestedDevice - 1];
    if (device->protocol == 0)
    {
      irsend.sendSAMSUNG(device->irCode, 32);
    }
    else if (device->protocol == 1)
    {
      irsend.sendEpson(device->irCode, 32);
    }
    else if (device->protocol == 2)
    {
      irsend.sendSymphony(device->irCode, 12);
    }
  }

  requestedDevice = 0;

  digitalWrite(CONNECTED_LED, (WiFi.status() == WL_CONNECTED));
}