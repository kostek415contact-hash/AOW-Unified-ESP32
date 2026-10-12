#pragma once
// ======================================================================
// actuators/actuators.h
// EN: Two implement actuators: the steering motor driver (Cytron MD13S, PWM + DIR, P regulator, current/runaway
//     protections) and the hydraulic lift output.
// PL: Dwa siłowniki: sterownik silnika kierownicy (Cytron MD13S, PWM + DIR, regulator P, ochrony prądowa i
//     przed ucieczką) oraz wyjście hydrauliki.
// Used by: connectivity/network_manager.h, web/web_server.h
// Depends on: config.h, current_sensor.h, log_buffer.h, pgn_receivers.h, pwm_math.h, runaway_guard.h,
//     steering_angle_sensor.h
// ======================================================================
#include <Arduino.h>
#include <atomic>
#include <cmath>
#include "../config/config.h"
#include "../web/log_buffer.h"
#include "../connectivity/pgn_receivers.h"
#include "../sensors/steering_angle_sensor.h"
#include "../sensors/current_sensor.h"
#include "runaway_guard.h"
#include "pwm_math.h"
#include "lift_pulse.h"
#include "motor_safety.h"
#include "motor_pipeline.h"

// Two implement actuators, combined in one file.

// Cytron MD13S motor driver. STATUS: code complete and tested on a computer; the motor has NOT yet been
// checked under load on a machine (as of 2026-10-09). Enable ENABLE_MOTOR_DRIVER (panel -> Settings -> Features)
// when the hardware is connected - network_manager.h calls begin()/update() unconditionally.
//
// CONTROLLER: a simple P (proportional), NOT a full PID (no I/D term) -
// exactly how the reference AgOpenGPS/AgOpenWeb modules work.
// The parameters (gainP, highPWM, minPWM) come from AgOpenWeb via PGN252
// (see connectivity/pgn_receivers.h, SteerSettingsReceiver) - INDEPENDENT
// of the broken "Zero WAS" (that bug only affects the angle calibration, not these
// fields), so they should be trustworthy right away.
//
// UNCERTAINTIES TO VERIFY IN THE FIELD (I cannot check without a
// motor):
//   1. The SIGN of the error (error = target - actual) - the NUMERIC CONVENTION is
//      now CONFIRMED from two independent sources: the AgOpenWeb interface
//      (Wheel Angle Sensor: "Turn wheels RIGHT - the angle should read
//      POSITIVE") and the mature reference implementation mtz8302/
//      AOG_Autosteer_ESP32 ("Steering to the left must be minus") - i.e.
//      RIGHT = positive, LEFT = negative, matching what our
//      SteeringAngleSensor already computes. What remains to check in the field is which
//      state of the DIR pin (HIGH/LOW) physically corresponds to which direction -
//      THAT depends solely on how your motor is wired to the wheels and cannot
//      be determined in advance. If the wheel turns the wrong way on the
//      first test, invert the line "digitalWrite(MOTOR_DIR_PIN,
//      ...)" in the code (pwmSigned >= 0 ? LOW : HIGH instead of HIGH : LOW) - NOT
//      the wires.
//   2. The exact role of lowPWM (received, but unused for now) - in
//      some versions of the reference firmware it is a separate threshold for
//      low vehicle speeds, in others part of the Ackermann logic. To be
//      determined once the behaviour can be observed live.
//   3. The PWM frequency (20 kHz) and resolution (8-bit, 0-255) are
//      reasonable starting values for a Cytron MD13S-type driver
//      (above the audible threshold, standard range) - to be tuned
//      if needed.
class MotorDriver {
private:
    static float targetAngleDeg;
    static constexpr float END_STOP_MARGIN_DEG = 3.0f; // clearance beyond the calibrated end angles

    // Internal state for the panel (Calibration -> Motor, Diagnostics) and the runaway detector.
    static RunawayGuard guard;
    static int lastPwm;                    // last PWM output (signed, angle convention: positive = toward the positive angle)
    static float lastErrorDeg;
    static bool resetRequested;            // from the panel (core 0), handled in update() (core 1)
    static bool loggedTrip;
    // --- manual motor test from the panel (Calibration -> Motor: Left / Right) ---
    // A single pulse of LIMITED duration and PWM, stopped by the firmware (not by the browser). The panel (core 0) stores the
    // parameters and sets testReqPending; the loop (core 1) starts, supervises and ends the pulse. The controller and the runaway detector
    // are skipped during this time (it tests the motor alone); the current protection and enabling steering in AgOpenWeb abort the test.
    static std::atomic<bool> testReqPending;
    static int testReqDir, testReqPwm;
    static unsigned long testReqMs;
    static bool testActive;
    static int testDir, testPwm;
    static unsigned long testStartMs, testDurMs, testFinishMs;
    static float testAngleStart, testPeakA, testDeltaDeg;
    static uint8_t testState;              // 0 idle, 1 running, 2 finished, 3 aborted
    static uint8_t testReason;             // when 3: see TestReason
    static bool hwReady;                   // whether PWM (ledcAttach) and the DIR pin are already configured - done LAZILY, also after enabling from the panel
    static unsigned long lastLogMs;
    // Safety latches (actuators/motor_safety.h): the dead-man timer cuts the motor when loop() hangs; arming blocks driving after a restart.
    static MotorDeadman deadman;
    static SteerArming arming;
    static DirGuard dirGuard;
    static bool deadmanTaskStarted;
    static uint32_t deadmanSeen;           // trips already reported in the log
    static bool loggedUnarmed;

    // Runs on core 0 at high priority, independent of loop(): when the main loop stops calling update() (a hang, a long block),
    // the PWM generated by hardware would keep running until the watchdog reset - here we cut it after MotorDeadman::LIMIT_MS.
    static void deadmanTask(void*) {
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (deadman.poll(millis())) ledcWrite(MOTOR_PWM_PIN, 0);
        }
    }

    // pwmSigned uses the ANGLE convention (positive = positive angle = RIGHT). MOTOR_INVERT_DIR (the panel) and "Invert motor" from AgOpenWeb (PGN 251) together invert only the physical DIR pin,
    // so the runaway detector and the controller keep working in one, unchanging convention.
    static void drive(int pwmSigned) {
        bool dirHigh = (pwmSigned >= 0) != (MOTOR_INVERT_DIR != SteerConfigReceiver::invertMotor());   // panel setting XOR AgOpenWeb "Invert motor"
        digitalWrite(MOTOR_DIR_PIN, dirHigh ? HIGH : LOW);
        ledcWrite(MOTOR_PWM_PIN, abs(pwmSigned));
    }
    static void stopMotor() { ledcWrite(MOTOR_PWM_PIN, 0); lastPwm = 0; }

public:
    // FIRST thing in setup(), regardless of ENABLE_MOTOR_DRIVER: drive both driver inputs LOW so they do not float while WiFi, GPS and the
    // rest start up (that takes seconds). It does NOT replace external pull-down resistors: until this code runs, and while the chip
    // resets, the pins are high-impedance.
    static void safePins() {
        pinMode(MOTOR_PWM_PIN, OUTPUT); digitalWrite(MOTOR_PWM_PIN, LOW);
        pinMode(MOTOR_DIR_PIN, OUTPUT); digitalWrite(MOTOR_DIR_PIN, LOW);
    }

    // Hardware configuration (20 kHz PWM on the PWM pin, DIR pin as output). Idempotent. Called at startup AND from update(),
    // when the features were enabled in the panel after startup - previously begin() returned immediately with the feature disabled, and after enabling from the panel
    // PWM was never attached to the pin (the motor got no signal even though the driving conditions were met).
    static void initHardware() {
        if (hwReady) return;
        pinMode(MOTOR_DIR_PIN, OUTPUT);
        digitalWrite(MOTOR_DIR_PIN, LOW);
        // API NOTE: ledcAttach(pin, freq, resolution) is the ESP32
        // Arduino 3.x core API. Older cores (2.x) use a different sequence:
        //   ledcSetup(channel, freq, resolution); ledcAttachPin(pin, channel);
        //   ledcWrite(channel, duty); // by channel number, not pin
        // If this does not compile, check your core version.
        ledcAttach(MOTOR_PWM_PIN, 20000, 8); // 20kHz, 8-bit (0-255)
        ledcWrite(MOTOR_PWM_PIN, 0);
        lastPwm = 0;
        hwReady = true;
        if (!deadmanTaskStarted) {
            deadmanTaskStarted = true;
            xTaskCreatePinnedToCore(deadmanTask, "MotorDM", 2048, nullptr, configMAX_PRIORITIES - 2, nullptr, 0);
        }
        LogBuffer::println("[MOTOR] Cytron MD13S initialised (P regulator ready).");
    }

    // Test request result (the panel turns it into an HTTP code and message) and the abort reason.
    enum TestResult : uint8_t { MT_OK = 0, MT_DISABLED = 1, MT_ENGAGED = 2, MT_BUSY = 3, MT_CURRENT = 4, MT_ENDSTOP = 5, MT_ARGS = 6 };
    enum TestReason : uint8_t { TR_NONE = 0, TR_ENGAGED = 1, TR_CURRENT = 2, TR_DISABLED = 3, TR_ENDSTOP = 4 };
    static const int TEST_PWM_MIN = 10, TEST_PWM_MAX = 150;
    static const unsigned long TEST_MS_MIN = 100, TEST_MS_MAX = 1000;

    // The wheel is at (or beyond) the end of the calibrated range in direction dir (+1 right, -1 left), with a margin. Only when the angle is trustworthy.
    static bool testAtEndStop(int dir) {
        if (!SteeringAngleSensor::isFresh()) return false;
        float a = SteeringAngleSensor::getAngleDeg();
        float lo = fminf(STEERING_ANGLE_LEFT_DEG, STEERING_ANGLE_RIGHT_DEG) - END_STOP_MARGIN_DEG;
        float hi = fmaxf(STEERING_ANGLE_LEFT_DEG, STEERING_ANGLE_RIGHT_DEG) + END_STOP_MARGIN_DEG;
        return (dir < 0 && a <= lo) || (dir > 0 && a >= hi);
    }

    // Check of the safety conditions (called from the panel and again when the pulse starts in the loop).
    static uint8_t testPrecheck(int dir) {
        if (!ENABLE_MOTOR_DRIVER) return MT_DISABLED;
        if (SteerCommandReceiver::isAutoSteerActive()) return MT_ENGAGED;
        if (CurrentSensor::isKickoutTriggered()) return MT_CURRENT;
        if (testAtEndStop(dir)) return MT_ENDSTOP;   // end stop: do not push beyond the range + margin (when the angle is trustworthy)
        return MT_OK;
    }

    // dir: +1 = RIGHT (toward the positive angle, as in the controller), -1 = LEFT. Called from the panel (core 0).
    static uint8_t requestTestPulse(int dir, int pwm, unsigned long ms) {
        if ((dir != 1 && dir != -1) || pwm < TEST_PWM_MIN || pwm > TEST_PWM_MAX || ms < TEST_MS_MIN || ms > TEST_MS_MAX) return MT_ARGS;
        if (testActive || testReqPending.load(std::memory_order_acquire)) return MT_BUSY;
        uint8_t pre = testPrecheck(dir);
        if (pre != MT_OK) return pre;
        testReqDir = dir; testReqPwm = pwm; testReqMs = ms;
        testReqPending.store(true, std::memory_order_release);   // parameters stored BEFORE the flag is set
        return MT_OK;
    }

    static void finishTest(unsigned long now, uint8_t state, uint8_t reason) {
        stopMotor();
        testActive = false;
        testState = state; testReason = reason; testFinishMs = now ? now : 1;
        testDeltaDeg = SteeringAngleSensor::isFresh() ? (SteeringAngleSensor::getAngleDeg() - testAngleStart) : -999.0f;
        uint8_t cs = CurrentSensor::stateCode();
        if (!(CurrentSensor::isEnabled() && (cs == 2 || cs == 3))) testPeakA = -1.0f;   // no trustworthy current measurement
        LogBuffer::printf("[MOTOR] TEST %s: pwm=%d %lu ms -> angle %s, current peak %s%s\n", testDir > 0 ? "RIGHT" : "LEFT", testPwm, testDurMs,
                          testDeltaDeg > -900.0f ? String(testDeltaDeg, 1).c_str() : "no reading",
                          testPeakA >= 0.0f ? String(testPeakA, 2).c_str() : "no measurement", state == 3 ? " (ABORTED)" : "");
    }

    // Test handling in the loop: true = the test took over the motor in this pass (we skip the controller).
    static bool serviceTest(unsigned long now) {
        if (testReqPending.load(std::memory_order_acquire)) {
            testReqPending.store(false, std::memory_order_release);
            uint8_t pre = testPrecheck(testReqDir);
            if (!testActive && pre == MT_OK) {
                testActive = true; testDir = testReqDir; testPwm = testReqPwm; testDurMs = testReqMs; testStartMs = now;
                testAngleStart = SteeringAngleSensor::isFresh() ? SteeringAngleSensor::getAngleDeg() : 0.0f;
                testPeakA = 0.0f; testState = 1; testReason = TR_NONE;
            } else {
                testState = 3; testReason = (pre == MT_ENGAGED) ? TR_ENGAGED : (pre == MT_CURRENT ? TR_CURRENT : (pre == MT_ENDSTOP ? TR_ENDSTOP : TR_DISABLED)); testFinishMs = now ? now : 1; testDeltaDeg = -999.0f; testPeakA = -1.0f;
            }
        }
        if (!testActive) return false;
        if (SteerCommandReceiver::isAutoSteerActive()) { finishTest(now, 3, TR_ENGAGED); }
        else if (CurrentSensor::isKickoutTriggered()) { finishTest(now, 3, TR_CURRENT); }
        else if (testAtEndStop(testDir)) { finishTest(now, 3, TR_ENDSTOP); }   // the wheel reached the end of its range during the pulse
        else if (now - testStartMs >= testDurMs) { finishTest(now, 2, TR_NONE); }
        else {
            drive(testDir * testPwm);
            lastPwm = testDir * testPwm;
            if (CurrentSensor::isEnabled()) { float pk = CurrentSensor::getRecentPeakA(); if (pk > testPeakA) testPeakA = pk; }
        }
        return true;
    }

    static void begin() {
        if (!ENABLE_MOTOR_DRIVER) return;
        initHardware();
        targetAngleDeg = 0.0f;
    }

    // Called from nmUpdateMotorFromCommand() (network_manager.h) ONLY when
    // steering is really enabled and we have a fresh command - see there.
    static void setTargetAngleDeg(float targetDeg) {
        if (!ENABLE_MOTOR_DRIVER) return;
        targetAngleDeg = targetDeg;
    }

    // Call every loop() pass UNCONDITIONALLY (not only when steering is
    // enabled) - it is the only way for the motor to stop RELIABLY
    // on disable/loss of communication/current overrun, instead of
    // "freezing" at the last PWM output.
    static void update() {
        deadman.feed(millis());   // always, also with the motor disabled: only a real hang of the loop may starve the dead-man timer
        if (!ENABLE_MOTOR_DRIVER) {
            if (testActive) finishTest(millis(), 3, TR_DISABLED);   // features disabled during the test: stop
            // The features were disabled (e.g. from the panel) - zero the PWM if the motor was moving. Without this the last duty cycle stayed
            // on the pin and the motor kept running despite the disable.
            if (hwReady && lastPwm != 0) stopMotor();
            arming.reset();      // switching the feature on again needs a fresh frame with steering OFF
            dirGuard.reset();
            return;
        }
        initHardware();   // idempotent: the first pass after enabling from the panel configures PWM and DIR
        unsigned long now = millis();
        guard.configure({ENABLE_RUNAWAY_DETECT, (uint32_t)RUNAWAY_WINDOW_MS, RUNAWAY_MIN_MOVE_DEG, RUNAWAY_MIN_ERROR_DEG});
        if (resetRequested) { resetRequested = false; guard.reset(); loggedTrip = false; LogBuffer::println("[MOTOR] Runaway detector lock cleared from the panel."); }
        guard.tick(now, !SteerCommandReceiver::isDisengagedConfirmed());   // the latch is released only by a confirmed "steering off", not by a link loss
        if (serviceTest(now)) return;   // the manual test takes over the motor; it requires steering to be off, so the runaway detector window is inactive anyway
        if (!guard.isTripped()) loggedTrip = false;

        // Arming: after a restart the motor stays still until steering was seen switched OFF in AgOpenWeb at least once.
        arming.update(SteerCommandReceiver::isFresh(), SteerCommandReceiver::isAutoSteerEngaged());
        if (arming.blocking(SteerCommandReceiver::isAutoSteerActive())) {
            if (!loggedUnarmed) {
                loggedUnarmed = true;
                LogBuffer::println("[MOTOR] Steering is requested but the motor is NOT armed after start-up: switch steering OFF in AgOpenWeb and ON again.");
            }
        } else if (arming.armed()) loggedUnarmed = false;
        if (deadman.trips() != deadmanSeen) {
            deadmanSeen = deadman.trips();
            LogBuffer::printf("[MOTOR] The main loop stalled for over %u ms - the motor output was cut by the safety timer (stall no. %u).\n",
                              (unsigned)MotorDeadman::LIMIT_MS, (unsigned)deadmanSeen);
        }

        // The whole decision chain (gates, end stop, P controller, runaway detector, reversal pause) is the pure function
        // MotorPipeline::step (actuators/motor_pipeline.h), simulated on a computer with faults. Here: gather the inputs, apply the result.
        MotorPipeline::In in;
        in.now = (uint32_t)now;
        in.engagedActive = SteerCommandReceiver::isAutoSteerActive();
        in.sensorFresh = SteeringAngleSensor::isFresh();   // without a fresh sensor the actual angle is frozen and the P controller would push blindly
        in.kickout = CurrentSensor::isKickoutTriggered();
        in.target = targetAngleDeg;
        in.actual = SteeringAngleSensor::getAngleDeg();
        in.sampleSeq = SteeringAngleSensor::getSampleSeq();
        in.raw = SteeringAngleSensor::getRaw();
        in.gainP = (float)SteerSettingsReceiver::getGainP();
        in.highPwm = SteerSettingsReceiver::getHighPWM();
        in.minPwm = SteerSettingsReceiver::getMinPWM();
        in.lo = fminf(STEERING_ANGLE_LEFT_DEG, STEERING_ANGLE_RIGHT_DEG) - END_STOP_MARGIN_DEG;
        in.hi = fmaxf(STEERING_ANGLE_LEFT_DEG, STEERING_ANGLE_RIGHT_DEG) + END_STOP_MARGIN_DEG;
        const MotorPipeline::Out out = MotorPipeline::step(guard, dirGuard, arming, in);
        if (out.why == MotorPipeline::W_BLOCKED) { stopMotor(); return; }
        lastErrorDeg = out.error;
        const float actualAngle = in.actual, error = out.error;
        if (out.why == MotorPipeline::W_ENDSTOP) { stopMotor(); return; }
        if (out.why == MotorPipeline::W_RUNAWAY) {
            stopMotor();
            if (!loggedTrip) {
                loggedTrip = true;
                if (guard.state() == RunawayGuard::S_WRONG_DIR)
                    LogBuffer::printf("[MOTOR] STOP: the wheel moved the OPPOSITE way to the command (movement %.1f deg) - reversed direction? Tick \"Reverse direction\" in the panel (Calibration -> Motor).\n", guard.lastMoveDeg());
                else if (guard.state() == RunawayGuard::S_FROZEN)
                    LogBuffer::printf("[MOTOR] STOP: raw angle sensor reading identical for %u samples despite drive - sensor frozen or ADS1115 hung? Check the sensor power and wiring.\n", (unsigned)RunawayGuard::FROZEN_SAMPLES);
                else
                    LogBuffer::printf("[MOTOR] STOP: motor driven but the angle does not change (movement %.1f deg in %lu ms) - check motor power, clutch, angle sensor.\n", guard.lastMoveDeg(), RUNAWAY_WINDOW_MS);
            }
            return;
        }
        const int pwmOut = out.pwm;
        if (pwmOut == 0) stopMotor(); else { drive(pwmOut); lastPwm = pwmOut; }
        if (DEBUG_MOTOR && now - lastLogMs >= 1000) {
            lastLogMs = now;
            LogBuffer::printf("[MOTOR] pwm=%d error=%.1f angle=%.1f window_movement=%.1f state=%u\n", pwmOut, error, actualAngle, guard.moveDeg(), (unsigned)guard.state());
        }
    }

    static bool isEnabled() { return ENABLE_MOTOR_DRIVER; }

    // --- web panel ---
    static bool isArmed() { return arming.armed(); }
    // steering is requested but the motor is not armed after a restart (the operator must switch steering off and on)
    static bool isArmBlocking() { return ENABLE_MOTOR_DRIVER && arming.blocking(SteerCommandReceiver::isAutoSteerActive()); }
    static uint32_t deadmanTrips() { return deadman.trips(); }
    static int getLastPwm() { return lastPwm; }
    static float getLastErrorDeg() { return lastErrorDeg; }
    static uint8_t runawayState() { return ENABLE_MOTOR_DRIVER ? (uint8_t)guard.state() : (uint8_t)RunawayGuard::S_OFF; }
    static float runawayMoveDeg() { return guard.moveDeg(); }
    static uint32_t runawayTrips() { return guard.trips(); }
    static uint8_t runawayLastReason() { return (uint8_t)guard.lastReason(); }
    static float runawayLastMoveDeg() { return guard.lastMoveDeg(); }
    static void requestRunawayReset() { resetRequested = true; }
    // result of the last manual test (panel)
    static uint8_t getTestState() { return testState; }
    static uint8_t getTestReason() { return testReason; }
    static int getTestDir() { return testDir; }
    static int getTestPwm() { return testPwm; }
    static unsigned long getTestMs() { return testDurMs; }
    static float getTestDeltaDeg() { return testDeltaDeg; }       // -999 = no angle reading
    static float getTestPeakA() { return testPeakA; }             // -1 = no current measurement
    static long getTestAgeMs() { return testFinishMs == 0 ? -1L : (long)(millis() - testFinishMs); }
};

float MotorDriver::targetAngleDeg = 0.0f;
RunawayGuard MotorDriver::guard;
int MotorDriver::lastPwm = 0;
float MotorDriver::lastErrorDeg = 0.0f;
bool MotorDriver::resetRequested = false;
bool MotorDriver::loggedTrip = false;
bool MotorDriver::hwReady = false;
std::atomic<bool> MotorDriver::testReqPending(false);
int MotorDriver::testReqDir = 0;
int MotorDriver::testReqPwm = 0;
unsigned long MotorDriver::testReqMs = 0;
bool MotorDriver::testActive = false;
int MotorDriver::testDir = 0;
int MotorDriver::testPwm = 0;
unsigned long MotorDriver::testStartMs = 0;
unsigned long MotorDriver::testDurMs = 0;
unsigned long MotorDriver::testFinishMs = 0;
float MotorDriver::testAngleStart = 0.0f;
float MotorDriver::testPeakA = 0.0f;
float MotorDriver::testDeltaDeg = -999.0f;
uint8_t MotorDriver::testState = 0;
uint8_t MotorDriver::testReason = 0;
unsigned long MotorDriver::lastLogMs = 0;
MotorDeadman MotorDriver::deadman;
SteerArming MotorDriver::arming;
DirGuard MotorDriver::dirGuard;
bool MotorDriver::deadmanTaskStarted = false;
uint32_t MotorDriver::deadmanSeen = 0;
bool MotorDriver::loggedUnarmed = false;

// Control of the implement's hydraulic lift (valve/solenoid) by the "hydLift" command of PGN239 (see
// connectivity/pgn_receivers.h): AgOpenWeb sends 0 = nothing, 1 = LOWER (tool inside the worked area), 2 = RAISE (headland).
// Two outputs can carry it - HydUp and HydDown - on whichever pins AgOpenWeb assigns them in its Pin Config (PGN 236, see
// io/machine_outputs.h); by default HydUp sits on the last MCP23017 pin. Level mode: the output follows the command; timed
// pulses (Settings -> Features): raiseTime / lowerTime seconds from PGN 238. In addition there is a MANUAL override from the
// web page (see web/web_server.h) for bench tests, or when you want to raise/lower the implement without AgOpenWeb.
//
// IMPORTANT - the physical pin write does NOT happen here: the lift shares the MCP23017 chip with the sections
// (io/section_expander.h), which drives all 16 pins with ONE I2C write (writeGPIOAB) for performance. Two independent
// writeGPIOAB() calls would overwrite each other, so SectionExpander::update() calls HydraulicLift::update() and merges
// isActive() (raise) and isDownActive() (lower) with the sections into one shared write.
class HydraulicLift {
private:
    // Manual override (from the web page) or the AgOpenWeb command: HydControl (io/machine_outputs.h, host-tested).
    // It starts in automatic mode - a safe default after every start, so manual control from a previous session is never left "frozen".
    static HydControl ctl;

public:
    // Call once per loop pass, before isActive() / isDownActive() are used (SectionExpander::update does).
    static void update() {
        const bool cfg = MachineConfigReceiver::hasReceived();   // unknown time -> level mode
        ctl.update(MachineCommandReceiver::getHydLift(), MachineCommandReceiver::isFresh(), millis(),
                   cfg ? MachineConfigReceiver::getRaiseTime() : 0, cfg ? MachineConfigReceiver::getLowerTime() : 0);
        // A manual override with no lift pin left in the Pin Config could never be cleared by the operator and would come back
        // when a pin is mapped again: give control back to AgOpenWeb.
        if (ctl.manual() && !isMapped()) ctl.setAuto();
    }
    // The expander is switched off: nothing is driven.
    static void idle() { ctl.idle(); }

    // true if the implement should currently be raised (the HydUp output) - from the manual override or from AgOpenWeb
    // (hydLift == 2; false also when the command is stale - a safe default on loss of communication, same as the sections).
    static bool isActive() { return ctl.up(); }
    // true if the LOWER output (HydDown) is on.
    static bool isDownActive() { return ctl.down(); }

    static bool isManualOverride() { return ctl.manual(); }
    static bool getManualState() { return ctl.manualRaise(); }

    // Called from the web page - enables manual control: raise (true) or lower (false).
    static void setManual(bool raise) {
        ctl.setManual(raise);
        LogBuffer::printf("[HYDRAULICS] Manual control: %s\n", raise ? "RAISE" : "LOWER");
    }

    // Returns to automatic control (AgOpenWeb) - the lift immediately follows the command from PGN239 again.
    static void setAuto() {
        ctl.setAuto();
        LogBuffer::println("[HYDRAULICS] Back to automatic control (AgOpenWeb).");
    }

    // true when the AgOpenWeb pin map has a lift output (HydUp or HydDown) on one of the existing pins
    static bool isMapped() { const MachinePinMap& m = MachinePinConfigReceiver::map(); return m.has(MachineFn::HYD_UP) || m.has(MachineFn::HYD_DOWN); }
};

HydControl HydraulicLift::ctl;
