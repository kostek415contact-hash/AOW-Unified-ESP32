#pragma once
// ======================================================================
// sensors/gps_baud_retry.h
// EN: Non-blocking schedule for re-detecting the GPS UART speed when no valid NMEA line has ever arrived.
// PL: Nieblokujący harmonogram ponownego wykrywania prędkości UART GPS, gdy nie przyszła żadna poprawna linia NMEA.
// Tests: tests/test_gps_baud_retry.cpp (host, plain C++).
// Used by: sensors/gps_universal.h
// ======================================================================
#include <cstdint>

// The receiver may power up later than the ESP32, so the one-off speed detection at boot can hit the void. Every RETRY_EVERY_MS
// the whole candidate list is tried again - but WITHOUT waiting inside loop(): the old code busy-waited 150 ms per candidate
// (7 passes of 150 ms, about 1 s of a frozen main loop every 16 s for as long as the GPS stayed silent - PANDA, IMU, the angle sensor
// and the motor update all stalled). Here the caller switches the UART speed when told to and the normal parser (which runs every
// pass anyway) decides whether a valid line arrived; this class only measures time.
//
// Usage, once per loop pass AFTER the parser ran:
//     Out o = retry.tick(millis(), everGotValidNmea);
//     A_SWITCH   -> restart the UART at CANDIDATE[o.index]
//     A_SUCCESS  -> a valid line arrived while testing candidate o.index: keep that speed, configure the receiver
//     A_FALLBACK -> every candidate failed: restart the UART at the fallback speed (9600) and wait for the next round
class GpsBaudRetry {
public:
    static constexpr uint32_t RETRY_EVERY_MS = 15000;   // start of one round to the start of the next
    static constexpr uint32_t WINDOW_MS      = 150;     // listening time per candidate speed
    static constexpr uint8_t  CANDIDATES     = 7;

    enum Act : uint8_t { A_NONE, A_SWITCH, A_SUCCESS, A_FALLBACK };
    struct Out { Act act; uint8_t index; };

    // The speed was (re)configured by begin(): the next round is due RETRY_EVERY_MS from now.
    void restartClock(uint32_t now) { lastRound_ = now; active_ = false; }

    Out tick(uint32_t now, bool everValid) {
        if (everValid) {
            if (active_) { active_ = false; return {A_SUCCESS, idx_}; }
            return {A_NONE, 0};
        }
        if (!active_) {
            if ((uint32_t)(now - lastRound_) < RETRY_EVERY_MS) return {A_NONE, 0};
            lastRound_ = now;
            active_ = true;
            idx_ = 0;
            windowStart_ = now;
            attempts_++;
            return {A_SWITCH, 0};
        }
        if ((uint32_t)(now - windowStart_) < WINDOW_MS) return {A_NONE, 0};
        idx_++;
        if (idx_ >= CANDIDATES) { active_ = false; return {A_FALLBACK, 0}; }
        windowStart_ = now;
        return {A_SWITCH, idx_};
    }

    bool active() const { return active_; }
    uint8_t index() const { return idx_; }
    uint32_t attempts() const { return attempts_; }

private:
    bool     active_ = false;
    uint8_t  idx_ = 0;
    uint32_t lastRound_ = 0;
    uint32_t windowStart_ = 0;
    uint32_t attempts_ = 0;
};
