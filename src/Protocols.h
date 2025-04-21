#pragma once
#include <cstdint>

// Typ funkcji wysyłającej sygnał IR:
using IRSendFunc = void(*)(uint32_t code, uint8_t bits);

// Struktura opisu protokołu
struct ProtocolInfo {
  const char*   name;
  uint8_t       bits;
  IRSendFunc    send;
};

// Rejestr protokołów i jego rozmiar
extern const ProtocolInfo PROTOCOLS[];
extern const uint8_t         PROTOCOL_COUNT;

const uint16_t ALLOWED_BITS[] = {
  12, 13, 15, 16, 24, 28, 32, 36, 48
};
const uint8_t ALLOWED_BITS_COUNT = sizeof(ALLOWED_BITS) / sizeof(uint16_t);