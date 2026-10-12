#pragma once
// ======================================================================
// sensors/current_sensor.h
// EN: Motor current sensor driver: reads the shared ADS1115 (channel A1) in gaps between angle reads and
//     applies the protection logic.
// PL: Sterownik czujnika prądu silnika: odczyt ze wspólnego ADS1115 (kanał A1) w przerwach między odczytami
//     kąta i logika ochrony.
// Used by: actuators/actuators.h, connectivity/network_manager.h, sensors/steering_angle_sensor.h,
//     web/web_server.h
// Depends on: config.h, current_math.h, log_buffer.h, pgn_receivers.h
// ======================================================================
#include <Arduino.h>
#include <Preferences.h>
#include "../config/config.h"
#include "../web/log_buffer.h"
#include "../connectivity/pgn_receivers.h"
#include "current_math.h"

// Motor current sensor ACS71240KEXBLT-030B3 (+-30 A, 44 mV/A, zero 1.65 V) - reading, zero calibration and overcurrent
// protection ("kickout"). All the conversion and protection logic is in sensors/current_math.h (host-tested);
// this class only ties it to the hardware and settings.
//
// READING: the same ADS1115 as the angle sensor. Conversions of the current channel (CURRENT_SENSOR_ADS_CHANNEL) are ordered by a non-blocking
// state machine in SteeringAngleSensor::update() - the angle has priority, current is measured in the gaps (about 30-60 samples/s) and arrives here
// through onSample().
//
// PROTECTION (MotorDriver::update() asks isKickoutTriggered() every pass): the motor STOPS when
//   - overcurrent (soft threshold after a time, or hard) - a latch, cleared by disabling steering for 1 s or from the panel,
//   - sensor error (voltage outside 0.15-3.15 V: not connected/short),
//   - no fresh samples (I2C/ADS) - current cannot be monitored, so we do not drive.
// With the sensor disabled (ENABLE_CURRENT_SENSOR = false) the protection is inactive, as before.
//
// ZERO CALIBRATION: the sensor has a zero tolerance (tens of mV = about 1 A), so with the motor off we zero it
// from the panel (averaging 64 samples, stored in NVS). Without calibration the nominal zero of 1.65 V is used.
class CurrentSensor {
private:
    static Preferences prefs;
    static CurrentGuard guard;
    static ZeroCalibrator cal;
    static float zeroV;
    static bool calRequested, resetRequested;   // set from the panel (core 0), handled in update() (core 1)
    static unsigned long lastLogMs;

public:
    static void begin() {
        prefs.begin("current", false);
        float z = prefs.getFloat("zeroV", CurrentMath::ZERO_V_NOMINAL);
        zeroV = CurrentMath::zeroPlausible(z) ? z : CurrentMath::ZERO_V_NOMINAL;
        applyLimits();
        if (!ENABLE_CURRENT_SENSOR) return;
        LogBuffer::printf("[CURRENT] ACS71240 active: channel A%u, zero %.3f V, kickout %.1f A / %lu ms, hard %.1f A\n",
                          (unsigned)CURRENT_SENSOR_ADS_CHANNEL, zeroV, CURRENT_KICKOUT_A, CURRENT_KICKOUT_MS, CURRENT_HARD_LIMIT_A);
    }

    // The thresholds may change from the panel while running - we copy them every pass (3 numbers).
    static void applyLimits() {
        guard.configure({CURRENT_KICKOUT_A, (uint32_t)CURRENT_KICKOUT_MS, CURRENT_HARD_LIMIT_A});
    }

    // Called from SteeringAngleSensor::update() when the current channel conversion finishes (core 1).
    static void onSample(float volts, unsigned long now) {
        if (!ENABLE_CURRENT_SENSOR) return;
        if (cal.running() && cal.add(volts) && cal.status() == ZeroCalibrator::Z_DONE) {
            zeroV = cal.result();
            prefs.putFloat("zeroV", zeroV);
            LogBuffer::printf("[CURRENT] Zero calibrated: %.3f V\n", zeroV);
        }
        guard.onSample(now, volts, zeroV);
    }

    // Called every loop pass (NM_STEP in network_manager.h), BEFORE MotorDriver::update().
    static void update() {
        if (!ENABLE_CURRENT_SENSOR) return;
        unsigned long now = millis();
        applyLimits();
        if (calRequested) { calRequested = false; cal.start(64); }
        if (resetRequested) { resetRequested = false; guard.reset(); LogBuffer::println("[CURRENT] Overcurrent latch cleared from the panel."); }
        CurrentGuard::State before = guard.state(now);
        guard.update(now, !SteerCommandReceiver::isDisengagedConfirmed());   // released only by a confirmed "steering off", not by a link loss
        if (before == CurrentGuard::S_OVERCURRENT && guard.state(now) != CurrentGuard::S_OVERCURRENT)
            LogBuffer::println("[CURRENT] Overcurrent latch released (steering disengaged).");
        if (now - lastLogMs >= 1000) {
            lastLogMs = now;
            LogBuffer::printfIf(DEBUG_CURRENT_SENSOR, "[CURRENT] %.2fA (avg %.2fA, peak %.2fA) voltage=%.3fV state=%u age=%ldms\n",
                                guard.amps(), guard.avgAmps(), guard.peakAmps(), guard.volts(), (unsigned)guard.state(now), guard.ageMs(now));
        }
    }

    // true = the motor MUST STOP (overcurrent, sensor error or no data). An inactive sensor never blocks.
    static bool isKickoutTriggered() { return ENABLE_CURRENT_SENSOR && guard.tripped(millis()); }
    static bool isEnabled() { return ENABLE_CURRENT_SENSOR; }

    // --- web panel (core 0): only number reads and request submissions, executed in update() ---
    static uint8_t stateCode() { return ENABLE_CURRENT_SENSOR ? (uint8_t)guard.state(millis()) : (uint8_t)CurrentGuard::S_OFF; }
    static float getAmps() { return guard.amps(); }
    static float getAvgAmps() { return guard.avgAmps(); }
    static float getPeakAmps() { return guard.peakAmps(); }
    static float getVolts() { return guard.volts(); }
    static float getZeroV() { return zeroV; }
    static long getAgeMs() { return guard.ageMs(millis()); }
    static bool isLatched() { return guard.isLatched(); }
    static uint32_t getSamples() { return guard.sampleCount(); }
    static uint32_t getTrips() { return guard.trips(); }
    static float getLastTripA() { return guard.latchedAmps_(); }               // current at the moment of the last trip (0 = none yet)
    static float getRecentPeakA() { return guard.recentPeakA(millis()); }      // largest |current| of the last ~1 s (charts, history)
    static uint8_t calStatus() { return (uint8_t)cal.status(); }
    static uint16_t calProgress() { return cal.progressPercent(); }
    static const char* calFailReason() { return cal.failReason(); }
    static void requestZeroCalibration() { calRequested = true; }
    static void requestReset() { resetRequested = true; }
};

Preferences CurrentSensor::prefs;
CurrentGuard CurrentSensor::guard;
ZeroCalibrator CurrentSensor::cal;
float CurrentSensor::zeroV = CurrentMath::ZERO_V_NOMINAL;
bool CurrentSensor::calRequested = false;
bool CurrentSensor::resetRequested = false;
unsigned long CurrentSensor::lastLogMs = 0;
