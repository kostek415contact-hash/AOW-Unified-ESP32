#pragma once
// ======================================================================
// sensors/imu_sensor.h
// EN: BNO085 IMU in UART-RVC mode: heading/roll/pitch at 100 Hz, hardware reset via RST, self-healing on a
//     stalled stream, board-rotation correction.
// PL: IMU BNO085 w trybie UART-RVC: kurs/roll/pitch 100 Hz, reset sprzętowy przez RST, samoleczenie przy
//     zatrzymanym strumieniu, korekta obrotu płytki.
// Used by: connectivity/network_manager.h, connectivity/pgn_messages.h, web/web_server.h
// Depends on: config.h, live_window.h, log_buffer.h
// ======================================================================
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Adafruit_BNO08x_RVC.h>
#include "../config/config.h"
#include "imu_mount.h"
#include "../web/log_buffer.h"
#include "live_window.h"

// BNO085 in UART-RVC mode (NOT I2C). STATUS: works on hardware (100 packets/s, confirmed 2026-10-09).
// The earlier sensor hangs were caused by a too weak supply from USB; on an external converter it works correctly.
// Enable ENABLE_IMU in the web panel (Settings).
//
// The main source of heading/roll/pitch (the compass was removed from the project -
// the IMU fully replaced it, see pgn_messages.h::nmSendPanda()).
//
// WHY UART-RVC, NOT I2C (an I2C version was briefly in this project -
// we came back from it to UART-RVC, see the commit history): the BNO085 on I2C has a
// CONFIRMED, documented problem with ESP32/ESP32-S3 - the sensor aggressively
// holds the clock line (clock stretching) during its own sensor-fusion
// calculations, and older versions of the I2C driver in Arduino-ESP32 (core older
// than 3.2.0 / ESP-IDF older than 5.4.0) cannot cope with this - bus hangs,
// random read errors (confirmed in many independent
// sources: the Adafruit forum, espressif/esp-idf issues, learn.adafruit.com/
// i2c-addresses/troublesome-chips). UART-RVC avoids this problem completely -
// the BNO085 computes the sensor fusion (accelerometer+gyroscope+magnetometer) ITSELF
// internally and streams the ready yaw/pitch/roll over a plain serial
// port, with no clock to "hold". For the heading alone (which is all AgOpenWeb
// needs) it is also the simplest approach and the most commonly used in the
// AgOpenGPS community - full I2C/SPI only makes sense if raw data
// were needed (quaternions, linear acceleration), which we do not
// need here.
//
// WIRING: BNO085 (the pin the manufacturer labels as the UART-RVC output,
// often the same as the module's "TX") -> ESP32 RX on IMU_UART_RX_PIN (see
// config.h). The speed is FIXED 115200 8N1 - this is a requirement of the RVC mode in the
// BNO085 itself, not changeable in code. We use a separate UART (number 2) from the GPS
// (which takes UART 1, see sensors/gps_universal.h) - no conflict.
//
// UNCERTAINTIES TO CHECK ONLY ONCE THE HARDWARE ARRIVES (they cannot be
// verified without a physical module):
//   1. Some BNO085 boards require setting the PS0/PS1 pins (a jumper or
//      solder pad, often labelled "P0") in a specific
//      configuration for the chip to start in UART-RVC mode instead of the
//      default I2C - PS0=3.3V, PS1=3.3V for UART-RVC. Check the datasheet/description
//      of YOUR particular module exactly (different manufacturers
//      do this differently - e.g. on Adafruit it is soldering the P0 jumper).
//   2. CONFIRMED by a test (log): yaw comes out in the range -180..180 (not
//      0-360), matching the AgOpenWeb convention (right=positive, left=negative) -
//      it needs no correction in getHeadingDeg().
//   3. IMU_UART_RX_PIN (currently GPIO6 - see config.h) is only a preliminary,
//      reasonable proposal (a free pin, it does not collide with anything currently
//      used) - confirm on the silkscreen/schematic of your particular
//      ESP32-S3 board that this pin is actually broken out and free.
class ImuSensor {
private:
    static Adafruit_BNO08x_RVC rvc;
    static HardwareSerial imuSerial;
    static bool sensorFound;
    static float headingDeg;
    static float pitchDeg;
    static float rollDeg;
    static float rawRollDeg;   // raw roll/pitch from the sensor (before the board-rotation correction)
    static float rawPitchDeg;
    static float accelX; // m/s^2, in the sensor axes (not corrected for mounting)
    static float accelY;
    static float accelZ;
    static unsigned long lastLogMillis;
    // TIME-BASED diagnostics (not based on the number of loop passes - the previous
    // "times in a row" counter counted passes without a new packet, so it grew
    // in proportion to the loop SPEED and was misleading).
    static unsigned long lastSuccessfulReadMillis; // 0 = no packet yet
    static unsigned long beganMillis;              // moment of port start (reference before the first packet)
    static unsigned long maxGapMillis;             // the longest gap between packets since start
    static uint32_t totalPackets;
    // Raw bytes from the UART - diagnostics "the sensor is silent" vs "there are bytes, but no frames".
    static int availAfterPrev;          // bytes waiting in the buffer after the last update()
    static uint32_t bytesTotal;
    static uint32_t windowBytes;
    static uint32_t bytesPerSec;        // a healthy stream ~1900 B/s (100 packets x 19 B)
    static unsigned long lastByteMillis; // 0 = no byte ever received
    // Hardware reset of the sensor through the RST pin (IMU_RESET_PIN in config.h).
    static uint16_t sensorResetCount;
    // Manual RST test (a button in the web panel). The request comes from the web thread (core 0)
    // as a FLAG, and the pulse itself and the measurement are done by update() on the loop's core - only one thread
    // touches the pins and the state.
    static volatile bool manualResetRequested;
    static bool manualResetPending;
    static bool manualWasSilent;          // the sensor was silent (>1 s) at the moment of the request
    static unsigned long manualResetStartMillis;
    static long manualRecoverMs;          // ms from the start of the pulse to the first frame (-1 = no result)
    static uint8_t manualResetState;      // 0 none, 1 running, 2 sensor came back, 3 did not come back within 5 s
    static float manualYawBefore;
    static float manualYawAfter;
    static float rawYawDeg;               // yaw from the sensor WITHOUT the mounting offset (yaw returns to ~0 after a reset)
    static bool resetPulseActive;
    static unsigned long resetPulseStartMillis;
    static uint8_t healAttempts;         // how many self-healing attempts since the last valid data
    static uint32_t windowPackets;
    static unsigned long windowStartMillis;
    static uint16_t packetsPerSec;                 // expected ~100 (UART-RVC)
    static uint16_t uartRestartCount;
    static unsigned long lastRestartMillis;

    // Roll and pitch are ABSOLUTE (relative to gravity), so they depend on how the
    // board is rotated relative to the vehicle axis. A rotation by psi would mix pitch into
    // roll (roll = ~pitch * sin(psi)) - this corrects that rotation of the 2D tilt
    // vector. Yaw needs no correction (AgOpenWeb computes the offset to the GPS itself).
    // The angle psi is determined by the web panel (IMU Calibration): psi = atan(dPitch / dRoll)
    // for a sideways tilt.
    static void applyMountRotation() {
        // Full 3D mounting (calibrated in the panel): heading, pitch and roll of the VEHICLE from the sensor's raw yaw/pitch/roll.
        // The settings are written by the web thread, so work on a copy and skip a frame whose copy is inconsistent (a torn read).
        const ImuMount::Mount k = IMU_MOUNT3D;
        if (k.valid) {
            if (!ImuMount::mountOk(k)) return;
            float h, p3, r3;
            ImuMount::apply(k, rawYawDeg, rawPitchDeg, rawRollDeg, h, p3, r3);
            h += IMU_MOUNTING_OFFSET_DEG;
            while (h < -180.0f) h += 360.0f;
            while (h > 180.0f) h -= 360.0f;
            headingDeg = h;
            pitchDeg = p3;
            rollDeg = IMU_ROLL_INVERT ? -r3 : r3;
            return;
        }
        float r = rawRollDeg, p = rawPitchDeg;
        if (IMU_MOUNT_ROTATION_DEG != 0.0f) {
            float psi = IMU_MOUNT_ROTATION_DEG * (float)M_PI / 180.0f;
            float c = cosf(psi), s = sinf(psi);
            rollDeg = r * c + p * s;
            pitchDeg = -r * s + p * c;
        } else {
            rollDeg = r;
            pitchDeg = p;
        }
        if (IMU_ROLL_INVERT) rollDeg = -rollDeg;
    }

    // NON-BLOCKING RST pulse (used while running - the main loop cannot wait):
    // sets RST low, and its end (RST high + clearing the UART) is done by
    // update() after 20 ms. Does nothing when IMU_RESET_PIN < 0.
    static void beginSensorResetPulse() {
        if (IMU_RESET_PIN < 0) return;
        pinMode((uint8_t)IMU_RESET_PIN, OUTPUT);
        digitalWrite((uint8_t)IMU_RESET_PIN, LOW);
        resetPulseActive = true;
        resetPulseStartMillis = millis();
    }

    static void restartUart() {
        imuSerial.end();
        imuSerial.setRxBufferSize(2048);
        imuSerial.begin(115200, SERIAL_8N1, IMU_UART_RX_PIN, -1);
        rvc.begin(&imuSerial);
        uartRestartCount++;
        lastRestartMillis = millis();
        availAfterPrev = 0; // the driver buffer was cleared by end()/begin()
    }

public:
    static void begin() {
        if (!ENABLE_IMU) return;

        // -1 in place of the ESP32 TX - the RVC mode is receive-only (the BNO085 talks,
        // we listen), we do not need to send anything to the sensor.
        // The default RX buffer (256 B) may be too tight with the 100 Hz stream
        // of UART-RVC packets and our main loop running at a similar
        // rate (~90 Hz) - the same lesson as with the GPS (see gps_universal.h,
        // setRxBufferSize) - it must be called BEFORE begin().
        // Hardware reset of the sensor on EVERY start (if the RST pin is connected):
        // the BNO085 then re-reads the mode-select pins (PS0/PS1), and any
        // "hung" state from the previous session is cleared. This is setup(),
        // so a short wait here is acceptable (the main loop is not running yet).
        if (IMU_RESET_PIN >= 0) {
            pinMode((uint8_t)IMU_RESET_PIN, OUTPUT);
            digitalWrite((uint8_t)IMU_RESET_PIN, LOW);
            delay(20);
            digitalWrite((uint8_t)IMU_RESET_PIN, HIGH);
            delay(100);
            sensorResetCount++;
            LogBuffer::printfIf(true, "[IMU] Hardware reset of the sensor at start (RST pin = GPIO%d).\n", IMU_RESET_PIN);
        }
        imuSerial.setRxBufferSize(2048);
        imuSerial.begin(115200, SERIAL_8N1, IMU_UART_RX_PIN, -1);
        sensorFound = rvc.begin(&imuSerial);
        beganMillis = millis();
        windowStartMillis = millis();
        availAfterPrev = 0;

        // CONFIRMED (official documentation + Adafruit forum reports):
        // begin() in this library ALWAYS returns true, REGARDLESS of whether
        // anything is physically connected - this is not our bug, it is a
        // documented flaw of the library itself. The log below is therefore NOT
        // proof that it works - the only real proof is whether
        // update() below actually logs real (changing) data.
        LogBuffer::println("[IMU] BNO085 (UART-RVC) initialised (NOTE: this only confirms the serial port started, NOT a real connection - check that update() actually receives data).");
    }

    // Call every loop() pass.
    static void update() {
        if (!ENABLE_IMU || !sensorFound) return;

        // Manual RST test ordered from the panel (a flag set in the web thread).
        if (manualResetRequested && !resetPulseActive) {
            manualResetRequested = false;
            unsigned long refMs = (lastSuccessfulReadMillis != 0) ? lastSuccessfulReadMillis : beganMillis;
            manualWasSilent = (millis() - refMs) > 1000;
            manualYawBefore = rawYawDeg;
            manualYawAfter = 0.0f;
            manualRecoverMs = -1;
            manualResetState = 1;
            manualResetPending = true;
            manualResetStartMillis = millis();
            lastRestartMillis = millis(); // do not start automatic self-healing at the same moment
            beginSensorResetPulse();
            return;
        }

        // End of a running RST pulse (non-blocking - see beginSensorResetPulse()).
        // During the pulse (20 ms) we read nothing from the UART.
        if (resetPulseActive) {
            if (millis() - resetPulseStartMillis < 20) return;
            digitalWrite((uint8_t)IMU_RESET_PIN, HIGH);
            resetPulseActive = false;
            sensorResetCount++;
            restartUart(); // discards garbage from before the reset and starts with a clean buffer
            return;
        }

        // We read ALL packets waiting in the buffer, not just
        // one - the BNO085 in UART-RVC transmits at up to 100 Hz
        // (10 ms/packet), and our main loop makes passes about every 11 ms -
        // almost the same rate, with no margin at all. One read per
        // pass would assume that at most ONE packet has arrived since
        // the last check - if anything (even a short pause
        // invisible in our own [STEP] measurements, e.g. WiFi interrupt
        // handling) delays us by more than one cycle, the buffer
        // starts to fill and eventually overflows, losing data - this
        // is exactly what gave the periodic series of [IMU] NO DATA despite a stable
        // [LOOP AVG]. The loop below "catches up" the whole backlog in one
        // go, instead of leaving it for the following passes.
        // A counter of RAW bytes from the UART (diagnostics: distinguishes "the sensor is silent"
        // from "there are bytes on the line, but no valid frames"). We count the INCREASE
        // of bytes waiting in the buffer since the end of the previous update() - this
        // does not interfere with the library's frame reading and does not depend on how
        // it consumes bytes (available() only drops on a read).
        int availNow = imuSerial.available();
        if (availNow > availAfterPrev) {
            uint32_t d = (uint32_t)(availNow - availAfterPrev);
            bytesTotal += d;
            windowBytes += d;
            lastByteMillis = millis();
        }

        BNO08x_RVC_Data heading;
        bool gotAny = false;
        uint16_t packetsThisCall = 0;
        while (rvc.read(&heading)) {
            gotAny = true;
            packetsThisCall++;
            // IMU_MOUNTING_OFFSET_DEG (config.h) - a correction for the angle at which the
            // board is physically mounted in the tractor relative to the forward
            // driving direction. Set it ONLY once you physically
            // mount the module - compare the IMU reading with a trustworthy
            // reference (e.g. the GPS course when driving straight along a straight line)
            // and enter the difference.
            rawYawDeg = heading.yaw;
            float deg = heading.yaw + IMU_MOUNTING_OFFSET_DEG;
            while (deg < -180.0f) deg += 360.0f;
            while (deg > 180.0f) deg -= 360.0f;
            if (!IMU_MOUNT3D.valid) headingDeg = deg;   // with the 3D mounting applyMountRotation() computes the vehicle heading
            rawPitchDeg = heading.pitch;
            rawRollDeg = heading.roll;
            // Accelerations (m/s^2) - available in the UART-RVC structure next to
            // yaw/pitch/roll, until now not read at all. Raw sensor axes
            // (not corrected for the mounting angle) - useful e.g. for
            // detecting shocks/vibration, not for navigation.
            accelX = heading.x_accel;
            accelY = heading.y_accel;
            accelZ = heading.z_accel;
        }

        availAfterPrev = imuSerial.available();

        if (gotAny) applyMountRotation();

        unsigned long now = millis();

        if (gotAny) {
            if (lastSuccessfulReadMillis != 0) {
                unsigned long gap = now - lastSuccessfulReadMillis;
                if (gap > maxGapMillis) maxGapMillis = gap;
            }
            lastSuccessfulReadMillis = now;
            healAttempts = 0;
            if (manualResetPending) {           // the first frame after a manual reset
                manualRecoverMs = (long)(now - manualResetStartMillis);
                manualYawAfter = rawYawDeg;
                manualResetState = 2;
                manualResetPending = false;
            }
            totalPackets += packetsThisCall;
            windowPackets += packetsThisCall;
        }

        // RST test: the sensor did not come back within 5 s.
        if (manualResetPending && (now - manualResetStartMillis) > 5000) {
            manualResetState = 3;
            manualResetPending = false;
        }

        // Packets/s counted in a 1 s window - a real measure of the stream's health.
        if (now - windowStartMillis >= 1000) {
            packetsPerSec = (uint16_t)((windowPackets * 1000UL) / (now - windowStartMillis));
            bytesPerSec = (windowBytes * 1000UL) / (now - windowStartMillis);
            windowPackets = 0;
            windowBytes = 0;
            windowStartMillis = now;
        }

        // Silence longer than 3 s = a REAL failure (at 100 Hz a healthy stream
        // never goes silent even for 100 ms). We try self-healing: restarting the
        // UART port, at most every 5 s, briefly and non-blocking. If the IMU comes
        // back after that - the problem was on the ESP32 side (UART state); if not -
        // on the sensor hardware/power side.
        unsigned long silentMs = now - (lastSuccessfulReadMillis != 0 ? lastSuccessfulReadMillis : beganMillis);
        // The first 6 attempts every 5 s, then every 30 s (a dead sensor does not need
        // dozens of resets, and the log and UART are left in peace).
        unsigned long healEveryMs = (healAttempts < 6) ? 5000UL : 30000UL;
        // During a manual RST test self-healing waits - otherwise it could
        // claim the sensor's return for itself and falsify the test result.
        if (!manualResetPending && silentMs > 3000 && (now - lastRestartMillis) > healEveryMs) {
            healAttempts++;
            bool logIt = (healAttempts <= 5 || healAttempts % 6 == 0);
            if (IMU_RESET_PIN >= 0) {
                // Restarting the UART port alone does NOT help a hung sensor (confirmed:
                // 26 restarts to no effect) - we reset the SENSOR through RST.
                beginSensorResetPulse();
                lastRestartMillis = now;
                if (logIt) LogBuffer::printf("[IMU] Silence for %lu ms (UART bytes/s: %lu) - hardware reset of the sensor via RST (attempt no. %u).\n",
                    silentMs, (unsigned long)bytesPerSec, (unsigned)healAttempts);
            } else {
                restartUart();
                if (logIt) LogBuffer::printf("[IMU] Silence for %lu ms (UART bytes/s: %lu) - restarting the UART port (attempt no. %u; no RST pin - see IMU_RESET_PIN in config.h).\n",
                    silentMs, (unsigned long)bytesPerSec, (unsigned)healAttempts);
            }
        }

        // With a long silence (>10 s) the message goes every 10 s instead of every 1 s - otherwise it
        // floods the log buffer and displaces other entries from it (WiFi, GPS...).
        unsigned long logEveryMs = (silentMs > 10000) ? 10000 : 1000;
        if (now - lastLogMillis >= logEveryMs) {
            lastLogMillis = now;
            if (silentMs > 500) {
                LogBuffer::printf("[IMU] NO DATA for %lu ms, UART bytes/s: %lu (0 = the RX line is silent), UART restarts: %u - check power / wiring SDA->RX / the PS0 jumper.\n",
                                  silentMs, (unsigned long)bytesPerSec, (unsigned)uartRestartCount);
            } else {
                LogBuffer::printfIf(DEBUG_IMU, "[IMU] yaw=%.1f pitch=%.1f roll=%.1f  (%u packets/s, %lu B/s)\n",
                                    headingDeg, pitchDeg, rollDeg, (unsigned)packetsPerSec, (unsigned long)bytesPerSec);
            }
        }
    }

    // IMPORTANT: this is NOT just "was it detected at startup" - UART-RVC
    // (the Adafruit library) has a documented flaw that begin() ALWAYS
    // returns true, regardless of the actual connection (see begin()
    // below). sensorFound by itself never reflects the real
    // state. So isConnected() ALSO checks whether a read actually
    // succeeded RECENTLY (2 s) - without this, with a longer (not just
    // brief) loss of data, nmSendPanda() in pgn_messages.h
    // would send AgOpenWeb an ever more stale heading/roll/pitch
    // FOREVER, never falling back to the existing, but without this fix dead,
    // GPS-only fallback.
    static bool isConnected() {
        return sensorFound && lastSuccessfulReadMillis != 0 &&
        (millis() - lastSuccessfulReadMillis) < 2000;
    }
    // The IMU data is LIVE enough to be sent to AgOpenWeb as heading/roll/pitch: a successful read within the last 500 ms (the sensor
    // delivers 100 Hz). isConnected() keeps its 2 s window for the panel and the LED; $PANDA uses this stricter one, so a dead
    // sensor does not keep feeding a frozen heading for 2 s.
    static bool isLive() {
        return sensorFound && lastSuccessfulReadMillis != 0 &&
               (millis() - lastSuccessfulReadMillis) < LiveWindow::IMU_MS;
    }
    static float getHeadingDeg() { return headingDeg; }
    static float getPitchDeg() { return pitchDeg; }
    static float getRollDeg() { return rollDeg; }          // AFTER the rotation correction - this goes to AgOpenWeb
    static float getRawRollDeg() { return rawRollDeg; }    // raw from the sensor (for calibration)
    static float getRawPitchDeg() { return rawPitchDeg; }
    static bool isMount3dActive() { return IMU_MOUNT3D.valid; }
    static float getRawYawDeg() { return rawYawDeg; }      // raw yaw from the sensor (before the mounting offset and the 3D mounting)
    static float getAccelX() { return accelX; }
    static float getAccelY() { return accelY; }
    static float getAccelZ() { return accelZ; }
    static unsigned long getSilentMs() {
        return millis() - (lastSuccessfulReadMillis != 0 ? lastSuccessfulReadMillis : beganMillis);
    }
    static unsigned long getMaxGapMs() { return maxGapMillis; }
    static uint16_t getPacketsPerSec() { return packetsPerSec; }
    static uint32_t getBytesPerSec() { return bytesPerSec; }
    static bool isResetPinEnabled() { return IMU_RESET_PIN >= 0; }
    static uint16_t getSensorResetCount() { return sensorResetCount; }
    // Manual sensor reset (RST test from the panel). Called from the web thread - it only sets a flag;
    // the pulse is done by update() on the loop's core. Returns false when impossible (no RST pin,
    // IMU disabled, or a reset already in progress).
    static bool requestManualReset() {
        if (IMU_RESET_PIN < 0 || !ENABLE_IMU || !sensorFound) return false;
        if (manualResetRequested || manualResetPending || resetPulseActive) return false;
        manualResetRequested = true;
        return true;
    }
    static uint8_t getManualResetState() { return manualResetState; }
    static long getManualResetRecoverMs() { return manualRecoverMs; }
    static float getManualResetYawBefore() { return manualYawBefore; }
    static float getManualResetYawAfter() { return manualYawAfter; }
    static bool getManualResetWasSilent() { return manualWasSilent; }
    static uint32_t getBytesTotal() { return bytesTotal; }
    // How many ms ago the LAST byte arrived from the UART (-1 = never) - if it grows and there are
    // no packets, the RX line is completely dead (not just without frames).
    static long getMsSinceLastByte() { return lastByteMillis == 0 ? -1L : (long)(millis() - lastByteMillis); }
    static uint16_t getUartRestartCount() { return uartRestartCount; }
};

Adafruit_BNO08x_RVC ImuSensor::rvc;
HardwareSerial ImuSensor::imuSerial(2); // UART 2 - the GPS takes UART 1, see sensors/gps_universal.h
bool ImuSensor::sensorFound = false;
float ImuSensor::headingDeg = 0.0f;
float ImuSensor::pitchDeg = 0.0f;
float ImuSensor::rollDeg = 0.0f;
float ImuSensor::rawRollDeg = 0.0f;
float ImuSensor::rawPitchDeg = 0.0f;
float ImuSensor::accelX = 0.0f;
float ImuSensor::accelY = 0.0f;
float ImuSensor::accelZ = 0.0f;
unsigned long ImuSensor::lastLogMillis = 0;
unsigned long ImuSensor::lastSuccessfulReadMillis = 0;
unsigned long ImuSensor::beganMillis = 0;
unsigned long ImuSensor::maxGapMillis = 0;
uint32_t ImuSensor::totalPackets = 0;
uint32_t ImuSensor::windowPackets = 0;
unsigned long ImuSensor::windowStartMillis = 0;
uint16_t ImuSensor::packetsPerSec = 0;
uint16_t ImuSensor::sensorResetCount = 0;
volatile bool ImuSensor::manualResetRequested = false;
bool ImuSensor::manualResetPending = false;
bool ImuSensor::manualWasSilent = false;
unsigned long ImuSensor::manualResetStartMillis = 0;
long ImuSensor::manualRecoverMs = -1;
uint8_t ImuSensor::manualResetState = 0;
float ImuSensor::manualYawBefore = 0.0f;
float ImuSensor::manualYawAfter = 0.0f;
float ImuSensor::rawYawDeg = 0.0f;
bool ImuSensor::resetPulseActive = false;
unsigned long ImuSensor::resetPulseStartMillis = 0;
uint8_t ImuSensor::healAttempts = 0;
int ImuSensor::availAfterPrev = 0;
uint32_t ImuSensor::bytesTotal = 0;
uint32_t ImuSensor::windowBytes = 0;
uint32_t ImuSensor::bytesPerSec = 0;
unsigned long ImuSensor::lastByteMillis = 0;
uint16_t ImuSensor::uartRestartCount = 0;
unsigned long ImuSensor::lastRestartMillis = 0;
