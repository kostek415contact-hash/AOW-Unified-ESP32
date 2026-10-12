#pragma once
// ======================================================================
// connectivity/pgn_receivers.h
// EN: Parsers for PGNs sent by AgOpenWeb to the module (steer command 254, machine 239, steer settings 252,
//     steer config 251, machine config 238, machine pins 236) with freshness tracking.
// PL: Parsery PGN wysyłanych przez AgOpenWeb do modułu (komenda steer 254, machine 239, ustawienia steer 252,
//     konfiguracja steer 251, konfiguracja maszyny 238, piny maszyny 236) ze śledzeniem świeżości.
// Used by: actuators/actuators.h, connectivity/network_manager.h, io/section_expander.h, io/status_led.h,
//     ota/arduino_ota.h, sensors/current_sensor.h, sensors/steering_angle_sensor.h, web/runtime_settings.h ...
// Depends on: config.h, log_buffer.h, log_throttle.h, pgn_frame.h
// ======================================================================
#include "../io/machine_outputs.h"
#include "steer_settings_store.h"
#include "../diagnostics/log_throttle.h"
#include "pgn_frame.h"
#include <Arduino.h>
#include <IPAddress.h>
#include "../config/config.h"
#include "../web/log_buffer.h"

// Three parsers for PGNs sent by AgOpenWeb TO the module - combined in one
// file, because they are almost identical in structure (parse() + getters + freshness).

// PGN 254 ("To AutoSteer") - the TARGET steering angle (setpoint), speed, the steering
// motor status and the section state. Format confirmed in the AgOpenWeb
// sources (PgnBuilder.cs, BuildAutoSteerPgn):
//   [0x80,0x81,0x7F,0xFE,8, speed(2)LE*10, status, steerAngle(2)LE*100,
//    xte, sections1-8, sections9-16, CRC]
//
// FOR NOW RECEIVE/PREVIEW ONLY - the motor driver is not connected yet.
// The values are available through getters, ready to be hooked into the control
// loop once the hardware arrives (Cytron MD13S + angle sensor).
class SteerCommandReceiver {
private:
    static float speedKmh;
    static bool steerSwitchActiveFromAgio;
    static bool autoSteerEngaged;
    // Which bits of the PGN 254 status byte mean "steering enabled" (a setting from the panel, Settings -> AgOpenWeb senders):
    // 0x01 = AgOpenWeb develop (default: status byte 1 = enabled), 0x04 = another code, 0x05 = both.
    static uint8_t engagedMask;
    static uint8_t lastStatusByte;
    static float targetSteerAngleDeg;
    static int8_t xteRaw;
    static uint8_t sections1to8;
    static uint8_t sections9to16;
    static unsigned long lastCommandMillis;
    static bool loggedEngaged;
    static IPAddress lastSourceIp;
    // Diagnostics for the panel: the last frame in raw form and the gaps between commands.
    static const uint8_t RAW_MAX = 16;
    static uint8_t lastRaw[RAW_MAX];
    static uint8_t lastRawLen;
    static uint32_t rxCount;
    static uint32_t lastDeltaMs;
    static uint32_t maxDeltaMs;

public:
    // Call from the incoming PGN parser when incomingPGN == PGN_AUTOSTEER_COMMAND.
    // data is the whole frame (from 0x80), len is the Len byte from the header (we expect 8).
    static void parse(const uint8_t* data, uint8_t len, IPAddress sourceIp) {
        if (len < 8) return; // incomplete frame - ignore

        unsigned long now = millis();
        unsigned long deltaMs = (lastCommandMillis == 0) ? 0 : (now - lastCommandMillis);

        // DIAGNOSTICS: a full dump of EVERY received packet (not only
        // on a state change) - sender address, time gap, raw bytes.
        // Enable DEBUG_STEER_COMMAND_RAW in config.h to see it -
        // it helps determine whether both variants (enable/disable) really
        // come from the same source.
        // AgOpenWeb sends these frames even ~100-200 times per second, so a full dump of each would scroll through 8 KB of the
        // log in a few seconds. The log gets a bulk entry every 5 s (the first frame immediately): the frame count,
        // min/avg/max gaps and the LAST frame in hex. All counters are also in the panel (Diagnostics -> Steer command).
        if (DEBUG_STEER_COMMAND_RAW) {
            static IntervalThrottle rawLog(5000);
            IntervalReport rep;
            if (rawLog.note(now, (uint32_t)deltaMs, rep)) {
                char hex[3 * 14 + 1];
                uint8_t n = (len + 5 > 14) ? 14 : (uint8_t)(len + 5);
                for (uint8_t i = 0; i < n; i++) {
                    snprintf(hex + i * 3, 4, "%02X ", data[i]);
                }
                LogBuffer::printf("[STEER CMD RAW] from=%s: %lu frames in %lu ms (interval min/avg/max %lu/%lu/%lu ms) last=%s\n",
                                  sourceIp.toString().c_str(), (unsigned long)rep.count, rep.spanMs,
                                  (unsigned long)rep.minDt, (unsigned long)rep.avgDt, (unsigned long)rep.maxDt, hex);
            }
        }

        // Storing the raw frame and the gaps (panel: Diagnostics -> "Steer command - raw bytes").
        uint8_t nraw = (len + 5 > RAW_MAX) ? RAW_MAX : (uint8_t)(len + 5);
        memcpy(lastRaw, data, nraw);
        lastRawLen = nraw;
        rxCount++;
        if (deltaMs > 0) {
            lastDeltaMs = (uint32_t)deltaMs;
            if (deltaMs > maxDeltaMs) maxDeltaMs = (uint32_t)deltaMs;
        }

        uint16_t speedRaw = (uint16_t)(data[5] | (data[6] << 8));
        speedKmh = speedRaw / 10.0f;

        uint8_t status = data[7];
        steerSwitchActiveFromAgio = (status & 0x01) != 0;
        lastStatusByte = status;
        autoSteerEngaged = PgnFrame::engagedFromStatus(status, engagedMask);

        int16_t angleRaw = (int16_t)(data[8] | (data[9] << 8));
        targetSteerAngleDeg = angleRaw / 100.0f;

        xteRaw = (int8_t)data[10];
        sections1to8 = data[11];
        sections9to16 = data[12];

        lastCommandMillis = now;
        lastSourceIp = sourceIp;

        if (autoSteerEngaged != loggedEngaged) {
            loggedEngaged = autoSteerEngaged;
            LogBuffer::printfIf(DEBUG_STEER_COMMAND, "[STEER CMD] AgOpenWeb: steering %s (target angle=%.1f* speed=%.1fkm/h)\n",
                                autoSteerEngaged ? "ENGAGED" : "off", targetSteerAngleDeg, speedKmh);
        }
    }

    // true if we received a command within the last maxAgeMs (AgOpenWeb sends
    // it cyclically - no fresh command = lost connection with the app).
    static bool isFresh(unsigned long maxAgeMs = 500) {
        return lastCommandMillis != 0 && (millis() - lastCommandMillis) <= maxAgeMs;
    }
    static uint32_t getRxCount() { return rxCount; }
    static uint32_t getLastDeltaMs() { return lastDeltaMs; }
    static uint32_t getMaxDeltaMs() { return maxDeltaMs; }
    // Copies the last raw frame (from 0x80), returns the number of bytes (0 = none yet).
    static uint8_t copyLastRaw(uint8_t* out, uint8_t maxLen) {
        uint8_t n = lastRawLen < maxLen ? lastRawLen : maxLen;
        memcpy(out, lastRaw, n);
        return n;
    }

    static float getTargetAngleDeg() { return targetSteerAngleDeg; }
    static float getSpeedKmh() { return speedKmh; }
    // The raw flag from the last PGN254 frame. NOTE: set only when a frame is received and never expires by itself - when AgOpenWeb
    // stops transmitting (program closed, disconnection), it stays "ENGAGED" until restart. Use isAutoSteerActive() for decisions.
    static bool isAutoSteerEngaged() { return autoSteerEngaged; }
    static void setEngagedMask(uint8_t m) { engagedMask = PgnFrame::sanitizeEngageMask(m); }
    static uint8_t getEngagedMask() { return engagedMask; }
    static uint8_t getLastStatusByte() { return lastStatusByte; }
    // Steering is ACTUALLY active: the flag set AND fresh commands. This is what the motor, panel, latches and protections use.
    static bool isAutoSteerActive() { return autoSteerEngaged && isFresh(); }
    // AgOpenWeb is KNOWN to have steering switched off: a fresh frame says "not engaged". Unlike !isAutoSteerActive() this is false
    // when the link is down (then nothing is known). Safety latches are released only on this, never by a link loss: after a
    // WiFi drop AgOpenWeb may still be engaged and the motor would otherwise restart on the same fault when the link returns.
    static bool isDisengagedConfirmed() { return !autoSteerEngaged && isFresh(); }
    static uint8_t getSections1to8() { return sections1to8; }
    static uint8_t getSections9to16() { return sections9to16; }
};

// PGN 252 ("Steer Settings") - the angle sensor calibration (countsPerDeg,
// steerOffset) comes FROM HERE, rather than from our own local calibration.
// You set it in AgOpenWeb: Settings -> Steer -> Wheel Angle Sensor (there
// is a wizard for the zero and counts/degree calibration, and the Ackerman Fix).
//
// Format per the official documentation (AgOpenGPS-Official/Boards/PGN.md):
//   [0x80,0x81,0x7F,0xFC,8, gainP, highPWM, lowPWM, minPWM, countsPerDeg,
//    steerOffsetLo, steerOffsetHi, ackermanFix, CRC]
//
// STATE: currently NOT USED to compute the angle (see sensors/steering_angle_
// sensor.h) - AgOpenWeb's "Zero WAS", SmartWAS and counts-per-degree wizard all assume the module applies the offset and
// counts-per-degree from this PGN to the raw ADC and reports the corrected angle (WasCalibration.cs: new offset = old offset -
// angle x CPD). We calibrate locally instead (panel -> Calibration), so those AgOpenWeb functions have no effect here.
// We still receive and log the PGN for diagnostics.
//
// UPDATE (confirmed in the interface of the reference implementation
// mtz8302/AOG_Autosteer_ESP32, the same AgOpenGPS/AgOpenWeb ecosystem):
// ackermanFix is a SCALING FACTOR for one steering side relative to the
// other, not a simple multiplication of only the negative angles, as we
// assumed here earlier:
//   - 100 = neutral (both sides go the same)
//   - 35-99 = the RIGHT side has LARGER values than the left (the right scales
//     down relative to the left)
//   - 101-300 = the LEFT side has LARGER values than the right
// Still UNUSED in our code (local calibration, see
// sensors/steering_angle_sensor.h) - it stays as documentation for the future,
// should AgOpenWeb fix "Zero WAS" and we go back to PGN252.
class SteerSettingsReceiver {
private:
    static uint8_t gainP;
    static uint8_t highPWM;
    static uint8_t lowPWM;
    static uint8_t minPWM;
    static uint8_t countsPerDeg;
    static int16_t steerOffset;
    static uint8_t ackermanFix;
    static unsigned long lastReceivedMillis;
    static bool restored;                     // values were loaded from the module's memory (no frame received since start-up)
    static SteerSettingsSaveGate saveGate;    // when to write the received values to non-volatile memory

public:
    // The values as one block (for storage).
    static SteerSettingsData snapshot() {
        SteerSettingsData d; d.gainP = gainP; d.highPWM = highPWM; d.lowPWM = lowPWM; d.minPWM = minPWM;
        d.countsPerDeg = countsPerDeg; d.steerOffset = steerOffset; d.ackermanFix = ackermanFix; return d;
    }
    // At start-up: take the values AgOpenWeb sent last time (kept in NVS), so the motor works right after a restart
    // without waiting for AgOpenWeb to resend them. Anything received later overrides them.
    static void restore(const SteerSettingsData& d) {
        gainP = d.gainP; highPWM = d.highPWM; lowPWM = d.lowPWM; minPWM = d.minPWM;
        countsPerDeg = d.countsPerDeg ? d.countsPerDeg : 1; steerOffset = d.steerOffset; ackermanFix = d.ackermanFix;
        restored = true; saveGate.setStored(d);
    }
    static bool isFromStorage() { return restored && lastReceivedMillis == 0; }
    static SteerSettingsSaveGate& gate() { return saveGate; }

    // Call from the incoming PGN parser when incomingPGN == PGN_STEER_SETTINGS.
    static void parse(const uint8_t* data, uint8_t len) {
        if (len < 8) return; // incomplete frame - ignore

        gainP = data[5];
        highPWM = data[6];
        lowPWM = data[7];
        minPWM = data[8];
        countsPerDeg = data[9];
        steerOffset = (int16_t)((uint16_t)data[10] | ((uint16_t)data[11] << 8));
        ackermanFix = data[12];

        bool first = (lastReceivedMillis == 0);
        lastReceivedMillis = millis();
        saveGate.onReceived(snapshot(), lastReceivedMillis);

        if (first) {
            LogBuffer::printf("[STEER SETTINGS] Received from AgOpenWeb: countsPerDeg=%u steerOffset=%d ackermanFix=%u\n",
                              countsPerDeg, steerOffset, ackermanFix);
        }
    }

    // true if we have ever received the settings (AgOpenWeb sends them once
    // after connecting/after a settings change, not as a constant heartbeat - so
    // there is no "freshness" limit here as with sections/AutoSteer, only "did we
    // receive them at all").
    static bool hasReceived() { return lastReceivedMillis != 0 || restored; }

    static uint8_t getCountsPerDeg() { return countsPerDeg; }
    static int16_t getSteerOffset() { return steerOffset; }
    static uint8_t getAckermanFix() { return ackermanFix; }
    static uint8_t getGainP() { return gainP; }
    static uint8_t getHighPWM() { return highPWM; }
    static uint8_t getLowPWM() { return lowPWM; }
    static uint8_t getMinPWM() { return minPWM; }
};

// PGN 239 ("Machine Data") - sections, hydraulic lift, tramlines,
// geo-stop, u-turn state. Format confirmed in the AgOpenWeb sources
// (PgnBuilder.cs, BuildMachinePgn):
//   [0x80,0x81,0x7F,0xEF,8, uturn, speed*10, hydLift, tram, geoStop,
//    reserved, sections1-8, sections9-16, CRC]
class MachineCommandReceiver {
private:
    static uint8_t uturnState;
    static float speedKmh;
    static uint8_t hydLift;
    static uint8_t tramState;
    static uint8_t geoStop;
    static uint8_t sections1to8;
    static uint8_t sections9to16;
    static unsigned long lastCommandMillis;
    static uint16_t loggedSections;

public:
    // Call from the incoming PGN parser when incomingPGN == PGN_MACHINE_COMMAND.
    // data is the whole frame (from 0x80), len is the Len byte from the header (we expect 8).
    static void parse(const uint8_t* data, uint8_t len) {
        if (len < 8) return; // incomplete frame - ignore

        unsigned long now = millis();
        unsigned long deltaMs = (lastCommandMillis == 0) ? 0 : (now - lastCommandMillis);
        if (DEBUG_MACHINE_COMMAND) {   // in bulk every 5 s - see the note at [STEER CMD RAW]
            static IntervalThrottle machLog(5000);
            IntervalReport rep;
            if (machLog.note(now, (uint32_t)deltaMs, rep))
                LogBuffer::printf("[MACHINE CMD] %lu frames in %lu ms (interval min/avg/max %lu/%lu/%lu ms)\n",
                                  (unsigned long)rep.count, rep.spanMs,
                                  (unsigned long)rep.minDt, (unsigned long)rep.avgDt, (unsigned long)rep.maxDt);
        }

        uturnState = data[5];
        speedKmh = data[6] / 10.0f;
        hydLift = data[7];
        tramState = data[8];
        geoStop = data[9];
        sections1to8 = data[11];
        sections9to16 = data[12];

        lastCommandMillis = now;

        uint16_t currentSections = (uint16_t)sections1to8 | ((uint16_t)sections9to16 << 8);
        if (currentSections != loggedSections) {
            loggedSections = currentSections;
            LogBuffer::printfIf(DEBUG_MACHINE_COMMAND, "[MACHINE CMD] Sections: %04X hydLift=%u tram=%u geoStop=%u\n",
                                currentSections, hydLift, tramState, geoStop);
        }
    }

    // true if we received a command within the last maxAgeMs. NOTE: the default
    // limit is much higher than for SteerCommandReceiver (500 ms) -
    // measured in practice, the gaps between successive PGN239s range from
    // ~0 ms to over 3 s (AgOpenWeb sends this sporadically/on demand, not
    // as a constant heartbeat like AutoSteer) - a narrower limit caused constant,
    // false "expiry" of the sections despite a correct, live connection.
    static bool isFresh(unsigned long maxAgeMs = 8000) {
        return lastCommandMillis != 0 && (millis() - lastCommandMillis) <= maxAgeMs;
    }

    // sectionNumber1to16: the section number counted from 1. Returns false (section
    // off) if the command is stale - a safe default state.
    static bool isSectionOn(uint8_t sectionNumber1to16) {
        if (!isFresh()) return false;
        if (sectionNumber1to16 < 1 || sectionNumber1to16 > 16) return false;
        uint16_t mask = (uint16_t)sections1to8 | ((uint16_t)sections9to16 << 8);
        return (mask & (1u << (sectionNumber1to16 - 1))) != 0;
    }

    static uint16_t getSectionsMask() { return (uint16_t)sections1to8 | ((uint16_t)sections9to16 << 8); }
    static uint8_t getSections1to8() { return sections1to8; }
    static uint8_t getSections9to16() { return sections9to16; }
    static uint8_t getHydLift() { return hydLift; }
    static uint8_t getTramState() { return tramState; }
    static uint8_t getGeoStop() { return geoStop; }
    static float getSpeedKmh() { return speedKmh; }
    static uint8_t getUturnState() { return uturnState; }
};

// PGN 238 ("Machine Config") - hydraulic lift timing and options from AgOpenWeb:
//   [0x80,0x81,0x7F,0xEE,8, raiseTime, lowerTime, enableToolLift, set0, user1..user4, CRC]
// raiseTime/lowerTime in seconds; set0 bit 0 = "Invert relay" (the relay board is active LOW). Layout as in the
// AgOpenGPS machine module; sent by AgOpenWeb at its start and when the machine configuration is saved (NOT again when a module reconnects: the values are kept in the module memory).
// The Invert-relay bit is kept in the module memory (see web/runtime_settings.h): the outputs must idle in the right
// state right after a restart, before AgOpenWeb says anything.
class MachineConfigReceiver {
private:
    static uint8_t raiseTime, lowerTime, enableToolLift, set0;
    static uint8_t userVal[4];
    static bool invertRelay, invertSaved;
    static unsigned long lastMillis;
    static uint32_t rxCount;
public:
    static void parse(const uint8_t* data, uint8_t len) {
        if (len < 8) return;                      // incomplete frame - ignore
        const bool changed = rxCount == 0 || raiseTime != data[5] || lowerTime != data[6] || enableToolLift != data[7] || set0 != data[8];
        raiseTime = data[5]; lowerTime = data[6]; enableToolLift = data[7]; set0 = data[8];
        for (uint8_t i = 0; i < 4; i++) userVal[i] = data[9 + i];   // len >= 8 covers data bytes 5..12
        invertRelay = (set0 & 0x01) != 0;
        lastMillis = millis(); rxCount++;
        if (changed) LogBuffer::printfIf(DEBUG_MACHINE_COMMAND, "[MACHINE CFG] raiseTime=%u s lowerTime=%u s enableToolLift=%u set0=%u\n",
                                         raiseTime, lowerTime, enableToolLift, set0);
    }
    static bool hasReceived() { return rxCount > 0; }
    static uint8_t getRaiseTime() { return raiseTime; }
    static uint8_t getLowerTime() { return lowerTime; }
    static uint8_t getEnableToolLift() { return enableToolLift; }
    static uint8_t getUser(uint8_t i) { return i < 4 ? userVal[i] : 0; }
    static uint32_t getRxCount() { return rxCount; }
    static bool getInvertRelay() { return invertRelay; }
    // Start-up: the polarity AgOpenWeb sent last time (from the module memory).
    static void restoreInvertRelay(bool v) { invertRelay = v; invertSaved = v; }
    // true once when the polarity differs from what is stored (the caller writes it and calls markInvertSaved()).
    static bool invertNeedsSave() { return invertRelay != invertSaved; }
    static void markInvertSaved() { invertSaved = invertRelay; }
};
uint8_t MachineConfigReceiver::raiseTime = 0;
uint8_t MachineConfigReceiver::lowerTime = 0;
uint8_t MachineConfigReceiver::enableToolLift = 0;
uint8_t MachineConfigReceiver::set0 = 0;
uint8_t MachineConfigReceiver::userVal[4] = {0, 0, 0, 0};
bool MachineConfigReceiver::invertRelay = false;
bool MachineConfigReceiver::invertSaved = false;
unsigned long MachineConfigReceiver::lastMillis = 0;
uint32_t MachineConfigReceiver::rxCount = 0;

// PGN 251 ("Steer Config") - the hardware options from the AgOpenWeb steer wizard:
//   [0x80,0x81,0x7F,0xFB,5, set0, trip, minSpeed*10, set1, angVel, CRC]
// set0: bit0 invert WAS, bit1 invert relays, bit2 INVERT MOTOR, bit3 single-ended ADC, bit4 driver (0 IBT2, 1 Cytron),
//       bit5-6 external enable (0 none, 1 switch, 2 button), bit7 turn sensor.
// set1: bit0 Danfoss, bit1 pressure sensor, bit2 current sensor.
// Used here: "Invert motor" (turns the motor direction, together with the panel setting - applied only while steering is
// switched off, never in the middle of a run); the rest is shown in the panel. The angle sign comes from the module's
// own LEFT/RIGHT calibration, so "Invert WAS" is not needed; the driver is a Cytron (PWM + DIR) by construction.
class SteerConfigReceiver {
private:
    static uint8_t set0, trip, minSpeed10, set1;
    static bool invertMotorApplied_, invertMotorPending_, savedInvert_;
    static uint32_t rxCount;
public:
    static void parse(const uint8_t* data, uint8_t len) {
        if (len < 5) return;                      // incomplete frame - ignore
        set0 = data[5]; trip = data[6]; minSpeed10 = data[7]; set1 = data[8];
        invertMotorPending_ = (set0 & 0x04) != 0;
        rxCount++;
        if (rxCount == 1) LogBuffer::printf("[STEER CONFIG] Received from AgOpenWeb: set0=0x%02X set1=0x%02X (invert motor %s)\n", set0, set1, invertMotorPending_ ? "on" : "off");
    }
    // Call every loop pass: a changed "Invert motor" takes effect only while steering is disengaged.
    static void tick(bool steeringEngaged) {
        if (steeringEngaged || invertMotorApplied_ == invertMotorPending_) return;
        invertMotorApplied_ = invertMotorPending_;
        LogBuffer::printf("[STEER CONFIG] AgOpenWeb: invert motor %s - applied.\n", invertMotorApplied_ ? "ON" : "OFF");
    }
    static bool hasReceived() { return rxCount > 0; }
    static uint32_t getRxCount() { return rxCount; }
    static bool invertMotor() { return invertMotorApplied_; }          // what the motor driver uses
    static bool invertMotorRequested() { return invertMotorPending_; }
    static uint8_t getSet0() { return set0; }
    static uint8_t getSet1() { return set1; }
    static uint8_t getTrip() { return trip; }
    static uint8_t getMinSpeed10() { return minSpeed10; }
    // Start-up: the value from the module memory.
    static void restoreInvertMotor(bool v) { invertMotorApplied_ = invertMotorPending_ = savedInvert_ = v; }
    static bool invertNeedsSave() { return invertMotorApplied_ != savedInvert_; }
    static void markInvertSaved() { savedInvert_ = invertMotorApplied_; }
};
uint8_t SteerConfigReceiver::set0 = 0;
uint8_t SteerConfigReceiver::trip = 0;
uint8_t SteerConfigReceiver::minSpeed10 = 0;
uint8_t SteerConfigReceiver::set1 = 0;
bool SteerConfigReceiver::invertMotorApplied_ = false;
bool SteerConfigReceiver::invertMotorPending_ = false;
bool SteerConfigReceiver::savedInvert_ = false;
uint32_t SteerConfigReceiver::rxCount = 0;

// PGN 236 ("Machine Pin Config") - which function each of the 24 "pins" has in AgOpenWeb (Sec1..16, HydUp, HydDown,
// TramLeft, TramRight, GeoStop): [0x80,0x81,0x7F,0xEC,24, pin1..pin24, CRC]. This board has 16 outputs (pins 1-16); the
// map is kept in the module memory and applies at once (see io/machine_outputs.h, web/runtime_settings.h).
class MachinePinConfigReceiver {
private:
    static MachinePinMap map_, saved_;
    static uint32_t rxCount;
    static bool restored_;
public:
    static void parse(const uint8_t* data, uint8_t len) {
        if (len < MachinePinMap::PINS) return;    // incomplete frame - ignore
        MachinePinMap next; next.load(data + 5);
        if (rxCount == 0 || next != map_) LogBuffer::println("[MACHINE PINS] Pin configuration received from AgOpenWeb.");
        map_ = next; rxCount++;
    }
    static const MachinePinMap& map() { return map_; }
    static bool hasReceived() { return rxCount > 0; }
    static uint32_t getRxCount() { return rxCount; }
    static void restore(const MachinePinMap& m) { map_ = m; saved_ = m; restored_ = true; }
    static bool needsSave() { return map_ != saved_; }
    static void markSaved() { saved_ = map_; }
};
MachinePinMap MachinePinConfigReceiver::map_;
MachinePinMap MachinePinConfigReceiver::saved_;
uint32_t MachinePinConfigReceiver::rxCount = 0;
bool MachinePinConfigReceiver::restored_ = false;

float SteerCommandReceiver::speedKmh = 0.0f;
bool SteerCommandReceiver::steerSwitchActiveFromAgio = false;
bool SteerCommandReceiver::autoSteerEngaged = false;
uint8_t SteerCommandReceiver::engagedMask = 0x01;
uint8_t SteerCommandReceiver::lastStatusByte = 0;
float SteerCommandReceiver::targetSteerAngleDeg = 0.0f;
int8_t SteerCommandReceiver::xteRaw = 0;
uint8_t SteerCommandReceiver::sections1to8 = 0;
uint8_t SteerCommandReceiver::sections9to16 = 0;
unsigned long SteerCommandReceiver::lastCommandMillis = 0;
bool SteerCommandReceiver::loggedEngaged = false;
IPAddress SteerCommandReceiver::lastSourceIp;
uint8_t SteerCommandReceiver::lastRaw[SteerCommandReceiver::RAW_MAX] = {0};
uint8_t SteerCommandReceiver::lastRawLen = 0;
uint32_t SteerCommandReceiver::rxCount = 0;
uint32_t SteerCommandReceiver::lastDeltaMs = 0;
uint32_t SteerCommandReceiver::maxDeltaMs = 0;

uint8_t SteerSettingsReceiver::gainP = 0;
uint8_t SteerSettingsReceiver::highPWM = 0;
uint8_t SteerSettingsReceiver::lowPWM = 0;
uint8_t SteerSettingsReceiver::minPWM = 0;
uint8_t SteerSettingsReceiver::countsPerDeg = 1; // 1, not 0 - a divisor elsewhere, so we never divide by zero before AgOpenWeb sends anything
int16_t SteerSettingsReceiver::steerOffset = 0;
uint8_t SteerSettingsReceiver::ackermanFix = 100; // 100 = no correction (neutral), before AgOpenWeb sends anything
unsigned long SteerSettingsReceiver::lastReceivedMillis = 0;
bool SteerSettingsReceiver::restored = false;
SteerSettingsSaveGate SteerSettingsReceiver::saveGate;

uint8_t MachineCommandReceiver::uturnState = 0;
float MachineCommandReceiver::speedKmh = 0.0f;
uint8_t MachineCommandReceiver::hydLift = 0;
uint8_t MachineCommandReceiver::tramState = 0;
uint8_t MachineCommandReceiver::geoStop = 0;
uint8_t MachineCommandReceiver::sections1to8 = 0;
uint8_t MachineCommandReceiver::sections9to16 = 0;
unsigned long MachineCommandReceiver::lastCommandMillis = 0;
uint16_t MachineCommandReceiver::loggedSections = 0xFFFF;
