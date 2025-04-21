#ifndef DEVICECONFIG_H
#define DEVICECONFIG_H

#include <Arduino.h>
#include "fauxmoESP.h"  // Dołączamy pełną definicję fauxmoESP
#include "Protocols.h"

// Definicja struktury urządzenia
struct Device {
  String deviceName;
  uint32_t irCode;
  uint8_t  bits;
  Protocol protocol;
};

#define MAX_DEVICES 10

// Zmienne globalne – lista urządzeń oraz liczba dodanych urządzeń
extern Device devices[MAX_DEVICES];
extern uint8_t numDevices;

// Deklaracja instancji fauxmo
extern fauxmoESP fauxmo;

#endif // DEVICECONFIG_H