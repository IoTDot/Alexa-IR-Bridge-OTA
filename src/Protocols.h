#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#include <cstdint>

/// Unikalne nazwy, żeby nie kolidowały z makrami
enum Protocol : uint8_t {
  PROTO_SAMSUNG = 0,
  PROTO_EPSON   = 1,
  PROTO_SYMPHONY= 2,

  PROTOCOL_COUNT
};

/// Tablice z nazwami i długościami bitów
static constexpr const char* PROTOCOL_NAMES[PROTOCOL_COUNT] = {
  "SAMSUNG",
  "EPSON",
  "SYMPHONY"
};

static constexpr uint8_t PROTOCOL_BITS[PROTOCOL_COUNT] = {
  32,   // SAMSUNG
  32,   // EPSON
  12    // SYMPHONY
};

#endif // PROTOCOLS_H