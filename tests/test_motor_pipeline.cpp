// Simulation of the whole motor decision chain (actuators/motor_pipeline.h) on a simulated steering wheel with faults:
// reversed motor, dead motor, frozen sensor, lost sensor, current kickout, end stops, reversal pause, arming.
//     g++ -std=c++17 -Wall -Wextra tests/test_motor_pipeline.cpp -o /tmp/tmp && /tmp/tmp
#include <cstdio>
#include <cmath>
#include "../actuators/motor_pipeline.h"
static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

// A steering wheel: angle changes by pwm * k per 10 ms step (k < 0 = reversed wiring, 0 = dead motor).
struct Rig {
    RunawayGuard guard; DirGuard dir; SteerArming arming;
    float angle = 0.0f, k = 0.003f;      // deg per PWM unit per 10 ms step (full PWM = about 36 deg/s)
    bool frozen = false, sensorFresh = true, engaged = true, kickout = false;
    uint32_t t = 0, seq = 1; int16_t raw = 1000;
    int lastPwm = 0, maxAbsPwm = 0, tripCount = 0;
    float target = 20.0f;
    Rig() {
        guard.configure({true, 1000, 1.0f, 3.0f});
        arming.update(true, false);      // armed: a fresh frame with steering OFF was seen
    }
    MotorPipeline::Out tick() {
        t += 10;
        seq++;                            // a new sample every step; a frozen sensor keeps delivering the SAME value
        if (!frozen) raw = (int16_t)(1000 + (int)(angle * 100.0f) + (int)(seq % 3));
        MotorPipeline::In in;
        in.now = t; in.engagedActive = engaged; in.sensorFresh = sensorFresh; in.kickout = kickout;
        in.target = target; in.actual = angle; in.sampleSeq = seq; in.raw = raw;
        in.gainP = 10; in.highPwm = 120; in.minPwm = 30; in.lo = -43; in.hi = 43;
        MotorPipeline::Out o = MotorPipeline::step(guard, dir, arming, in);
        lastPwm = o.pwm; if (std::abs(o.pwm) > maxAbsPwm) maxAbsPwm = std::abs(o.pwm);
        angle += (float)o.pwm * k;       // the plant
        guard.tick(t, engaged);
        return o;
    }
    void run(int steps) { for (int i = 0; i < steps; i++) tick(); }
};

int main() {
    // 1. Healthy wheel: reaches the target and stays there, never above highPwm
    { Rig r; r.run(600);
      CHECK(std::fabs(r.angle - r.target) < 3.0f);
      CHECK(r.maxAbsPwm <= 120 && !r.guard.isTripped()); }
    // 2. Reversed motor: the wheel runs away from the target; the runaway detector stops it within about a second
    { Rig r; r.k = -0.003f; r.run(150);
      CHECK(r.guard.isTripped() && r.guard.state() == RunawayGuard::S_WRONG_DIR);
      CHECK(r.lastPwm == 0);
      float a = r.angle; r.run(200); CHECK(r.angle == a && r.lastPwm == 0);          // stays stopped while engaged
      CHECK(std::fabs(r.angle) < 15.0f); }                                           // did not run far
    // 3. Dead motor (no movement): "no progress" within window + margin
    { Rig r; r.k = 0.0f; r.run(150);
      CHECK(r.guard.isTripped() && r.guard.state() == RunawayGuard::S_NO_PROGRESS && r.lastPwm == 0); }
    // 4. Frozen sensor while the motor drives: stops within about 0.5 s
    { Rig r; r.run(5); r.frozen = true; r.run(80);
      CHECK(r.guard.isTripped() && r.guard.state() == RunawayGuard::S_FROZEN && r.lastPwm == 0); }
    // 5. Sensor lost: no output at all, whatever the target
    { Rig r; r.sensorFresh = false; r.run(50); CHECK(r.maxAbsPwm == 0 && r.angle == 0.0f); }
    // 6. Steering switched off or link lost: no output
    { Rig r; r.engaged = false; r.run(50); CHECK(r.maxAbsPwm == 0); }
    // 7. Current kickout: no output and the motor stops at once when it trips in the middle of a move
    { Rig r; r.run(20); CHECK(r.maxAbsPwm > 0); r.kickout = true; r.tick(); CHECK(r.lastPwm == 0); r.run(50); CHECK(r.lastPwm == 0); }
    // 8. Not armed after a restart (steering already on): no output until a frame with steering OFF was seen
    { Rig r; r.arming.reset(); r.run(50); CHECK(r.maxAbsPwm == 0);
      r.arming.update(true, false); r.run(5); CHECK(r.maxAbsPwm > 0); }
    // 9. End stops: never pushes further out of the range, but may come back in
    { Rig r; r.target = 80.0f; r.k = 0.01f; r.run(600);
      CHECK(r.angle <= 43.0f + 2.5f);                                                // stopped at the limit (plus one step of coasting)
      r.target = 0.0f; r.guard.reset(); r.run(10); CHECK(r.lastPwm < 0); }
    // 10. Output limits at any time, including the sudden target flip
    { Rig r; for (int i = 0; i < 400; i++) { if (i % 37 == 0) r.target = (i % 74 == 0) ? 40.0f : -40.0f; r.tick(); }
      CHECK(r.maxAbsPwm <= 120); }
    // 11. Reversal pause: when the output flips sign, at least one zero output lies between (DirGuard)
    { Rig r; r.k = 0.05f; r.target = 5.0f; int prev = 0; bool bad = false;
      for (int i = 0; i < 300; i++) { r.tick(); if ((prev > 0 && r.lastPwm < 0) || (prev < 0 && r.lastPwm > 0)) bad = true; prev = r.lastPwm; }
      CHECK(!bad); }
    // 12. NaN / Inf from a broken sensor or settings can never produce an output
    { Rig r; r.target = NAN; r.run(20); CHECK(r.maxAbsPwm == 0);
      Rig q; q.angle = INFINITY; q.run(20); CHECK(q.maxAbsPwm == 0); }
    // 13. A tripped detector is released only by a confirmed "steering off" (1 s), then the motor works again
    { Rig r; r.k = 0.0f; r.run(150); CHECK(r.guard.isTripped());
      r.k = 0.003f; r.engaged = false; r.run(120); CHECK(!r.guard.isTripped());
      r.engaged = true; r.run(50); CHECK(r.maxAbsPwm > 0 && r.lastPwm != 0); }
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
