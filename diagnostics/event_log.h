#pragma once
// ======================================================================
// diagnostics/event_log.h
// EN: RAM ring log of events shown as markers on the charts; optional receiver for persistent storage.
// PL: Dziennik zdarzeń w RAM (znaczniki na wykresach); opcjonalny odbiornik do zapisu trwałego.
// Tests: tests/test_event_log.cpp (host, plain C++).
// Used by: diagnostics/event_detector.h, web/web_server.h
// ======================================================================
// Event log for chart markers (vertical lines): module restart, fix quality change, NTRIP loss/return, WiFi,
// AgOpenWeb, steering, motor protection trips. Pure C++ (no Arduino) - host-tested (tests/test_event_log.cpp).
// Written from the main loop (core 1, once per second), read from the web server (core 0) - a lock injected via setLock() sits between them.
// Memory in RAM: a module restart clears the log (same as the chart history in PSRAM); the browser keeps its own copy and has
// a restart marker from the new start (EV_BOOT).
#include <cstdint>

class EventLog {
public:
    enum Type : uint8_t {
        EV_BOOT = 1,          // arg: restart reason (esp_reset_reason) or 100 = after OTA, 101 = after WiFi loss
        EV_FIX = 2,           // arg: fix level as in the fixLvl series (0 none, 1 GPS, 2 DGPS, 3 FLOAT, 4 FIX)
        EV_NTRIP = 3,         // arg: 1 = correction stream flowing, 0 = lost (own NTRIP client)
        EV_NET = 4,           // arg: 1 = network (WiFi/Ethernet) connected, 0 = lost
        EV_AGIO = 5,          // arg: 1 = AgOpenWeb (selected sender) is talking, 0 = went silent
        EV_AGIO_SWITCH = 6,   // the selected AgOpenWeb sender changed
        EV_STEER = 7,         // arg: 1 = steering enabled, 0 = disabled
        EV_MOTOR = 8          // arg: 1 = overcurrent latch, 2 = runaway detector
    };
    struct Event { uint32_t ms; uint8_t type; int16_t arg; };
    static const uint8_t MAX = 64;

    typedef void (*LockFn)();
    static void setLock(LockFn l, LockFn u) { lock_ = l; unlock_ = u; }
    // Optional receiver of every new event (persistent record in RTC memory - persist_log.h). Called under the lock, must be short.
    typedef void (*SinkFn)(uint8_t type, int16_t arg, uint32_t ms);
    static void setSink(SinkFn s) { sink_ = s; }

    static void add(uint8_t type, int16_t arg, uint32_t nowMs) {
        if (lock_) lock_();
        ring[head] = Event{nowMs, type, arg};
        head = (uint8_t)((head + 1) % MAX);
        if (cnt < MAX) cnt++;
        total++;
        if (sink_) sink_(type, arg, nowMs);
        if (unlock_) unlock_();
    }
    // Copy to out (oldest to newest), at most max items; returns the count.
    static uint8_t snapshot(Event* out, uint8_t max) {
        if (lock_) lock_();
        uint8_t n = cnt < max ? cnt : max;
        uint8_t first = (uint8_t)((head + MAX - n) % MAX);
        for (uint8_t i = 0; i < n; i++) out[i] = ring[(first + i) % MAX];
        if (unlock_) unlock_();
        return n;
    }
    static uint32_t totalAdded() { return total; }
    static void reset() { head = 0; cnt = 0; total = 0; }

private:
    static Event ring[MAX];
    static uint8_t head, cnt;
    static uint32_t total;
    static LockFn lock_, unlock_;
    static SinkFn sink_;
};
EventLog::Event EventLog::ring[EventLog::MAX];
uint8_t EventLog::head = 0;
uint8_t EventLog::cnt = 0;
uint32_t EventLog::total = 0;
EventLog::LockFn EventLog::lock_ = nullptr;
EventLog::LockFn EventLog::unlock_ = nullptr;
EventLog::SinkFn EventLog::sink_ = nullptr;
