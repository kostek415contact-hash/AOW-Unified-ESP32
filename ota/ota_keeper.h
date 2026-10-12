#pragma once
// ======================================================================
// ota/ota_keeper.h
// EN: Decides when to re-announce ArduinoOTA/mDNS (name stops resolving after IP changes or interface loss).
// PL: Decyduje, kiedy ponownie ogłosić ArduinoOTA/mDNS (nazwa przestaje się rozwiązywać po zmianie IP lub
//     utracie interfejsu).
// Tests: tests/test_ota_keeper.cpp (host, plain C++).
// Used by: ota/arduino_ota.h
// ======================================================================
#include <stdint.h>

// Decision on when to restart the ArduinoOTA/mDNS announcement (pure logic, host-tested).
// The symptom it fixes: right after startup the network port in the Arduino IDE and the <name>.local address work, and after a
// while they vanish (mDNS stops responding, e.g. after an IP change or loss of the interface), although the panel
// at the IP address works. A restart is cheap (a few ms) and safe, as long as an upload is not in progress.
namespace OtaKeeper {

static const uint32_t REFRESH_MS = 120000UL;     // periodic refresh (the mDNS record TTL is ~120 s)
static const uint32_t RETRY_MS   = 3000UL;       // after an IP change, no more often than this

enum Reason : uint8_t { NONE = 0, FIRST_IP = 1, IP_CHANGED = 2, IP_LOST = 3, PERIODIC = 4 };

struct State {
    uint32_t lastIp = 0;          // IP address at the last start (0 = none)
    uint32_t lastInitMs = 0;
    bool     everInit = false;
};

// ip: current address (0 = no network). Returns the restart reason or NONE.
// blocked = an upload is in progress or steering is enabled (then we touch nothing).
inline Reason decide(const State& s, uint32_t ip, uint32_t nowMs, bool blocked) {
    if (blocked) return NONE;
    if (ip == 0) return NONE;                                  // nothing to announce without an address; after it returns there will be IP_CHANGED
    const uint32_t since = nowMs - s.lastInitMs;
    if (!s.everInit) return FIRST_IP;
    if (ip != s.lastIp && since >= RETRY_MS) return IP_CHANGED;
    if (since >= REFRESH_MS) return PERIODIC;
    return NONE;
}

inline void markInit(State& s, uint32_t ip, uint32_t nowMs) { s.lastIp = ip; s.lastInitMs = nowMs; s.everInit = true; }

}  // namespace OtaKeeper
