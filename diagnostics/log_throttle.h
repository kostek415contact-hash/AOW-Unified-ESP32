#pragma once
// ======================================================================
// diagnostics/log_throttle.h
// EN: Collapses repeating events (e.g. AgIO Hello) into one summary log line per period.
// PL: Zbija powtarzające się zdarzenia (np. Hello z AgIO) w jedną linię podsumowania na okres.
// Tests: tests/test_log_throttle.cpp (host, plain C++).
// Used by: connectivity/pgn_messages.h, connectivity/pgn_receivers.h
// ======================================================================
// Collects repeating events (e.g. Hello from AgIO ~9 times/s) into one log line per period.
// It reports the first event immediately (so you can see that something started arriving), sums the following ones and reports
// once per `periodMs` with the count and the window time. Pure C++ - host-tested.
#include <cstdint>

class CountingThrottle {
public:
    explicit CountingThrottle(unsigned long periodMs = 5000) : period(periodMs) {}
    // Call on every event. Returns true when a line should be PRINTED; then `count` = how many events
    // since the previous print (for the first: 1), `spanMs` = how many ms elapsed (for the first: 0).
    bool note(unsigned long nowMs, uint32_t& count, unsigned long& spanMs) {
        pending++;
        if (!started) {
            started = true; windowStart = nowMs; count = pending; spanMs = 0; pending = 0;
            return true;
        }
        if (nowMs - windowStart >= period) {
            count = pending; spanMs = nowMs - windowStart; windowStart = nowMs; pending = 0;
            return true;
        }
        return false;
    }
private:
    unsigned long period;
    bool started = false;
    unsigned long windowStart = 0;
    uint32_t pending = 0;
};

// The same as CountingThrottle, but for streams of frames with GAPS (commands from AgOpenWeb): besides the count it collects
// the smallest, average and largest gap between frames. The first frame is reported immediately (without gaps).
struct IntervalReport {
    uint32_t count;        // how many frames since the previous report (for the first: 1)
    unsigned long spanMs;  // how many ms elapsed (for the first: 0)
    uint32_t minDt, avgDt, maxDt;   // gaps between frames [ms]; 0 if there were none
};

class IntervalThrottle {
public:
    explicit IntervalThrottle(unsigned long periodMs = 5000) : period(periodMs) {}
    // nowMs = millis(), dtMs = the gap since the PREVIOUS frame (0 = first frame, not counted in the statistics).
    bool note(unsigned long nowMs, uint32_t dtMs, IntervalReport& r) {
        pending++;
        if (dtMs > 0) {
            if (nDt == 0 || dtMs < mn) mn = dtMs;
            if (dtMs > mx) mx = dtMs;
            sum += dtMs; nDt++;
        }
        if (!started) {
            started = true; windowStart = nowMs;
            r = {pending, 0, mn, nDt ? (uint32_t)(sum / nDt) : 0, mx};
            reset();
            return true;
        }
        if (nowMs - windowStart >= period) {
            r = {pending, nowMs - windowStart, mn, nDt ? (uint32_t)(sum / nDt) : 0, mx};
            windowStart = nowMs;
            reset();
            return true;
        }
        return false;
    }
private:
    void reset() { pending = 0; nDt = 0; sum = 0; mn = 0; mx = 0; }
    unsigned long period;
    bool started = false;
    unsigned long windowStart = 0;
    uint32_t pending = 0, nDt = 0, mn = 0, mx = 0;
    uint64_t sum = 0;
};
