#pragma once
// ======================================================================
// sensors/heading_math.h
// EN: Normalises heading for $PANDA: degrees x10 in 0-3599, 65535 = none.
// PL: Normalizuje kurs do $PANDA: stopnie x10 w zakresie 0-3599, 65535 = brak.
// Tests: tests/test_heading_math.cpp (host, plain C++).
// Used by: sensors/gps_universal.h
// ======================================================================
// Heading in $PANDA: degrees x10 in the range 0-3599 (0-359.9 deg), 65535 = none. Pure math (tests: tests/test_heading_math.cpp).
#include <cmath>

namespace HeadingMath {

constexpr int NO_HEADING_X10 = 65535;

// Any angle in degrees (also negative, e.g. -180...180 from the IMU, or 360.0 from GPS) -> 0 <= x < 360.
// NaN/Inf => -1 (the caller decides what to do).
inline float normalize360(float deg) {
    if (!std::isfinite(deg)) return -1.0f;
    float r = std::fmod(deg, 360.0f);
    if (r < 0.0f) r += 360.0f;
    if (r >= 360.0f) r = 0.0f;   // an fmod result close to -0 after adding 360 in float can give 360.0
    return r;
}

// Degrees -> integer x10 in 0..3599; infinite/NaN value => 65535 (no heading).
inline int toX10(float deg) {
    float n = normalize360(deg);
    if (n < 0.0f) return NO_HEADING_X10;
    int v = (int)(n * 10.0f);    // truncation, as in the existing code
    if (v > 3599) v = 0;
    return v;
}

// GPS course for the $PANDA heading field when there is no IMU: AgOpenWeb treats ANY value other than 65535 there as a live IMU
// heading and fuses it. A course is only meaningful while moving; at a standstill it is stale or frozen, so: none (65535).
constexpr float MIN_COURSE_KNOTS = 0.5f;   // about 0.9 km/h
inline int courseX10(float courseDeg, float speedKnots, bool fresh) {
    if (!fresh || !std::isfinite(speedKnots) || speedKnots < MIN_COURSE_KNOTS) return NO_HEADING_X10;
    return toX10(courseDeg);
}

}  // namespace HeadingMath
