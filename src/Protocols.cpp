#include "Protocols.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>

// Globalna instancja IRsend z main.cpp:
extern IRsend irsend;

// Wrappers wywołań:
static void sendSamsung( uint32_t c, uint8_t b ) { irsend.sendSAMSUNG(c, b); }
static void sendEpson(   uint32_t c, uint8_t b ) { irsend.sendEpson(  c, b); }
static void sendSymphony(uint32_t c, uint8_t b ) { irsend.sendSymphony(c, b); }

// Wypełnij: każdy nowy protokół dodaj jako kolejny element
const ProtocolInfo PROTOCOLS[] = {
  { "SAMSUNG",  32, sendSamsung  },
  { "EPSON",    32, sendEpson    },
  { "SYMPHONY", 12, sendSymphony }
};

const uint8_t PROTOCOL_COUNT = sizeof(PROTOCOLS) / sizeof(PROTOCOLS[0]);

