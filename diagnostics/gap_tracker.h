#pragma once
// ======================================================================
// diagnostics/gap_tracker.h
// EN: Measures gaps between consecutive events (GGA, $PANDA attempts, successful sends) to locate the cause of
//     AgOpenWeb disconnects.
// PL: Mierzy przerwy między kolejnymi zdarzeniami (GGA, próby i udane wysyłki $PANDA), by ustalić przyczynę
//     rozłączeń w AgOpenWeb.
// Tests: tests/test_gap_tracker.cpp (host, plain C++).
// Used by: connectivity/pgn_messages.h, sensors/gps_universal.h
// ======================================================================
// Measuring gaps between successive events (e.g. successive $PANDA sends, successive GGA sentences from the receiver). Pure C++
// (no Arduino), host-tested (tests/test_gap_tracker.cpp). Written by the main loop, read by the web server:
// 32-bit counters, atomic read on ESP32 - enough for diagnostics.
//
// WHY: AgOpenWeb disconnects the GPS module when the gap between position frames exceeds about 2 s. To find the CAUSE, we measure
// three things: gaps between GGA from the receiver (is data flowing at all), between ATTEMPTS to send $PANDA (is the loop stalled)
// and between SUCCESSFUL sends (is UDP failing). The difference between them points to the culprit.
#include <cstdint>

class GapTracker {
public:
    static constexpr uint32_t BIG_MS  = 1000;   // "big" gap
    static constexpr uint32_t HUGE_MS = 2000;   // the gap at which AgOpenWeb considers the module disconnected

    // Call on every event. The first call only sets the reference point.
    void note(unsigned long nowMs) {
        if (last != 0) {
            unsigned long gap = nowMs - last;
            lastGap_ = (uint32_t)gap;
            if (gap > max_) max_ = (uint32_t)gap;
            if (gap > BIG_MS)  { nBig_++; lastBigGap_ = (uint32_t)gap; lastBigAt_ = nowMs ? nowMs : 1; }
            if (gap > HUGE_MS) nHuge_++;
        }
        last = nowMs ? nowMs : 1;
    }
    uint32_t maxMs() const { return max_; }
    uint32_t lastMs() const { return lastGap_; }
    uint32_t countOver1s() const { return nBig_; }
    uint32_t countOver2s() const { return nHuge_; }
    uint32_t lastBigGapMs() const { return lastBigGap_; }            // length of the last gap > 1 s (0 = none yet)
    long msSinceLastBigGap(unsigned long nowMs) const { return lastBigAt_ == 0 ? -1L : (long)(nowMs - lastBigAt_); }
    void reset() { *this = GapTracker(); }
private:
    unsigned long last = 0, lastBigAt_ = 0;
    uint32_t max_ = 0, lastGap_ = 0, lastBigGap_ = 0, nBig_ = 0, nHuge_ = 0;
};

// Three counters for diagnosing GPS -> AgOpenWeb connectivity.
class GapStats {
public:
    static GapTracker gga;        // successive GGA sentences from the GPS receiver
    static GapTracker pandaTry;   // successive ATTEMPTS to send $PANDA (schedule in the main loop)
    static GapTracker pandaOk;    // successive SUCCESSFUL $PANDA sends (endPacket() = true)
};
GapTracker GapStats::gga;
GapTracker GapStats::pandaTry;
GapTracker GapStats::pandaOk;
