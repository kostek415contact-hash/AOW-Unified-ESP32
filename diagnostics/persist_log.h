#pragma once
// ======================================================================
// diagnostics/persist_log.h
// EN: Recent events kept in RTC memory so the panel can show what happened right before a restart (survives
//     soft reset, watchdog, panic, brownout).
// PL: Ostatnie zdarzenia w pamięci RTC, by panel pokazał, co działo się tuż przed restartem (przeżywa restart
//     programowy, watchdog, panikę, brownout).
// Tests: tests/test_persist_log.cpp (host, plain C++).
// Used by: connectivity/network_manager.h, diagnostics/event_sampler.h, web/web_server.h
// ======================================================================
// A record of the latest events in RTC memory (RTC_NOINIT_ATTR) that SURVIVES a software restart, watchdog, panic and brownout
// (it does not survive a power cycle). After startup the panel can show WHAT happened right before the restart - the ordinary event
// log (event_log.h) is in normal RAM and disappears with the restart. Zero flash wear. Pure C++ - host-tested
// (tests/test_persist_log.cpp); the memory block is supplied by the caller (network_manager.h).
// Write: only from the main loop (EventLog::add and nmEventTick). Reading the "previous run" - from the web server (data does not change after begin()).
#include <cstdint>
#include <cstring>

class PersistLog {
public:
    static const uint8_t MAX = 32;
    static const uint32_t MAGIC = 0x50455632u;   // "PEV2"
    struct Entry { uint32_t ms; uint8_t type; int16_t arg; };
    struct Blob {
        uint32_t magic;
        uint32_t crc;
        uint32_t aliveMs;     // last known moment of operation (millis of this run), refreshed every ~1 s
        uint32_t utcSec;      // last known UTC time from GPS (seconds since 1970) at aliveMs; 0 = unknown
        uint32_t session;     // run number since the last power loss
        uint8_t head, cnt;
        Entry ring[MAX];
    };

    // Checks the block from the previous run (copies the events to normal memory) and starts a new record.
    static void begin(Blob* b) {
        blob_ = b;
        prevCnt_ = 0; prevEndMs_ = 0; prevSession_ = 0; prevUtc_ = 0;
        uint32_t next = 1;
        if (b->magic == MAGIC && b->head < MAX && b->cnt <= MAX && b->crc == crcOf(*b)) {
            const uint8_t first = (uint8_t)((b->head + MAX - b->cnt) % MAX);
            for (uint8_t i = 0; i < b->cnt; i++) prev_[i] = b->ring[(first + i) % MAX];
            prevCnt_ = b->cnt;
            prevEndMs_ = b->aliveMs;
            for (uint8_t i = 0; i < prevCnt_; i++) if (prev_[i].ms > prevEndMs_) prevEndMs_ = prev_[i].ms;
            prevSession_ = b->session;
            prevUtc_ = b->utcSec;
            next = b->session + 1;
        }
        std::memset(b, 0, sizeof(*b));
        b->magic = MAGIC;
        b->session = next;
        seal(*b);
    }
    static void add(uint8_t type, int16_t arg, uint32_t nowMs) {
        if (!blob_) return;
        blob_->ring[blob_->head] = Entry{nowMs, type, arg};
        blob_->head = (uint8_t)((blob_->head + 1) % MAX);
        if (blob_->cnt < MAX) blob_->cnt++;
        blob_->aliveMs = nowMs;
        seal(*blob_);
    }
    // utcSec: current UTC time from GPS (0 = unknown - the last known value from this run is kept).
    static void alive(uint32_t nowMs, uint32_t utcSec = 0) {
        if (!blob_) return;
        blob_->aliveMs = nowMs;
        if (utcSec != 0) blob_->utcSec = utcSec;
        seal(*blob_);
    }

    // Previous run (oldest first): how many events, when it stopped working, which run it was.
    static uint8_t prevCount() { return prevCnt_; }
    static const Entry& prevAt(uint8_t i) { return prev_[i]; }
    static uint32_t prevEndMs() { return prevEndMs_; }
    static uint32_t prevSession() { return prevSession_; }
    static uint32_t prevUtc() { return prevUtc_; }       // UTC time (s since 1970) of the end of the previous run, or 0
    static uint32_t session() { return blob_ ? blob_->session : 0; }

    static uint32_t crcOf(const Blob& b) {
        uint32_t c = 0xFFFFFFFFu;
        feed(c, b.magic); feed(c, b.aliveMs); feed(c, b.utcSec); feed(c, b.session); feed(c, b.head); feed(c, b.cnt);
        for (uint8_t i = 0; i < MAX; i++) { feed(c, b.ring[i].ms); feed(c, b.ring[i].type); feed(c, (uint32_t)(uint16_t)b.ring[i].arg); }
        return ~c;
    }

private:
    static void seal(Blob& b) { b.crc = crcOf(b); }
    static void feed(uint32_t& c, uint32_t v) {
        for (int k = 0; k < 4; k++) {
            c ^= (v >> (8 * k)) & 0xFFu;
            for (int j = 0; j < 8; j++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    static Blob* blob_;
    static Entry prev_[MAX];
    static uint8_t prevCnt_;
    static uint32_t prevEndMs_, prevSession_, prevUtc_;
};
PersistLog::Blob* PersistLog::blob_ = nullptr;
PersistLog::Entry PersistLog::prev_[PersistLog::MAX];
uint8_t PersistLog::prevCnt_ = 0;
uint32_t PersistLog::prevEndMs_ = 0;
uint32_t PersistLog::prevSession_ = 0;
uint32_t PersistLog::prevUtc_ = 0;
