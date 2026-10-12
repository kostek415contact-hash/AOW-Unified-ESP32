#pragma once
// ======================================================================
// connectivity/kickout_signal.h
// EN: Builds the 'kick-out' value sent to AgOpenWeb (PGN 250) when a motor protection trips, so AgOpenWeb
//     disengages steering.
// PL: Wylicza sygnał „kickout” dla AgOpenWeb (PGN 250) po zadziałaniu ochrony silnika, żeby AgOpenWeb wyłączył
//     prowadzenie.
// Tests: tests/test_kickout_signal.cpp (host, plain C++).
// Used by: connectivity/pgn_messages.h
// ======================================================================
#include <stdint.h>

// The "kickout" signal to AgOpenWeb after the motor protection trips (current latch, current sensor error/no data,
// runaway detector). Pure logic without Arduino - host-tested (tests/test_kickout_signal.cpp).
//
// HOW AGOPENWEB RECEIVES IT (sources: AutoSteerService.ProcessSensorData, PgnBuilder.TryParseSensorData):
//   PGN 250, byte 5 = sensor value 0-255, AgOpenWeb computes a percentage from it (value/255*100). When in AgOpenWeb
//   (AutoSteer configuration) the "Current sensor" is enabled and the threshold (CurrentTripPoint) > 0, and the percentage >= the threshold,
//   AgOpenWeb itself DISABLES steering (Disengage). Bit 0x02 in PGN 253 is only information about the switch
//   (it does not disable steering), which is why the signal goes in PGN 250.
//   After steering is disabled AgOpenWeb does not enable it on its own - the operator must enable it again.
//
// VALUE IN THE BYTE:
//   - protection tripped (or tripped a moment ago, HOLD_MS) -> 255 (100%), i.e. >= any threshold of 1-100%,
//   - normal operation -> load (0..1) * LOAD_MAX_BYTE, at most 150 (about 59%), so that with the threshold set
//     in AgOpenWeb to 70-100% normal operation does not disable steering, while AgOpenWeb shows the load.
//   Recommended threshold in AgOpenWeb: 80-100%.
// CAUSE of the trip (for the log and panel). Priority: motor motion supervision, then the current sensor.
// STARTUP_GRACE_MS: right after startup the current sensor has no first samples yet ("no data") - that is not a fault, so in this window
// we do NOT report kickout to AgOpenWeb for that reason (the motor is standing still anyway - the protection in MotorDriver works independently of this signal).
enum KickoutCause : uint8_t {
    KC_NONE = 0, KC_CURRENT_LATCH = 1, KC_CURRENT_ERROR = 2, KC_CURRENT_NODATA = 3,
    KC_RUN_NOPROGRESS = 4, KC_RUN_REVERSED = 5, KC_RUN_FROZEN = 6, KC_COUNT = 7
};
static constexpr uint32_t KICKOUT_STARTUP_GRACE_MS = 5000;

// runawayState: 0 off, 1 OK, 2 no progress, 3 reversed direction, 4 frozen angle sensor.
// currState: 0 off, 1 no data, 2 OK, 3 overcurrent (latch), 4 sensor error (as CurrentSensor::stateCode()).
inline uint8_t kickoutCause(uint8_t runawayState, uint8_t currState, uint32_t uptimeMs) {
    if (runawayState == 2) return KC_RUN_NOPROGRESS;
    if (runawayState == 3) return KC_RUN_REVERSED;
    if (runawayState == 4) return KC_RUN_FROZEN;
    if (currState == 3) return KC_CURRENT_LATCH;
    if (currState == 4) return KC_CURRENT_ERROR;
    if (currState == 1 && uptimeMs >= KICKOUT_STARTUP_GRACE_MS) return KC_CURRENT_NODATA;
    return KC_NONE;
}
inline const char* kickoutCauseText(uint8_t c) {
    switch (c) {
        case KC_CURRENT_LATCH:   return "overcurrent (current latch)";
        case KC_CURRENT_ERROR:   return "current sensor error (voltage out of range)";
        case KC_CURRENT_NODATA:  return "current sensor gives no samples";
        case KC_RUN_NOPROGRESS:  return "wheel does not move despite the drive";
        case KC_RUN_REVERSED:    return "motor direction reversed";
        case KC_RUN_FROZEN:      return "angle sensor frozen";
        default:                 return "none";
    }
}

class KickoutSignal {
public:
    static constexpr uint32_t HOLD_MS = 3000;       // the signal is held this long after the cause ends (dozens of frames every 40 ms)
    static constexpr uint8_t  LOAD_MAX_BYTE = 150;
    static constexpr uint8_t  KICKOUT_BYTE  = 255;

    // Call every loop pass. tripped = any motor protection is holding the motor still; load = load 0..1 (clipped outside the range).
    void update(uint32_t now, bool tripped, float load) {
        if (tripped) {
            if (!wasTripped) { count_++; }
            lastTripMs = now; everTripped = true;
        }
        wasTripped = tripped;
        load_ = load;
    }

    // true = the kickout signal is being sent now (the protection is holding, or the HOLD_MS window is still running)
    bool active(uint32_t now) const {
        return wasTripped || (everTripped && (uint32_t)(now - lastTripMs) < HOLD_MS);
    }

    uint8_t sensorByte(uint32_t now) const {
        if (active(now)) return KICKOUT_BYTE;
        float l = load_;
        if (!(l > 0.0f)) return 0;          // also NaN
        if (l > 1.0f) l = 1.0f;
        return (uint8_t)(l * LOAD_MAX_BYTE + 0.5f);
    }

    uint32_t count() const { return count_; }    // how many times the protection tripped since start (edges)
    uint8_t lastCause() const { return lastCause_; }   // cause of the last trip (KickoutCause), 0 = none yet
    void noteCause(uint8_t c) { if (c != KC_NONE) lastCause_ = c; }

private:
    bool wasTripped = false, everTripped = false;
    uint32_t lastTripMs = 0, count_ = 0;
    float load_ = 0.0f;
    uint8_t lastCause_ = 0;
};
