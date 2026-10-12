#pragma once
// ======================================================================
// ntrip/rtcm_stats.h
// EN: RTCM3 frame parser for diagnostics only (message types, CRC24Q); never alters or delays the bytes sent to
//     the GPS.
// PL: Parser ramek RTCM3 wyłącznie do diagnostyki (typy komunikatów, CRC24Q); nigdy nie zmienia ani nie opóźnia
//     bajtów do GPS.
// Tests: tests/test_rtcm_stats.cpp (host, plain C++).
// Used by: ntrip/ntrip_client.h
// ======================================================================
#include <stdint.h>
#include <stddef.h>

// An RTCM3 frame parser used EXCLUSIVELY for diagnostics (it never changes or delays the bytes going to the GPS - it runs
// in parallel with writeRtcm()). Pure logic without Arduino, tests: tests/test_rtcm_stats.cpp.
//
// An RTCM3 frame: 0xD3 + 2 bytes (6 reserved bits = 0, 10 bits of length) + payload (first 12 bits = message
// type number, e.g. 1005 base position, 1077/1087/1097/1127 MSM observations for GPS/GLONASS/Galileo/BeiDou) + 3 bytes of CRC24Q
// (poly 0x1864CFB, init 0, computed from 0xD3 to the end of the payload).
// We count the type only for frames with a MATCHING CRC; frames with a bad checksum go to the crcErrors counter (a damaged stream:
// weak coverage, TCP dropped mid-frame, a bad decoder) - so the type statistics are not polluted
// with false "types" from garbage, and the panel can show the stream quality.
// Known limitation: with a damaged length the parser swallows as many bytes as were declared before it finds the checksum error,
// so the next valid frame may be skipped (diagnostics, not a decoder).
class RtcmMessageStats {
private:
    static const uint8_t MAX_TYPES = 24;
    struct TypeCount { uint16_t type; uint32_t count; };
    static TypeCount types[MAX_TYPES];
    static uint8_t typeCountUsed;
    static uint32_t framesParsed;
    static uint32_t crcErrors;
    static uint32_t overflowDropped; // frames of a type outside the first MAX_TYPES - rare, counter only

    enum class ParseState { PREAMBLE, LEN_HI, LEN_LO, PAYLOAD, CRC };
    static ParseState pstate;
    static uint16_t payloadLen;
    static uint16_t payloadRead;
    static uint8_t payloadFirst2[2];
    static uint8_t crcRemaining;
    static uint32_t crcRun;        // running CRC24Q
    static uint32_t crcGot;        // the 3 received checksum bytes

    static uint32_t crc24q(uint32_t crc, uint8_t b) {
        crc ^= (uint32_t)b << 16;
        for (uint8_t i = 0; i < 8; i++) {
            crc <<= 1;
            if (crc & 0x1000000UL) crc ^= 0x1864CFBUL;
        }
        return crc & 0xFFFFFFUL;
    }

    static void recordType(uint16_t type) {
        for (uint8_t i = 0; i < typeCountUsed; i++) {
            if (types[i].type == type) { types[i].count++; return; }
        }
        if (typeCountUsed < MAX_TYPES) {
            types[typeCountUsed].type = type;
            types[typeCountUsed].count = 1;
            typeCountUsed++;
        } else {
            overflowDropped++;
        }
    }

public:
    static void reset() {
        typeCountUsed = 0;
        framesParsed = 0;
        crcErrors = 0;
        overflowDropped = 0;
        pstate = ParseState::PREAMBLE;
    }

    static void feedByte(uint8_t b) {
        switch (pstate) {
            case ParseState::PREAMBLE:
                if (b == 0xD3) { crcRun = crc24q(0, b); pstate = ParseState::LEN_HI; }
                break;
            case ParseState::LEN_HI:
                if (b & 0xFC) {                     // reserved bits must be 0: this is not a header
                    pstate = ParseState::PREAMBLE;
                    feedByte(b);                    // this byte may be the start of a real frame
                    break;
                }
                payloadLen = ((uint16_t)(b & 0x03)) << 8;
                crcRun = crc24q(crcRun, b);
                pstate = ParseState::LEN_LO;
                break;
            case ParseState::LEN_LO:
                payloadLen |= b;
                payloadRead = 0;
                crcRun = crc24q(crcRun, b);
                pstate = (payloadLen >= 2) ? ParseState::PAYLOAD : ParseState::PREAMBLE;
                break;
            case ParseState::PAYLOAD:
                if (payloadRead < 2) payloadFirst2[payloadRead] = b;
                payloadRead++;
                crcRun = crc24q(crcRun, b);
                if (payloadRead >= payloadLen) {
                    crcRemaining = 3;
                    crcGot = 0;
                    pstate = ParseState::CRC;
                }
                break;
            case ParseState::CRC:
                crcGot = (crcGot << 8) | b;
                crcRemaining--;
                if (crcRemaining == 0) {
                    if (crcGot == crcRun) {
                        recordType((uint16_t)(((uint16_t)payloadFirst2[0] << 4) | (payloadFirst2[1] >> 4)));
                        framesParsed++;
                    } else {
                        crcErrors++;
                    }
                    pstate = ParseState::PREAMBLE;
                }
                break;
        }
    }

    static void feed(const uint8_t* buf, size_t len) {
        for (size_t i = 0; i < len; i++) feedByte(buf[i]);
    }

    static uint8_t getTypeCount() { return typeCountUsed; }
    static uint16_t getTypeAt(uint8_t i) { return types[i].type; }
    static uint32_t getCountAt(uint8_t i) { return types[i].count; }
    static uint32_t getFramesParsed() { return framesParsed; }
    static uint32_t getCrcErrors() { return crcErrors; }
    static uint32_t getOverflowDropped() { return overflowDropped; }
};

RtcmMessageStats::TypeCount RtcmMessageStats::types[RtcmMessageStats::MAX_TYPES];
uint8_t RtcmMessageStats::typeCountUsed = 0;
uint32_t RtcmMessageStats::framesParsed = 0;
uint32_t RtcmMessageStats::crcErrors = 0;
uint32_t RtcmMessageStats::overflowDropped = 0;
RtcmMessageStats::ParseState RtcmMessageStats::pstate = RtcmMessageStats::ParseState::PREAMBLE;
uint16_t RtcmMessageStats::payloadLen = 0;
uint16_t RtcmMessageStats::payloadRead = 0;
uint8_t RtcmMessageStats::payloadFirst2[2] = {0, 0};
uint8_t RtcmMessageStats::crcRemaining = 0;
uint32_t RtcmMessageStats::crcRun = 0;
uint32_t RtcmMessageStats::crcGot = 0;
