#pragma once
// ======================================================================
// sensors/current_math.h
// EN: Motor current sensor math (ACS71240, 44 mV/A): conversion, zero calibration and over-current latch logic.
// PL: Matematyka czujnika prądu silnika (ACS71240, 44 mV/A): przeliczanie, kalibracja zera i logika zatrzasku
//     nadprądowego.
// Used by: sensors/current_sensor.h
// Depends on: msg.h
// ======================================================================
#include "../web/msg.h"
// Motor current sensor ACS71240KEXBLT-030B3 (Allegro): conversion, zero calibration and protection logic.
// Pure C++ (no Arduino) - host-tested (tests/test_current.cpp).
//
// Data from the datasheet for this variant (3.3 V, bidirectional):
//   range +-30 A, sensitivity 44 mV/A, voltage at 0 A = 1.65 V, supply 3.0-3.6 V,
//   total accuracy typically +-6% (which is why the protection thresholds have margin and we calibrate the zero in place).
//   At +-30 A the output is about 0.33-2.97 V, i.e. within the range of an ADS1115 powered from 3.3 V.
#include <cstdint>
#include <cmath>

namespace CurrentMath {
    constexpr float SENS_V_PER_A   = 0.044f;   // 44 mV/A
    constexpr float ZERO_V_NOMINAL = 1.65f;    // output voltage at 0 A
    // Outside this range the output cannot be with a working sensor (the usable range is 0.33-2.97 V):
    // 0 V = not connected/unpowered/shorted, >3.15 V = short to supply or damage.
    constexpr float SENSOR_V_MIN   = 0.15f;
    constexpr float SENSOR_V_MAX   = 3.15f;

    inline bool voltsPlausible(float v) { return std::isfinite(v) && v >= SENSOR_V_MIN && v <= SENSOR_V_MAX; }
    inline float voltsToAmps(float volts, float zeroV) { return (volts - zeroV) / SENS_V_PER_A; }
    inline bool zeroPlausible(float z) { return std::isfinite(z) && z >= 1.55f && z <= 1.75f; }   // +-0.10 V (about 2.3 A) around 1.65 V; the typical zero spread is only tens of mV
}

// ---------------------------------------------------------------------------------------------------------
// Zero calibration: averages N samples with the motor OFF (no current). Rejects the result when the samples are
// unstable (current flowing or interference) or the average is out of range (sensor not connected).
class ZeroCalibrator {
public:
    enum Status : uint8_t { Z_IDLE = 0, Z_RUNNING = 1, Z_DONE = 2, Z_FAILED = 3 };
    static constexpr float MAX_SPREAD_V = 0.060f;   // min-max spread in the window; ~1.4 A p-p, i.e. clearly above the sensor noise

    void start(uint16_t samples = 64) {
        target = samples ? samples : 1; n = 0; sum = 0.0; mn = 1e9f; mx = -1e9f; st = Z_RUNNING; reason = "";
    }
    bool running() const { return st == Z_RUNNING; }
    // true = that was the last sample (the result is ready: status() == DONE or FAILED)
    bool add(float volts) {
        if (st != Z_RUNNING) return false;
        if (!CurrentMath::voltsPlausible(volts)) { st = Z_FAILED; reason = MSG("voltage out of range (sensor not connected?)"); return true; }
        sum += volts; if (volts < mn) mn = volts; if (volts > mx) mx = volts; n++;
        if (n < target) return false;
        float mean = (float)(sum / n);
        if (mx - mn > MAX_SPREAD_V) { st = Z_FAILED; reason = MSG("unstable reading (current flowing or interference?)"); return true; }
        if (!CurrentMath::zeroPlausible(mean)) { st = Z_FAILED; reason = MSG("zero out of range (current flowing or sensor not connected?)"); return true; }
        res = mean; st = Z_DONE; return true;
    }
    Status status() const { return st; }
    float result() const { return res; }
    const char* failReason() const { return reason; }
    uint16_t progressPercent() const { return st == Z_RUNNING ? (uint16_t)((100u * n) / target) : (st == Z_DONE ? 100 : 0); }
private:
    Status st = Z_IDLE; uint16_t target = 64, n = 0; double sum = 0.0; float mn = 1e9f, mx = -1e9f, res = CurrentMath::ZERO_V_NOMINAL; const char* reason = "";
};

// ---------------------------------------------------------------------------------------------------------
// Motor protection ("kickout"). The motor MUST stop (tripped() == true) when:
//   - current exceeds the hard threshold (hardA) in >= 2 consecutive samples  -> overcurrent block (latch),
//   - current exceeds the soft threshold (kickoutA) continuously for >= kickoutMs -> overcurrent block (latch),
//   - the sensor voltage is out of range (not connected/short)      -> sensor error (no latch: clears after 10 good samples),
//   - no fresh samples (I2C, ADS, sensor disabled)               -> no data (current cannot be monitored).
// The latch is cleared by: a manual reset from the panel, or (current below the soft threshold AND steering disabled for RELEASE_MS).
class CurrentGuard {
public:
    enum State : uint8_t { S_OFF = 0, S_NO_DATA = 1, S_OK = 2, S_OVERCURRENT = 3, S_FAULT = 4 };
    struct Limits { float kickoutA; uint32_t kickoutMs; float hardA; };
    static constexpr uint32_t STALE_MS = 250;        // this long without a sample = no data (samples arrive every ~15-40 ms)
    static constexpr uint32_t RELEASE_MS = 1000;     // steering disabled AND current below the threshold this long for the latch to clear itself
    static constexpr uint8_t  FAULT_SAMPLES = 3;     // this many bad samples in a row = sensor error (a single interference glitch does not block)
    static constexpr uint8_t  RECOVER_SAMPLES = 10;  // this many good ones in a row for the error to clear
    static constexpr uint8_t  HARD_SAMPLES = 2;      // hard threshold: two consecutive samples (immune to a single glitch)

    void configure(const Limits& l) { lim = l; }
    const Limits& limits() const { return lim; }

    void onSample(unsigned long now, float volts, float zeroV) {
        lastSampleMs = now ? now : 1; hasSample = true; lastVolts = volts; samples++;
        if (!CurrentMath::voltsPlausible(volts)) {
            goodRun = 0; if (badRun < 255) badRun++;
            if (badRun >= FAULT_SAMPLES) fault = true;
            overSince = 0; hardRun = 0;                      // we do not judge current on garbage
            return;
        }
        badRun = 0; if (goodRun < 255) goodRun++;
        if (fault && goodRun >= RECOVER_SAMPLES) fault = false;
        float a = CurrentMath::voltsToAmps(volts, zeroV);
        ampsNow = a;
        if (!avgInit) { avg = a; avgInit = true; } else avg += 0.2f * (a - avg);
        float mag = std::fabs(a);
        if (mag > peak) peak = mag;
        notePeak(now, mag);
        if (fault) return;                                    // while the sensor is untrustworthy, we do not trust the current value either
        hardRun = (mag >= lim.hardA) ? (uint8_t)(hardRun < 255 ? hardRun + 1 : 255) : 0;
        if (mag >= lim.kickoutA) { if (!overSince) overSince = now ? now : 1; } else overSince = 0;
        if (!latched && (hardRun >= HARD_SAMPLES || (overSince && (now - overSince) >= lim.kickoutMs))) {
            latched = true; latchedAmps = a; releaseSince = 0; tripCount++;
        }
    }

    // Call every loop pass. engaged = true while AgOpenWeb has steering enabled OR its state is unknown (link down): only a
    // confirmed "steering off" releases the latch.
    void update(unsigned long now, bool engaged) {
        if (!latched) return;
        // The latch clears only when there is NO cause left: a fresh, trustworthy reading is below the soft threshold.
        // Without this, when the current still exceeded the threshold (e.g. a wrongly connected sensor), the latch cleared every second and set
        // again (log spam, short windows with no block). No fresh data = cannot confirm, so we hold.
        bool cleared = !stale(now) && !fault && std::fabs(ampsNow) < lim.kickoutA;
        if (engaged || !cleared) { releaseSince = 0; return; }
        if (!releaseSince) releaseSince = now ? now : 1;
        if (now - releaseSince >= RELEASE_MS) clearLatch();
    }

    bool stale(unsigned long now) const { return !hasSample || (now - lastSampleMs) > STALE_MS; }
    bool tripped(unsigned long now) const { return latched || fault || stale(now); }
    State state(unsigned long now) const {
        if (fault) return S_FAULT;
        if (latched) return S_OVERCURRENT;
        if (stale(now)) return S_NO_DATA;
        return S_OK;
    }
    void reset() { clearLatch(); peak = 0.0f; }      // manual clear from the panel (also clears the peak)
    void resetPeak() { peak = 0.0f; }

    float amps() const { return ampsNow; }
    float avgAmps() const { return avg; }
    float peakAmps() const { return peak; }
    float latchedAmps_() const { return latchedAmps; }
    float volts() const { return lastVolts; }
    uint32_t sampleCount() const { return samples; }        // samples since start (the panel computes the sampling rate from this)
    uint32_t trips() const { return tripCount; }            // how many times the overcurrent latch was set since start
    // The largest |current| of the last ~1.2-1.6 s (4 buckets of 400 ms) - NOT cleared on read, so the same
    // value can be read by the panel (live) and the chart history (once per second) without losing short peaks.
    float recentPeakA(unsigned long now) const {
        unsigned long e = now / BUCKET_MS; float m = 0.0f;
        for (int i = 0; i < NBUCKETS; i++) if (bucketValid[i] && (e - bucketEpoch[i]) < NBUCKETS && bucketPeak[i] > m) m = bucketPeak[i];
        return m;
    }
    bool isLatched() const { return latched; }
    long ageMs(unsigned long now) const { return hasSample ? (long)(now - lastSampleMs) : -1L; }
private:
    static constexpr unsigned long BUCKET_MS = 400;
    static constexpr int NBUCKETS = 4;
    void notePeak(unsigned long now, float mag) {
        unsigned long e = now / BUCKET_MS; int i = (int)(e % NBUCKETS);
        if (!bucketValid[i] || bucketEpoch[i] != e) { bucketValid[i] = true; bucketEpoch[i] = e; bucketPeak[i] = mag; }
        else if (mag > bucketPeak[i]) bucketPeak[i] = mag;
    }
    void clearLatch() { latched = false; overSince = 0; hardRun = 0; releaseSince = 0; }
    uint32_t samples = 0, tripCount = 0;
    bool bucketValid[NBUCKETS] = {false, false, false, false};
    unsigned long bucketEpoch[NBUCKETS] = {0, 0, 0, 0};
    float bucketPeak[NBUCKETS] = {0.0f, 0.0f, 0.0f, 0.0f};
    Limits lim{8.0f, 400, 15.0f};
    bool hasSample = false, fault = false, latched = false, avgInit = false;
    unsigned long lastSampleMs = 0, overSince = 0, releaseSince = 0;
    uint8_t badRun = 0, goodRun = 0, hardRun = 0;
    float ampsNow = 0.0f, avg = 0.0f, peak = 0.0f, latchedAmps = 0.0f, lastVolts = 0.0f;
};
