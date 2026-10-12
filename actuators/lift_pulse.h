#pragma once
// ======================================================================
// actuators/lift_pulse.h
// EN: Timed-pulse logic for the hydraulic lift output: on a "raise" command the output is on for raiseTime seconds
//     (the value AgOpenWeb sends in PGN 238) and then released; it is dropped at once when the command goes away.
//     Pure C++ - host-tested.
// PL: Logika impulsu czasowego dla wyjścia podnośnika hydraulicznego: po komendzie „podnieś” wyjście jest włączone
//     przez raiseTime sekund (wartość z PGN 238 od AgOpenWeb), potem puszczane; od razu opada, gdy komenda znika.
//     Czysty C++ - testowane na komputerze.
// Tests: tests/test_lift_pulse.cpp (host, plain C++).
// Used by: actuators/actuators.h
// ======================================================================
#include <cstdint>

class LiftPulse {
public:
    // cmdRaised: AgOpenWeb wants the implement raised (and the command is fresh).
    // raiseSec: raiseTime from PGN 238; 0 = unknown/none -> the output simply follows the command (level mode).
    // Returns true while the output should be ON.
    bool update(bool cmdRaised, unsigned long nowMs, uint8_t raiseSec) {
        if (raiseSec == 0) {                       // no pulse length known: plain level, forget any pulse
            pulsing = false; prevCmd = cmdRaised;
            return cmdRaised;
        }
        if (!cmdRaised) {
            pulsing = false;                       // command gone: release at once
        } else if (!prevCmd) {
            pulsing = true; startMs = nowMs;       // rising edge: start the pulse
        }
        if (pulsing && (unsigned long)(nowMs - startMs) >= (unsigned long)raiseSec * 1000UL) pulsing = false;
        prevCmd = cmdRaised;
        return pulsing;
    }
    void reset() { pulsing = false; prevCmd = false; startMs = 0; }

private:
    bool pulsing = false, prevCmd = false;
    unsigned long startMs = 0;
};
