/*  Limited IR remote control for a Samsung TV via Amazon Echo Dot and Alexa.                                     *
 *  (c) Dec 2019 vwlowen.co.uk.                                                                              *
 *  Based on fauxmoESP examples. Original fauxmoESP comments left in place to assist with modifications. */
 

#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#else
#error "Not supported board!"
#endif

#ifndef IRLED
#error "PIN IR LED not defined!"
#endif

#define IRLED_PIN IRLED

#include "fauxmoESP.h"              //  https://bitbucket.org/xoseperez/fauxmoesp/src/master/
                                    // (Download: https://bitbucket.org/xoseperez/fauxmoesp/downloads/  )

                                    // fauxmoESP requires a support library from:
                                    // https://github.com/me-no-dev/ESPAsyncTCP

                                    // Install both libraries in the usual way in the Arduino\libraries\ folder.

                                    // In Arduino IDE Tools:  LwIP Variant needs to be set to "v1.4 Higher Bandwidth"

                                    // In Arduino IDE Tools | Boards | Board Manager, switch to ESP8266 Board version 2.3.0
                                    

#include <IRremoteESP8266.h>        // https://github.com/crankyoldgit/IRremoteESP8266

#include <IRsend.h>

#define CONNECTED_LED 2             // GPIO 2  (WeMos D4). LED OFF when Wifi is connected.

const uint16_t IrLed = IRLED_PIN;           // ESP8266 GPIO pin to use for IR LEDs. Recommended: 4 (WeMos D2).

IRsend irsend(IrLed);               // 

#define WIFI_SSID "Trojan_test_v2"       // Your WiFi  SSID.
#define WIFI_PASS "$j2vFHjW^tM!JV2$vw!9tGaM"      // Your WiFi password.

const char * devices[] = {          // Define device names for Alexa voice recognition.
  
   "TV",
   "Skip",
   "Mute",
   "Plus",
   "Minus",
   "Speakers",

};

//  *Note* "Channel on" and "Channel off" can be given 'aliases' - "Channel up" and "Channel down"
//  by setting up two 'Routines' in the Alexa App.  "Alexa, channel up" will be translated to
//  "Channel ON" and "Alexa, channel down" will be translated to "Channel OFF" to send the
//  appropriate IR remote code.


#define numDevices (sizeof(devices)/sizeof(char *))    // Define number of devices in 'devices' array.

                                                       
// Two variables to hold data received from callback function in setup() for transferring to loop().

volatile int requestedDevice = 0;                      // Variable to hold device ID from callback function.
volatile boolean receivedState = false;                // True/false (On/Off) state from callback function.

fauxmoESP fauxmo;                                      // create fauxmo instance.

// Wi-Fi Connection
void wifiSetup() {
  // Set WIFI module to STA mode
  WiFi.mode(WIFI_STA);

  // Connect
  Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Wait
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  digitalWrite(CONNECTED_LED, LOW);

  // Connected!
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

  // Wi-Fi connection
  wifiSetup();

  
  // By default, fauxmoESP creates it's own webserver on the defined port
  // The TCP port must be 80 for gen3 devices (default is 1901)
  // This has to be done before the call to enable()
  fauxmo.createServer(true); // not needed, this is the default value
  fauxmo.setPort(80); // This is required for gen3 devices

  // You have to call enable(true) once you have a WiFi connection
  // You can enable or disable the library at any moment
  // Disabling it will prevent the devices from being discovered and switched

  fauxmo.enable(false);

  fauxmo.enable(true);
  
  // You can use different ways to invoke alexa to modify the devices state:
  // "Alexa, turn the TV on", "Alexa, turn on TV", "Alexa, TV on"

  // Add virtual devices

  for (unsigned int i = 0; i < numDevices; i++) {
    fauxmo.addDevice(devices[i]);
  }


  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
    // Callback when a command from Alexa is received. 
    // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
    // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
    // Just remember not to delay too much here, this is a callback, exit as soon as possible.
    // If you have to do something more involved here set a flag and process it in your main loop.
        
    Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);

    // Because there's no way to use the callback function variables in loop(), we assign global variables to the values received. 

    // We'll match received device by 'device_id' because it must be faster than comparing character arrays.

    requestedDevice = device_id + 1;     // Returned device_id's start at 0 so we add 1 so we can use 0
                                         // as a 'no device' flag in the main loop.
    receivedState = state;               // state is true or false which we use for IR Remote Up and Down.
  });

}

void loop() {
  // fauxmoESP uses an async TCP server but a sync UDP server
  // Therefore, we have to manually poll for UDP packets
  fauxmo.handle();
  
  switch (requestedDevice) {                              // Send IR codes. The IR codes and channel numbers
    case 0: break;                                        // shown here are for my TV! 

    case 1: irsend.sendSAMSUNG(0xE0E040BF, 32);           // TV on/off
            break;

    case 2: irsend.sendSAMSUNG(0xE0E016E9, 32);          // TV ok/skip
            break;

    case 3: irsend.sendEpson(0x8322EE11, 32);            // Speakers mute
            break;

    case 4: irsend.sendEpson(0x8322E21D, 32);            // Speakers Vol_Up
            break;

    case 5: irsend.sendEpson(0x8322E31C, 32);            // Speakers Vol_Down
            break;

    case 6: irsend.sendEpson(0x8322E11E, 32);            // Speakers on/off
            break;

                                                       
  }

  requestedDevice = 0;               // Reset requestedDevice before re-entering loop()

  static unsigned long last = millis();     
  if (millis() - last > 5000) {
    last = millis();
    Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
    digitalWrite(CONNECTED_LED,  (WiFi.status() != WL_CONNECTED));
    
  }
}
