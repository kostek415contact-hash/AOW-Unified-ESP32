#pragma once
// ======================================================================
// sensors/gps_sats.h
// EN: Satellite table from GSV (C/N0 per satellite/signal) and GSA (satellites used in the solution).
// PL: Tabela satelitów z GSV (C/N0 na satelitę i sygnał) oraz GSA (satelity użyte w rozwiązaniu).
// Tests: tests/test_gps_sats.cpp (host, plain C++).
// Used by: sensors/gps_universal.h
// ======================================================================
// Table of satellites from NMEA GSV messages (signal strength C/N0 in dB-Hz separately for each satellite and signal) and GSA (which
// satellites the receiver USES in the solution). Pure C++ (no Arduino) - host-tested (tests/test_gps_sats.cpp).
//
// Input: sentence fields SPLIT the way GpsHandler::splitFields() does, i.e. on commas AND on the asterisk - the last field is the
// checksum (already validated by the caller). fields[0] = "$GPGSV".
//
// Write (main loop, one task) and read (web server, another core/task) are separated SEQUENTIALLY (seqlock): the writer
// increments a counter before and after the change, the reader copies and repeats if the counter changed. No locks, so there is no risk of
// deadlock (also on the single-core ESP32-C6); after running out of attempts the reader takes the last copy instead of waiting.
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <atomic>

class GpsSats {
public:
    enum Sys : uint8_t { SYS_GPS = 0, SYS_GLO = 1, SYS_GAL = 2, SYS_BDS = 3, SYS_QZS = 4, SYS_OTH = 5, SYS_COUNT = 6 };
    static const int MAX = 128;                          // entries (satellite x signal): 4 systems x about 12 satellites x 2-3 signals
    static const uint8_t SNR_NONE = 255;                 // satellite visible in GSV, but with no signal (empty SNR field)
    // An entry not refreshed for longer than STALE_MS disappears; the "used" information is valid for USED_VALID_MS after the last GSA of a given system.
    static const unsigned long STALE_MS = 4000;
    static const unsigned long USED_VALID_MS = 4000;
    static const unsigned long USED_CYCLE_MS = 500;      // successive GSAs of the same system within this window add up (>12 satellites)

    struct Sat { uint8_t sys; uint8_t prn; uint8_t elev; uint16_t az; uint8_t snr; uint8_t sig; bool used; };

    // --- counter of received sentences (diagnostics: does the receiver send GSV/GSA at all) ---
    static uint32_t rxGsv, rxGsa;

    static void reset() {
        beginWrite();
        memset(tab, 0, sizeof(tab));
        memset(usedBits, 0, sizeof(usedBits));
        memset(usedStamp, 0, sizeof(usedStamp));
        rxGsv = rxGsa = 0;
        endWrite();
    }

    static int sysFromTalker(const char* header) {       // header = "GPGSV" (without '$')
        char a = header[0], b = header[1];
        if (a == 'G' && b == 'P') return SYS_GPS;
        if (a == 'G' && b == 'L') return SYS_GLO;
        if (a == 'G' && b == 'A') return SYS_GAL;
        if ((a == 'G' && b == 'B') || (a == 'B' && b == 'D')) return SYS_BDS;
        if (a == 'G' && b == 'Q') return SYS_QZS;
        return -1;
    }

    // $xxGSV,total,no,inView, {prn,elev,azim,snr} x1..4 [,signalId] *CS
    static void feedGsv(char* f[], uint8_t n, unsigned long now) {
        rxGsv++;
        if (n < 9) return;                               // header + 3 fields + at least one group + checksum
        int sys = sysFromTalker(f[0] + 1);
        if (sys < 0) return;
        uint8_t body = n - 1;                            // without the checksum field
        uint8_t data = body - 4;
        uint8_t groups = data / 4;
        uint8_t sig = (data % 4 == 1) ? hexDigit(f[body - 1]) : 0;   // NMEA 4.10+: signal id after the last group
        beginWrite();
        for (uint8_t g = 0; g < groups; g++) {
            uint8_t b = 4 + 4 * g;
            if (!*f[b]) continue;                        // empty group
            long prn = atol(f[b]);
            if (prn <= 0 || prn > 255) continue;
            long elev = *f[b + 1] ? atol(f[b + 1]) : 0;
            long az = *f[b + 2] ? atol(f[b + 2]) : 0;
            uint8_t snr = SNR_NONE;
            if (*f[b + 3]) { long v = atol(f[b + 3]); snr = (v < 0) ? 0 : (v > 99 ? 99 : (uint8_t)v); }
            upsert((uint8_t)sys, (uint8_t)prn, (uint8_t)(elev < 0 ? 0 : (elev > 90 ? 90 : elev)),
                   (uint16_t)(az < 0 ? 0 : (az > 359 ? 359 : az)), snr, sig, now);
        }
        endWrite();
    }

    // $xxGSA,mode,fixType, 12 x prn, PDOP,HDOP,VDOP [,systemId] *CS
    static void feedGsa(char* f[], uint8_t n, unsigned long now) {
        rxGsa++;
        if (n < 19) return;
        int sys = -1;
        if (n >= 20) {                                   // NMEA 4.10+: system id (1 GPS, 2 GLONASS, 3 Galileo, 4 BeiDou, 5 QZSS)
            long id = atol(f[18]);
            if (id >= 1 && id <= 5) sys = (int)id - 1;
        }
        if (sys < 0) sys = sysFromTalker(f[0] + 1);      // without a system id: from the talker (GN is not enough)
        if (sys < 0) return;
        beginWrite();
        if (usedStamp[sys] == 0 || now - usedStamp[sys] > USED_CYCLE_MS) memset(usedBits[sys], 0, sizeof(usedBits[sys]));   // new cycle
        for (uint8_t i = 3; i <= 14; i++) {
            if (!*f[i]) continue;
            long prn = atol(f[i]);
            if (prn > 0 && prn < 256) usedBits[sys][prn >> 3] |= (uint8_t)(1u << (prn & 7));
        }
        usedStamp[sys] = now ? now : 1;
        endWrite();
    }

    // A copy of the live entries for the web server. usedKnown = whether any system has a fresh GSA (otherwise "used" is unknown).
    static int snapshot(Sat* out, int maxOut, unsigned long now, bool* usedKnown) {
        Entry copy[MAX]; uint8_t ub[SYS_COUNT][32]; unsigned long us[SYS_COUNT];
        for (int attempt = 0; attempt < 8; attempt++) {
            uint32_t s1 = seq.load(std::memory_order_acquire);
            memcpy(copy, tab, sizeof(copy)); memcpy(ub, usedBits, sizeof(ub)); memcpy(us, usedStamp, sizeof(us));
            std::atomic_thread_fence(std::memory_order_acquire);
            if (testHook) { void (*h)() = testHook; testHook = nullptr; h(); }     // tests only: a write during copying
            if (!(s1 & 1) && seq.load(std::memory_order_relaxed) == s1) break;     // consistent copy; otherwise retry (after 8 attempts we take the last one)
        }
        bool known = false;
        for (int s = 0; s < SYS_COUNT; s++) if (us[s] != 0 && now - us[s] <= USED_VALID_MS) known = true;
        if (usedKnown) *usedKnown = known;
        int k = 0;
        for (int i = 0; i < MAX && k < maxOut; i++) {
            const Entry& e = copy[i];
            if (!e.valid || now - e.ms > STALE_MS) continue;
            bool u = us[e.sys] != 0 && now - us[e.sys] <= USED_VALID_MS && (ub[e.sys][e.prn >> 3] & (1u << (e.prn & 7)));
            out[k].sys = e.sys; out[k].prn = e.prn; out[k].elev = e.elev; out[k].az = e.az; out[k].snr = e.snr; out[k].sig = e.sig; out[k].used = u;
            k++;
        }
        return k;
    }

    static std::atomic<uint32_t> seq;                    // public only for tests (simulating a write in progress)
    static void (*testHook)();                           // tests only; always nullptr in production

private:
    struct Entry { unsigned long ms; uint16_t az; uint8_t sys, prn, elev, snr, sig, valid; };
    static Entry tab[MAX];
    static uint8_t usedBits[SYS_COUNT][32];              // 256 bits per system: whether the satellite with a given PRN is in the solution
    static unsigned long usedStamp[SYS_COUNT];

    static void beginWrite() { seq.fetch_add(1, std::memory_order_release); }
    static void endWrite() { seq.fetch_add(1, std::memory_order_release); }

    static uint8_t hexDigit(const char* s) {
        char c = s[0];
        if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
        if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
        if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
        return 0;
    }

    static void upsert(uint8_t sys, uint8_t prn, uint8_t elev, uint16_t az, uint8_t snr, uint8_t sig, unsigned long now) {
        int slot = -1, oldest = 0;
        for (int i = 0; i < MAX; i++) {
            Entry& e = tab[i];
            if (e.valid && e.sys == sys && e.prn == prn && e.sig == sig) { slot = i; break; }
            if (!e.valid) { if (slot < 0) slot = i; }                                  // first free
            else if ((now - e.ms) > (now - tab[oldest].ms)) oldest = i;                 // and when none is free: the oldest (expired ones are the oldest)
        }
        if (slot < 0) slot = oldest;                     // table full: overwrite the oldest
        Entry& d = tab[slot];
        d.ms = now ? now : 1; d.az = az; d.sys = sys; d.prn = prn; d.elev = elev; d.snr = snr; d.sig = sig; d.valid = 1;
    }
};
uint32_t GpsSats::rxGsv = 0;
uint32_t GpsSats::rxGsa = 0;
std::atomic<uint32_t> GpsSats::seq(0);
void (*GpsSats::testHook)() = nullptr;
GpsSats::Entry GpsSats::tab[GpsSats::MAX];
uint8_t GpsSats::usedBits[GpsSats::SYS_COUNT][32];
unsigned long GpsSats::usedStamp[GpsSats::SYS_COUNT];
