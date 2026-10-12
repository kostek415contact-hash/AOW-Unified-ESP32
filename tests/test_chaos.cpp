// Chaos test: random sequences of faults and settings changes thrown at the motor chain and at the machine outputs,
// with the safety rules checked after EVERY step (not only at the end of a scenario):
//   motor:   no output unless every gate is open; |PWM| never above the limit in force at that moment; no direct sign flip;
//            the output always points toward the target; nothing pushes further out of the angle range; a tripped detector
//            holds the motor; NaN / Inf never reach the output - whatever the order of link loss, sensor loss, kickout,
//            restart (arming reset) and settings changes (gain, limits, range, target) in the middle of a move.
//   outputs: a stale command (no PGN 239) switches off every section, tram line and geo-stop; the lift never has both
//            valves on; a stale automatic lift is off; nothing is driven outside the mapped pins.
//     g++ -std=c++17 -Wall -Wextra tests/test_chaos.cpp -o /tmp/tmp && /tmp/tmp
#include <cstdio>
#include <cmath>
#include <cstdint>
#include "../actuators/motor_pipeline.h"
#include "../io/machine_outputs.h"
static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

static uint32_t rs = 12345;
static uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static float frand(float a, float b) { return a + (b - a) * (float)(rnd() % 100001) / 100000.0f; }
static bool chance(int permille) { return (int)(rnd() % 1000) < permille; }

static void motorChaos(int runs, int steps) {
    long bad_gate = 0, bad_limit = 0, bad_flip = 0, bad_dir = 0, bad_end = 0, bad_trip = 0, bad_nan = 0, drove = 0;
    for (int run = 0; run < runs; run++) {
        RunawayGuard guard; DirGuard dir; SteerArming arming;
        guard.configure({true, 1000, 1.0f, 3.0f});
        arming.update(true, false);
        float angle = frand(-30, 30), k = frand(-0.004f, 0.006f), target = frand(-40, 40);
        bool engaged = true, sensorFresh = true, kickout = false, frozen = false;
        float gainP = 10; int high = 120, minp = 30; float lo = -43, hi = 43;
        uint32_t t = 0, seq = 1; int16_t raw = 1000; int prev = 0;
        for (int i = 0; i < steps; i++) {
            t += 10; seq++;
            // random events (about 1 per 150 steps each)
            if (chance(7)) engaged = !engaged;
            if (chance(7)) sensorFresh = !sensorFresh;
            if (chance(5)) kickout = !kickout;
            if (chance(4)) frozen = !frozen;
            if (chance(3)) arming.reset();                       // a restart in the middle
            if (arming.armed() == false && chance(40)) arming.update(true, chance(500));
            if (chance(5)) arming.update(true, false);           // a frame with steering off
            if (chance(8)) target = chance(50) ? (chance(500) ? NAN : INFINITY) : frand(-80, 80);
            if (chance(6)) gainP = chance(100) ? 0.0f : frand(0, 250);
            if (chance(6)) { high = (int)(rnd() % 256); minp = (int)(rnd() % 120); if (minp > high) minp = high; }
            if (chance(5)) { lo = frand(-60, -5); hi = frand(5, 60); }
            if (chance(3)) { angle = chance(300) ? NAN : frand(-90, 90); }
            if (chance(2)) guard.reset();
            if (!frozen && std::isfinite(angle)) raw = (int16_t)(1000 + (int)(angle * 100.0f) + (int)(seq % 3));
            const bool trippedBefore = guard.isTripped();
            const bool armedBefore = arming.armed();
            MotorPipeline::In in;
            in.now = t; in.engagedActive = engaged; in.sensorFresh = sensorFresh; in.kickout = kickout;
            in.target = target; in.actual = angle; in.sampleSeq = seq; in.raw = raw;
            in.gainP = gainP; in.highPwm = high; in.minPwm = minp; in.lo = lo; in.hi = hi;
            MotorPipeline::Out o = MotorPipeline::step(guard, dir, arming, in);
            const bool gates = engaged && sensorFresh && !kickout && !trippedBefore && armedBefore;
            if (!gates && o.pwm != 0) bad_gate++;
            if (std::abs(o.pwm) > high) bad_limit++;
            if ((prev > 0 && o.pwm < 0) || (prev < 0 && o.pwm > 0)) bad_flip++;
            if (o.pwm != 0) {
                drove++;
                const float err = target - angle;
                if (!(std::isfinite(err)) ) bad_nan++;
                else if ((o.pwm > 0) != (err > 0)) bad_dir++;
                if ((angle <= lo && o.pwm < 0) || (angle >= hi && o.pwm > 0)) bad_end++;
                if (guard.isTripped()) bad_trip++;
            }
            prev = o.pwm;
            if (std::isfinite(angle)) angle += (float)o.pwm * k;
            guard.tick(t, engaged);
        }
    }
    CHECK(bad_gate == 0);   // output while a gate was closed
    CHECK(bad_limit == 0);  // above the limit in force
    CHECK(bad_flip == 0);   // direct reversal
    CHECK(bad_dir == 0);    // output away from the target
    CHECK(bad_end == 0);    // pushed out of the range
    CHECK(bad_trip == 0);   // output while tripped
    CHECK(bad_nan == 0);    // NaN / Inf reached the output
    CHECK(drove > 1000);    // the scenarios really drive the motor (not vacuous)
}

static void outputsChaos(int runs, int steps) {
    long bad_stale = 0, bad_both = 0, bad_unmapped = 0, bad_lift = 0, on_pass = 0;
    for (int run = 0; run < runs; run++) {
        uint8_t raw[24]; for (auto& b : raw) b = (uint8_t)(rnd() % 40);   // anything, including nonsense values
        MachinePinMap m; m.load(raw);
        uint16_t mappedBits = 0;
        for (uint8_t pin = 0; pin < MachinePinMap::HW; pin++) if (m.fn[pin] != 0) mappedBits |= (uint16_t)(1u << pin);
        HydControl hc; uint32_t now = 1000; bool fresh = true;
        for (int i = 0; i < steps; i++) {
            now += 10;
            if (chance(30)) fresh = !fresh;
            if (chance(5)) { if (chance(500)) hc.setManual(chance(500)); else hc.setAuto(); }
            const uint8_t hydLift = (uint8_t)(rnd() % 4);
            hc.update(hydLift, fresh, now, (uint8_t)(rnd() % 3 == 0 ? 0 : 1 + rnd() % 5), (uint8_t)(rnd() % 3 == 0 ? 0 : 1 + rnd() % 5));
            MachineInputs in; in.fresh = fresh; in.sections = (uint16_t)rnd(); in.tram = (uint8_t)rnd(); in.geoStop = chance(500);
            in.hydUp = hc.up(); in.hydDown = hc.down();
            const uint16_t mask = machineLogicalMask(m, in);
            if (hc.up() && hc.down()) bad_both++;
            if (!hc.manual() && !fresh && (hc.up() || hc.down())) bad_lift++;   // a stale automatic lift must be off
            if (mask & ~mappedBits) bad_unmapped++;
            if (!fresh) {
                uint16_t allowed = 0;   // only the lift pins may be on while the command is stale (and only via a manual override)
                for (uint8_t pin = 0; pin < MachinePinMap::HW; pin++)
                    if (m.fn[pin] == MachineFn::HYD_UP || m.fn[pin] == MachineFn::HYD_DOWN) allowed |= (uint16_t)(1u << pin);
                if (mask & ~allowed) bad_stale++;
            } else if (mask) on_pass++;
            // polarity: the physical idle value is the exact complement for an active-low board
            if (machinePhysicalMask(mask, true) != (uint16_t)~mask || machinePhysicalMask(mask, false) != mask) bad_unmapped++;
        }
    }
    CHECK(bad_stale == 0);     // section / tram / geo-stop on without a fresh command
    CHECK(bad_both == 0);      // raise and lower together
    CHECK(bad_lift == 0);      // automatic lift on with a stale command
    CHECK(bad_unmapped == 0);  // a pin outside the map driven, or polarity wrong
    CHECK(on_pass > 1000);     // not vacuous
}

int main() {
    motorChaos(1500, 3000);
    outputsChaos(1500, 2000);
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
