#pragma once
// ======================================================================
// sensors/imu_mount.h
// EN: Full 3D correction of the IMU mounting: the board may sit at ANY fixed angle in the vehicle (e.g. an enclosure on the
//     cab wall with the board tilted 45 deg). Rebuilds the sensor orientation from the UART-RVC yaw/pitch/roll, applies a fixed
//     board->vehicle rotation and returns the heading, pitch and roll of the VEHICLE. Calibration needs two poses (level, then
//     tilted) and also detects the sign convention of the sensor's angles from the accelerometer.
// PL: Pelna korekcja 3D montazu IMU: plytka moze byc pod DOWOLNYM stalym katem w pojezdzie (np. puszka na scianie kabiny z plytka
//     pochylona o 45 st.). Odtwarza orientacje czujnika z yaw/pitch/roll z UART-RVC, nakłada stały obrót plytka->pojazd i zwraca
//     kurs, pitch i roll POJAZDU. Kalibracja wymaga dwóch pozycji (poziomo, potem przechył) i sama wykrywa konwencję znaków kątów
//     czujnika z akcelerometru.
// Tests: tests/test_imu_mount.cpp (host, plain C++).
// Used by: sensors/imu_sensor.h, web/runtime_settings.h, web/web_server.h
// ======================================================================
//
// MODEL. The sensor reports Euler angles of the aerospace kind, Z-Y-X: its orientation is R = Rz(a) Ry(b) Rx(c) with
//   a = sy * yaw, b = sp * pitch, c = sr * roll, sy = -sa      (angles in radians)
// where sp, sr, sa are +1 / -1. sa says whether the sensor's "world Z" points UP (+1) or DOWN (-1); sy = -sa because the yaw
// of this project is measured clockwise (a right turn is positive - confirmed on the hardware). The third row of R is the world Z
// axis seen in the sensor axes: g = (-sin b, sin c cos b, cos c cos b); when the sensor stands still, the accelerometer reads
// (up * 9.81) in the same axes, so g = sa * accel/|accel|. Calibration tries the 8 sign combinations (sp, sr, sa) and keeps the one
// that explains the accelerometer in both poses; if two combinations explain it equally well the answer is "unclear" (this happens
// for a board lying flat - then the plain board rotation in config.h is the right tool).
//
// MOUNT. m[] (row-major 3x3) has the VEHICLE axes as columns, written in the sensor axes: column 0 = forward, column 2 = the
// vehicle's "Z" (the same sense as the sensor's world Z), column 1 = column2 x column0. Vehicle orientation: Rv = R * M. Its Euler angles
// (extracted with the same Z-Y-X formulas and given back their signs) are what a sensor mounted flat, X forward, would report.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../web/msg.h"

namespace ImuMount {

constexpr float DEG2RAD = 0.017453292519943295f;
constexpr float RAD2DEG = 57.29577951308232f;

struct V3 { float x, y, z; };

inline V3 cross(const V3& a, const V3& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float norm(const V3& a) { return std::sqrt(dot(a, a)); }
inline V3 scale(const V3& a, float k) { return { a.x * k, a.y * k, a.z * k }; }
inline bool finite3(const V3& a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }

struct Mount {
    bool valid = false;
    int8_t sp = 1, sr = 1, sa = 1;                 // sign conventions of the sensor (see MODEL)
    float m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};      // vehicle axes (columns) in sensor axes, row-major
};

// A reading taken while the vehicle stands still: the sensor's raw pitch/roll (deg) and accelerometer (m/s^2).
struct Pose { float pitchDeg, rollDeg; V3 accel; };

enum class Tilt : uint8_t { SIDE_RIGHT_DOWN = 0, NOSE_UP = 1 };
enum class Result : uint8_t { OK = 0, BAD_INPUT, NOT_GRAVITY, GIMBAL, TILT_TOO_SMALL, CONVENTION_UNCLEAR };

// Safe limits (documented in the panel text).
constexpr float GRAVITY_MIN = 7.5f, GRAVITY_MAX = 12.5f;   // |accel| of a resting sensor must look like 9.81 m/s^2
constexpr float MAX_ABS_PITCH_DEG = 75.0f;                 // near +-90 the Z-Y-X angles lose a degree of freedom (gimbal lock)
constexpr float MIN_TILT_DEG = 3.0f;                       // the second pose must differ from the first by at least this
constexpr float MAX_MATCH_ERR = 0.03f;                     // sum of squared differences (about 6 deg per pose)

// Third row of the rotation matrix: the world Z axis in the sensor axes.
inline V3 worldZ(float pitchDeg, float rollDeg, int sp, int sr) {
    const float b = sp * pitchDeg * DEG2RAD, c = sr * rollDeg * DEG2RAD;
    return { -std::sin(b), std::sin(c) * std::cos(b), std::cos(c) * std::cos(b) };
}

namespace detail {
inline void rotation(float yawDeg, float pitchDeg, float rollDeg, int sp, int sr, int sa, float R[9]) {
    const float a = -sa * yawDeg * DEG2RAD, b = sp * pitchDeg * DEG2RAD, c = sr * rollDeg * DEG2RAD;
    const float ca = std::cos(a), sa_ = std::sin(a), cb = std::cos(b), sb = std::sin(b), cc = std::cos(c), sc = std::sin(c);
    R[0] = cb * ca;  R[1] = sc * sb * ca - cc * sa_;  R[2] = cc * sb * ca + sc * sa_;
    R[3] = cb * sa_; R[4] = sc * sb * sa_ + cc * ca;  R[5] = cc * sb * sa_ - sc * ca;
    R[6] = -sb;      R[7] = sc * cb;                  R[8] = cc * cb;
}
}  // namespace detail

// True when the matrix is a proper rotation (orthonormal, det +1) and the signs are +-1.
inline bool mountOk(const Mount& k) {
    if (!(k.sp == 1 || k.sp == -1) || !(k.sr == 1 || k.sr == -1) || !(k.sa == 1 || k.sa == -1)) return false;
    for (int i = 0; i < 9; i++) if (!std::isfinite(k.m[i]) || std::fabs(k.m[i]) > 1.001f) return false;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        float d = 0;
        for (int r = 0; r < 3; r++) d += k.m[3 * r + i] * k.m[3 * r + j];
        if (std::fabs(d - (i == j ? 1.0f : 0.0f)) > 0.01f) return false;
    }
    const V3 c0{k.m[0], k.m[3], k.m[6]}, c1{k.m[1], k.m[4], k.m[7]}, c2{k.m[2], k.m[5], k.m[8]};
    return dot(cross(c0, c1), c2) > 0.99f;
}

// Heading, pitch and roll of the VEHICLE from the sensor's raw angles. Heading in -180..180 (same convention as the raw yaw).
inline void apply(const Mount& k, float yawDeg, float pitchDeg, float rollDeg, float& hdgOut, float& pitchOut, float& rollOut) {
    float R[9], Rv[9];
    detail::rotation(yawDeg, pitchDeg, rollDeg, k.sp, k.sr, k.sa, R);
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++)
        Rv[3 * i + j] = R[3 * i] * k.m[j] + R[3 * i + 1] * k.m[3 + j] + R[3 * i + 2] * k.m[6 + j];
    const float a = std::atan2(Rv[3], Rv[0]);
    float s = -Rv[6]; if (s > 1.0f) s = 1.0f; if (s < -1.0f) s = -1.0f;
    const float b = std::asin(s);
    const float c = std::atan2(Rv[7], Rv[8]);
    hdgOut = -k.sa * a * RAD2DEG;          // sy = -sa
    pitchOut = k.sp * b * RAD2DEG;
    rollOut = k.sr * c * RAD2DEG;
}

// Solves the mounting from two poses. A = vehicle level. B = tilted by `kind`: SIDE_RIGHT_DOWN (the right side lower, e.g. the right wheels on
// a plank) or NOSE_UP (the nose higher, e.g. the front wheels on a ramp). On OK `out` is filled and valid.
inline Result solve(const Pose& A, const Pose& B, Tilt kind, Mount& out) {
    out = Mount();
    const Pose* P[2] = {&A, &B};
    V3 acc[2];
    for (int i = 0; i < 2; i++) {
        if (!std::isfinite(P[i]->pitchDeg) || !std::isfinite(P[i]->rollDeg) || !finite3(P[i]->accel)) return Result::BAD_INPUT;
        const float n = norm(P[i]->accel);
        if (n < GRAVITY_MIN || n > GRAVITY_MAX) return Result::NOT_GRAVITY;
        if (std::fabs(P[i]->pitchDeg) > MAX_ABS_PITCH_DEG) return Result::GIMBAL;
        acc[i] = scale(P[i]->accel, 1.0f / n);
    }
    // The second pose must really differ from the first (measured with the accelerometer, which needs no convention).
    if (norm(cross(acc[1], acc[0])) < std::sin(MIN_TILT_DEG * DEG2RAD)) return Result::TILT_TOO_SMALL;
    float best = 1e9f, second = 1e9f;
    int bsp = 1, bsr = 1, bsa = 1;
    for (int sp = -1; sp <= 1; sp += 2) for (int sr = -1; sr <= 1; sr += 2) for (int sa = -1; sa <= 1; sa += 2) {
        float e = 0;
        for (int i = 0; i < 2; i++) {
            const V3 g = worldZ(P[i]->pitchDeg, P[i]->rollDeg, sp, sr);
            const V3 d{g.x - sa * acc[i].x, g.y - sa * acc[i].y, g.z - sa * acc[i].z};
            e += dot(d, d);
        }
        if (e < best) { second = best; best = e; bsp = sp; bsr = sr; bsa = sa; }
        else if (e < second) second = e;
    }
    if (best > MAX_MATCH_ERR) return Result::CONVENTION_UNCLEAR;                 // the angles do not agree with the accelerometer at all
    if (second < 0.1f && second < 4.0f * best + 0.01f) return Result::CONVENTION_UNCLEAR;   // a different sign set explains it about as well
    // "up" in the sensor axes in both poses (from the angles, the same source that is used at run time).
    const V3 gA = worldZ(A.pitchDeg, A.rollDeg, bsp, bsr), gB = worldZ(B.pitchDeg, B.rollDeg, bsp, bsr);
    const V3 upA = scale(gA, (float)bsa), upB = scale(gB, (float)bsa);
    V3 axis = cross(upB, upA);                      // along the tilt axis
    const float sinT = norm(axis);
    if (sinT < 0.5f * std::sin(MIN_TILT_DEG * DEG2RAD)) return Result::TILT_TOO_SMALL;
    axis = scale(axis, 1.0f / sinT);
    V3 f, r;
    if (kind == Tilt::SIDE_RIGHT_DOWN) { f = axis; r = cross(f, upA); }
    else { r = axis; f = cross(upA, r); }
    const V3 z = gA;                                // the vehicle's Z has the sense of the sensor's world Z
    const V3 y = cross(z, f);
    const float col0[3] = {f.x, f.y, f.z}, col1[3] = {y.x, y.y, y.z}, col2[3] = {z.x, z.y, z.z};
    for (int i = 0; i < 3; i++) { out.m[3 * i] = col0[i]; out.m[3 * i + 1] = col1[i]; out.m[3 * i + 2] = col2[i]; }
    out.sp = (int8_t)bsp; out.sr = (int8_t)bsr; out.sa = (int8_t)bsa;
    out.valid = mountOk(out);
    (void)r;
    return out.valid ? Result::OK : Result::BAD_INPUT;
}

// The board's own angles in the vehicle (what the calibration found): yaw (turned in the plane of the vehicle), pitch, roll in degrees,
// as Z-Y-X Euler angles of the rotation sensor->vehicle, written in the vehicle axes (forward, y, z) of the mount. Information for the panel.
inline void boardAngles(const Mount& k, float& yawDeg, float& pitchDeg, float& rollDeg) {
    // R = M^T maps sensor coordinates to vehicle coordinates: R[i][j] = M[j][i]
    const float r00 = k.m[0], r10 = k.m[1], r20 = k.m[2], r21 = k.m[5], r22 = k.m[8];
    float s = -r20; if (s > 1.0f) s = 1.0f; if (s < -1.0f) s = -1.0f;
    yawDeg = std::atan2(r10, r00) * RAD2DEG;
    pitchDeg = std::asin(s) * RAD2DEG;
    rollDeg = std::atan2(r21, r22) * RAD2DEG;
}

// ---------- text form (settings backup): "1;sp;sr;sa;m0,m1,...,m8" ----------
inline int format(const Mount& k, char* buf, size_t n) {
    if (!k.valid) { if (n) buf[0] = 0; return 0; }
    return std::snprintf(buf, n, "1;%d;%d;%d;%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f", (int)k.sp, (int)k.sr, (int)k.sa,
                         k.m[0], k.m[1], k.m[2], k.m[3], k.m[4], k.m[5], k.m[6], k.m[7], k.m[8]);
}
// Empty text = "off" (valid = false, returns true). Returns false for anything malformed or not a proper rotation.
inline bool parse(const char* s, Mount& out) {
    out = Mount();
    if (!s || !*s) return true;
    char* e = nullptr;
    if (std::strtol(s, &e, 10) != 1 || *e != ';') return false;
    s = e + 1;
    int sg[3];
    for (int i = 0; i < 3; i++) {
        sg[i] = (int)std::strtol(s, &e, 10);
        if (e == s || *e != ';') return false;
        s = e + 1;
    }
    float m[9];
    for (int i = 0; i < 9; i++) {
        m[i] = std::strtof(s, &e);
        if (e == s) return false;
        s = e;
        if (i < 8) { if (*s != ',') return false; s++; }
    }
    if (*s) return false;
    Mount k; k.sp = (int8_t)sg[0]; k.sr = (int8_t)sg[1]; k.sa = (int8_t)sg[2];
    std::memcpy(k.m, m, sizeof(m));
    if (!mountOk(k)) return false;
    k.valid = true; out = k;
    return true;
}

inline const char* resultText(Result r) {
    switch (r) {
        case Result::OK: return "OK";
        case Result::BAD_INPUT: return MSG("IMU mount: invalid input (no sensor reading?)");
        case Result::NOT_GRAVITY: return MSG("IMU mount: the accelerometer does not read about 9.8 m/s2 - hold the vehicle still");
        case Result::GIMBAL: return MSG("IMU mount: the board is too close to standing on its edge (pitch above 75 deg) - rotate it in its own plane");
        case Result::TILT_TOO_SMALL: return MSG("IMU mount: tilt too small - tilt the vehicle by at least 3 degrees");
        case Result::CONVENTION_UNCLEAR: return MSG("IMU mount: could not tell the sensor's angle convention - tilt the vehicle sideways (right side down) instead of nose up, or the board lies nearly flat (use the board rotation), or the readings are noisy");
    }
    return MSG("IMU mount: error");
}

}  // namespace ImuMount
