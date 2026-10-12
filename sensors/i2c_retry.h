#pragma once
// ======================================================================
// sensors/i2c_retry.h
// EN: Retry back-off for an I2C device that stops answering, plus a "lost" verdict and a re-probe timer.
// PL: Wydłużanie przerw między próbami dla urządzenia I2C, które przestało odpowiadać, werdykt „utracone"
//     i timer ponownego sprawdzania obecności.
// Tests: tests/test_i2c_retry.cpp (host, plain C++).
// Used by: sensors/steering_angle_sensor.h, io/section_expander.h
// ======================================================================
#include <cstdint>

// A dead I2C device must not eat the main loop. Every failed transaction can cost up to the bus timeout
// (Wire.setTimeOut, 10 ms), and the angle sensor is polled every 30 ms - so a vanished chip would take a
// third of the loop time for as long as it stays away. The rule:
//   - the first failures are retried at once (a single glitch costs nothing),
//   - after a few failures the retries slow down (250 ms), after more still (1 s),
//   - after LOST_AFTER failures in a row the device is declared lost; the owner then stops using it and only
//     probes its presence every PROBE_MS until it answers again (a late-powered or re-plugged chip recovers
//     without a restart).
// All times use unsigned subtraction, so they survive the millis() wrap-around.
class I2cRetry {
public:
    static constexpr uint8_t  FAST_FAILS  = 3;      // failures retried without any extra delay
    static constexpr uint8_t  SLOW_FAILS  = 6;      // up to here: SLOW_MS between tries, then VERY_SLOW_MS
    static constexpr uint8_t  LOST_AFTER  = 10;     // failures in a row => lost
    static constexpr uint32_t SLOW_MS      = 250;
    static constexpr uint32_t VERY_SLOW_MS = 1000;
    static constexpr uint32_t PROBE_MS     = 2000;  // presence probe period while lost

    void ok() { fails_ = 0; }
    // Returns true when this failure made the device "lost" (once per loss).
    bool fail(uint32_t now) {
        if (fails_ < 255) fails_++;
        lastFailMs_ = now;
        return fails_ == LOST_AFTER;
    }
    // May the owner try the device now?
    bool due(uint32_t now) const {
        uint32_t wait = fails_ < FAST_FAILS ? 0 : (fails_ < SLOW_FAILS ? SLOW_MS : VERY_SLOW_MS);
        return wait == 0 || (uint32_t)(now - lastFailMs_) >= wait;
    }
    bool lost() const { return fails_ >= LOST_AFTER; }
    uint8_t failures() const { return fails_; }

    // Presence probe while lost: true at most once per PROBE_MS.
    bool probeDue(uint32_t now) {
        if (probed_ && (uint32_t)(now - lastProbeMs_) < PROBE_MS) return false;
        probed_ = true;
        lastProbeMs_ = now;
        return true;
    }
    // The device answered again: back to a clean state.
    void recovered() { fails_ = 0; }
    // Marks the device as lost from the start (not found at boot).
    void markLost(uint32_t now) { fails_ = LOST_AFTER; lastFailMs_ = now; probed_ = true; lastProbeMs_ = now; }

private:
    uint8_t  fails_ = 0;
    uint32_t lastFailMs_ = 0;
    bool     probed_ = false;
    uint32_t lastProbeMs_ = 0;
};
