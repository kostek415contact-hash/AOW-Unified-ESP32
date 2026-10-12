#pragma once
// ======================================================================
// diagnostics/net_stats.h
// EN: Network counters for the panel (TX ok/fail, PGN table, CRC errors).
// PL: Liczniki komunikacji sieciowej dla panelu (TX ok/błąd, tabela PGN, błędy CRC).
// Used by: connectivity/network_manager.h, connectivity/pgn_messages.h, web/web_server.h
// ======================================================================
// Network communication counters for the panel (Diagnostics -> "Network" and "PGN" cards). Pure C++ (no
// Arduino) - host-tested. Written by the main loop (core 1), read by the web server (core 0):
// 32-bit counters are read atomically on ESP32; a PGN table entry may be "half-written"
// at a rare moment (pgn set, counter not yet) - this is irrelevant for diagnostics.
#include <cstdint>

class NetStats {
public:
    static const uint8_t MAX_PGN = 16;       // how many distinct PGN numbers we remember
    static uint32_t txOk;                    // successful UDP sends to AgOpenWeb
    static uint32_t txFail;                  // failed (endPacket() = false) - "TX FAIL"
    static unsigned long lastTxFailMs;       // 0 = none yet
    static uint32_t udpRebuilds;             // how many times the UDP sockets were rebuilt after the network returned
    static uint32_t badFrames;               // frames without the 0x80 0x81 header, too short, or with a mismatched length
    static uint32_t crcOk, crcBad;           // checksums of received PGN frames
    static uint32_t crcRejected;             // control frames (PGN 254/239/252) rejected because of a bad checksum
    static uint32_t otherPgn;                // frames with a PGN outside the first MAX_PGN distinct ones

    struct Entry { uint16_t pgn; uint32_t count; };
    static Entry table[MAX_PGN];
    static uint8_t used;

    static void noteTx(bool ok, unsigned long nowMs) {
        if (ok) { txOk++; } else { txFail++; lastTxFailMs = (nowMs == 0) ? 1 : nowMs; }
    }
    static void noteUdpRebuild() { udpRebuilds++; }
    static void noteBadFrame() { badFrames++; }
    // result of PgnFrame::crcCheck(): 1 = ok, 0 = mismatch, -1 = cannot be judged (skipped)
    static void noteCrc(int r) { if (r == 1) crcOk++; else if (r == 0) crcBad++; }
    static void noteRx(uint16_t pgn) {
        for (uint8_t i = 0; i < used; i++) {
            if (table[i].pgn == pgn) { table[i].count++; return; }
        }
        if (used < MAX_PGN) {
            table[used].pgn = pgn;
            table[used].count = 1;
            used++;                                // entry first, then publish (the reader sees 'used')
        } else {
            otherPgn++;
        }
    }
    // -1 if there was never a send error
    static long msSinceLastTxFail(unsigned long nowMs) { return lastTxFailMs == 0 ? -1L : (long)(nowMs - lastTxFailMs); }
};

uint32_t NetStats::txOk = 0;
uint32_t NetStats::txFail = 0;
unsigned long NetStats::lastTxFailMs = 0;
uint32_t NetStats::udpRebuilds = 0;
uint32_t NetStats::badFrames = 0;
uint32_t NetStats::otherPgn = 0;
uint32_t NetStats::crcOk = 0;
uint32_t NetStats::crcBad = 0;
uint32_t NetStats::crcRejected = 0;
NetStats::Entry NetStats::table[NetStats::MAX_PGN];
uint8_t NetStats::used = 0;
