#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#ifndef IRLED
#error "PIN IR LED not defined!"
#endif

#define IRLED_PIN IRLED

#include "fauxmoESP.h"              

#include <IRremoteESP8266.h>

#include <IRsend.h>

#define CONNECTED_LED 2

const uint16_t IrLed = IRLED_PIN;

IRsend irsend(IrLed);

#define WIFI_SSID "Trojan_test_v2"
#define WIFI_PASS "$j2vFHjW^tM!JV2$vw!9tGaM"

const char * devices[] = {
  
    "TestA",
    "TestB",
};

#define numDevices (sizeof(devices)/sizeof(char *))

volatile int requestedDevice = 0;
volatile boolean receivedState = false;

fauxmoESP fauxmo;

void wifiSetup() {
  WiFi.mode(WIFI_STA);

  Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  digitalWrite(CONNECTED_LED, LOW);

  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

void setup() {
  #if defined(ESP8266) && defined(ESP01_1M)
  pinMode(3, FUNCTION_3); // Wykonaj tylko dla ESP01_1M
  #endif

  irsend.begin();

  #if defined(ESP8266) && defined(ESP01_1M)
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

  #elif defined(ESP32)
  Serial.begin(115200);
  #endif
  
  pinMode(CONNECTED_LED, OUTPUT);
  digitalWrite(CONNECTED_LED, HIGH);

  wifiSetup();

  fauxmo.createServer(true);
  fauxmo.setPort(80);

  fauxmo.enable(true);

  for (unsigned int i = 0; i < numDevices; i++) {
    fauxmo.addDevice(devices[i]);
  }


  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {

    Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
    
    requestedDevice = device_id + 1;
    receivedState = state;
  });

}

void loop() {
  fauxmo.handle();
  
  switch (requestedDevice) {
    case 0: break;
    
    case 1: irsend.sendSAMSUNG(0xE0E040BF, 32);
            break;

    case 2: irsend.sendEpson(0x8322E11E, 32);
            break;
  }

  requestedDevice = 0;

    digitalWrite(CONNECTED_LED,  (WiFi.status() != WL_CONNECTED));
}
