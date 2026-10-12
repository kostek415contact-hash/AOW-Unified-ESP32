#pragma once
// ======================================================================
// sensors/gps_age.h
// EN: Age of differential corrections (GGA field 13); an empty field means 'none', not 0 s.
// PL: Wiek poprawek różnicowych (GGA, pole 13); puste pole znaczy „brak”, a nie 0 s.
// Tests: tests/test_gps_age.cpp (host, plain C++).
// Used by: sensors/gps_universal.h
// ======================================================================
// Age of the differential corrections (GGA, field 13: "age of differential data" in seconds). Pure C++ (no Arduino) - host-tested
// (tests/test_gps_age.cpp).
//
// Why a separate file: an empty field means "the receiver is not applying corrections / does not report the age", NOT "0 s" (fresh corrections).
// Until now both cases gave 0.0 s and the panel looked as if the corrections were perfect, though there were none. $PANDA (AgOpenWeb)
// still gets 0 when there is no age - that stays unchanged in GpsHandler; here only what the panel shows matters.
#include <cstdlib>
#include <cmath>

namespace GpsAge {

// Level to show in the panel: 0 = normal, 1 = warning, 2 = bad, 3 = no data (neutral color).
enum Level : int { LVL_OK = 0, LVL_WARN = 1, LVL_BAD = 2, LVL_NONE = 3 };

// VRS sends corrections about every 1 s, so an age above a dozen or so seconds is a sign that the RTCM stream is stalling.
// Quectel (LG290P RTK application note): the differential age should be below 120 s - above that the receiver loses RTK.
static const float WARN_S = 10.0f;
static const float BAD_S  = 120.0f;

// GGA field -> seconds. Returns false for an empty, non-numeric, negative, infinite or absurdly large field
// (then *out is not changed).
inline bool parse(const char* field, float* out) {
    if (!field || !field[0]) return false;
    char* end = nullptr;
    float v = strtof(field, &end);
    if (end == field) return false;                  // no digits
    while (*end == ' ') end++;
    if (*end != '\0') return false;                  // garbage after the number (e.g. "12x")
    if (!std::isfinite(v) || v < 0.0f || v > 9999.0f) return false;
    *out = v;
    return true;
}

// fixQuality per GGA: 0 none, 1 plain GPS (no corrections), 2 DGPS, 4 RTK FIX, 5 RTK FLOAT.
// The age makes sense only when the receiver actually applies corrections (2/4/5).
inline int level(bool valid, float ageS, int fixQuality) {
    if (fixQuality != 2 && fixQuality != 4 && fixQuality != 5) return LVL_NONE;
    if (!valid) return LVL_NONE;                     // we do not know - we pretend neither normal nor a fault
    if (ageS > BAD_S)  return LVL_BAD;
    if (ageS > WARN_S) return LVL_WARN;
    return LVL_OK;
}

} // namespace GpsAge
