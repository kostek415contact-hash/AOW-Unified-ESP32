#pragma once
// ======================================================================
// sensors/gps_universal.h
// EN: Universal NMEA-0183 GPS handler: autobaud, retry after a late start, GGA/VTG/RMC/GST/GSV/GSA parsing,
//     checksum validation, receiver configuration via GpsConfigFsm.
// PL: Uniwersalna obsługa GPS NMEA-0183: autobaud, ponawianie po spóźnionym starcie, parsowanie
//     GGA/VTG/RMC/GST/GSV/GSA, kontrola sumy, konfiguracja odbiornika przez GpsConfigFsm.
// Used by: connectivity/network_manager.h, connectivity/pgn_messages.h, io/status_led.h, ntrip/ntrip_client.h,
//     ntrip/rtcm_relay.h, web/web_server.h
// Depends on: config.h, gap_tracker.h, gps_age.h, nmea.h, gps_clock.h, gps_config_fsm.h, gps_sats.h, heading_math.h,
//     log_buffer.h, gps_baud_retry.h, live_window.h
// ======================================================================
#include <HardwareSerial.h>
#include <esp_task_wdt.h>
#include <string.h>
#include <stdlib.h>
#include "../config/config.h"
#include "../web/log_buffer.h"
#include "gps_sats.h"
#include "gps_age.h"
#include "nmea.h"
#include "gps_clock.h"
#include "gps_config_fsm.h"
#include "gps_baud_retry.h"
#include "live_window.h"
#include "heading_math.h"
#include "../diagnostics/gap_tracker.h"

// Universal GPS receiver: works with ANY module that sends standard NMEA-0183 sentences (GGA + VTG and/or RMC),
// regardless of manufacturer or constellation. The talker ID (e.g. "GP", "GN", "GL", "GA", "GB") is ignored - only the
// sentence type counts ("GGA"/"VTG"/"RMC"), so GPGGA, GNGGA, GLGGA etc. are handled by the same code.
//
// The result is converted to $PANDA - the only GPS position format AgOpenWeb actually parses
// (see network_manager.h / NmeaParserServiceFast.cs).
class GpsHandler {
private:
    static HardwareSerial gpsSerial;
    static char lineBuffer[128];
    static uint8_t bufferIdx;

    // Last raw (unparsed) GGA sentence - see the comment in dispatchSentence(). Used by the own NTRIP client, among others.
    static char lastRawGga[128];
    // Fields for the retry mechanism (see retryIfNeeded() below): the original begin() parameters, so they can be repeated
    // later, and whether any valid NMEA line has ever arrived (proof that the current port configuration works).
    static uint8_t savedRxPin;
    static uint8_t savedTxPin;
    static uint32_t savedRequestedBaud;
    // State of the retry machine (retryIfNeeded()) - see the rationale there.
    static GpsBaudRetry baudRetry;   // non-blocking schedule of the speed re-detection (sensors/gps_baud_retry.h)
    static bool everGotValidNmea;
    static unsigned long lastRawGgaMillis;
    // Diagnostics for the panel (GPS card): which UART speed is IN USE now and where it came from, which chipset it is
    // (reply to PQTMVER) and the state of the detection retry.
    static uint32_t activeBaud;
    static uint8_t baudSource;      // 0 = from config.h, 1 = detected by autobaud, 2 = fallback 9600, 3 = detected on retry
    static uint8_t chipsetState;    // 0 = not checked yet, 1 = Quectel (answered PQTMVER), 2 = no PQTM answer
    static char chipsetInfo[48];    // e.g. "LG290P03AANR01A03S,2022/10/28,17:25:10"
    static GpsConfigFsm cfgFsm;     // non-blocking receiver configuration (see gps_config_fsm.h)

    // Most common baud rates used by NMEA modules - tried by the automatic baud-rate detection.
    static const uint32_t CANDIDATE_BAUD_RATES[7];

    // For measuring the refresh rate actually achieved (Hz).
    static unsigned long previousGgaMillis;
    static float measuredIntervalMs;

    // Diagnostic counters - show at which stage parsing stands (see [GPS DIAG] in update()).
    static uint32_t diagBytesTotal;
    static uint32_t diagDollarLines;
    static uint32_t diagValidChecksum;
    static uint32_t diagRecognized;
    static unsigned long diagLastReportMillis;

    // Last known position (from GGA, or from RMC as a fallback - see parseRMC). Kept as raw NMEA strings (DDmm.mmmmm),
    // because that is exactly the format $PANDA expects - nothing to convert.
    static char lastTime[12];
    static char lastLat[13];
    static char lastLatDir;
    static char lastLon[14];
    static char lastLonDir;
    static int lastFixQuality;
    static int loggedFixQuality; // to detect a fix-quality change (for logging)
    static int lastSatellites;
    static float lastHdop;
    static float lastAltitude;
    // Accuracy from GST (the real error in metres, a better measure than HDOP, which is only a dimensionless satellite
    // geometry factor) - see parseGST() below.
    static float lastHAcc;      // horizontal accuracy (m), -1 = no data/unknown
    static float lastVAcc;      // vertical accuracy (m), -1 = no data/unknown
    static unsigned long lastGstMillis;
    // Age of corrections (GGA field 13). lastAgeDgps goes into $PANDA and is 0 when the field is absent (as before);
    // whether an age was given at all and whether it is fresh is told by lastAgeValid/lastAgeMillis - the panel uses
    // those (see gps_age.h).
    static float lastAgeDgps;
    static bool lastAgeValid;
    static unsigned long lastAgeMillis;
    static unsigned long lastGgaMillis;

    // Last known speed/course - from VTG, or from RMC as a fallback (some cheap receivers do not send VTG at all).
    static float lastSpeedKnots;
    static float lastHeadingDeg;
    static unsigned long lastVelocityMillis;

    static uint8_t nmeaChecksum(const char* s, uint8_t len) { return Nmea::checksum(s, len); }

    // Checks the checksum of a complete "$....*HH" line. Shared by update() and detectBaudRate() to avoid duplicating logic.
    static bool validateLineChecksum(const char* line) { return Nmea::validateLine(line); }

    // Splits an NMEA line into fields (separated by commas/asterisk), modifying the buffer in place. Returns the number of fields.
    static uint8_t splitFields(char* line, char* fields[], uint8_t maxFields) { return Nmea::splitFields(line, fields, maxFields); }

    // Checks the sentence-type suffix (e.g. "GPGGA" -> "GGA"), ignoring the 2-letter talker/constellation identifier -
    // this is the core of the "universality" across receiver manufacturers.
    static bool sentenceEndsWith(const char* header, const char* suffix) { return Nmea::sentenceEndsWith(header, suffix); }

    static const char* describeFixQuality(int q) {
        switch (q) {
            case 0: return "NO FIX";
            case 1: return "GPS";
            case 2: return "DGPS";
            case 4: return "RTK FIXED";
            case 5: return "RTK FLOAT";
            default: return "?";
        }
    }

    static void parseGGA(char* fields[], uint8_t count) {
        // $xxGGA,time,lat,N/S,lon,E/W,fixQuality,numSat,hdop,alt,M,geoid,M,age,stationId  (field parsing: sensors/nmea.h)
        Nmea::Gga g;
        if (!Nmea::parseGga(fields, count, g)) return; // incomplete sentence - ignored, we do not risk garbage
        if (g.hasTime) strcpy(lastTime, g.time);
        if (g.hasLat) strcpy(lastLat, g.lat);
        if (g.hasLatDir) lastLatDir = g.latDir;
        if (g.hasLon) strcpy(lastLon, g.lon);
        if (g.hasLonDir) lastLonDir = g.lonDir;
        lastFixQuality = g.fix;
        if (lastFixQuality != loggedFixQuality) {
            LogBuffer::printfIf(DEBUG_GPS, "[GPS] Fix quality change: %s -> %s\n",
                                describeFixQuality(loggedFixQuality), describeFixQuality(lastFixQuality));
            loggedFixQuality = lastFixQuality;
        }
        lastSatellites = g.sats;
        lastHdop = g.hdop;
        lastAltitude = g.alt;
        lastAgeValid = g.ageValid;
        lastAgeDgps = g.age; // for $PANDA: 0 when the age is absent, as before
        lastAgeMillis = millis();

        unsigned long now = millis();
        if (previousGgaMillis != 0) {
            unsigned long delta = now - previousGgaMillis;
            // Gaps < 5 ms are not a real interval between epochs but several GGAs parsed back to back from a backlog in the
            // buffer (e.g. right after start/retry) - skipped, otherwise the Hz measurement shows absurd thousands of Hz.
            if (delta >= 5) {
                // Simple moving average so the Hz reading does not jump on a single late sentence.
                measuredIntervalMs = (measuredIntervalMs == 0.0f) ? (float)delta : (measuredIntervalMs * 0.8f + (float)delta * 0.2f);
            }
        }
        previousGgaMillis = now;
        lastGgaMillis = now;
        GapStats::gga.note(now);   // diagnostics: gaps between consecutive GGAs (panel: Diagnostics -> Network)
    }

    static void parseVTG(char* fields[], uint8_t count) {
        Nmea::Vtg v;
        if (!Nmea::parseVtg(fields, count, v)) return;
        if (v.hasHeading) lastHeadingDeg = v.heading;
        if (v.hasSpeed) lastSpeedKnots = v.speedKnots;
        lastVelocityMillis = millis();
    }

    // $xxGST,time,rms,semiMajor,semiMinor,orientation,errLat,errLon,errAlt
    // The real, estimated position error in metres (1-sigma) - a better measure of "how much the position can be trusted"
    // than HDOP alone (which is only a dimensionless satellite-geometry multiplier and says nothing about the real
    // precision in metres). hAcc is computed as the radial error from the lat/lon errors (the same formula as in the
    // confirmed reference code: hAcc = sqrt(errLat^2 + errLon^2)).
    static void parseGST(char* fields[], uint8_t count) {
        Nmea::Gst g;
        if (!Nmea::parseGst(fields, count, g)) return;
        lastHAcc = g.hAcc;
        lastVAcc = g.vAcc;
        lastGstMillis = millis();
    }

    static void parseRMC(char* fields[], uint8_t count) {
        // $xxRMC,time,status,lat,N/S,lon,E/W,speedKnots,course,date,...
        // Used as a fallback source of speed/course (when the module does not send VTG) and as a fallback source of POSITION
        // when the module does not send GGA at all (some very cheap/old receivers do not).
        Nmea::Rmc r;
        if (!Nmea::parseRmc(fields, count, r)) return; // too short, or status "V" = unreliable data
        if (r.hasDate) GpsClock::noteRmc(fields[1], fields[9], millis());   // UTC clock (time + date): see gps_clock.h

        if (r.hasSpeed) lastSpeedKnots = r.speedKnots;
        if (r.hasCourse) lastHeadingDeg = r.course;
        lastVelocityMillis = millis();

        // RMC position is taken only as a fallback, when GGA never arrived or is already stale - GGA has priority because it
        // also carries fix quality/HDOP/satellites, which RMC does not.
        if (lastGgaMillis == 0 || (millis() - lastGgaMillis) > 3000) {
            if (r.hasTime) strcpy(lastTime, r.time);
            if (r.hasLat) strcpy(lastLat, r.lat);
            if (r.hasLatDir) lastLatDir = r.latDir;
            if (r.hasLon) strcpy(lastLon, r.lon);
            if (r.hasLonDir) lastLonDir = r.lonDir;
            if (lastFixQuality == 0) lastFixQuality = 1; // RMC does not say which fix type - assume a plain GPS fix
            lastGgaMillis = millis(); // treat as a fresh position fix
        }
    }

    // Returns true if the line was recognised as GGA/VTG/RMC (for the diagnostic counters in update()).
    static bool dispatchSentence(char* line) {
        // line e.g. "$GPGGA,...." (checksum already validated in update()).
        // We keep a COPY of the raw line BEFORE splitFields() mutates it (it inserts \0 in place of commas) - at this point
        // we do not yet know whether it is GGA (the header is read below), so we always copy (cheap - short lines, 10 Hz) and
        // finalise (lastRawGga, timestamp) ONLY if it really turns out to be GGA. This lets us forward EXACTLY this sentence,
        // e.g. to the own NTRIP client - safer than assembling our own GGA from separately parsed fields.
        char rawCopy[128];
        bool haveRawCopy = strlen(line) < sizeof(rawCopy);
        if (haveRawCopy) strcpy(rawCopy, line);

        // IMPORTANT: split into fields ONCE and check the sentence type on fields[0] (e.g. "GPGGA") - checking it on the whole
        // undivided line (as before) looked at the last 3 characters right before the checksum, not at the header, so
        // NOTHING was ever recognised.
        char* fields[24];   // GSV: header + 3 + 4 groups x 4 + signal id + checksum = 22 fields (a limit of 20 cut off the signal id)
        uint8_t n = splitFields(line, fields, 24);
        if (n == 0) return false;

        char* header = fields[0] + 1; // skip the '$' at the start of fields[0]

        if (sentenceEndsWith(header, "GGA")) {
            cfgFsm.onNmea(GpsConfigFsm::T_GGA, millis());
            parseGGA(fields, n);
            if (haveRawCopy) {
                strcpy(lastRawGga, rawCopy);
                lastRawGgaMillis = millis();
            }
            return true;
        } else if (sentenceEndsWith(header, "VTG")) {
            cfgFsm.onNmea(GpsConfigFsm::T_VTG, millis());
            parseVTG(fields, n);
            return true;
        } else if (sentenceEndsWith(header, "GST")) {
            cfgFsm.onNmea(GpsConfigFsm::T_GST, millis());
            parseGST(fields, n);
            return true;
        } else if (sentenceEndsWith(header, "RMC")) {
            cfgFsm.onNmea(GpsConfigFsm::T_RMC, millis());
            parseRMC(fields, n);
            return true;
        } else if (sentenceEndsWith(header, "GSV")) {
            // C/N0 per satellite and signal (panel: Diagnostics -> GPS, bar chart). Handled, but NOT counted as "recognised"
            // (that counter covers the position sentences GGA/VTG/RMC/GST and is used to detect whether the module sends NMEA at all).
            cfgFsm.onNmea(GpsConfigFsm::T_GSV, millis());
            GpsSats::feedGsv(fields, n, millis());
            return false;
        } else if (sentenceEndsWith(header, "GSA")) {
            cfgFsm.onNmea(GpsConfigFsm::T_GSA, millis());
            GpsSats::feedGsa(fields, n, millis());    // which satellites are in the solution
            return false;
        }
        // Other sentences (GLL, TXT, PQTM*...) are deliberately ignored but do not break the parser. This is part of the
        // "universality": unknown/extra sentences are simply skipped instead of breaking the whole handling.
        return false;
    }

    // Sends any text command as a valid "$BODY*CS\r\n" frame, computing the checksum itself - used by the auto-configuration
    // (PQTM commands) and could be used for other chipset families.
    static void sendCommand(const char* body) {
        char sentence[96];
        uint8_t cs = nmeaChecksum(body, (uint8_t)strlen(body));
        snprintf(sentence, sizeof(sentence), "$%s*%02X\r\n", body, cs);
        gpsSerial.print(sentence);
    }

    // Listens for the given time for any line containing "needle" (e.g. a fragment of a specific manufacturer's reply
    // name). Used both for baud-rate detection and for recognising the chipset family.
    static bool listenFor(const char* needle, unsigned long timeoutMs, char* outLine = nullptr, size_t outSize = 0) {
        unsigned long start = millis();
        char probeBuffer[128];
        uint8_t probeIdx = 0;

        while (millis() - start < timeoutMs) {
            while (gpsSerial.available() > 0) {
                char c = gpsSerial.read();
                if (c == '\r') continue;
                if (c == '\n') {
                    if (probeIdx > 0) {
                        probeBuffer[probeIdx] = '\0';
                        if (strstr(probeBuffer, needle) != nullptr) {
                            if (outLine && outSize > 0) {          // return the found line (e.g. the chipset version)
                                strncpy(outLine, probeBuffer, outSize - 1);
                                outLine[outSize - 1] = '\0';
                            }
                            return true;
                        }
                        probeIdx = 0;
                    }
                } else if (probeIdx < sizeof(probeBuffer) - 1) {
                    probeBuffer[probeIdx++] = c;
                } else {
                    probeIdx = 0;
                }
            }
        }
        return false;
    }

    // Every complete line starting with '$' and having a valid checksum counts as "found" - that was enough and actually
    // worked for this hardware; extra restrictions (requiring a specific sentence type, more than one match) only
    // spoiled the detection.
    static bool listenForAnyValidNmea(unsigned long timeoutMs) {
        unsigned long start = millis();
        char probeBuffer[128];
        uint8_t probeIdx = 0;
        uint32_t totalBytes = 0;
        uint8_t firstBytes[16];
        uint8_t firstBytesCount = 0;

        while (millis() - start < timeoutMs) {
            while (gpsSerial.available() > 0) {
                char c = gpsSerial.read();
                totalBytes++;
                if (firstBytesCount < 16) firstBytes[firstBytesCount++] = (uint8_t)c;
                if (c == '\r') continue;
                if (c == '\n') {
                    if (probeIdx > 0) {
                        probeBuffer[probeIdx] = '\0';
                        if (probeBuffer[0] == '$' && validateLineChecksum(probeBuffer)) {
                            LogBuffer::printfIf(DEBUG_GPS, "[GPS AUTOBAUD] Valid line found: %s\n", probeBuffer);
                            return true;
                        }
                        probeIdx = 0;
                    }
                } else if (probeIdx < sizeof(probeBuffer) - 1) {
                    probeBuffer[probeIdx++] = c;
                } else {
                    probeIdx = 0;
                }
            }
        }

        // Diagnostics EVEN on failure - shows the raw bytes (hex) to tell "complete silence" (0 bytes - a physical problem)
        // from "something arrives but does not form NMEA" (wrong speed/noise).
        if (totalBytes > 0) {
            char hex[16 * 3 + 1] = {0};
            for (uint8_t i = 0; i < firstBytesCount; i++) {
                snprintf(hex + i * 3, 4, "%02X ", firstBytes[i]);
            }
            LogBuffer::printfIf(DEBUG_GPS, "[GPS AUTOBAUD] No valid line - received %lu bytes in a %lums window, first: %s\n",
                                (unsigned long)totalBytes, timeoutMs, hex);
        } else {
            LogBuffer::printlnIf(DEBUG_GPS, "[GPS AUTOBAUD] No valid line - ZERO bytes received in this window.");
        }
        return false;
    }

public:
    // Automatically detects the baud rate: tries the most common values in turn and checks whether a valid NMEA sentence
    // (with a good checksum) appears. Returns the detected baud rate, or 0 if nothing was found after trying all candidates.
    //
    // 460800 was put in FIRST place (highest priority) - it is the CONFIRMED (tested with PyGPSClient) default speed of
    // the LG290P. Without it the autobaud only tried "typical" speeds (115200 and lower) and never hit the right one -
    // bytes did arrive on the UART (the [GPS DIAG] UART byte counter kept growing), but at a wrong baud rate they are only
    // garbled noise that never forms a valid NMEA sentence (hence lines$=0 despite a growing byte counter - the hint
    // that it was a speed problem, not wiring).
    static uint32_t detectBaudRate(uint8_t rxPin, uint8_t txPin, unsigned long testDurationMs = 1000) {
        delay(300); // gives the GPS module a moment to boot before we start listening
        for (uint8_t i = 0; i < 7; i++) {
            uint32_t candidate = CANDIDATE_BAUD_RATES[i];
            LogBuffer::printfIf(DEBUG_GPS, "[GPS AUTOBAUD] Trying %lu baud...\n", (unsigned long)candidate);

            gpsSerial.end();
            // The default RX buffer (256 B) is too small at high speeds (especially 460800) - it must be set BEFORE begin(), and
            // EVERY time (end() resets it). Confirmed in working reference code (another project by the same author, the same
            // LG290P board) - the comment there: "at 460800 baud and 5 Hz the default 256 B is not enough".
            gpsSerial.setRxBufferSize(4096); gpsSerial.setTxBufferSize(2048);   // TX ring: an RTCM write (up to 1.2 kB) must not block the steering loop on the 128 B FIFO
            gpsSerial.begin(candidate, SERIAL_8N1, rxPin, txPin);

            if (listenForAnyValidNmea(testDurationMs)) {
                LogBuffer::printfIf(DEBUG_GPS, "[GPS AUTOBAUD] Detected %lu baud (valid NMEA checksum)\n", (unsigned long)candidate);
                return candidate;
            }
        }
        LogBuffer::printlnIf(DEBUG_GPS, "[GPS AUTOBAUD] No speed detected - check the wiring / module.");
        return 0;
    }

    // Sends the binary UBX-CFG-RATE command (u-blox) - sets the interval between measurements. Format confirmed in the
    // UBX protocol documentation: header 0xB5 0x62, class 0x06, id 0x08, payload length (2 B LE), payload
    // (measRate, navRate=1, timeRef=0/UTC), Fletcher-8 checksum computed from the class to the end of the payload. Safe
    // to send "blindly" - if the module is not a u-blox, these bytes are simply ignored as noise.
    static void sendUbxCfgRate(uint16_t measRateMs) {
        uint8_t frame[10]; // class, id, lenLo, lenHi, + 6B payload
        frame[0] = 0x06;
        frame[1] = 0x08;
        frame[2] = 6;
        frame[3] = 0;
        frame[4] = (uint8_t)(measRateMs & 0xFF);
        frame[5] = (uint8_t)((measRateMs >> 8) & 0xFF);
        frame[6] = 1; frame[7] = 0; // navRate = 1 (every measurement)
        frame[8] = 0; frame[9] = 0; // timeRef = 0 (UTC)

        uint8_t ckA = 0, ckB = 0;
        for (uint8_t i = 0; i < sizeof(frame); i++) {
            ckA = (uint8_t)(ckA + frame[i]);
            ckB = (uint8_t)(ckB + ckA);
        }

        gpsSerial.write((uint8_t)0xB5);
        gpsSerial.write((uint8_t)0x62);
        gpsSerial.write(frame, sizeof(frame));
        gpsSerial.write(ckA);
        gpsSerial.write(ckB);
    }

    // Automatic selection of the module parameters - NON-BLOCKING (GpsConfigFsm state machine, see gps_config_fsm.h):
    // this function only STARTS the sequence, update() executes the following steps every loop pass.
    // Sequence: observe the NMEA stream -> detect a Quectel (PQTMVERNO / PQTMVER) -> write settings and PQTMSAVEPAR ONLY
    // when the stream differs from the target (formerly it saved on every start) -> diagnostic protocol query.
    // UBX-CFG-RATE (u-blox) is sent only when the receiver did NOT answer like a Quectel (needless noise for a Quectel).
    static void autoConfigure(uint32_t targetRateHz) {
        LogBuffer::printlnIf(DEBUG_GPS, "[GPS AUTOCONFIG] Observing the NMEA stream, then checking whether the module is a Quectel (PQTMVER)...");
        chipsetState = 0;
        cfgFsm.start(millis(), targetRateHz);
    }

    // Sets chipsetState / chipsetInfo from the PQTMVERNO / PQTMVER reply (safe characters only - it ends up in JSON).
    static void applyVersionLine(const char* verLine) {
        chipsetState = 1;
        chipsetInfo[0] = '\0';
        // Version only from a real reply; "PQTMVER,ERROR,..." is not a version.
        const char* p = strstr(verLine, "PQTMVERNO,");
        if (p) p += 10;
        else {
            p = strstr(verLine, "PQTMVER,");
            if (p) { p += 8; if (strncmp(p, "ERROR", 5) == 0) p = nullptr; }
        }
        size_t o = 0;
        if (!p) p = "";
        while (*p && *p != '*' && o < sizeof(chipsetInfo) - 1) {
            char c = *p++;
            chipsetInfo[o++] = ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == ',' || c == '.' || c == '/' || c == ':' || c == '-' || c == '_' || c == ' ') ? c : '?';
        }
        chipsetInfo[o] = '\0';
    }

    // One configuration step: executes the machine's action (sending a command) and turns its events into log entries.
    static void configStep() {
        if (cfgFsm.running()) {
            GpsConfigFsm::Action a = cfgFsm.tick(millis());
            if (a.type == GpsConfigFsm::A_SEND) sendCommand(a.cmd);
            else if (a.type == GpsConfigFsm::A_UBX) sendUbxCfgRate(cfgFsm.intervalMs());
        }
        GpsConfigFsm::Event e;
        while ((e = cfgFsm.popEvent()) != GpsConfigFsm::EV_NONE) {
            switch (e) {
            case GpsConfigFsm::EV_OBSERVED_MATCH:
                LogBuffer::printfIf(DEBUG_GPS, "[GPS AUTOCONFIG] The stream already matches (%.1f Hz, all required sentences) - the module settings stay untouched.\n", (double)cfgFsm.observedRateHz());
                break;
            case GpsConfigFsm::EV_OBSERVED_DIFFERS:
                LogBuffer::printfIf(DEBUG_GPS, "[GPS AUTOCONFIG] The stream differs from the target (%.1f Hz measured) - the module will be configured.\n", (double)cfgFsm.observedRateHz());
                break;
            case GpsConfigFsm::EV_QUECTEL:
                applyVersionLine(cfgFsm.verLine());
                LogBuffer::printlnIf(DEBUG_GPS, "[GPS AUTOCONFIG] Quectel chipset detected.");
                break;
            case GpsConfigFsm::EV_NOT_QUECTEL:
                chipsetState = 2;
                LogBuffer::printlnIf(DEBUG_GPS, "[GPS AUTOCONFIG] No PQTM response.");
                break;
            case GpsConfigFsm::EV_WROTE:
                LogBuffer::printfIf(DEBUG_GPS, "[GPS AUTOCONFIG] PQTM: requested %u ms (~%lu Hz), saved to the module memory.\n",
                                    (unsigned)cfgFsm.intervalMs(), (unsigned long)GPS_TARGET_RATE_HZ);
                break;
            case GpsConfigFsm::EV_UBX:
                LogBuffer::printfIf(DEBUG_GPS, "[GPS AUTOCONFIG] UBX: requested %u ms (~%lu Hz) - works only on u-blox, no confirmation.\n",
                                    (unsigned)cfgFsm.intervalMs(), (unsigned long)GPS_TARGET_RATE_HZ);
                break;
            case GpsConfigFsm::EV_PROTO_REPLY:
                // Diagnostics: the RAW answer to whether RTCM3 input is enabled on UART3 (1=NMEA, bit2=RTCM3).
                LogBuffer::printf("[GPS DIAG] UART%u protocol configuration: %s\n", 3u, cfgFsm.protoLine());
                break;
            case GpsConfigFsm::EV_PROTO_NONE:
                LogBuffer::printf("[GPS DIAG] No response to the UART%u configuration query (PQTMCFGPROT).\n", 3u);
                break;
            case GpsConfigFsm::EV_DONE:
                LogBuffer::printlnIf(DEBUG_GPS, "[GPS AUTOCONFIG] Configuration finished.");
                break;
            default: break;
            }
        }
    }

    static void begin(uint8_t rxPin, uint8_t txPin, uint32_t baudRate) {
        savedRxPin = rxPin;
        savedTxPin = txPin;
        savedRequestedBaud = baudRate;
        everGotValidNmea = false;
        baudRetry.restartClock((uint32_t)millis());

        uint32_t finalBaud = baudRate;
        baudSource = 0;
        if (finalBaud == 0) {
            finalBaud = detectBaudRate(rxPin, txPin);
            baudSource = 1;
            if (finalBaud == 0) {
                finalBaud = 9600; // safest default fallback
                baudSource = 2;
                LogBuffer::printlnIf(DEBUG_GPS, "[GPS AUTOBAUD] Falling back to 9600 baud.");
            }
        }

        gpsSerial.end();
        gpsSerial.setRxBufferSize(4096); gpsSerial.setTxBufferSize(2048);   // TX ring: an RTCM write (up to 1.2 kB) must not block the steering loop on the 128 B FIFO // see the rationale in detectBaudRate()
        gpsSerial.begin(finalBaud, SERIAL_8N1, rxPin, txPin);
        activeBaud = finalBaud;

        lastFixQuality = 0;
        loggedFixQuality = -1;
        lastSatellites = 0;
        lastGgaMillis = 0;
        lastAgeValid = false;
        lastAgeMillis = 0;
        lastVelocityMillis = 0;
        previousGgaMillis = 0;
        measuredIntervalMs = 0.0f;
        diagBytesTotal = 0;
        diagDollarLines = 0;
        diagValidChecksum = 0;
        diagRecognized = 0;
        diagLastReportMillis = 0;
        strcpy(lastTime, "000000.00");
        strcpy(lastLat, "0000.00000");
        lastLatDir = 'N';
        strcpy(lastLon, "00000.00000");
        lastLonDir = 'E';
        lastHAcc = -1.0f;
        lastVAcc = -1.0f;
        lastGstMillis = 0;

        LogBuffer::printlnIf(DEBUG_GPS, "\n--- [GPS HANDLER - UART DIAGNOSTICS] ---");
        LogBuffer::printfIf(DEBUG_GPS, "RX=%d, TX=%d, Baud=%lu\n", rxPin, txPin, (unsigned long)finalBaud);
        LogBuffer::printlnIf(DEBUG_GPS, "Handling: any NMEA-0183 module (GGA/VTG/RMC, any talker / constellation)");
        LogBuffer::printlnIf(DEBUG_GPS, "---------------------------------------------------------");
    }

    // Call every loop() pass - reads and parses everything that arrived from the UART.
    static void update() {
        // OPTIMISATION: read in CHUNKS (up to 128 B at a time), not byte by byte. The CPU profile showed ~20 us per byte
        // (available()+read() for every character, ~3.7 KB/s from the GPS at 20 Hz = about 7.5% of core 1). The parser itself
        // (below) is unchanged - only the way the bytes are fetched changes.
        uint8_t chunk[128];
        int avail;
        while ((avail = gpsSerial.available()) > 0) {
            size_t want = ((size_t)avail > sizeof(chunk)) ? sizeof(chunk) : (size_t)avail;
            size_t got = gpsSerial.read(chunk, want);
            if (got == 0) break;
            diagBytesTotal += got;

            for (size_t k = 0; k < got; k++) {
                char c = (char)chunk[k];
                if (c == '\r') continue;

                if (c == '\n') {
                    if (bufferIdx > 0) {
                        lineBuffer[bufferIdx] = '\0';
                        if (lineBuffer[0] == '$') {
                            diagDollarLines++;
                            // Checksum validation - rejects corrupted frames (typical with a wrong baud rate/wiring). Part of the
                            // "universality" is not blindly trusting every byte from any module.
                            if (validateLineChecksum(lineBuffer)) {
                                diagValidChecksum++;
                                everGotValidNmea = true;
                                if (strncmp(lineBuffer, "$PQTM", 5) == 0) cfgFsm.onLine(lineBuffer);   // replies to configuration commands
                                if (dispatchSentence(lineBuffer)) {
                                    diagRecognized++;
                                }
                            }
                        }
                        bufferIdx = 0;
                    }
                } else {
                    if (bufferIdx < sizeof(lineBuffer) - 1) {
                        lineBuffer[bufferIdx++] = c;
                    } else {
                        bufferIdx = 0; // buffer overflow - drop the line
                    }
                }
            }
        }

        configStep();   // non-blocking receiver configuration (at most one command per loop pass)

        // Every 3 s shows at which stage parsing actually stands - raw UART bytes -> lines starting with '$' -> valid
        // checksum -> recognised type (GGA/VTG/RMC). If e.g. bytes flow but there is never a '$', it is a wiring/level
        // problem; if '$' is there but the checksum is always wrong, they are transmission errors; if the checksum is fine
        // but nothing is "recognised", the module does not send GGA/VTG/RMC (only other sentences).
        if (millis() - diagLastReportMillis >= 3000) {
            diagLastReportMillis = millis();
            LogBuffer::printfIf(DEBUG_GPS,
                                "[GPS DIAG] UARTbytes=%lu lines$=%lu validChecksum=%lu recognised=%lu\n",
                                (unsigned long)diagBytesTotal, (unsigned long)diagDollarLines,
                                (unsigned long)diagValidChecksum, (unsigned long)diagRecognized);
        }
    }

    // true if we have a fresh position fix (not older than maxAgeMs)
    static bool hasFreshFix(unsigned long maxAgeMs = 2000) {
        return lastGgaMillis != 0 && (millis() - lastGgaMillis) <= maxAgeMs;
    }

    // Last raw GGA sentence received from the GPS receiver (e.g. to forward with the own NTRIP client) - an empty
    // string if none has been received yet. maxAgeMs=0 disables the freshness check (returns whatever it has, even stale).
    static const char* getLastRawGga(unsigned long maxAgeMs = 2000) {
        if (lastRawGgaMillis == 0) return "";
        if (maxAgeMs > 0 && (millis() - lastRawGgaMillis) > maxAgeMs) return "";
        return lastRawGga;
    }

    static int getSatelliteCount() {
        return lastSatellites;
    }

    // Returns 0 (no fix) if the position is STALE (> 2 s since the last GGA/RMC) - without this, with the GPS physically
    // disconnected, this function would return the last known quality (e.g. "4" RTK FIX) FOREVER, although the module
    // sends nothing any more. This directly drives the status LED and the NtripClient decisions - the same pattern as
    // ImuSensor::isConnected().
    static int getFixQuality() {
        if (!hasFreshFix(2000)) return 0;
        return lastFixQuality;
    }

    // UART diagnostic counters - the same ones that are already counted every 3 s internally and written to the Serial
    // log (see update() below) - exposed here to show them ALSO in the web panel (Diagnostics GPS, visible under
    // DEBUG_GPS), with no need to connect to Serial.
    static uint32_t getDiagBytesTotal()    { return diagBytesTotal; }
    static uint32_t getActiveBaud()        { return activeBaud; }
    static uint8_t getBaudSource()         { return baudSource; }
    static uint8_t getChipsetState()       { return chipsetState; }
    static const char* getChipsetInfo()    { return chipsetInfo; }
    // Receiver configuration result: 0 = in progress / not started, 1 = new settings saved, 2 = already correct (nothing written), 3 = not a Quectel (UBX)
    static uint8_t getConfigResult()       { return (uint8_t)cfgFsm.result(); }
    static bool isRetryActive()            { return baudRetry.active(); }
    static uint32_t getRetryAttempts()     { return baudRetry.attempts(); }
    static bool hasEverGotValidNmea()      { return everGotValidNmea; }
    static uint32_t getDiagDollarLines()   { return diagDollarLines; }
    static uint32_t getDiagValidChecksum() { return diagValidChecksum; }
    static uint32_t getDiagRecognized()    { return diagRecognized; }

    // Call every loop() pass - if NOT A SINGLE valid NMEA line has ever arrived since the port was started, repeat the
    // whole detection/configuration sequence from scratch, no more often than every 15 s.
    //
    // WHY: fixes the case where the ESP32 (powered e.g. from its own regulator/USB) starts FASTER than the external 3.3 V
    // converter, or the GPS module itself needs a moment after power-up before it actually starts transmitting. Without
    // this, the one-off detection attempt at start hits the void (the module is not physically ready yet) and never
    // repairs itself - the device had to be restarted by hand (from the web panel or by re-flashing), by which time the
    // peripherals had already "warmed up".
    static void retryIfNeeded() {
        // NON-BLOCKING: the parser (update(), which ran just before this in the same pass) already decides whether a valid line
        // arrived at the current speed; this only moves the schedule. The old version busy-waited 150 ms per candidate speed
        // (7 passes, about 1 s of frozen loop every 16 s while the GPS stayed silent).
        GpsBaudRetry::Out o = baudRetry.tick((uint32_t)millis(), everGotValidNmea);
        switch (o.act) {
        case GpsBaudRetry::A_NONE:
            return;
        case GpsBaudRetry::A_SUCCESS: {
            uint32_t found = CANDIDATE_BAUD_RATES[o.index];
            LogBuffer::printfIf(DEBUG_GPS, "[GPS] Retry: detected %lu baud.\n", (unsigned long)found);
            activeBaud = found;
            baudSource = 3;
            autoConfigure(GPS_TARGET_RATE_HZ);
            return;
        }
        case GpsBaudRetry::A_FALLBACK:
            // All candidates exhausted - stay on the last resort (9600) and try the whole sequence again in 15 s.
            LogBuffer::printlnIf(DEBUG_GPS, "[GPS] Retry: no speed worked - next attempt in 15 s.");
            restartPort(9600);
            baudSource = 2;
            return;
        case GpsBaudRetry::A_SWITCH:
            if (o.index == 0) {
                LogBuffer::printlnIf(DEBUG_GPS, "[GPS] No valid NMEA line at all since start - retrying speed detection (one speed per loop pass, so the rest of the system is NOT blocked).");
            }
            restartPort(CANDIDATE_BAUD_RATES[o.index]);
            return;
        }
    }

    // Restarts the GPS UART at the given speed (the RX buffer is re-created, as in begin()).
    static void restartPort(uint32_t baud) {
        gpsSerial.end();
        gpsSerial.setRxBufferSize(4096); gpsSerial.setTxBufferSize(2048);   // TX ring: an RTCM write (up to 1.2 kB) must not block the steering loop on the 128 B FIFO
        gpsSerial.begin(baud, SERIAL_8N1, savedRxPin, savedTxPin);
        activeBaud = baud;
    }

    // Refresh rate of the position actually achieved (Hz), measured live from the gaps between consecutive GGAs - to
    // confirm that autoConfigure() really worked (or for diagnostics when it did not).
    static float getMeasuredRateHz() {
        if (measuredIntervalMs <= 0.0f) return 0.0f;
        return 1000.0f / measuredIntervalMs;
    }

    // Additional position fields - collected internally so far (for $PANDA and buildGGA) but never exposed - needed for
    // the full GPS state view on the web page.
    static const char* getLatitude()  { return lastLat; }
    static char getLatDir()           { return lastLatDir; }
    static const char* getLongitude() { return lastLon; }
    static char getLonDir()           { return lastLonDir; }
    static const char* getTime()      { return lastTime; }
    static float getHdop()            { return lastHdop; }
    static float getAltitude()        { return lastAltitude; }
    // Returns -1 if GST is stale (> maxAgeMs) - as in the confirmed reference code: we do not show a "frozen" old
    // accuracy when the module stopped sending GST (e.g. GSTMSGRATE has not been saved yet - see autoConfigure()).
    static float getHAcc(unsigned long maxAgeMs = 3000) {
        if (lastGstMillis == 0 || millis() - lastGstMillis > maxAgeMs) return -1.0f;
        return lastHAcc;
    }
    static float getVAcc(unsigned long maxAgeMs = 3000) {
        if (lastGstMillis == 0 || millis() - lastGstMillis > maxAgeMs) return -1.0f;
        return lastVAcc;
    }
    static float getAgeDgps()         { return lastAgeDgps; }
    // Age of corrections FOR THE PANEL: -1 = none (empty field in GGA, the receiver applies no corrections, or GGA stopped flowing).
    // Differs from getAgeDgps(), which gives 0 when absent (needed in $PANDA) and would look like "fresh corrections".
    static float getAgeDgpsOrNone(unsigned long maxAgeMs = 3000) {
        if (!lastAgeValid || lastAgeMillis == 0 || millis() - lastAgeMillis > maxAgeMs) return -1.0f;
        return lastAgeDgps;
    }
    // Level for colouring in the panel (GpsAge::Level): 0 ok, 1 warning, 2 bad, 3 no data.
    static int getAgeLevel() {
        float a = getAgeDgpsOrNone();
        return GpsAge::level(a >= 0.0f, a, lastFixQuality);
    }
    static float getSpeedKnots()      { return lastSpeedKnots; }
    // Course computed by the GPS from movement (VTG/RMC) - NOT the same as a compass; see the comment at buildPanda()
    // about why it is unreliable at low speed.
    static float getGpsHeadingDeg()   { return lastHeadingDeg; }

    // Builds a standard $GPGGA sentence from the latest data - needed to report the own position to the NTRIP caster
    // (some VRS/network RTK servers require it to pick/interpolate the right corrections).
    static void buildGGA(char* out, size_t outSize) {
        char body[100];
        snprintf(body, sizeof(body),
                 "GPGGA,%s,%s,%c,%s,%c,%d,%02d,%.1f,%.1f,M,0.0,M,%.1f,",
                 lastTime, lastLat, lastLatDir, lastLon, lastLonDir,
                 lastFixQuality, lastSatellites, lastHdop, lastAltitude, lastAgeDgps);

        uint8_t cs = nmeaChecksum(body, (uint8_t)strlen(body));
        snprintf(out, outSize, "$%s*%02X\r\n", body, cs);
    }

    // Builds a ready "$PANDA,...*CS\r\n" frame from the latest received data. If the fix is stale/absent, it sends
    // fixQuality=0 and zero speed so that AgOpenWeb correctly shows no signal instead of a "frozen" old position.
    //
    // useCompassHeading/compassHeadingDeg: when true, REPLACES the GPS-derived direction (VTG/RMC) with the direction from
    // a compass - the GPS direction is computed from position change, so at low/zero speed it is practically random (the
    // classic symptom: the vehicle "spins in circles" while standing still). A compass works independently of speed.
    //
    // hasImuRollPitchYaw/rollDeg/pitchDeg/yawRateDegS: PREPARATION for a full IMU (e.g. BNO085) - a compass alone gives
    // only direction, not roll/pitch/yaw. As long as nobody supplies them (hasImuRollPitchYaw = false, the default), these
    // fields go out as 0 - exactly as before, no change in behaviour. When a real IMU arrives, its module just passes its
    // data here without changing this signature.
    static void buildPanda(char* out, size_t outSize,
                           bool useCompassHeading = false, float compassHeadingDeg = 0.0f,
                           bool hasImuRollPitchYaw = false, float rollDeg = 0.0f,
                           float pitchDeg = 0.0f, float yawRateDegS = 0.0f) {
        // The position sent to AgOpenWeb must be LIVE: 5 sentence periods (see live_window.h), not the 2 s used for display.
        bool freshPos = hasFreshFix(LiveWindow::forGps(measuredIntervalMs));
        bool freshVel = (lastVelocityMillis != 0) && (millis() - lastVelocityMillis) <= 2000;

        int fixQuality = freshPos ? lastFixQuality : 0;
        int satellites = freshPos ? lastSatellites : 0;
        float speedKnots = freshVel ? lastSpeedKnots : 0.0f;

        // The $PANDA course is degrees x10 in 0-3599 (65535 = none). The IMU gives -180...180 and the GPS course can be 360.0 -
        // normalised to 0-359.9 (the same angle, in the range AgOpenWeb expects); NaN => 65535.
        int headingX10;
        if (useCompassHeading) {
            headingX10 = HeadingMath::toX10(compassHeadingDeg);
        } else {
            headingX10 = HeadingMath::courseX10(lastHeadingDeg, lastSpeedKnots, freshVel);   // 65535 = no IMU: a course is sent only while moving
        }

        int rollX10 = hasImuRollPitchYaw ? (int)(rollDeg * 10.0f) : 0;
        float pitch = hasImuRollPitchYaw ? pitchDeg : 0.0f;
        float yawRate = hasImuRollPitchYaw ? yawRateDegS : 0.0f;

        char body[110];
        snprintf(body, sizeof(body),
                 "PANDA,%s,%s,%c,%s,%c,%d,%d,%.1f,%.1f,%.1f,%.1f,%d,%d,%.1f,%.1f",
                 lastTime, lastLat, lastLatDir, lastLon, lastLonDir,
                 fixQuality, satellites, lastHdop, lastAltitude, lastAgeDgps,
                 speedKnots, headingX10, rollX10, pitch, yawRate);

        uint8_t cs = nmeaChecksum(body, (uint8_t)strlen(body));
        snprintf(out, outSize, "$%s*%02X\r\n", body, cs);
                           }

                           // Writing the NTRIP/RTCM corrections from the network to the GPS receiver (unchanged).
                           static void writeRtcm(const uint8_t* buffer, size_t len) {
                               size_t written = gpsSerial.write(buffer, len);
                               if (written != len) {
                                   LogBuffer::printfIf(DEBUG_RTCM, "[RTCM] WARNING: write() sent only %u of %u bytes!\n",
                                                       (unsigned)written, (unsigned)len);
                               }
                           }
};

HardwareSerial GpsHandler::gpsSerial(1);
char GpsHandler::lineBuffer[128];
uint8_t GpsHandler::bufferIdx = 0;
const uint32_t GpsHandler::CANDIDATE_BAUD_RATES[7] = {460800, 115200, 9600, 38400, 19200, 57600, 4800};
unsigned long GpsHandler::previousGgaMillis = 0;
char GpsHandler::lastRawGga[128] = "";
uint8_t GpsHandler::savedRxPin = 0;
uint8_t GpsHandler::savedTxPin = 0;
uint32_t GpsHandler::savedRequestedBaud = 0;
GpsBaudRetry GpsHandler::baudRetry;
bool GpsHandler::everGotValidNmea = false;
unsigned long GpsHandler::lastRawGgaMillis = 0;
uint32_t GpsHandler::activeBaud = 0;
uint8_t GpsHandler::baudSource = 0;
uint8_t GpsHandler::chipsetState = 0;
GpsConfigFsm GpsHandler::cfgFsm;
char GpsHandler::chipsetInfo[48] = "";
float GpsHandler::measuredIntervalMs = 0.0f;
uint32_t GpsHandler::diagBytesTotal = 0;
uint32_t GpsHandler::diagDollarLines = 0;
uint32_t GpsHandler::diagValidChecksum = 0;
uint32_t GpsHandler::diagRecognized = 0;
unsigned long GpsHandler::diagLastReportMillis = 0;
char GpsHandler::lastTime[12];
char GpsHandler::lastLat[13];
char GpsHandler::lastLatDir = 'N';
float GpsHandler::lastHAcc = -1.0f;
float GpsHandler::lastVAcc = -1.0f;
unsigned long GpsHandler::lastGstMillis = 0;
char GpsHandler::lastLon[14];
char GpsHandler::lastLonDir = 'E';
int GpsHandler::lastFixQuality = 0;
int GpsHandler::loggedFixQuality = -1; // forces the first log at start
int GpsHandler::lastSatellites = 0;
float GpsHandler::lastHdop = 0.0f;
float GpsHandler::lastAltitude = 0.0f;
float GpsHandler::lastAgeDgps = 0.0f;
bool GpsHandler::lastAgeValid = false;
unsigned long GpsHandler::lastAgeMillis = 0;
unsigned long GpsHandler::lastGgaMillis = 0;
float GpsHandler::lastSpeedKnots = 0.0f;
float GpsHandler::lastHeadingDeg = 0.0f;
unsigned long GpsHandler::lastVelocityMillis = 0;
