#pragma once
// ======================================================================
// io/machine_outputs.h
// EN: Pure logic of the machine outputs driven from AgOpenWeb: which MCP23017 pin carries which function (PGN 236 pin
//     config), the hydraulic lift (hydLift 1 = lower, 2 = raise, timed pulses when AgOpenWeb gives raise / lower times), tram lines, geo-stop and the
//     "Invert relay" polarity (PGN 238 set0 bit 0). No hardware access - host-tested.
// PL: Czysta logika wyjść maszyny sterowanych z AgOpenWeb: która noga MCP23017 ma jaką funkcję (konfiguracja pinów
//     PGN 236), hydraulika (hydLift 1 = opuść, 2 = podnieś, impulsy czasowe, gdy AgOpenWeb podaje czasy), tory, geo-stop i polaryzacja
//     „Invert relay” (PGN 238 set0 bit 0). Bez dostępu do sprzętu - testowane na komputerze.
// Tests: tests/test_machine_outputs.cpp (host, plain C++).
// Used by: io/section_expander.h, actuators/actuators.h, connectivity/pgn_receivers.h
// ======================================================================
#include <cstdint>
#include <cstring>
#include "../actuators/lift_pulse.h"

// Function codes of one output pin, exactly as AgOpenWeb sends them in PGN 236 (PinFunction enum there).
namespace MachineFn {
    static const uint8_t NONE = 0;
    static const uint8_t SEC_FIRST = 1, SEC_LAST = 16;          // Sec1 .. Sec16
    static const uint8_t HYD_UP = 17, HYD_DOWN = 18;
    static const uint8_t TRAM_LEFT = 19, TRAM_RIGHT = 20;
    static const uint8_t GEO_STOP = 21;
    static const uint8_t MAX = 21;
}

// hydLift byte of PGN 239 as AgOpenWeb sends it: 0 = nothing, 1 = lower (tool inside the worked area), 2 = raise (headland).
namespace HydState { static const uint8_t NONE = 0, LOWER = 1, RAISE = 2; }

// "Pin 1..24" of the AgOpenWeb pin configuration. This board has 16 outputs (the MCP23017), pins 17..24 have no hardware.
struct MachinePinMap {
    static const uint8_t PINS = 24;        // entries in PGN 236
    static const uint8_t HW = 16;          // outputs that exist
    uint8_t fn[PINS];

    // Layout before AgOpenWeb sends its Pin Config: Sec1..Sec16 on pins 1..16, nothing else (no pin is special).
    void setDefault() {
        memset(fn, 0, sizeof(fn));
        for (uint8_t i = 0; i < HW; i++) fn[i] = (uint8_t)(i + 1);
    }
    MachinePinMap() { setDefault(); }

    // From the 24 data bytes of PGN 236. Anything outside the known function codes becomes "none".
    void load(const uint8_t* data24) {
        for (uint8_t i = 0; i < PINS; i++) fn[i] = (data24[i] <= MachineFn::MAX) ? data24[i] : (uint8_t)MachineFn::NONE;
    }
    bool operator==(const MachinePinMap& o) const { return memcmp(fn, o.fn, PINS) == 0; }
    bool operator!=(const MachinePinMap& o) const { return !(*this == o); }
    // true when one of the existing outputs carries this function
    bool has(uint8_t function) const {
        for (uint8_t i = 0; i < HW; i++) if (fn[i] == function) return true;
        return false;
    }
};

// Everything the outputs depend on, for one pass.
struct MachineInputs {
    bool     fresh = false;     // a PGN 239 arrived recently (a stale command must switch everything off)
    uint16_t sections = 0;      // bit n-1 = section n
    uint8_t  tram = 0;          // bit 0 = right wheel on the tram line, bit 1 = left wheel
    bool     geoStop = false;
    bool     hydUp = false;     // raise output wanted
    bool     hydDown = false;   // lower output wanted
};

// Logical state of every output pin (bit i = pin i+1 should be "on"), before the polarity is applied.
inline uint16_t machineLogicalMask(const MachinePinMap& map, const MachineInputs& in) {
    uint16_t out = 0;
    for (uint8_t pin = 0; pin < MachinePinMap::HW; pin++) {
        const uint8_t f = map.fn[pin];
        bool on = false;
        if (f >= MachineFn::SEC_FIRST && f <= MachineFn::SEC_LAST)  on = in.fresh && ((in.sections >> (f - 1)) & 1u);
        else if (f == MachineFn::HYD_UP)     on = in.hydUp;
        else if (f == MachineFn::HYD_DOWN)   on = in.hydDown;
        else if (f == MachineFn::TRAM_LEFT)  on = in.fresh && (in.tram & 0x02);
        else if (f == MachineFn::TRAM_RIGHT) on = in.fresh && (in.tram & 0x01);
        else if (f == MachineFn::GEO_STOP)   on = in.fresh && in.geoStop;
        if (on) out |= (uint16_t)(1u << pin);
    }
    return out;
}

// What goes to the chip. "Invert relay" = the relay board is active LOW: every output, including the unused ones, idles HIGH.
inline uint16_t machinePhysicalMask(uint16_t logical, bool invertRelay) { return invertRelay ? (uint16_t)~logical : logical; }

// The state to write while nothing is commanded (start-up, chip recovery): everything off.
inline uint16_t machineIdleMask(bool invertRelay) { return machinePhysicalMask(0, invertRelay); }

// Hydraulic lift: turns the hydLift byte into the raise / lower outputs.
//  - time 0 / not known yet: level mode, the output follows the command while it lasts;
//  - time > 0 (raiseTime / lowerTime from AgOpenWeb): on a new command the output is on for that many seconds, dropped at
//    once when the command goes away - the same behaviour as the reference AgOpenGPS machine module.
class HydLogic {
public:
    void update(uint8_t hydLift, bool fresh, unsigned long nowMs, uint8_t raiseSec, uint8_t lowerSec) {
        const bool wantUp = fresh && hydLift == HydState::RAISE;
        const bool wantDown = fresh && hydLift == HydState::LOWER;
        up_ = pulseUp_.update(wantUp, nowMs, raiseSec);
        down_ = pulseDown_.update(wantDown, nowMs, lowerSec);
    }
    void reset() { pulseUp_.reset(); pulseDown_.reset(); up_ = down_ = false; }
    bool up() const { return up_; }
    bool down() const { return down_; }
private:
    LiftPulse pulseUp_, pulseDown_;
    bool up_ = false, down_ = false;
};

// The lift as the outputs see it: automatic (AgOpenWeb command) or a manual override from the panel. Manual wins over everything and
// is NEVER the start state (a restart always returns to automatic). Pure logic - host-tested (tests/test_machine_outputs.cpp).
class HydControl {
public:
    void update(uint8_t hydLift, bool fresh, unsigned long nowMs, uint8_t raiseSec, uint8_t lowerSec) {
        if (manual_) { logic_.reset(); up_ = manualRaise_; down_ = !manualRaise_; return; }
        logic_.update(hydLift, fresh, nowMs, raiseSec, lowerSec);
        up_ = logic_.up(); down_ = logic_.down();
    }
    void idle() { logic_.reset(); up_ = down_ = false; }
    void setManual(bool raise) { manual_ = true; manualRaise_ = raise; }
    void setAuto() { manual_ = false; }
    bool up() const { return up_; }
    bool down() const { return down_; }
    bool manual() const { return manual_; }
    bool manualRaise() const { return manualRaise_; }
private:
    HydLogic logic_;
    bool manual_ = false, manualRaise_ = false, up_ = false, down_ = false;
};
