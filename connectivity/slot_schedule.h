#pragma once
// ======================================================================
// connectivity/slot_schedule.h
// EN: Which heartbeat PGN is sent in which 5 ms slot of the 40-slot cycle (a pure function, host-tested).
// PL: Który heartbeat PGN idzie w którym 5 ms slocie 40-slotowego cyklu (czysta funkcja, testowana na komputerze).
// Tests: tests/test_slot_schedule.cpp (host, plain C++).
// Used by: connectivity/pgn_messages.h
// ======================================================================
#include <cstdint>

// The schedule of nmProcessHeartbeats(). It lives in a pure function so a test can count the slots per PGN: before, GPS status was
// written as "slots 4 and 24" but slot 24 also matches the AutoSteer rule (24 % 8 == 0) which is tested first, so the GPS status
// went out every 200 ms instead of 100 ms and nobody noticed.
//
// One cycle = 40 slots x 5 ms = 200 ms:
//   AutoSteer  every 40 ms  -> 0, 8, 16, 24, 32
//   Machine    every 40 ms  -> 3, 11, 19, 27, 35   (shifted so it never shares a tick with AutoSteer)
//   Sensor     every 40 ms  -> 7, 15, 23, 31, 39   (PGN 250, kick-out)
//   PANDA      every 100 ms -> 1, 21
//   IMU        every 100 ms -> 2, 22
//   GPS status every 100 ms -> 5, 25
namespace SlotSchedule {
    static const uint8_t CYCLE = 40;
    enum Kind : uint8_t { K_NONE, K_STEER, K_MACHINE, K_PANDA, K_IMU, K_GPS_STATUS, K_SENSOR };

    inline Kind kindOf(uint8_t slot) {
        if (slot % 8 == 0) return K_STEER;
        if (slot % 8 == 3) return K_MACHINE;
        if (slot == 1 || slot == 21) return K_PANDA;
        if (slot == 2 || slot == 22) return K_IMU;
        if (slot == 5 || slot == 25) return K_GPS_STATUS;
        if (slot % 8 == 7) return K_SENSOR;
        return K_NONE;
    }
}
