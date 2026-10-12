#pragma once
// ======================================================================
// diagnostics/lifetime_stats.h
// EN: Counters that outlive restarts (stored in the module memory): starts, abnormal restarts, motor running time, protection trips.
//     Pure bookkeeping - the caller reads and writes the storage. Host-tested.
// PL: Liczniki przeżywające restarty (w pamięci modułu): starty, nienormalne restarty, czas pracy silnika, zadziałania ochron.
//     Czysta księgowość - odczyt i zapis pamięci robi wywołujący. Testowane na komputerze.
// Tests: tests/test_lifetime_stats.cpp (host, plain C++).
// Used by: connectivity/network_manager.h, web/runtime_settings.h, web/web_server.h
// ======================================================================
#include <cstdint>

class LifetimeStats {
public:
    struct Data {
        uint32_t boots = 0;           // starts of the module
        uint32_t abnormalBoots = 0;   // restarts after a crash, watchdog or power dip
        uint32_t motorRunSec = 0;     // seconds with the motor driven
        uint32_t runawayTrips = 0;    // runaway detector trips
        uint32_t kickoutTrips = 0;    // current protection / kickout trips
        uint32_t deadmanTrips = 0;    // dead-man timer cuts (main loop hung)
    };
    static constexpr uint32_t TRIP_MIN_GAP_MS = 20000;  // trips are saved at once, but not more often than this
    static constexpr uint32_t SAVE_EVERY_MS = 300000;   // periodic save while running (also only when nothing risky is going on)

    void load(const Data& d) { d_ = d; dirty_ = false; trip_ = false; everSaved_ = false; savedAt_ = 0; }
    const Data& data() const { return d_; }

    // Once per start: abnormal = the reset reason was a crash / watchdog / brownout.
    void noteBoot(bool abnormal) { d_.boots++; if (abnormal) d_.abnormalBoots++; dirty_ = true; urgent_ = true; }

    // Every loop pass. The three *Now arguments are the SESSION counters (they start at 0 on every boot); only their growth is added.
    void tick(uint32_t nowMs, bool motorDriven, uint32_t runawayNow, uint32_t kickoutNow, uint32_t deadmanNow) {
        if (motorDriven && lastMs_ != 0) {
            const uint32_t dt = nowMs - lastMs_;
            if (dt < 5000) { runMsAcc_ += dt; }          // a long gap (stalled loop) is not motor time
            while (runMsAcc_ >= 1000) { runMsAcc_ -= 1000; d_.motorRunSec++; dirty_ = true; }
        }
        lastMs_ = nowMs ? nowMs : 1;
        if (runawayNow > seenRunaway_) { d_.runawayTrips += runawayNow - seenRunaway_; dirty_ = trip_ = true; }
        if (kickoutNow > seenKickout_) { d_.kickoutTrips += kickoutNow - seenKickout_; dirty_ = trip_ = true; }
        if (deadmanNow > seenDeadman_) { d_.deadmanTrips += deadmanNow - seenDeadman_; dirty_ = trip_ = true; }
        seenRunaway_ = runawayNow; seenKickout_ = kickoutNow; seenDeadman_ = deadmanNow;
    }

    // Whether the caller should write the counters now. safeToWrite = a flash write is acceptable (steering not engaged).
    bool shouldSave(uint32_t nowMs, bool safeToWrite) const {
        if (!dirty_ || !safeToWrite) return false;
        if (urgent_) return true;                                                  // a start: once per boot
        if (trip_ && (!everSaved_ || (nowMs - savedAt_) >= TRIP_MIN_GAP_MS)) return true;   // a flapping trip must not wear the flash
        return (nowMs - savedAt_) >= SAVE_EVERY_MS;
    }
    void markSaved(uint32_t nowMs) { dirty_ = false; urgent_ = false; trip_ = false; everSaved_ = true; savedAt_ = nowMs; }
    bool dirty() const { return dirty_; }

private:
    Data d_;
    bool dirty_ = false, urgent_ = false, trip_ = false, everSaved_ = false;
    uint32_t savedAt_ = 0, lastMs_ = 0, runMsAcc_ = 0;
    uint32_t seenRunaway_ = 0, seenKickout_ = 0, seenDeadman_ = 0;
};
