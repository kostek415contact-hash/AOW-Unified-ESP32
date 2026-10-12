#pragma once
// ======================================================================
// actuators/pwm_math.h
// EN: Pure PWM math of the motor P regulator (error -> signed PWM with limits and dead-zone lift). NaN/Inf-
//     safe.
// PL: Czysta matematyka PWM regulatora P silnika (błąd -> PWM ze znakiem, ograniczenia, martwa strefa). Odporna
//     na NaN/Inf.
// Tests: tests/test_pwm_math.cpp (host, plain C++).
// Used by: actuators/actuators.h
// ======================================================================
// Pure PWM math for the motor P controller (no Arduino - tested on a computer: tests/test_pwm_math.cpp).
#include <cmath>
#include <cstdint>

namespace PwmMath {

// Signed result: positive = toward the positive angle. Order: P -> limit to highPwm -> lift
// to minPwm (motor dead zone). Safety rules:
//  - infinite / NaN error or gain => 0 (casting NaN to int is undefined behaviour),
//  - minPwm greater than highPwm is clipped to highPwm (otherwise the lift would break the highPwm limit and the motor
//    would get more than AgOpenWeb allows),
//  - highPwm = 0 (no settings from AgOpenWeb) => always 0.
inline int compute(float error, float gainP, int highPwm, int minPwm) {
    if (!std::isfinite(error) || !std::isfinite(gainP)) return 0;
    if (highPwm < 0) highPwm = 0;
    if (highPwm > 255) highPwm = 255;
    if (minPwm < 0) minPwm = 0;
    if (minPwm > highPwm) minPwm = highPwm;

    float raw = error * gainP;
    if (raw > (float)highPwm) raw = (float)highPwm;
    if (raw < -(float)highPwm) raw = -(float)highPwm;
    int pwm = (int)raw;   // truncation toward zero, as before

    if (pwm > 0 && pwm < minPwm) pwm = minPwm;
    if (pwm < 0 && pwm > -minPwm) pwm = -minPwm;
    return pwm;
}

// The PWM magnitude for the AgOpenWeb steering chart (PGN 253 byte 12): |pwm| limited to 0..255.
inline uint8_t displayByte(int signedPwm) {
    const long m = signedPwm < 0 ? -(long)signedPwm : (long)signedPwm;
    return (uint8_t)(m > 255 ? 255 : m);
}

}  // namespace PwmMath
