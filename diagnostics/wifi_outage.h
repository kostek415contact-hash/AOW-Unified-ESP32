#pragma once
// ======================================================================
// diagnostics/wifi_outage.h
// EN: Measures WiFi outage duration (last / longest / average / ongoing); the first connection after boot is
//     not an outage.
// PL: Mierzy czas zaniku WiFi (ostatni / najdłuższy / średni / trwający); pierwsze połączenie po starcie nie
//     jest zanikiem.
// Tests: tests/test_wifi_outage.cpp (host, plain C++).
// Used by: connectivity/wifi_handler.h
// ======================================================================
// Measuring WiFi connection outages and the TIME TO RECOVER. Pure C++ (no Arduino), host-tested
// (tests/test_wifi_outage.cpp). WifiHandler::maintain() reports on every loop pass whether the network is connected.
//
// WHY: a counter of "how many times the connection vanished" does not say how LONG there was no connectivity. And it is the outage length that decides
// whether AgOpenWeb manages to react (a gap > about 2 s = loss of the GPS module). The measurement also shows whether changes to connecting
// (fast scan, order of attempts) really shorten recovery.
// The first connection after startup is NOT an outage (outages are counted only from the first successful connection).
#include <cstdint>

class WifiOutage {
public:
    // Call on every pass. Returns true EXACTLY once: at the moment of recovery after an outage (the time is in lastMs()).
    bool update(bool connected, uint32_t nowMs) {
        if (connected) {
            if (!everConnected_) { everConnected_ = true; down_ = false; return false; }
            if (down_) {
                down_ = false;
                const uint32_t d = nowMs - downSince_;
                last_ = d; total_ += d; count_++;
                if (d > max_) max_ = d;
                return true;
            }
            return false;
        }
        if (everConnected_ && !down_) { down_ = true; downSince_ = nowMs; }
        return false;
    }
    bool inOutage() const { return down_; }
    uint32_t outageSoFarMs(uint32_t nowMs) const { return down_ ? nowMs - downSince_ : 0; }   // how long the current outage has lasted
    uint32_t count() const { return count_; }          // finished outages
    uint32_t lastMs() const { return last_; }          // duration of the last finished outage (0 = none)
    uint32_t maxMs() const { return max_; }
    uint32_t avgMs() const { return count_ ? (uint32_t)(total_ / count_) : 0; }

private:
    bool everConnected_ = false, down_ = false;
    uint32_t downSince_ = 0, last_ = 0, max_ = 0, count_ = 0;
    uint64_t total_ = 0;
};
