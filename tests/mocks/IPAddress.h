#pragma once
// EN: Host stand-in for IPAddress (only what the firmware headers use).  PL: Zamiennik IPAddress na komputerze.
#include "Arduino.h"
class IPAddress {
public:
    uint8_t b[4] = {0, 0, 0, 0};
    IPAddress() {}
    IPAddress(uint8_t a, uint8_t b1, uint8_t c, uint8_t d) { b[0] = a; b[1] = b1; b[2] = c; b[3] = d; }
    String toString() const { char t[20]; snprintf(t, sizeof t, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]); return String(t); }
    bool operator==(const IPAddress& o) const { return memcmp(b, o.b, 4) == 0; }
    uint8_t operator[](int i) const { return b[i]; }
};
