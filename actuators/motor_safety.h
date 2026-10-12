#pragma once
// ======================================================================
// actuators/motor_safety.h
// EN: Two small pure-C++ safety latches of the steering motor: MotorDeadman (cuts the motor when the main loop hangs)
//     and SteerArming (no driving after a restart until steering was seen switched OFF in AgOpenWeb).
// PL: Dwie małe zabezpieczenia silnika kierownicy w czystym C++: MotorDeadman (odcina silnik, gdy główna pętla się
//     zawiesi) i SteerArming (brak jazdy po restarcie, dopóki nie zobaczymy wyłączonego sterowania w AgOpenWeb).
// Tests: tests/test_motor_safety.cpp (host, plain C++).
// Used by: actuators/actuators.h
// ======================================================================
#include <cstdint>
#include <atomic>

// Dead-man timer. The PWM is produced by hardware (LEDC), so when loop() hangs the last duty cycle simply keeps running
// until the 5 s watchdog resets the chip. The main loop calls feed() on every pass; a separate high-priority task calls
// poll() every few tens of ms and, when it returns true, writes PWM = 0. Not armed until the first feed() (so boot-time
// delays before loop() starts are never mistaken for a hang).
class MotorDeadman {
public:
    static constexpr uint32_t LIMIT_MS = 300;   // the loop normally runs every 1-11 ms; stalls over 50 ms are already logged as [LOOP STALL]

    void feed(uint32_t now) { lastMs.store(now, std::memory_order_relaxed); fed.store(true, std::memory_order_release); }

    // True ONCE per stall: the moment the loop has not fed us for more than limitMs. Fed again => ready for the next stall.
    bool poll(uint32_t now, uint32_t limitMs = LIMIT_MS) {
        if (!fed.load(std::memory_order_acquire)) return false;
        // signed difference: feed() may land just after "now" was read (a tiny negative gap must not look like a 49-day stall)
        if ((int32_t)(now - lastMs.load(std::memory_order_relaxed)) <= (int32_t)limitMs) { tripped = false; return false; }
        if (tripped) return false;
        tripped = true;
        tripCount.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    uint32_t trips() const { return tripCount.load(std::memory_order_relaxed); }

private:
    std::atomic<uint32_t> lastMs{0};
    std::atomic<bool> fed{false};
    std::atomic<uint32_t> tripCount{0};
    bool tripped = false;   // used only by poll() (a single task)
};

// Direction change without a pause. When the controller output flips sign from one pass to the next (for example +minPWM -> -minPWM at
// the zero crossing of the error), the driver would be reversed under load: a mechanical jolt and a current spike. DirGuard inserts a
// short stop (output 0 for HOLD_MS) between the two directions. Going through 0 by itself, or driving the same way, is not delayed.
class DirGuard {
public:
    static constexpr uint32_t HOLD_MS = 20;
    int filter(int pwm, uint32_t now) {
        const int s = pwm > 0 ? 1 : (pwm < 0 ? -1 : 0);
        if (s == 0) { cur = 0; return 0; }
        if (cur != 0 && s != cur) { cur = 0; holding = true; holdUntil = now + HOLD_MS; return 0; }   // direct reversal: stop first
        if (cur == 0 && holding) {
            if ((int32_t)(now - holdUntil) < 0) return 0;
            holding = false;
        }
        cur = s;
        return pwm;
    }
    void reset() { cur = 0; holding = false; }
private:
    int cur = 0;
    bool holding = false;
    uint32_t holdUntil = 0;
};

// Arming after a restart. AgOpenWeb keeps sending "steering enabled" while the module reboots (watchdog, power dip), so without
// this the motor would start turning by itself as soon as the angle sensor came back. The motor may drive only after at least
// one FRESH command with steering switched OFF was seen since start-up - i.e. the operator has to switch steering off and on again.
class SteerArming {
public:
    // Call every pass. fresh = a recent PGN 254 frame; engagedFlag = the steering-enabled flag of that frame.
    void update(bool fresh, bool engagedFlag) {
        if (fresh && !engagedFlag) armed_ = true;
    }
    bool armed() const { return armed_; }
    // Forget the arming: used when the motor feature is switched off, so that switching it on again needs a fresh frame
    // with steering OFF (otherwise AgOpenWeb could still be "engaged" and the motor would start at once).
    void reset() { armed_ = false; }
    // true when steering is being requested but we are not armed (for the one-time log line and the panel)
    bool blocking(bool engagedActive) const { return engagedActive && !armed_; }
private:
    bool armed_ = false;
};
