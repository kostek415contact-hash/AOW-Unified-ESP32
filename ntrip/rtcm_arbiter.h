#pragma once
// ======================================================================
// ntrip/rtcm_arbiter.h
// EN: Lets only one RTCM source feed the receiver at a time (own NTRIP client > UDP 2233 > PGN 215) to avoid
//     interleaved frames.
// PL: Pozwala karmić odbiornik tylko jednemu źródłu RTCM naraz (własny NTRIP > UDP 2233 > PGN 215), by nie
//     mieszać ramek.
// Tests: tests/test_rtcm_arbiter.cpp (host, plain C++).
// Used by: connectivity/network_manager.h, ntrip/ntrip_client.h, ntrip/rtcm_relay.h, web/web_server.h
// ======================================================================
// One RTCM correction source at a time. THREE independent paths can write to the GPS receiver:
//   1) our own NTRIP client (ntrip_client.h),
//   2) RtcmRelay - raw UDP from AgOpenWeb on port 2233,
//   3) PGN 215 from AgOpenWeb on the main port.
// When more than one is flowing, INTERLEAVED fragments of different streams reach the receiver
// (RTCM3 frames cut in half), which breaks or delays RTK. Rule: our own NTRIP client takes
// precedence (it is a workaround for the VRS bug in AgOpenWeb). External sources (2 and 3) are rejected while the
// own client is delivering data, and return automatically about OWN_HOLD_MS after its last data.
// Pure C++ (no Arduino) - host-tested.
#include <cstdint>

class RtcmArbiter {
public:
    static const unsigned long OWN_HOLD_MS = 3000;   // this much silence from our own client hands the channel to an external source
    // Names with the SRC_ prefix: plain EXTERNAL/NONE collide with Arduino.h macros (#define EXTERNAL 0).
    enum Source : uint8_t { SRC_NONE = 0, SRC_OWN = 1, SRC_AOG = 2 };

    static unsigned long lastOwnMs;           // 0 = our own client has not delivered anything yet
    static unsigned long lastExternalMs;      // the last external chunk that was PASSED THROUGH
    static uint32_t droppedBytes, droppedPackets;       // external source rejected because our own NTRIP is flowing
    static uint32_t forwardedBytes, forwardedPackets;   // external source passed through

    // Call on EVERY write of data from our own NTRIP client to the receiver.
    static void noteOwn(unsigned long nowMs) { lastOwnMs = (nowMs == 0) ? 1 : nowMs; }

    static bool ownActive(unsigned long nowMs) { return lastOwnMs != 0 && (nowMs - lastOwnMs) <= OWN_HOLD_MS; }

    // Call for every chunk from an EXTERNAL source. true = it may be passed to the receiver.
    static bool filterExternal(unsigned long nowMs, uint32_t bytes) {
        if (ownActive(nowMs)) { droppedBytes += bytes; droppedPackets++; return false; }
        forwardedBytes += bytes; forwardedPackets++;
        lastExternalMs = (nowMs == 0) ? 1 : nowMs;
        return true;
    }

    // Who is feeding the receiver now (for the panel).
    static uint8_t activeSource(unsigned long nowMs) {
        if (ownActive(nowMs)) return SRC_OWN;
        if (lastExternalMs != 0 && (nowMs - lastExternalMs) <= OWN_HOLD_MS) return SRC_AOG;
        return SRC_NONE;
    }
};

unsigned long RtcmArbiter::lastOwnMs = 0;
unsigned long RtcmArbiter::lastExternalMs = 0;
uint32_t RtcmArbiter::droppedBytes = 0;
uint32_t RtcmArbiter::droppedPackets = 0;
uint32_t RtcmArbiter::forwardedBytes = 0;
uint32_t RtcmArbiter::forwardedPackets = 0;
