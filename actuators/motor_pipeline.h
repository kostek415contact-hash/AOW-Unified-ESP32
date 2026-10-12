#pragma once
// ======================================================================
// actuators/motor_pipeline.h
// EN: The motor decision chain as ONE pure function: gates (steering active, fresh sensor, current kickout, runaway latch,
//     arming) -> end-stop check -> P controller -> runaway detector -> reversal pause. Host-simulated.
// PL: Łańcuch decyzji silnika jako JEDNA czysta funkcja: bramki (sterowanie aktywne, świeży czujnik, odcięcie prądowe,
//     blokada runaway, uzbrojenie) -> koniec zakresu -> regulator P -> detektor runaway -> przerwa przy zmianie kierunku.
// Tests: tests/test_motor_pipeline.cpp (host, plain C++: a simulated steering wheel with faults).
// Used by: actuators/actuators.h
// Depends on: motor_safety.h, pwm_math.h, runaway_guard.h
// ======================================================================
#include <cstdint>
#include <cmath>
#include "motor_safety.h"
#include "pwm_math.h"
#include "runaway_guard.h"

namespace MotorPipeline {

struct In {
    uint32_t now = 0;
    bool engagedActive = false;   // steering switched on in AgOpenWeb AND the command is fresh
    bool sensorFresh = false;     // the angle sensor delivers current samples
    bool kickout = false;         // the current protection holds the motor
    float target = 0, actual = 0; // [deg], ANGLE convention
    uint32_t sampleSeq = 0;       // counter of new sensor samples (0 = do not check for a frozen sensor)
    int16_t raw = 0;              // raw reading of that sample
    float gainP = 0;
    int highPwm = 0, minPwm = 0;
    float lo = -90, hi = 90;      // allowed angle range INCLUDING the end-stop margin
};

enum Why : uint8_t { W_DRIVE = 0, W_BLOCKED, W_ENDSTOP, W_RUNAWAY };

struct Out {
    int pwm = 0;          // signed PWM to apply (0 = motor stopped)
    float error = 0;      // target - actual (valid unless W_BLOCKED)
    Why why = W_BLOCKED;
};

// guard / dirGuard / arming are owned by the caller (the panel reads their state).
inline Out step(RunawayGuard& guard, DirGuard& dirGuard, const SteerArming& arming, const In& in) {
    Out o;
    const bool shouldDrive = in.engagedActive && in.sensorFresh && !in.kickout && !guard.isTripped() && arming.armed();
    if (!shouldDrive) {
        dirGuard.reset();
        guard.update(in.now, false, 0, 0.0f, 0.0f);   // not driving: the evaluation window starts over
        return o;                                      // W_BLOCKED, pwm 0
    }
    o.error = in.target - in.actual;
    // End-position protection: never push FURTHER beyond the calibrated range + margin; moving back toward it is always allowed.
    if ((in.actual <= in.lo && o.error < 0.0f) || (in.actual >= in.hi && o.error > 0.0f)) {
        guard.update(in.now, false, 0, 0.0f, 0.0f);
        o.why = W_ENDSTOP;
        return o;
    }
    int pwm = PwmMath::compute(o.error, in.gainP, in.highPwm, in.minPwm);
    // Does driving toward X really change the angle toward X? If not - stop in the SAME pass, before any output.
    guard.update(in.now, true, pwm, o.error, in.actual, in.sampleSeq, in.raw);
    if (guard.isTripped()) { o.why = W_RUNAWAY; return o; }
    o.pwm = dirGuard.filter(pwm, in.now);
    o.why = W_DRIVE;
    return o;
}

}  // namespace MotorPipeline
