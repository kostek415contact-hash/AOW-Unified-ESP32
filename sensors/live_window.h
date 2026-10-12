#pragma once
// ======================================================================
// sensors/live_window.h
// EN: How old a GPS position or IMU sample may be and still be sent to AgOpenWeb as "live".
// PL: Jak stara może być pozycja GPS lub próbka IMU, żeby nadal iść do AgOpenWeb jako „aktualna”.
// Tests: tests/test_live_window.cpp (host, plain C++).
// Used by: sensors/gps_universal.h, sensors/imu_sensor.h
// ======================================================================
#include <cmath>
#include <cstdint>

// $PANDA goes out every 100 ms. With a fixed 2 s freshness window a dead GPS cable (or a stopped receiver) kept PANDA alive for up
// to 2 s with the frozen position, fix quality 4 and the old speed - at 8 km/h that is up to 4.4 m of error while AgOpenWeb still
// showed RTK FIX. The window now follows the real sentence rate: 5 sentence periods, but never below 500 ms (a short stall of the
// main loop must not flap the fix) and never above 2 s (a slow 1 Hz receiver). The display paths (LED, panel, NTRIP) keep the long
// 2 s window on purpose - they must not flicker.
namespace LiveWindow {
    static const uint32_t MIN_MS = 500;
    static const uint32_t MAX_MS = 2000;
    static const uint32_t UNKNOWN_MS = 1000;   // the rate was not measured yet
    static const uint32_t IMU_MS = 500;        // BNO085 UART-RVC delivers 100 Hz: 500 ms = 50 missed samples

    // intervalMs = measured period between two GGA sentences (<= 0 or NaN = not measured yet).
    inline uint32_t forGps(float intervalMs) {
        if (!(intervalMs > 0.0f) || !std::isfinite(intervalMs)) return UNKNOWN_MS;
        float w = intervalMs * 5.0f;
        if (w < (float)MIN_MS) return MIN_MS;
        if (w > (float)MAX_MS) return MAX_MS;
        return (uint32_t)w;
    }
}
