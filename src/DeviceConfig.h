#ifndef DEVICECONFIG_H
#define DEVICECONFIG_H

#include <Arduino.h>
#include "fauxmoESP.h"  // Dołączamy pełną definicję fauxmoESP

// Definicja struktury urządzenia
struct Device {
  String deviceName;
  uint32_t irCode;
  uint8_t protocol; // 0: SAMSUNG, 1: EPSON, 2: Symphony
};

#define MAX_DEVICES 10

// Zmienne globalne – lista urządzeń oraz liczba dodanych urządzeń
extern Device devices[MAX_DEVICES];
extern uint8_t numDevices;

// Deklaracja instancji fauxmo
extern fauxmoESP fauxmo;

#endif // DEVICECONFIG_H
