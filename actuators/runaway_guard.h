#pragma once
// ======================================================================
// actuators/runaway_guard.h
// EN: Detects a steering motor 'runaway' (wrong DIR or sensor polarity): driven one way, angle moves the other
//     way or not at all.
// PL: Wykrywa „ucieczkę” silnika kierownicy (odwrócony DIR lub biegunowość czujnika): napędzany w jedną stronę,
//     kąt idzie w drugą albo stoi.
// Used by: actuators/actuators.h
// ======================================================================
// Steering motor "runaway" detector. Pure C++ (no Arduino) - host-tested (tests/test_runaway.cpp)
// and in a closed loop with a simulated motor (see the change description).
//
// WHY: at the first motor start the most dangerous mistake is a REVERSED DIRECTION (DIR): the P controller sees an error,
// spins the motor, and the wheel goes the OPPOSITE way - the error grows, the controller spins harder and harder, until the end stop.
// The detector watches the relation "we drive toward X => the angle changes toward X":
//   - the angle moves the OPPOSITE way by >= WRONG_DIR_DEG after >= WRONG_DIR_MIN_MS  -> STOP (reversed direction),
//   - within the windowMs window the angle did not move by minMoveDeg in the requested direction  -> STOP (no motion: unpowered motor,
//     broken coupling, jammed mechanism, frozen/disconnected angle sensor),
//   - the raw angle sensor reading is BIT-FOR-BIT identical for FROZEN_SAMPLES consecutive NEW samples, despite driving
//     -> STOP faster than the window (frozen sensor / hung ADC: a live signal always jitters by a few LSB, and the wheel may
//     be moving in the meantime, so we do not wait for the end of the window). Counted only while driving (a standing wheel before
//     enabling does not count) and only when the caller supplies a sample number (sampleSeq != 0).
// Evaluation only when the motor is being driven (pwm != 0) and the error is >= minErrorDeg (near the target the controller dithers).
// The latch is cleared by: disabling steering for RELEASE_MS, or a manual reset from the panel.
#include <cstdint>
#include <cmath>

class RunawayGuard {
public:
    enum State : uint8_t { S_OFF = 0, S_OK = 1, S_NO_PROGRESS = 2, S_WRONG_DIR = 3, S_FROZEN = 4 };
    struct Params { bool enabled; uint32_t windowMs; float minMoveDeg; float minErrorDeg; };
    static constexpr float    WRONG_DIR_DEG    = 1.5f;   // this much the OPPOSITE way = reversed direction (sensor noise is ~0.1 deg)
    static constexpr uint32_t WRONG_DIR_MIN_MS = 200;    // we do not judge the first ms of the window (backlash, slip)
    static constexpr uint32_t FROZEN_SAMPLES   = 12;     // this many CONSECUTIVE identical new samples (at 30 ms sampling about 0.36 s) = frozen sensor
    static constexpr uint32_t RELEASE_MS       = 1000;   // steering disabled for this long => the latch clears itself

    void configure(const Params& p) { prm = p; if (!prm.enabled) windowActive = false; }
    const Params& params() const { return prm; }

    // Call every loop pass when the motor is (or could be) driven.
    //   driving  - true when the controller is allowed to drive the motor (steering enabled, fresh data, no other blocks)
    //   pwm      - signed controller output in the ANGLE convention (positive = toward the positive angle), BEFORE any DIR reversal
    //   errorDeg - target - current angle; angleDeg - current angle
    //   sampleSeq - counter of NEW angle sensor samples (increments on every fresh ADC reading; 0 = we do not check for freezing),
    //   rawAdc    - the raw reading of that sample
    void update(unsigned long now, bool driving, int pwm, float errorDeg, float angleDeg, uint32_t sampleSeq = 0, int16_t rawAdc = 0) {
        if (!prm.enabled || latched) { windowActive = false; frozenReset(); return; }
        bool active = driving && pwm != 0 && std::isfinite(errorDeg) && std::isfinite(angleDeg) && std::fabs(errorDeg) >= prm.minErrorDeg;
        if (!active) { windowActive = false; frozenReset(); return; }
        if (sampleSeq != 0 && sampleSeq != lastSeq) {                         // a new sample while driving
            if (haveRaw && rawAdc == lastRawAdc) frozenRun++; else frozenRun = 0;
            lastRawAdc = rawAdc; haveRaw = true; lastSeq = sampleSeq;
            if (frozenRun >= FROZEN_SAMPLES) { trip(S_FROZEN, windowActive ? (angleDeg - winStartAngle) * (float)(pwm > 0 ? 1 : -1) : 0.0f); return; }
        }
        int dir = pwm > 0 ? 1 : -1;
        if (windowActive && dir != winDir) windowActive = false;              // direction change = new window
        if (!windowActive) { windowActive = true; winDir = dir; winStartMs = now; winStartAngle = angleDeg; moveDeg_ = 0.0f; return; }
        float moved = (angleDeg - winStartAngle) * (float)dir;                // > 0: the angle is going the way we are driving
        moveDeg_ = moved;
        unsigned long el = now - winStartMs;
        if (el >= WRONG_DIR_MIN_MS && moved <= -WRONG_DIR_DEG) { trip(S_WRONG_DIR, moved); return; }
        if (el >= prm.windowMs) {
            if (moved >= prm.minMoveDeg) { winStartMs = now; winStartAngle = angleDeg; moveDeg_ = 0.0f; }   // progress made - next window
            else trip(S_NO_PROGRESS, moved);
        }
    }

    // Call every pass (also when the motor is standing): clearing the latch after steering is disabled.
    // engaged = true while steering is on OR its state is unknown (link down): only a confirmed "steering off" releases the latch.
    void tick(unsigned long now, bool engaged) {
        if (!latched) return;
        if (engaged) { releaseSince = 0; return; }
        if (!releaseSince) releaseSince = now ? now : 1;
        if (now - releaseSince >= RELEASE_MS) clearLatch();
    }
    void reset() { clearLatch(); }

    bool isTripped() const { return latched; }
    State state() const { return !prm.enabled ? S_OFF : (latched ? reason_ : S_OK); }
    float moveDeg() const { return moveDeg_; }                 // movement in the window toward the drive direction [deg] (for the panel)
    uint32_t trips() const { return tripCount; }
    State lastReason() const { return lastReason_; }           // S_OFF = no trip yet
    float lastMoveDeg() const { return lastMove_; }
private:
    void trip(State why, float moved) { latched = true; reason_ = why; lastReason_ = why; lastMove_ = moved; tripCount++; windowActive = false; releaseSince = 0; }
    void clearLatch() { latched = false; windowActive = false; releaseSince = 0; frozenReset(); }
    void frozenReset() { frozenRun = 0; haveRaw = false; lastSeq = 0; }
    Params prm{true, 1000, 1.0f, 3.0f};
    bool latched = false, windowActive = false;
    State reason_ = S_OK, lastReason_ = S_OFF;
    int winDir = 0;
    unsigned long winStartMs = 0, releaseSince = 0;
    float winStartAngle = 0.0f, moveDeg_ = 0.0f, lastMove_ = 0.0f;
    uint32_t tripCount = 0;
    uint32_t frozenRun = 0, lastSeq = 0;
    int16_t lastRawAdc = 0;
    bool haveRaw = false;
};
