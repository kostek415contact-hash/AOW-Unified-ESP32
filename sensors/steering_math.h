#pragma once
// ======================================================================
// sensors/steering_math.h
// EN: Converts raw ADC counts to degrees; guards against a degenerate calibration (equal left/right points).
// PL: Przelicza surowy odczyt ADC na stopnie; chroni przed zdegenerowaną kalibracją (równe punkty lewo/prawo).
// Used by: sensors/steering_angle_sensor.h
// ======================================================================
// Converting the raw ADC reading of the angle sensor to degrees - pure C++ (no Arduino),
// host-tested. Split out because dividing by (RIGHT - LEFT) gave NaN/Inf when
// both calibration points had the same (or nearly the same) value, and such an angle ended up in AgOpenWeb
// (PGN253) and in the motor controller.
#include <cstdint>
#include <cmath>

namespace SteeringMath {

    // Minimum calibration span in ADC counts (1 count = about 0.19 mV at +-6.144 V,
    // so 200 = about 37 mV). Below this the angle cannot be determined reliably.
    static const int MIN_ADC_SPAN = 200;
    // A physically unrealistic angle (sensor disconnected/damaged) is clipped to sensible limits,
    // so absurd values are not sent to AgOpenWeb or to the motor.
    static const float ANGLE_CLAMP_DEG = 180.0f;

    inline bool spanOk(int16_t left, int16_t right) {
        int d = (int)right - (int)left;
        if (d < 0) d = -d;
        return d >= MIN_ADC_SPAN;
    }

    // Returns the angle in degrees. *valid = false (and angle 0.0) when the calibration is invalid:
    // LEFT-RIGHT span too small, or infinite/NaN end angles.
    // Plausibility of a raw reading: it must lie inside the calibrated LEFT..RIGHT range plus a margin (marginFrac of the span, but
    // at least MIN_MARGIN_ADC counts). A broken or loose sensor wire makes the ADC input float to some arbitrary value, often far
    // outside the range the wheel can really reach - the linear conversion would still turn it into a "valid" angle.
    static const int MIN_MARGIN_ADC = 300;   // about 56 mV at the 6.144 V range; above the noise and the mechanical play at the stops
    inline bool rawPlausible(int16_t raw, int16_t left, int16_t right, float marginFrac) {
        int lo = left < right ? left : right;
        int hi = left < right ? right : left;
        float m = (float)(hi - lo) * marginFrac;
        if (!(m >= (float)MIN_MARGIN_ADC)) m = (float)MIN_MARGIN_ADC;   // also catches NaN / negative fractions
        return (float)raw >= (float)lo - m && (float)raw <= (float)hi + m;
    }

    inline float rawToAngleDeg(int16_t raw, int16_t left, int16_t right,
                               float leftDeg, float rightDeg, bool* valid) {
        if (!spanOk(left, right) || !std::isfinite(leftDeg) || !std::isfinite(rightDeg)) {
            if (valid) *valid = false;
            return 0.0f;
        }
        float t = (float)((int)raw - (int)left) / (float)((int)right - (int)left);
        float a = leftDeg + t * (rightDeg - leftDeg);
        if (!std::isfinite(a)) {
            if (valid) *valid = false;
            return 0.0f;
        }
        if (a > ANGLE_CLAMP_DEG) a = ANGLE_CLAMP_DEG;
        if (a < -ANGLE_CLAMP_DEG) a = -ANGLE_CLAMP_DEG;
        if (valid) *valid = true;
        return a;
                               }

} // namespace SteeringMath
