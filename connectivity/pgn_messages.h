#pragma once
// ======================================================================
// connectivity/pgn_messages.h
// EN: Content of outgoing PGN frames: building, CRC, sending ($PANDA, PGN 253/250 ...), Hello/Scan replies and
//     the heartbeat schedule.
// PL: Treść wysyłanych ramek PGN: budowanie, CRC, wysyłka ($PANDA, PGN 253/250 ...), odpowiedzi Hello/Scan i
//     harmonogram heartbeatów.
// Used by: connectivity/network_manager.h, io/status_led.h
// Depends on: agio_senders.h, config.h, gap_tracker.h, gps_universal.h, imu_sensor.h, inputs.h,
//     kickout_signal.h, log_buffer.h, log_throttle.h, net_addr.h ...
// ======================================================================
#include "../diagnostics/gap_tracker.h"
#include <Arduino.h>
#include <WiFi.h>
#include "../config/config.h"
#include "../web/log_buffer.h"
#include "../sensors/gps_universal.h"
#include "../sensors/steering_angle_sensor.h"
#include "../sensors/imu_sensor.h"
#include "../io/inputs.h"
#include "../diagnostics/net_stats.h"
#include "../diagnostics/log_throttle.h"
#include "net_addr.h"
#include "agio_senders.h"
#include "kickout_signal.h"
#include "ethernet_handler.h"
#include "slot_schedule.h"
#include <ETH.h>

// Everything about the CONTENT of PGN frames - building, CRC, sending, replies
// to discovery (Hello/Scan) and the heartbeat schedule. This is the place
// you look into when adding NEW content to an existing frame (e.g.
// hooking a real heading into PGN253) or a NEW frame type into the schedule.
//
// Orchestration (begin/loop, FreeRTOS tasks, WiFi/Ethernet handling) is
// in network_manager.h - this file knows NOTHING about who calls it or when,
// only how to build and send a specific PGN.

// AogUdpClass (WiFiUDP or eventually EthernetUDP) is defined in
// config.h - see there for the details of the Ethernet preparation.
static AogUdpClass udp8888;
static AogUdpClass udp5126;

// The "kickout" signal (PGN 250) after the motor protection trips - updated by nmKickoutTick() in network_manager.h.
static KickoutSignal kickoutSig;
// Motor PWM magnitude 0-255 for the AgOpenWeb PWM chart (PGN 253 byte 12); set after MotorDriver::update() in network_manager.h.
static uint8_t nmPwmDisplay = 0;

// The AgOpenWeb address the module talks to. There may be several senders (e.g. a computer and a phone with the program in the background) - the table and the selection
// rules (the first active one, or pinned from the panel) are in agio_senders.h. Until someone is selected, we send to broadcast as
// before - but once we know, we switch to unicast, because broadcast is what routers/phone hotspots
// buffer/drop the most (DTIM) - this is a direct cause of Machine/IMU flickering on an AP with no control over the network settings
// (e.g. a phone hotspot in the field, the target usage scenario).
static IPAddress agioKnownIp;
static bool agioIpKnown = false;
static unsigned long lastAgioPacketMillis = 0;   // the last frame from the SELECTED sender (foreign frames do not refresh it)

// Called from network_manager.h::nmCheckUDP for every valid frame. Returns true when the frame is from the sender we talk to
// (it is to be executed); false = a foreign sender - the frame stays only on the senders list in the panel and is NOT executed.
inline bool nmNoteAgioSender(IPAddress ip) {
    unsigned long now = millis();
    if (!AgioSenders::note((uint32_t)ip, (uint32_t)now)) return false;
    lastAgioPacketMillis = now;
    if (!agioIpKnown || agioKnownIp != ip) {
        agioIpKnown = true;
        agioKnownIp = ip;
        LogBuffer::printfIf(DEBUG_NET, "[NET] AgOpenWeb address learned: %s - switching to unicast.\n",
                            ip.toString().c_str());
    }
    return true;
}

// After the sender selection is changed in the panel: we stop sending to the previous address at once (until the first frame from the new one the subnet
// broadcast is used, usually a few ms), instead of waiting for the old address to expire after 5 s.
inline void nmAgioSelectionChanged() { agioIpKnown = false; }

// true if we received ANY valid packet from AgOpenWeb within the
// last maxAgeMs - a universal "AgOpenWeb is actually talking to us"
// signal, independent of a specific PGN type (unlike e.g.
// SteerCommandReceiver::isFresh(), which can be "stale" even with a
// real connection - AgOpenWeb does not always send every PGN type
// regardless of the state of its interface).
inline bool nmIsAgioFresh(unsigned long maxAgeMs = 2000) {
    return lastAgioPacketMillis != 0 && (millis() - lastAgioPacketMillis) <= maxAgeMs;
}

// The actual address, mask and gateway of the link CURRENTLY in use (Ethernet, if it has an address; otherwise WiFi). 0.0.0.0 = no
// address (e.g. AP mode, not connected yet) - then the configured values apply.
inline void nmActiveNet(IPAddress& ip, IPAddress& mask, IPAddress& gw) {
    if (ENABLE_ETHERNET) {
        IPAddress e = ETH.localIP();
        if (e != IPAddress(0, 0, 0, 0)) { ip = e; mask = ETH.subnetMask(); gw = ETH.gatewayIP(); return; }
    }
    ip = WiFi.localIP(); mask = WiFi.subnetMask(); gw = WiFi.gatewayIP();
}

// Whether we still consider the learned AgOpenWeb address valid. Addresses on a phone hotspot are CHANGING (the device with AgOpenWeb gets a new address
// after reconnecting), so without a packet from AgOpenWeb for 5 s we go back to the subnet broadcast until the next packet arrives.
inline bool nmAgioKnownNow() {
    const unsigned long staleMs = 5000;
    return agioIpKnown && lastAgioPacketMillis != 0 && (millis() - lastAgioPacketMillis) <= staleMs;
}

// Where to send frames to AgOpenWeb (logic in net_addr.h, host-tested): a freshly learned address, only if it is in the CURRENT
// subnet; otherwise the configured AGIO_IP, if it matches the subnet; otherwise the broadcast of the current subnet.
inline IPAddress nmAgioDest() {
    IPAddress ip, mask, gw;
    nmActiveNet(ip, mask, gw);
    return IPAddress(NetAddr::pickDest(nmAgioKnownNow(), (uint32_t)agioKnownIp, (uint32_t)AGIO_IP, (uint32_t)ip, (uint32_t)mask));
}

inline uint8_t nmCalculateCRC(const uint8_t* buffer, uint16_t length) {
    uint8_t crc = 0;
    for (uint16_t i = 2; i < length - 1; i++) {
        crc += buffer[i];
    }
    return crc;
}

// Sends to AgOpenWeb - unicast if we already know its address (see above),
// otherwise broadcast as before (needed at startup, before anything
// from AgOpenWeb has arrived at all). We log failed attempts, because without
// this we cannot see whether disconnects come from real send errors.
//
// A forced minimum gap of 1 ms between successive sends - when "catching up"
// the TSS schedule (see nmProcessHeartbeats) several PGNs may fall in the
// same loop() pass completely back to back, and the small internal W5500 buffer
// (already burdened by an unreliable SPI) cannot handle that - hence the "free size (0)
// < send length" errors under load, even though individual sends work.
inline bool nmSendPacket(const uint8_t* data, uint16_t len) {
    // 1 ms spacing only on the W5500 (tiny buffer); WiFi/ETH queue internally, so no stall there.
    if (EthernetHandler::isConnected()) {
        static unsigned long lastSendMillis = 0;
        if (millis() == lastSendMillis) delay(1);
        lastSendMillis = millis();
    }

    IPAddress dest = nmAgioDest();
    udp8888.beginPacket(dest, AGIO_PORT);
    udp8888.write(data, len);
    bool ok = udp8888.endPacket();
    NetStats::noteTx(ok, millis());   // counters for the panel (Diagnostics -> Network)
    if (!ok) {
        // Limited to once per 200 ms - with a series of errors (e.g. Ethernet in a
        // bad state) logging EVERY failed send (Serial +
        // a write to PSRAM) itself takes real CPU time,
        // potentially making things worse instead of merely describing them.
        static unsigned long lastFailLog = 0;
        static uint32_t failsSinceLastLog = 0;
        failsSinceLastLog++;
        if (millis() - lastFailLog >= 200) {
            lastFailLog = millis();
            LogBuffer::printfIf(DEBUG_NET, "[TX FAIL] len=%u t=%lu ms RSSI=%d (x%lu since the last log)\n",
                                len, millis(), WiFi.RSSI(), (unsigned long)failsSinceLastLog);
            failsSinceLastLog = 0;
        }
    }
    return ok;
}

inline void nmSendSubnetResponse(uint8_t srcID) {
    // We give the ACTUAL address and mask (with DHCP / a phone hotspot they differ from the configured ones); without an address - the configured ones.
    IPAddress ip, mask, gw;
    nmActiveNet(ip, mask, gw);
    if (ip == IPAddress(0, 0, 0, 0)) { ip = LOCAL_IP; mask = SUBNET_MASK; }
    (void)mask;   // the reply carries the network PREFIX (first three octets of the IP), not the mask - see PgnFrame::buildScanReply
    const uint8_t ipb[4] = {ip[0], ip[1], ip[2], ip[3]};
    uint8_t packet[13];
    PgnFrame::buildScanReply(packet, srcID, ipb);
    nmSendPacket(packet, 13);
}

inline void nmSendHelloResponse(uint8_t srcID) {
    uint8_t packet[11] = {
        0x80, 0x81, srcID, srcID, 5,
        0, 0, 0, 0, 0,
        0
    };
    packet[10] = nmCalculateCRC(packet, 11);
    nmSendPacket(packet, 11);
}

inline void nmHandleDiscoveryRequest(uint8_t reqPGN) {
    if (reqPGN == PGN_SCAN_REQUEST) {
        LogBuffer::printlnIf(DEBUG_PGN, "[PGN] Scan Request received from AgIO - sending Subnet Reply.");
        nmSendSubnetResponse(SRC_AUTOSTEER);
        nmSendSubnetResponse(SRC_MACHINE);
        nmSendSubnetResponse(SRC_IMU);
        nmSendSubnetResponse(SRC_GPS);
        nmSendSubnetResponse(SRC_GPS2);
    } else if (reqPGN == PGN_HELLO_REQUEST) {
        {   // Hello arrives several times per second - the log gets a bulk line every 5 s (counters: the PGN card)
            static CountingThrottle helloLog(5000);
            uint32_t n; unsigned long spanMs;
            if (helloLog.note(millis(), n, spanMs))
                LogBuffer::printfIf(DEBUG_PGN, "[PGN] Hello Request from AgIO: %lu within %lu ms - answering every one with Hello Reply.\n", (unsigned long)n, spanMs);
        }
        // At most one set of replies per 50 ms (5 packets each) - a Hello burst must not flood the TX.
        static unsigned long lastHelloReply = 0;
        if (lastHelloReply != 0 && millis() - lastHelloReply < 50) return;
        lastHelloReply = millis();
        nmSendHelloResponse(SRC_AUTOSTEER);
        nmSendHelloResponse(SRC_MACHINE);
        nmSendHelloResponse(SRC_IMU);
        nmSendHelloResponse(SRC_GPS);
        nmSendHelloResponse(SRC_GPS2);
    }
}

// Builds and sends $PANDA based on data from the real GPS receiver
// (sensors/gps_universal.h) - the only position format AgOpenWeb actually
// parses. GpsHandler itself makes sure to send fixQuality=0 when there is no/a stale
// fix, instead of a "frozen" old position.
//
// Heading: we prefer the IMU (ImuSensor) when it is connected - it provides
// roll/pitch ADDITIONALLY. A course computed from GPS alone (VTG) is unreliable at
// low/zero speed (the classic symptom of "spinning in circles" while standing
// still) - which is why the IMU (when connected) takes precedence.
inline void nmSendPanda() {
    char sentence[128];
    if (ImuSensor::isLive()) {
        // yawRate (rate of turn) is sent as 0.0 - the UART-RVC mode does not
        // expose the raw rate of turn (BNO08x_RVC_Data has only
        // yaw/pitch/roll + accelerations), so we have nowhere to get it from.
        GpsHandler::buildPanda(sentence, sizeof(sentence), true, ImuSensor::getHeadingDeg(),
                               true, ImuSensor::getRollDeg(), ImuSensor::getPitchDeg(), 0.0f);
    } else {
        GpsHandler::buildPanda(sentence, sizeof(sentence));
    }
    // Gap diagnostics (AgOpenWeb disconnects the GPS module after about 2 s without a position): attempts and successful sends separately.
    unsigned long tryAt = millis();
    GapStats::pandaTry.note(tryAt);
    if (GapStats::pandaTry.lastMs() > GapTracker::BIG_MS)
        LogBuffer::printf("[PANDA] gap of %lu ms between send ATTEMPTS - the main loop stalled (GPS/WiFi/NTRIP thread blocking?)\n", (unsigned long)GapStats::pandaTry.lastMs());
    bool sentOk = nmSendPacket((const uint8_t*)sentence, strlen(sentence));
    if (sentOk) {
        GapStats::pandaOk.note(millis());
        if (GapStats::pandaOk.lastMs() > GapTracker::BIG_MS)
            LogBuffer::printf("[PANDA] gap of %lu ms between SUCCESSFUL sends - UDP or network send errors (RSSI %d)\n",
                              (unsigned long)GapStats::pandaOk.lastMs(), WiFi.RSSI());
    }
}

// AgOpenWeb computes the "liveness" of Machine and IMU EXCLUSIVELY by receiving a
// Hello frame (PGN = module ID) - confirmed in the AgOpenWeb sources
// (UdpCommunicationService.cs: "Machine PGNs (receive-only, only Hello
// matters)"), with a 2 second limit. AgOpenWeb itself sends a Hello query every
// ~1 s, but that is a broadcast - on an AP with no control over the network settings (e.g.
// a phone hotspot) it can get lost for several seconds at a time (buffering
// by the access point). AgOpenWeb does not check whether it ASKED for the Hello - only
// the fact of receiving a valid frame counts - so we send them on our own,
// regularly, regardless of whether the query reached us at all.
//
// We do this for ALL five sources, not just Machine/IMU - AutoSteer/
// GPS/GPS2 have other data paths as well (which is why they were never red),
// but since we know this is a cheap, effective mechanism, an extra safety margin
// for all of them costs practically nothing (5 small packets
// every 500 ms).
inline void nmSendPeriodicHello() {
    static unsigned long lastHelloResend = 0;
    if (millis() - lastHelloResend >= 500) {
        lastHelloResend = millis();
        // At most one set of replies per 50 ms (5 packets each) - a Hello burst must not flood the TX.
        static unsigned long lastHelloReply = 0;
        if (lastHelloReply != 0 && millis() - lastHelloReply < 50) return;
        lastHelloReply = millis();
        nmSendHelloResponse(SRC_AUTOSTEER);
        nmSendHelloResponse(SRC_MACHINE);
        nmSendHelloResponse(SRC_IMU);
        nmSendHelloResponse(SRC_GPS);
        nmSendHelloResponse(SRC_GPS2);
    }
}

// ==========================================
// HARD SLOT SCHEDULE (TSS)
// ==========================================
// AgOpenWeb timeouts (UdpCommunicationService.cs): a module counts as present while its Hello arrives within 3 s, and the
// AutoSteer "data OK" needs one of the data PGNs (253/250/254/252/251) within 1.5 s. Machine and IMU depend on the Hello only.
// The cadence below (every 40 ms / 100 ms) is far inside both limits.
inline void nmProcessHeartbeats() {
    nmSendPeriodicHello();

    static unsigned long lastTick = 0;
    static_assert(SLOTS_PER_CYCLE == SlotSchedule::CYCLE, "slot_schedule.h and config.h disagree on the cycle length");
    static uint8_t slot = 0;
    unsigned long now = millis();

    // CATCH-UP LIMIT: if the backlog exceeds the whole cycle (e.g. after
    // startup - the first pass can take seconds - or after a longer
    // pause in loop()), we do not replay hundreds of slots at once (a burst
    // of packets) - we simply reset the clock to "now" and carry on
    // normally. Catching up a few slots (a normal, small slip every
    // pass) stays unchanged - see the comment at the while below.
    if (now - lastTick > (unsigned long)SLOTS_PER_CYCLE * SLOT_DURATION_MS) {
        lastTick = now - SLOT_DURATION_MS;
    }

    // WHILE, not IF: if loop() takes longer than SLOT_DURATION_MS (normal -
    // the sum of I2C/UART/UDP reads in one pass easily exceeds 5 ms), a plain
    // "if" would lose the excess time on every pass (lastTick = now erases
    // it irrevocably) and the WHOLE schedule would run slower than designed
    // - exactly what caused the Machine/IMU flicker (a tighter margin
    // against the AgOpenWeb limits than AutoSteer/GPS). "while" + lastTick +=
    // SLOT_DURATION_MS catches up the backlog, processing more than one
    // slot in a single loop() pass when needed.
    while (now - lastTick >= SLOT_DURATION_MS) {
        lastTick += SLOT_DURATION_MS;
        slot++;
        if (slot >= SLOTS_PER_CYCLE) slot = 0;

        const SlotSchedule::Kind kind = SlotSchedule::kindOf(slot);

        // AutoSteer (every 40 ms) -> slots 0, 8, 16, 24, 32
        if (kind == SlotSchedule::K_STEER) {
            uint8_t steerPkt[14] = {0x80, 0x81, SRC_AUTOSTEER, PGN_AUTOSTEER_HB, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0};

            // Bytes 5-6: actualSteerAngle (int16 LE, angle in degrees * 100)
            // - confirmed in AgOpenWeb's PgnBuilder.cs (TryParseSteerData). As long as the
            // angle sensor (SteeringAngleSensor) is not connected, 0.0 goes out -
            // when the sensor appears, the real angle will start flowing with no changes
            // here.
            // We GO BACK to sending our OWN, already converted angle (not the
            // raw ADC) - "Zero WAS" in AgOpenWeb assumes the module applies the
            // offset and counts-per-degree from PGN 252 (new offset = old offset - angle x CPD) and reports the corrected angle;
            // we do not apply them, so the angle never changes and the offset keeps growing. The calibration
            // again happens locally on the module (see
            // sensors/steering_angle_sensor.h and the web panel "Angle
            // sensor calibration"). The raw ADC reading without this calibration
            // (the previous version of this code) gave AgOpenWeb data that it
            // could not use correctly anyway.
            int16_t angleRaw = (int16_t)(SteeringAngleSensor::getAngleDeg() * 100.0f);
            steerPkt[5] = (uint8_t)(angleRaw & 0xFF);
            steerPkt[6] = (uint8_t)((angleRaw >> 8) & 0xFF);

            // Switches byte (index 11): bit 0x02 = Steer switch active,
            // bit 0x01 = Work switch, INVERTED (confirmed in AgOpenWeb's PgnBuilder.cs
            // - TryParseSteerData: "workSwitchActive = (switches
            // & 0x01) == 0" - i.e. 0 = working, 1 = transport).
            if (SteerButton::isPressed()) {
                steerPkt[11] |= 0x02;
            }
            if (!WorkSwitch::isActive()) {
                steerPkt[11] |= 0x01;
            }
            steerPkt[12] = nmPwmDisplay;   // PWM display (byte 12), shown in the AgOpenWeb steering chart
            steerPkt[13] = nmCalculateCRC(steerPkt, 14);
            nmSendPacket(steerPkt, 14);
        }
        // Machine (every 40 ms, shifted by 3 slots relative to AutoSteer so that
        // they do not collide in the same tick) -> slots 3, 11, 19, 27, 35
        else if (kind == SlotSchedule::K_MACHINE) {
            uint8_t machPkt[14] = {0x80, 0x81, SRC_MACHINE, PGN_MACHINE_HB, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            machPkt[13] = nmCalculateCRC(machPkt, 14);
            nmSendPacket(machPkt, 14);
        }
        // PANDA position (every 100 ms) -> slots 1, 21
        else if (kind == SlotSchedule::K_PANDA) {
            nmSendPanda();
        }
        // IMU (every 100 ms, limit 300 ms - a safe margin) -> slots 2, 22
        else if (kind == SlotSchedule::K_IMU) {
            uint8_t imuPkt[14] = {0x80, 0x81, SRC_IMU, PGN_IMU_HB, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            imuPkt[13] = nmCalculateCRC(imuPkt, 14);
            nmSendPacket(imuPkt, 14);
        }
        // GPS Status (every 100 ms) -> slots 5, 25 (not 4/24: slot 24 belongs to AutoSteer)
        else if (kind == SlotSchedule::K_GPS_STATUS) {
            uint8_t gpsPkt[14] = {0x80, 0x81, SRC_GPS, PGN_GPS_STATUS_HB, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            gpsPkt[13] = nmCalculateCRC(gpsPkt, 14);
            nmSendPacket(gpsPkt, 14);
        }
        // Sensor / kickout (PGN 250, every 40 ms) -> slots 7, 15, 23, 31, 39. Byte 5 = load, or 255 after
        // the motor protection trips (AgOpenWeb itself disables steering when "Current sensor" is enabled
        // in its configuration; see connectivity/kickout_signal.h).
        else if (kind == SlotSchedule::K_SENSOR) {
            uint8_t sensPkt[14] = {0x80, 0x81, SRC_AUTOSTEER, PGN_SENSOR_DATA, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            sensPkt[5] = kickoutSig.sensorByte((uint32_t)now);
            sensPkt[13] = nmCalculateCRC(sensPkt, 14);
            nmSendPacket(sensPkt, 14);
        }
        // In the remaining slots the module simply listens.
    }
}
