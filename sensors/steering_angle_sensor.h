#pragma once
// ======================================================================
// sensors/steering_angle_sensor.h
// EN: Steering angle sensor read through the ADS1115 (I2C), with left/right calibration stored in NVS.
// PL: Czujnik kąta skrętu odczytywany przez ADS1115 (I2C), z kalibracją lewo/prawo zapisaną w NVS.
// Used by: actuators/actuators.h, connectivity/network_manager.h, connectivity/pgn_messages.h, web/web_server.h
// Depends on: config.h, current_sensor.h, i2c_retry.h, log_buffer.h, pgn_receivers.h, steering_math.h
// ======================================================================
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Preferences.h>
#include "../config/config.h"
#include "../web/log_buffer.h"
#include "../connectivity/pgn_receivers.h"
#include "steering_math.h"
#include "i2c_retry.h"
#include "current_sensor.h"

// Reading the axle angle sensor (NTY ECX-VW-000 - a potentiometric
// headlight levelling sensor, used here as the steering angle sensor) through an external
// 16-bit ADC ADS1115 (I2C).
//
// This class only READS the angle; the motor control loop is in actuators.h.
//
// CALIBRATION: BACK LOCALLY ON THE MODULE (not in AgOpenWeb) - the "Zero WAS" button
// in the current version of AgOpenWeb is broken (confirmed: it does not
// zero the displayed angle, WAS Offset grows endlessly with every
// click instead of being replaced). Until this is fixed we use our
// own calibration (as before switching to PGN252) - see the panel
// "Angle sensor calibration" on the web page: turn the wheel all the way LEFT,
// click "Save as LEFT", then all the way RIGHT, "Save as RIGHT".
// It works immediately and is stored permanently in NVS (survives a restart).
//
// We STILL RECEIVE PGN252 (countsPerDeg/steerOffset/ackermanFix) (see
// SteerSettingsReceiver) and log it - for reference/diagnostics, and ready to
// be reused, should AgOpenWeb fix "Zero WAS" - but we do NOT use
// it to compute the angle until we are sure these values are
// correct.
class SteeringAngleSensor {
private:
    static Adafruit_ADS1115 ads;
    static bool sensorFound;
    static int16_t lastRaw;
    static uint32_t sampleSeq;   // grows on every valid NEW angle reading (the runaway detector uses it to detect a frozen sensor)
    static float lastAngleDeg;
    static unsigned long lastLogMillis;
    static unsigned long lastReadMillis; // 0 = not read yet (the first read without delay)
    // A new conversion every this many ms (start-to-start) - see update().
    static const unsigned long READ_INTERVAL_MS = 30;

    // --- Non-blocking ADS1115 read: registers through Wire, without ads.readADC_*() ---
    static const uint8_t  ADS_REG_CONVERT = 0x00;
    static const uint8_t  ADS_REG_CONFIG  = 0x01;
    static const uint32_t CONVERSION_MIN_US     = 8000;   // 128 SPS = ~7.8 ms - we do not ask earlier
    static const uint32_t CONVERSION_TIMEOUT_US = 50000;  // no result after 50 ms = error, give up
    enum class AdcState : uint8_t { IDLE, CONVERTING };
    static AdcState adcState;
    // The second channel of the same ADS1115: motor current (sensors/current_sensor.h). The angle has PRIORITY - its
    // READ_INTERVAL_MS window is untouchable; current conversions go into the gaps (about 1-2 per 30 ms window), only when enough time
    // remains before the next angle read to finish the conversion (8 ms + margin).
    static const unsigned long CURRENT_MIN_INTERVAL_MS = 8;
    static const unsigned long CURRENT_GUARD_MS = 12;
    static bool convIsCurrent;              // whose conversion is running: current (true) or angle (false)
    static unsigned long lastCurrentStartMs; // 0 = not ordered yet
    static uint32_t convStartUs;
    static I2cRetry retry;       // back-off after failed transactions, "lost" verdict, re-probe timer
    static uint32_t i2cErrors;   // 32 bits - 16 bits wrapped around during long operation with interference
    static unsigned long lastGoodReadMillis; // 0 = never read successfully

    // Configuration of a single conversion - it MUST match what
    // begin() sets (gain ±6.144 V = GAIN_TWOTHIRDS = PGA bits 000, default 128 SPS):
    //   OS=1 (start) | MUX single-ended channel N | PGA 000 | MODE=1 (single-shot)
    //   | DR=100 (128 SPS) | comparator disabled (COMP_QUE=11)
    static uint16_t configForChannel(uint8_t ch) {
        return (uint16_t)(0x8000 | (0x4000 | ((uint16_t)ch << 12)) | 0x0000 | 0x0100 | 0x0080 | 0x0003);
    }
    static bool adsWriteReg(uint8_t reg, uint16_t value) {
        Wire.beginTransmission((uint8_t)ADS1115_I2C_ADDRESS);
        Wire.write(reg);
        Wire.write((uint8_t)(value >> 8));
        Wire.write((uint8_t)(value & 0xFF));
        return Wire.endTransmission() == 0;
    }
    static bool adsReadReg(uint8_t reg, uint16_t& value) {
        Wire.beginTransmission((uint8_t)ADS1115_I2C_ADDRESS);
        Wire.write(reg);
        if (Wire.endTransmission() != 0) return false;
        if (Wire.requestFrom((uint8_t)ADS1115_I2C_ADDRESS, (size_t)2) != 2) return false;
        uint8_t hi = Wire.read();
        uint8_t lo = Wire.read();
        value = ((uint16_t)hi << 8) | lo;
        return true;
    }

    static int16_t appliedAdcLeft;
    static int16_t appliedAdcRight;
    // false when the LEFT/RIGHT points are too close (see steering_math.h) - the angle is then
    // invalid (0), and the motor blocked (isFresh() = false), instead of dividing by zero.
    static bool calibrationValid;
    // Both end points were saved at least once (stored in NVS). Without this the defaults 0..32767 would count as a valid
    // calibration and a sensor that was never calibrated could drive the motor with a meaningless angle mapping.
    static bool calLeftSet, calRightSet;
    static bool calComplete() { return calLeftSet && calRightSet; }
    static bool rawInRange;      // the last raw reading lies within the calibrated range + margin (SteeringMath::rawPlausible)
    static bool loggedOutOfRange;
    static Preferences prefs;

    // Presence check: an empty write to the ADS1115 address (acknowledged = the chip is on the bus).
    static bool adsPresent() {
        Wire.beginTransmission((uint8_t)ADS1115_I2C_ADDRESS);
        return Wire.endTransmission() == 0;
    }
    // A failed transaction: counted, the state machine restarts, and after I2cRetry::LOST_AFTER failures in a row
    // the chip is declared lost (the owner stops polling it and only probes its presence every few seconds).
    static void adsFail(unsigned long now) {
        i2cErrors++;
        adcState = AdcState::IDLE;
        if (retry.fail((uint32_t)now)) {
            sensorFound = false;
            LogBuffer::println("[ANGLE] ADS1115 stopped answering on I2C - angle and current readings stopped, probing again every 2 s.");
        }
    }
    // While lost: a cheap presence probe, and a full re-initialisation once the chip answers again.
    static void tryRecover(unsigned long now) {
        if (!retry.probeDue((uint32_t)now) || !adsPresent()) return;
        if (!ads.begin(ADS1115_I2C_ADDRESS)) return;
        sensorFound = true;
        retry.recovered();
        adcState = AdcState::IDLE;
        LogBuffer::println("[ANGLE] ADS1115 is answering again - readings resumed.");
    }

public:
    static void begin() {
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
        // SAFEGUARD: without this, if the I2C bus hangs (bad
        // pull-up, interference, loose wire), Wire can block
        // for seconds or forever - which froze the whole main loop.
        // With a timeout the I2C transaction simply returns an error instead of hanging.
        Wire.setTimeout(50); // ms  (STREAM timeout - for readBytes; does not apply to the bus)
        // I2C BUS timeout (a separate setting, capital O): 50 ms by default on ESP32, so every
        // hung transaction (a power disturbance, a hanging chip) stopped the main loop for ~54 ms -
        // visible in the log as series of "[LOOP STALL] 5x ms". The ADS1115 and MCP23017 do not stretch the clock,
        // a transaction takes about 0.5 ms, so 10 ms is a large margin.
        Wire.setTimeOut(10);
        ads.setGain(GAIN_TWOTHIRDS); // range ±6.144 V - fits typical 0-5 V sensors
        // OPTIMIZATION: ads.readADC_SingleEnded() BLOCKS until the end of the
        // ADC conversion - at the library's default speed (128 SPS)
        // that is ~7-8 ms of waiting plus I2C overhead, which in [STEP] measurements turned out
        // to CONSUME NEARLY THE WHOLE time budget of one main loop pass
        // (~11 ms of the ~11-13 ms of the whole pass!). The steering angle is a slowly
        // changing mechanical signal (a potentiometer) - we do not need the
        // slowest/most accurate sample here. The fastest available
        // speed (860 SPS) cuts the wait to ~1.2 ms, freeing
        // ~9-10 ms on every loop pass for the rest of the code.
        // DISABLED: ads.setDataRate(RATE_ADS1115_860SPS) was meant to speed up
        // this read (theory: the default 128 SPS = ~7-8 ms of blocking
        // wait for the conversion), but in an isolated test on THIS
        // particular hardware (with no GPS connected at all) it was confirmed
        // that it was THIS very line that caused a real error - even though
        // the Adafruit documentation/forums suggest that 860 SPS should be
        // safe and faster. We leave the default speed (128 SPS)
        // until we find out why 860 SPS causes problems on this hardware in particular.
        // ads.setDataRate(RATE_ADS1115_860SPS);

        prefs.begin("steering", false);
        appliedAdcLeft = prefs.getShort("adcLeft", 0);
        appliedAdcRight = prefs.getShort("adcRight", 32767);
        calLeftSet = prefs.isKey("adcLeft");
        calRightSet = prefs.isKey("adcRight");
        if (calLeftSet) {
            LogBuffer::printf("[ANGLE] Saved calibration loaded: left=%d right=%d\n",
                              appliedAdcLeft, appliedAdcRight);
        }

        calibrationValid = calComplete() && SteeringMath::spanOk(appliedAdcLeft, appliedAdcRight);
        if (!calComplete()) {
            LogBuffer::println("[ANGLE] WARNING: the angle sensor has never been calibrated - angle invalid, motor blocked. Save the LEFT and RIGHT end points in the panel (Calibration).");
        } else if (!calibrationValid) {
            LogBuffer::printf("[ANGLE] WARNING: the saved LEFT/RIGHT points (%d/%d) are too close together (min. %d) - angle invalid, motor blocked. Save them again in the panel.\n",
                              appliedAdcLeft, appliedAdcRight, SteeringMath::MIN_ADC_SPAN);
        }

        sensorFound = ads.begin(ADS1115_I2C_ADDRESS);
        if (!sensorFound) {
            LogBuffer::println("[ANGLE] ADS1115 not found on I2C - check the connection / address / SDA-SCL pins.");
            retry.markLost((uint32_t)millis());   // it is probed again every few seconds
        } else {
            LogBuffer::printf("[ANGLE] ADS1115 detected at address 0x%02X, channel %d.\n",
                              ADS1115_I2C_ADDRESS, STEERING_SENSOR_ADS_CHANNEL);
        }
    }

    // Call every loop(). NON-BLOCKING: one pass starts a conversion,
    // the following ones (after >= 8 ms) check readiness and collect the result. Each step is
    // one short I2C transaction (<= ~0.5 ms) instead of ~10 ms of waiting in
    // ads.readADC_SingleEnded() (CPU profile: 32% of core 1's time, up to 10.7 ms
    // freezing of the whole loop every 30 ms - delays for IMU/UDP/GPS).
    //
    // The steering angle changes slowly (a potentiometer), so ~33 reads/s
    // are enough. The other passes use lastRaw/lastAngleDeg.
    static void update() {
        if (!sensorFound) { tryRecover(millis()); return; }

        if (adcState == AdcState::IDLE) {
            unsigned long now = millis();
            if (!retry.due((uint32_t)now)) return;   // back-off after failures (a dead chip must not eat the loop)
            uint8_t ch;
            bool forCurrent = false;
            if (lastReadMillis == 0 || (now - lastReadMillis) >= READ_INTERVAL_MS) {
                ch = STEERING_SENSOR_ADS_CHANNEL;            // angle: priority, window untouched
                lastReadMillis = now;
            } else if (CurrentSensor::isEnabled()
                       && (lastCurrentStartMs == 0 || (now - lastCurrentStartMs) >= CURRENT_MIN_INTERVAL_MS)
                       && (READ_INTERVAL_MS - (now - lastReadMillis)) >= CURRENT_GUARD_MS) {
                ch = CURRENT_SENSOR_ADS_CHANNEL;             // current: in the gap between angle reads
                lastCurrentStartMs = now;
                forCurrent = true;
            } else {
                return;
            }
            if (adsWriteReg(ADS_REG_CONFIG, configForChannel(ch))) {
                convStartUs = micros();
                convIsCurrent = forCurrent;
                adcState = AdcState::CONVERTING;
            } else {
                adsFail(now); // ADS not responding - the next attempt follows the back-off
            }
            return;
        }

        // CONVERTING
        uint32_t elapsed = micros() - convStartUs;
        if (elapsed < CONVERSION_MIN_US) return;

        uint16_t cfg;
        if (!adsReadReg(ADS_REG_CONFIG, cfg)) { adsFail(millis()); return; }
        if ((cfg & 0x8000) == 0) {            // OS=0: the conversion is still running
            if (elapsed > CONVERSION_TIMEOUT_US) adsFail(millis());
            return;
        }
        uint16_t raw;
        if (!adsReadReg(ADS_REG_CONVERT, raw)) { adsFail(millis()); return; }
        adcState = AdcState::IDLE;
        retry.ok();
        if (convIsCurrent) {                       // current result: handed to CurrentSensor, the angle is left alone
            CurrentSensor::onSample(ads.computeVolts((int16_t)raw), millis());
            return;
        }
        lastRaw = (int16_t)raw;
        sampleSeq++;
        if (sampleSeq == 0) sampleSeq = 1;   // 0 means "no number" - on counter wraparound we skip it
        lastGoodReadMillis = millis();

        // Linear interpolation between our own end points
        // (see the calibration above) - NOT the parameters from AgOpenWeb (PGN252),
        // which cannot be trusted at the moment (broken "Zero WAS").
        // With LEFT == RIGHT (or nearly) this used to give NaN/Inf, which ended up in AgOpenWeb
        // and in the motor controller. Now: invalid angle => 0 (finite), and isFresh() = false.
        bool angleValid = false;
        lastAngleDeg = SteeringMath::rawToAngleDeg(lastRaw, appliedAdcLeft, appliedAdcRight,
                                                   STEERING_ANGLE_LEFT_DEG, STEERING_ANGLE_RIGHT_DEG, &angleValid);
        if (!calComplete()) { angleValid = false; lastAngleDeg = 0.0f; }   // never calibrated: no angle, no driving
        calibrationValid = angleValid;
        rawInRange = SteeringMath::rawPlausible(lastRaw, appliedAdcLeft, appliedAdcRight, STEERING_RAW_RANGE_MARGIN);
        if (!rawInRange && !loggedOutOfRange) {
            loggedOutOfRange = true;
            LogBuffer::printf("[ANGLE] Raw reading %d is far outside the calibrated range %d..%d - sensor wire broken or loose? The angle is rejected and the motor stops.\n",
                              lastRaw, appliedAdcLeft, appliedAdcRight);
        } else if (rawInRange) loggedOutOfRange = false;

        if (millis() - lastLogMillis >= 1000) {
            lastLogMillis = millis();
            LogBuffer::printfIf(DEBUG_STEERING_SENSOR, "[ANGLE] raw=%d voltage=%.3fV angle=%.1f* I2Cerrors=%u\n",
                                lastRaw, ads.computeVolts(lastRaw), lastAngleDeg, (unsigned)i2cErrors);
        }
    }

    static uint32_t getI2cErrors() { return i2cErrors; }
    // ADC input voltage [V] per the gain set at startup - for diagnostics in the panel.
    static float getVolts() { return sensorFound ? ads.computeVolts(lastRaw) : 0.0f; }
    // How many ms ago the last VALID read was (-1: none yet).
    static long getReadAgeMs() { return lastGoodReadMillis == 0 ? -1L : (long)(millis() - lastGoodReadMillis); }

    static bool isConnected() { return sensorFound; }

    // true if the last VALID read is fresh (default <= 250 ms;
    // normally refreshed every ~40 ms). Important for motor control: when
    // I2C fails, lastRaw/getAngleDeg() stay frozen at the old
    // value and the P controller would "see" the wheel at a place where it no longer is.
    static bool isFresh(unsigned long maxAgeMs = 250) {
        return sensorFound && calibrationValid && rawInRange && lastGoodReadMillis != 0 && (millis() - lastGoodReadMillis) <= maxAgeMs;
    }
    static int16_t getRaw() { return lastRaw; }
    static uint32_t getSampleSeq() { return sampleSeq; }
    static float getAngleDeg() { return lastAngleDeg; }
    static bool isCalibrationValid() { return calibrationValid; }
    static bool isCalibrationSaved() { return calComplete(); }   // both end points were saved at least once
    static bool isRawInRange() { return rawInRange; }

    static int16_t getAppliedAdcLeft() { return appliedAdcLeft; }
    static int16_t getAppliedAdcRight() { return appliedAdcRight; }

    // Stores the CURRENT raw reading as the LEFT/RIGHT end value -
    // applies it immediately and saves it permanently in NVS. Call only when the wheel is
    // physically at the given end position.
    // Returns false (and saves nothing) when the sensor has not yet delivered any valid
    // reading - previously a "0" from the uninitialized lastRaw was saved then. The LEFT-RIGHT
    // span does NOT block the save (otherwise it would be impossible to recalibrate in the wrong order),
    // it only sets calibrationValid - see isCalibrationValid() and the warning in the panel.
    static bool captureLeft() {
        if (!sensorFound || lastGoodReadMillis == 0) {
            LogBuffer::println("[ANGLE] LEFT not saved: the angle sensor has not delivered a reading yet.");
            return false;
        }
        appliedAdcLeft = lastRaw;
        prefs.putShort("adcLeft", appliedAdcLeft);
        calLeftSet = true;
        calibrationValid = calComplete() && SteeringMath::spanOk(appliedAdcLeft, appliedAdcRight);
        LogBuffer::printf("[ANGLE] LEFT saved: %d\n", appliedAdcLeft);
        return true;
    }

    static bool captureRight() {
        if (!sensorFound || lastGoodReadMillis == 0) {
            LogBuffer::println("[ANGLE] RIGHT not saved: the angle sensor has not delivered a reading yet.");
            return false;
        }
        appliedAdcRight = lastRaw;
        prefs.putShort("adcRight", appliedAdcRight);
        calRightSet = true;
        calibrationValid = calComplete() && SteeringMath::spanOk(appliedAdcLeft, appliedAdcRight);
        LogBuffer::printf("[ANGLE] RIGHT saved: %d\n", appliedAdcRight);
        return true;
    }
};

Adafruit_ADS1115 SteeringAngleSensor::ads;
bool SteeringAngleSensor::sensorFound = false;
int16_t SteeringAngleSensor::lastRaw = 0;
float SteeringAngleSensor::lastAngleDeg = 0.0f;
unsigned long SteeringAngleSensor::lastLogMillis = 0;
uint32_t SteeringAngleSensor::sampleSeq = 0;
unsigned long SteeringAngleSensor::lastReadMillis = 0;
bool SteeringAngleSensor::convIsCurrent = false;
unsigned long SteeringAngleSensor::lastCurrentStartMs = 0;
SteeringAngleSensor::AdcState SteeringAngleSensor::adcState = SteeringAngleSensor::AdcState::IDLE;
uint32_t SteeringAngleSensor::convStartUs = 0;
I2cRetry SteeringAngleSensor::retry;
uint32_t SteeringAngleSensor::i2cErrors = 0;
unsigned long SteeringAngleSensor::lastGoodReadMillis = 0;
int16_t SteeringAngleSensor::appliedAdcLeft = 0;
int16_t SteeringAngleSensor::appliedAdcRight = 32767;
bool SteeringAngleSensor::calibrationValid = true;
bool SteeringAngleSensor::calLeftSet = false;
bool SteeringAngleSensor::calRightSet = false;
bool SteeringAngleSensor::rawInRange = true;
bool SteeringAngleSensor::loggedOutOfRange = false;
Preferences SteeringAngleSensor::prefs;
