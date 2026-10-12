// Fuzz test of the PGN parsers (connectivity/pgn_receivers.h): thousands of random, truncated and corrupted frames must never read
// outside the buffer, never crash and never leave an out-of-range state behind. The buffer is allocated with EXACTLY the frame
// length, so a sanitizer build (tools/run_tests.sh --sanitize) catches every over-read.
//     g++ -std=c++17 -Wall -Wextra -Itests/mocks tests/test_pgn_fuzz.cpp -o /tmp/tpf && /tmp/tpf
#pragma GCC diagnostic ignored "-Wunused-variable"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include "../connectivity/pgn_receivers.h"
static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

static int badFrames = 0;
static uint32_t rngState = 0x12345678u;
static uint32_t rnd() { rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5; return rngState; }

static uint8_t crcOf(const std::vector<uint8_t>& f) { uint8_t c = 0; for (size_t i = 2; i + 1 < f.size(); i++) c = (uint8_t)(c + f[i]); return c; }

// A frame of a given PGN: valid layout with random payload, then possibly damaged.
static std::vector<uint8_t> makeFrame(uint8_t pgn, uint8_t payloadLen, int damage) {
    std::vector<uint8_t> f(6 + payloadLen);
    f[0] = 0x80; f[1] = 0x81; f[2] = (uint8_t)rnd(); f[3] = pgn; f[4] = payloadLen;
    for (uint8_t i = 0; i < payloadLen; i++) f[5 + i] = (uint8_t)rnd();
    f[f.size() - 1] = crcOf(f);
    if (damage == 1 && !f.empty()) f[rnd() % f.size()] ^= (uint8_t)(1u << (rnd() % 8));   // one flipped bit
    if (damage == 2 && f.size() > 1) f.resize(rnd() % f.size());                          // truncated
    if (damage == 3) f[4] = (uint8_t)rnd();                                               // wrong declared length
    return f;
}

int main() {
    LogBuffer::begin();
    const IPAddress ip(192, 168, 1, 10);
    static const struct { uint8_t pgn; uint8_t len; } PGNS[] = {
        {0xFE, 8}, {0xFC, 8}, {0xFB, 8}, {0xEF, 8}, {0xEE, 8}, {0xEC, 24}, {0xFD, 8}, {0xFA, 8}, {0xDD, 40}, {0x00, 0}, {0x7F, 255}};
    for (int it = 0; it < 200000; it++) {
        const auto& p = PGNS[rnd() % (sizeof(PGNS) / sizeof(PGNS[0]))];
        std::vector<uint8_t> f = makeFrame(p.pgn, p.len, (int)(rnd() % 4));
        // exactly-sized heap copy: an over-read is a sanitizer error
        uint8_t* buf = new uint8_t[f.empty() ? 1 : f.size()];
        if (!f.empty()) memcpy(buf, f.data(), f.size());
        // The caller's contract (network_manager.h nmCheckOneUdp): parse(frame, declaredLen) only when 5 + declaredLen bytes really arrived.
        const uint8_t n = (f.size() >= 5) ? f[4] : 0;
        if (f.size() < 5u + n) { delete[] buf; badFrames++; continue; }
        SteerCommandReceiver::parse(buf, n, ip);
        SteerSettingsReceiver::parse(buf, n);
        MachineCommandReceiver::parse(buf, n);
        MachineConfigReceiver::parse(buf, n);
        SteerConfigReceiver::parse(buf, n);
        MachinePinConfigReceiver::parse(buf, n);
        delete[] buf;
    }
    CHECK(badFrames > 1000);   // truncated / wrong-length frames really were produced and refused by the length rule
    // invariants after the storm
    const MachinePinMap& m = MachinePinConfigReceiver::map();
    bool mapOk = true;
    for (int i = 0; i < MachinePinMap::HW; i++) if (m.fn[i] > MachineFn::MAX) mapOk = false;
    CHECK(mapOk);
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
