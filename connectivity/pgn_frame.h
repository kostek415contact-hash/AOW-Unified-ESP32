#pragma once
// ======================================================================
// connectivity/pgn_frame.h
// EN: AgOpenWeb PGN frame helpers: layout, CRC check and the policy for rejecting corrupted control frames.
// PL: Pomocnicze funkcje ramek PGN AgOpenWeb: układ, kontrola CRC i zasady odrzucania uszkodzonych ramek
//     sterujących.
// Tests: tests/test_pgn_frame.cpp (host, plain C++).
// Used by: connectivity/network_manager.h, connectivity/pgn_receivers.h
// ======================================================================
// Helper functions for AgOpenWeb PGN frames (pure C++, host-tested).
// Frame: 0x80 0x81 | source | PGN | data length | data... | CRC
// CRC = sum of bytes from index 2 to the last data byte (mod 256).
#include <cstdint>
#include <cstddef>

namespace PgnFrame {

    // 1 = CRC ok, 0 = mismatch, -1 = incomplete frame (cannot be judged).
    inline int crcCheck(const uint8_t* b, int len) {
        if (len < 6) return -1;
        int declared = b[4];
        int total = 5 + declared + 1;
        if (len < total) return -1;
        uint8_t crc = 0;
        for (int i = 2; i < total - 1; i++) crc = (uint8_t)(crc + b[i]);
        return crc == b[total - 1] ? 1 : 0;
    }

    // Whether to reject (not execute) a frame of the given PGN whose checksum is NOT confirmed. Applies only to frames that CONTROL the
    // module (steer command 254, machine command 239, machine config 238, steer settings 252, steer config 251, machine pins 236): a mismatch means a corrupted frame, and
    // an incomplete one (the CRC byte is missing, crcResult -1) cannot be verified at all - executing either (e.g. the target angle or
    // the lift pulse length) would be dangerous. Hello/Scan and RTCM stay unchanged (RTCM has its own CRC24). crcResult as in crcCheck().
    inline bool shouldRejectOnCrc(uint8_t pgn, int crcResult) {
        return crcResult != 1 && (pgn == 254 || pgn == 239 || pgn == 238 || pgn == 252 || pgn == 251 || pgn == 236);
    }

    // How many RTCM bytes a PGN 215 frame carries, received as a datagram of length datagramLen.
    // The length byte in the header is at most 255, and RTCM3 frames are longer - then AgOpenWeb may write
    // a wrapped value (e.g. 300 -> 44). The old code then took it as true and truncated the data.
    // Rules (the order matters):
    //  1. declared = actual (without CRC)                    -> actual
    //  2. actual > 255 and (actual & 0xFF) == declared       -> actual (wrapped length)
    //  3. declared > 0 and fits in the datagram              -> declared (behaviour from before the change)
    //  4. in all other cases                                 -> everything from byte 5 to the end (as before)
    inline int pgn215PayloadLen(int datagramLen, uint8_t declared) {
        if (datagramLen <= 5) return 0;
        int withoutCrc = datagramLen - 6;
        if (declared > 0 && withoutCrc == declared) return withoutCrc;
        if (withoutCrc > 255 && (withoutCrc & 0xFF) == declared) return withoutCrc;
        if (declared > 0 && datagramLen >= 5 + declared) return declared;
        return datagramLen - 5;
    }

    // PGN 254 command status byte (data byte 7): whether it means "steering enabled". Mask: 0x01 (default; AgOpenWeb develop sends exactly 1 = steer, 0 = not),
    // 0x04 (another/older code), 0x05 (both). Any other mask value = 0x01.
    inline uint8_t sanitizeEngageMask(uint8_t m) { return (m == 0x04 || m == 0x05) ? m : 0x01; }
    inline bool engagedFromStatus(uint8_t status, uint8_t mask) { return (status & sanitizeEngageMask(mask)) != 0; }

    // Scan / subnet reply (PGN 203, 13 bytes): module id, its IP at [5..8] and at [9..11] the NETWORK PREFIX (the first three
    // octets of the address, e.g. 192.168.5) - NOT the netmask. AgOpenWeb shows it in its "Change Module Subnet" boxes
    // and would broadcast it back as the new subnet (AgOpenWeb PgnBuilder.TryParseScanReply). Returns the frame length.
    inline int buildScanReply(uint8_t* out, uint8_t srcId, const uint8_t ip[4]) {
        const uint8_t f[13] = {0x80, 0x81, srcId, 203, 7, ip[0], ip[1], ip[2], ip[3], ip[0], ip[1], ip[2], 0};
        for (int i = 0; i < 13; i++) out[i] = f[i];
        uint8_t c = 0; for (int i = 2; i < 12; i++) c = (uint8_t)(c + out[i]);
        out[12] = c;
        return 13;
    }

}  // namespace PgnFrame
