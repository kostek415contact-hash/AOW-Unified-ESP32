#pragma once
// ======================================================================
// diagnostics/event_detector.h
// EN: Detects chart events (fix quality change, NTRIP/WiFi/AgOpenWeb loss, engagement, protection trips) from
//     values sampled once per second.
// PL: Wykrywa zdarzenia na wykresach (zmiana fixa, utrata NTRIP/WiFi/AgOpenWeb, prowadzenie, zadziałanie
//     ochron) z wartości próbkowanych raz na sekundę.
// Used by: diagnostics/event_sampler.h
// Depends on: event_log.h
// ======================================================================
// Detecting events from values checked once per second. Pure C++ - tested (tests/test_event_log.cpp).
// The first call only remembers the initial state (no events) - a restart is described separately by EV_BOOT.
#include "event_log.h"

struct EventInputs {
    int fixLevel;             // 0..4 as in the fixLvl series; -1 = unknown (skipped)
    bool ntripEnabled;        // own NTRIP client enabled in settings
    bool ntripStreaming;      // correction stream flowing
    bool netUp;               // WiFi or Ethernet connected
    bool agioActive;          // the selected AgOpenWeb sender is talking (a frame within 5 s)
    uint32_t agioSwitches;    // counter of selected-sender changes
    bool steerOn;             // steering enabled (AgOpenWeb is steering)
    bool currentLatched;      // overcurrent latch
    uint32_t runawayTrips;    // counter of runaway detector trips
};

class EventDetector {
public:
    static const uint8_t FIX_CONFIRM = 2;   // a fix change must persist for this many consecutive samples (seconds) to count as an event

    void update(uint32_t now, const EventInputs& in) {
        if (!init_) {
            init_ = true;
            fix_ = in.fixLevel; pendFix_ = in.fixLevel; pendCnt_ = 0;
            ntripOn_ = in.ntripEnabled; ntripStr_ = in.ntripStreaming; net_ = in.netUp; agio_ = in.agioActive;
            sw_ = in.agioSwitches; steer_ = in.steerOn; latched_ = in.currentLatched; trips_ = in.runawayTrips;
            return;
        }
        // fix quality: changes only after FIX_CONFIRM consecutive samples (FLOAT/FIX flicker every second does not flood the log)
        if (in.fixLevel >= 0) {
            if (in.fixLevel == fix_) { pendCnt_ = 0; pendFix_ = fix_; }
            else {
                if (in.fixLevel == pendFix_) pendCnt_++; else { pendFix_ = in.fixLevel; pendCnt_ = 1; }
                if (pendCnt_ >= FIX_CONFIRM) { fix_ = in.fixLevel; pendCnt_ = 0; EventLog::add(EventLog::EV_FIX, (int16_t)fix_, now); }
            }
        }
        // NTRIP: only when our own client is enabled (enabling/disabling in settings is not a loss)
        if (in.ntripEnabled && ntripOn_ && in.ntripStreaming != ntripStr_) EventLog::add(EventLog::EV_NTRIP, in.ntripStreaming ? 1 : 0, now);
        ntripOn_ = in.ntripEnabled; ntripStr_ = in.ntripStreaming;
        if (in.netUp != net_) { EventLog::add(EventLog::EV_NET, in.netUp ? 1 : 0, now); net_ = in.netUp; }
        if (in.agioActive != agio_) { EventLog::add(EventLog::EV_AGIO, in.agioActive ? 1 : 0, now); agio_ = in.agioActive; }
        if (in.agioSwitches != sw_) { EventLog::add(EventLog::EV_AGIO_SWITCH, 0, now); sw_ = in.agioSwitches; }
        if (in.steerOn != steer_) { EventLog::add(EventLog::EV_STEER, in.steerOn ? 1 : 0, now); steer_ = in.steerOn; }
        if (in.currentLatched && !latched_) EventLog::add(EventLog::EV_MOTOR, 1, now);
        latched_ = in.currentLatched;
        if (in.runawayTrips != trips_) { if (in.runawayTrips > trips_) EventLog::add(EventLog::EV_MOTOR, 2, now); trips_ = in.runawayTrips; }
    }

private:
    bool init_ = false;
    int fix_ = -1, pendFix_ = -1; uint8_t pendCnt_ = 0;
    bool ntripOn_ = false, ntripStr_ = false, net_ = false, agio_ = false, steer_ = false, latched_ = false;
    uint32_t sw_ = 0, trips_ = 0;
};
