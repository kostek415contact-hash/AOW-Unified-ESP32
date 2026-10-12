#pragma once
// ======================================================================
// connectivity/wifi_fallback.h
// EN: Decides when to restart into the safe path after WiFi was lost during operation (instead of switching the
//     radio to AP live).
// PL: Decyduje, kiedy zrestartować do sprawdzonej ścieżki po utracie WiFi w trakcie pracy (zamiast przełączać
//     radio na AP „w locie”).
// Tests: tests/test_wifi_fallback.cpp (host, plain C++).
// Used by: connectivity/network_manager.h
// ======================================================================
// Logic for an emergency restart on loss of WiFi (station) WHILE RUNNING. Pure C++ (no Arduino),
// host-tested (tests/test_wifi_fallback.cpp).
//
// WHY A RESTART, rather than switching the radio to AP "on the fly": switching station -> AP with
// the web server, OTA and open sockets (UDP, NTRIP) running ended in a core 0 panic
// (LoadProhibited, null pointer) - observed in the log. A restart enters the PROVEN startup
// path (networkManagerBegin): there connecting to the station takes WIFI_CONNECT_TIMEOUT_MS, and if there is still no
// network, our own access point starts - before the web server and sockets start.
//
// Protection against a restart loop: a counter in RTC memory survives a software restart. After
// MAX_RESTARTS restarts in a row (the network connects at startup, but vanishes right away) we stop
// restarting and just keep trying to connect. The counter is reset after STABLE_MS of continuous connectivity.
#include <cstdint>

namespace WifiFallback {

    static const uint32_t MAGIC = 0xA0C0FB01UL;      // marker "this restart was ordered by the WiFi emergency mechanism"
    static const uint32_t MAX_RESTARTS = 2;
    static const uint32_t STABLE_MS = 120000UL;
    static const uint32_t MAX_TOTAL_FACTOR = 3;      // hard countdown limit when failover keeps switching networks

    // How many restarts in a row preceded THIS boot. A normal start, power-on, a restart from the panel
    // and a panic give 0 - only a restart ordered by this mechanism counts (the MAGIC marker).
    inline uint32_t countAtBoot(bool softwareReset, uint32_t magic, uint32_t storedCount) {
        return (softwareReset && magic == MAGIC) ? storedCount : 0;
    }
    inline bool shouldRestart(uint32_t count) { return count < MAX_RESTARTS; }

    // WiFi connectivity monitor: decides WHEN to restart and when to give up. It does nothing itself -
    // it returns an action that network_manager.h carries out (restart, log entry).
    class Monitor {
    public:
        enum Action { NONE = 0, RESTART = 1, GIVE_UP = 2 };

        void setBootCount(uint32_t c) { count_ = c; }
        uint32_t count() const { return count_; }
        bool gaveUp() const { return gaveUp_; }
        // Time in ms as uint32_t (like millis() on ESP32) - counter-wraparound arithmetic works the same on every platform.
        uint32_t downForMs(uint32_t now) const { return down_ ? (uint32_t)(now - downSince_) : 0U; }

        // Call every loop pass. connected = WiFi station connected; timeoutMs = WIFI_CONNECT_TIMEOUT_MS.
        // Failover switched to another network - give it the full time to connect instead of restarting midway.
        // Hard limit: restart at the latest after MAX_TOTAL_FACTOR x timeout from the start of the loss (two visible
        // but non-working networks must not extend the countdown indefinitely).
        void noteProgress(uint32_t now) { if (down_) progressAt_ = now; }

        Action update(bool connected, uint32_t now, uint32_t timeoutMs) {
            if (!connected) {
                up_ = false;
                if (!down_) { down_ = true; downSince_ = now; progressAt_ = now; }
                if (!gaveUp_ && ((uint32_t)(now - progressAt_) >= timeoutMs || (uint32_t)(now - downSince_) >= MAX_TOTAL_FACTOR * timeoutMs)) {
                    if (shouldRestart(count_)) return RESTART;
                    gaveUp_ = true;
                    return GIVE_UP;
                }
            } else {
                down_ = false;
                if (!up_) { up_ = true; upSince_ = now; }
                if (count_ > 0 && (uint32_t)(now - upSince_) >= STABLE_MS) { count_ = 0; gaveUp_ = false; }   // the network is back for good
            }
            return NONE;
        }

    private:
        uint32_t count_ = 0;
        bool down_ = false, up_ = false, gaveUp_ = false;
        uint32_t downSince_ = 0, upSince_ = 0, progressAt_ = 0;
    };

} // namespace WifiFallback
