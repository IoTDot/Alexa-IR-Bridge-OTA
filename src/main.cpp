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

const struct Device
{
  const char *deviceName;
  uint32_t irCode;
  uint8_t protocol; // 0 for SAMSUNG, 1 for EPSON
} devices[] = {
    {"TV", 0xE0E040BF, 0},
    {"Skip", 0xE0E016E9, 0},
    {"Mute", 0x8322EE11, 1},
    {"Plus", 0x8322E21D, 1},
    {"Minus", 0x8322E31C, 1},
    {"Speakers", 0x8322E11E, 1}};

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

  wifiManager.setAPStaticIPConfig(IPAddress(4, 4, 4, 4), IPAddress(4, 4, 4, 4), IPAddress(255, 255, 255, 0));
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
  fauxmo.handle();

  if (requestedDevice > 0 && requestedDevice <= numDevices)
  {
    const Device *device = &devices[requestedDevice - 1];
    if (device->protocol == 0)
    {
      irsend.sendSAMSUNG(device->irCode, 32);
    }
    else
    {
      irsend.sendEpson(device->irCode, 32);
    }
  }

  requestedDevice = 0;

  digitalWrite(CONNECTED_LED, (WiFi.status() == WL_CONNECTED));
}