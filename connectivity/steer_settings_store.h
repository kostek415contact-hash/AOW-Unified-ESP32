#pragma once
// ======================================================================
// connectivity/steer_settings_store.h
// EN: Decides WHEN the steering settings received from AgOpenWeb (PGN 252: gainP, highPWM, ...) are written to
//     non-volatile memory: only when they differ from what is already stored and have stopped changing for a moment
//     (so dragging a slider in AgOpenWeb does not wear the flash). Pure C++ - host-tested.
// PL: Decyduje, KIEDY ustawienia sterowania odebrane z AgOpenWeb (PGN 252: gainP, highPWM, ...) zapisać w pamięci
//     nieulotnej: tylko gdy różnią się od zapisanych i przestały się zmieniać na chwilę (żeby przesuwanie suwaka
//     w AgOpenWeb nie zużywało flasha). Czysty C++ - testowane na komputerze.
// Tests: tests/test_steer_settings_store.cpp (host, plain C++).
// Used by: connectivity/pgn_receivers.h, web/runtime_settings.h
// ======================================================================
#include <cstdint>
#include <cstring>

struct SteerSettingsData {
    uint8_t gainP = 0, highPWM = 0, lowPWM = 0, minPWM = 0, countsPerDeg = 0;
    int16_t steerOffset = 0;
    uint8_t ackermanFix = 0;
    bool operator==(const SteerSettingsData& o) const {
        return gainP == o.gainP && highPWM == o.highPWM && lowPWM == o.lowPWM && minPWM == o.minPWM &&
               countsPerDeg == o.countsPerDeg && steerOffset == o.steerOffset && ackermanFix == o.ackermanFix;
    }
    bool operator!=(const SteerSettingsData& o) const { return !(*this == o); }

    // Fixed 8-byte layout for storage (a version/length check on load protects against foreign data).
    static const size_t BYTES = 8;
    void pack(uint8_t* out) const {
        out[0] = gainP; out[1] = highPWM; out[2] = lowPWM; out[3] = minPWM; out[4] = countsPerDeg;
        out[5] = (uint8_t)((uint16_t)steerOffset & 0xFF); out[6] = (uint8_t)(((uint16_t)steerOffset >> 8) & 0xFF);
        out[7] = ackermanFix;
    }
    static SteerSettingsData unpack(const uint8_t* in) {
        SteerSettingsData d;
        d.gainP = in[0]; d.highPWM = in[1]; d.lowPWM = in[2]; d.minPWM = in[3]; d.countsPerDeg = in[4];
        d.steerOffset = (int16_t)((uint16_t)in[5] | ((uint16_t)in[6] << 8)); d.ackermanFix = in[7];
        return d;
    }
};

class SteerSettingsSaveGate {
public:
    explicit SteerSettingsSaveGate(unsigned long settleMs = 3000) : settle(settleMs) {}

    // What is already in non-volatile memory (call after loading it at start-up).
    void setStored(const SteerSettingsData& v) { stored = v; haveStored = true; havePending = false; }

    // Call for every received PGN 252 frame.
    void onReceived(const SteerSettingsData& v, unsigned long nowMs) {
        if (haveStored && v == stored) { havePending = false; return; }   // nothing new to keep
        if (!havePending || v != pending) { pending = v; changedMs = nowMs; havePending = true; }
    }

    // True when the pending values have been steady for `settleMs` - then write `pending()` and call markSaved().
    bool shouldSave(unsigned long nowMs) const { return havePending && (unsigned long)(nowMs - changedMs) >= settle; }
    const SteerSettingsData& pendingValues() const { return pending; }
    void markSaved() { stored = pending; haveStored = true; havePending = false; }
    bool hasPending() const { return havePending; }

private:
    unsigned long settle;
    SteerSettingsData stored, pending;
    bool haveStored = false, havePending = false;
    unsigned long changedMs = 0;
};
