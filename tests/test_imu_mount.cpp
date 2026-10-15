// Testy pelnej korekcji 3D montazu IMU (sensors/imu_mount.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_imu_mount.cpp -o /tmp/test_imu_mount && /tmp/test_imu_mount
//
// Prawda wzorcowa jest liczona NIEZALEZNIE od testowanego kodu: pojazd w swiecie ENU (wschod, polnoc, gora) z kursem h (zgodnie z
// ruchem wskazowek zegara od polnocy), pitchem p (nos w gore +) i rollem phi (prawa strona w dol +), czujnik przymocowany pod
// dowolnym stalym katem, czujnik raportuje katy Eulera Z-Y-X w jednej z 8 konwencji znakow. Sprawdzamy, czy po kalibracji
// (poziomo + przechyl) kurs/pitch/roll pojazdu zgadzaja sie z prawda przy losowych ustawieniach pojazdu.
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <random>
#include <string>
#include "../sensors/imu_mount.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

using namespace ImuMount;

struct D3 { double x, y, z; };
static D3 add(D3 a, D3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static D3 mul(D3 a, double k) { return {a.x * k, a.y * k, a.z * k}; }
static double dt(D3 a, D3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static D3 cr(D3 a, D3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static D3 rot(D3 v, D3 k, double t) {   // Rodrigues, k unit
    return add(add(mul(v, std::cos(t)), mul(cr(k, v), std::sin(t))), mul(k, dt(k, v) * (1 - std::cos(t))));
}
static const double D2R = M_PI / 180.0, R2D = 180.0 / M_PI;

struct Axes { D3 f, r, u; };            // vehicle axes in the world
static Axes vehicle(double hDeg, double pDeg, double phiDeg) {
    const double h = hDeg * D2R;
    Axes a{{std::sin(h), std::cos(h), 0}, {std::cos(h), -std::sin(h), 0}, {0, 0, 1}};
    const D3 r0 = a.r;
    a.f = rot(a.f, r0, pDeg * D2R); a.u = rot(a.u, r0, pDeg * D2R);          // nose up
    const D3 f1 = a.f;
    a.r = rot(a.r, f1, phiDeg * D2R); a.u = rot(a.u, f1, phiDeg * D2R);      // right side down
    return a;
}

// A sensor mounted at a fixed rotation (mx, my, mz = Euler angles in degrees of the sensor axes in the vehicle frame (f, r, u)).
struct Rig {
    double mx, my, mz;      // mounting
    int sp, sr, sa;         // the sensor's sign convention
    double psi0;            // the sensor's own yaw reference (arbitrary)
};
struct Reading { float yaw, pitch, roll; V3 accel; };

static Reading sense(const Rig& g, const Axes& v) {
    // sensor axes in vehicle coordinates (f, left, u): columns of Rz(mz) Ry(my) Rx(mx)
    const double cx = std::cos(g.mx * D2R), sx = std::sin(g.mx * D2R), cy = std::cos(g.my * D2R), sy = std::sin(g.my * D2R),
                 cz = std::cos(g.mz * D2R), sz = std::sin(g.mz * D2R);
    const double S[3][3] = {{cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx},
                            {sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx},
                            {-sy, cy * sx, cy * cx}};
    D3 sax[3];
    // (f, left, u) is a RIGHT-handed basis; (f, right, u) is not, so a mounting rotation must be written in the former
    for (int j = 0; j < 3; j++) sax[j] = add(add(mul(v.f, S[0][j]), mul(v.r, -S[1][j])), mul(v.u, S[2][j]));
    // the sensor's world: z = sa * Up, x = a horizontal vector turned by psi0, y = z x x
    const D3 up{0, 0, 1};
    const D3 zW = mul(up, g.sa);
    const D3 xW = {std::cos(g.psi0 * D2R), std::sin(g.psi0 * D2R), 0};
    const D3 yW = cr(zW, xW);
    const D3 W[3] = {xW, yW, zW};
    double R[3][3];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) R[i][j] = dt(W[i], sax[j]);
    const double a = std::atan2(R[1][0], R[0][0]);
    const double b = -std::asin(std::fmax(-1.0, std::fmin(1.0, R[2][0])));
    const double c = std::atan2(R[2][1], R[2][2]);
    Reading o;
    o.yaw = (float)(-g.sa * a * R2D);        // sy = -sa
    o.pitch = (float)(g.sp * b * R2D);
    o.roll = (float)(g.sr * c * R2D);
    o.accel = {(float)(9.81 * dt(up, sax[0])), (float)(9.81 * dt(up, sax[1])), (float)(9.81 * dt(up, sax[2]))};
    return o;
}

static double wrap(double d) { while (d > 180) d -= 360; while (d < -180) d += 360; return d; }
static Pose poseOf(const Reading& r) { return {r.pitch, r.roll, r.accel}; }

// Calibrates with the pose pair and checks the vehicle angles at random orientations. Returns the largest error in degrees,
// or -1 when the solver declined (and `res` says why).
static double calibrateAndCheck(const Rig& g, Tilt kind, Result& res, double noiseDeg = 0, double noiseAcc = 0, unsigned seed = 1) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd(0.0, 1.0);
    auto noisy = [&](Reading r) {
        r.pitch += (float)(nd(rng) * noiseDeg); r.roll += (float)(nd(rng) * noiseDeg); r.yaw += (float)(nd(rng) * noiseDeg);
        r.accel.x += (float)(nd(rng) * noiseAcc); r.accel.y += (float)(nd(rng) * noiseAcc); r.accel.z += (float)(nd(rng) * noiseAcc);
        return r;
    };
    const double h0 = 37.0;
    const Reading A = noisy(sense(g, vehicle(h0, 0, 0)));
    const Reading B = noisy(sense(g, kind == Tilt::SIDE_RIGHT_DOWN ? vehicle(h0, 0, 11.0) : vehicle(h0, 11.0, 0)));
    Mount m;
    res = solve(poseOf(A), poseOf(B), kind, m);
    if (res != Result::OK) return -1;
    double worst = 0, off = 1e9;
    std::uniform_real_distribution<double> uh(-180, 180), up(-20, 20);
    for (int i = 0; i < 400; i++) {
        const double h = uh(rng), p = up(rng), phi = up(rng);
        const Reading r = sense(g, vehicle(h, p, phi));
        if (std::fabs(r.pitch) > 85.0f) continue;                 // the sensor itself is near gimbal lock - not a fair test of the maths
        float ho, po, ro;
        apply(m, r.yaw, r.pitch, r.roll, ho, po, ro);
        const double exPitch = g.sp * (g.sa == 1 ? -p : p), exRoll = g.sr * phi;
        const double dh = wrap(ho - h);
        if (off > 1e8) off = dh;
        worst = std::fmax(worst, std::fabs(wrap(dh - off)));
        worst = std::fmax(worst, std::fabs(po - exPitch));
        worst = std::fmax(worst, std::fabs(ro - exRoll));
    }
    return worst;
}

int main() {
    // ----- 1. identity mount returns the raw angles (nothing changes for a flat, forward-facing board) -----
    {
        Mount id; id.valid = true;
        bool allSame = true;
        std::mt19937 rng(7);
        std::uniform_real_distribution<float> uy(-179, 179), ua(-80, 80);
        for (int sa = -1; sa <= 1; sa += 2) for (int sp = -1; sp <= 1; sp += 2) for (int sr = -1; sr <= 1; sr += 2) {
            id.sa = (int8_t)sa; id.sp = (int8_t)sp; id.sr = (int8_t)sr;
            for (int i = 0; i < 50; i++) {
                const float y = uy(rng), p = ua(rng), r = uy(rng);
                float ho, po, ro;
                apply(id, y, p, r, ho, po, ro);
                if (std::fabs(wrap(ho - y)) > 0.01 || std::fabs(po - p) > 0.01 || std::fabs(wrap(ro - r)) > 0.01) allSame = false;
            }
        }
        CHECK(allSame);
        CHECK(mountOk(id));
    }

    // ----- 2. the user's case: an enclosure on the right wall of the cab, the board in the plane of the wall and turned 45 deg
    //          inside it (its normal points sideways) -----
    int userOk = 0, userN = 0;
    for (int tiltKind = 0; tiltKind < 2; tiltKind++)
    for (int sa = -1; sa <= 1; sa += 2) for (int sp = -1; sp <= 1; sp += 2) for (int sr = -1; sr <= 1; sr += 2)
    for (int spin = -1; spin <= 1; spin += 2) {
        // sensor Z along the vehicle's lateral axis (plus or minus), sensor X and Y turned by 45 deg in the plane of the wall:
        // rotation about the vehicle's lateral axis = the Euler "y" angle of the mounting when X is the sensor's first axis.
        // Mount = Rx(90) then Ry(45*spin) in the (f,r,u) frame: Z -> lateral.
        Rig g{90.0, 45.0 * spin, 0.0, sp, sr, sa, 123.0};
        Result res;
        const double w = calibrateAndCheck(g, tiltKind ? Tilt::NOSE_UP : Tilt::SIDE_RIGHT_DOWN, res);
        userN++;
        // sideways tilt: must work. Nose-up tilt turns the board about its own normal here, which leaves the sign convention
        // undecidable - the solver must say so instead of guessing.
        if (tiltKind == 0 ? (res == Result::OK && w < 0.05) : (res == Result::CONVENTION_UNCLEAR)) userOk++;
        else std::printf("  uwaga: user-mount tilt=%d sa=%d sp=%d sr=%d spin=%d -> res=%d worst=%.3f\n", tiltKind, sa, sp, sr, spin, (int)res, w);
    }
    CHECK(userOk == userN);

    // ----- 3. board tilted 45 deg from vertical about the lateral axis, plus other tilted mountings -----
    {
        const double mounts[][3] = {{0, 45, 0}, {0, -45, 0}, {45, 0, 0}, {-45, 0, 0}, {30, 40, 20}, {-20, 35, 100}, {0, 90 - 20, 0}, {60, -30, 15}};
        int ok = 0, n = 0, wrong = 0;
        for (const auto& mt : mounts) for (int sa = -1; sa <= 1; sa += 2) for (int sp = -1; sp <= 1; sp += 2) for (int sr = -1; sr <= 1; sr += 2)
        for (int k = 0; k < 2; k++) {
            Rig g{mt[0], mt[1], mt[2], sp, sr, sa, -71.0};
            Result res;
            const double w = calibrateAndCheck(g, k ? Tilt::NOSE_UP : Tilt::SIDE_RIGHT_DOWN, res);
            n++;
            if (res == Result::OK) { if (w < 0.05) ok++; else { wrong++; std::printf("  zle: mount=%g,%g,%g sa=%d sp=%d sr=%d k=%d worst=%.3f\n", mt[0], mt[1], mt[2], sa, sp, sr, k, w); } }
        }
        CHECK(wrong == 0);          // when it says OK it is right
        CHECK(ok * 10 >= n * 6);    // and it says OK for most mountings
        std::printf("  (losowe montaze: %d/%d zaakceptowanych)\n", ok, n);
    }

    // ----- 4. small noise in the calibration readings: the vehicle angles stay within about a degree -----
    {
        double worstAll = 0; int fails = 0;
        for (unsigned seed = 1; seed <= 20; seed++) {
            Rig g{90.0, 45.0, 0.0, -1, 1, 1, 10.0};
            Result res;
            const double w = calibrateAndCheck(g, Tilt::SIDE_RIGHT_DOWN, res, 0.05, 0.05, seed);
            if (res != Result::OK) { fails++; continue; }
            worstAll = std::fmax(worstAll, w);
        }
        CHECK(fails == 0);
        CHECK(worstAll < 1.0);
        std::printf("  (szum 0.05 deg / 0.05 m/s2: najgorszy blad %.2f deg)\n", worstAll);
    }

    // ----- 5. a flat board cannot be told apart in its sign convention -> asks for the plain rotation instead -----
    {
        Rig g{0, 0, 0, 1, 1, 1, 0};
        Result res; calibrateAndCheck(g, Tilt::SIDE_RIGHT_DOWN, res);
        CHECK(res == Result::CONVENTION_UNCLEAR);
    }

    // ----- 6. refusals -----
    {
        Rig g{90.0, 45.0, 0.0, 1, 1, 1, 0};
        const Reading A = sense(g, vehicle(0, 0, 0)), B = sense(g, vehicle(0, 0, 11));
        Mount m;
        CHECK(solve(poseOf(A), poseOf(B), Tilt::SIDE_RIGHT_DOWN, m) == Result::OK && m.valid);
        Pose bad = poseOf(B); bad.accel = {0.1f, 0.2f, 0.1f};                       // no gravity (moving / wrong units)
        CHECK(solve(poseOf(A), bad, Tilt::SIDE_RIGHT_DOWN, m) == Result::NOT_GRAVITY && !m.valid);
        bad = poseOf(B); bad.accel = {0.0f, 0.0f, 98.1f};                           // mg instead of m/s2
        CHECK(solve(poseOf(A), bad, Tilt::SIDE_RIGHT_DOWN, m) == Result::NOT_GRAVITY);
        bad = poseOf(B); bad.pitchDeg = NAN;
        CHECK(solve(poseOf(A), bad, Tilt::SIDE_RIGHT_DOWN, m) == Result::BAD_INPUT);
        bad = poseOf(B); bad.pitchDeg = 80.0f;
        CHECK(solve(poseOf(A), bad, Tilt::SIDE_RIGHT_DOWN, m) == Result::GIMBAL);
        CHECK(solve(poseOf(A), poseOf(A), Tilt::SIDE_RIGHT_DOWN, m) == Result::TILT_TOO_SMALL);
        const Reading C = sense(g, vehicle(0, 0, 1.5));                              // 1.5 deg only
        CHECK(solve(poseOf(A), poseOf(C), Tilt::SIDE_RIGHT_DOWN, m) == Result::TILT_TOO_SMALL);
        CHECK(std::string(resultText(Result::TILT_TOO_SMALL)).size() > 10);
    }

    // ----- 7. text form -----
    {
        Rig g{90.0, 45.0, 0.0, -1, 1, -1, 33.0};
        const Reading A = sense(g, vehicle(0, 0, 0)), B = sense(g, vehicle(0, 0, 11));
        Mount m, back;
        CHECK(solve(poseOf(A), poseOf(B), Tilt::SIDE_RIGHT_DOWN, m) == Result::OK);
        char buf[200];
        CHECK(format(m, buf, sizeof(buf)) > 20);
        CHECK(parse(buf, back) && back.valid && back.sp == m.sp && back.sr == m.sr && back.sa == m.sa);
        bool same = true;
        for (int i = 0; i < 9; i++) if (std::fabs(back.m[i] - m.m[i]) > 1e-5f) same = false;
        CHECK(same);
        CHECK(parse("", back) && !back.valid);                                       // empty = off
        CHECK(!parse("garbage", back) && !back.valid);
        CHECK(!parse("2;1;1;1;1,0,0,0,1,0,0,0,1", back));                            // unknown version
        CHECK(!parse("1;1;1;1;1,0,0,0,1,0,0,0", back));                              // too few numbers
        CHECK(!parse("1;1;1;1;1,0,0,0,1,0,0,0,1,0", back));                          // trailing junk
        CHECK(!parse("1;2;1;1;1,0,0,0,1,0,0,0,1", back));                            // sign must be +-1
        CHECK(!parse("1;1;1;1;1,1,0,0,1,0,0,0,1", back));                            // not orthonormal
        CHECK(!parse("1;1;1;1;1,0,0,0,-1,0,0,0,1", back));                           // a reflection, not a rotation
        CHECK(!parse("1;1;1;1;nan,0,0,0,1,0,0,0,1", back));
        CHECK(parse("1;1;1;1;1,0,0,0,1,0,0,0,1", back) && back.valid);
        Mount none; CHECK(format(none, buf, sizeof(buf)) == 0 && buf[0] == 0);
    }

    // ----- 7b. the board's angles in the vehicle (shown in the panel) -----
    {
        Mount id; id.valid = true;
        float y, p, r;
        boardAngles(id, y, p, r);
        CHECK(std::fabs(y) < 0.01f && std::fabs(p) < 0.01f && std::fabs(r) < 0.01f);
        // a board turned by +30 deg in the plane of the vehicle: R(sensor->vehicle) = Rz(30), M = its transpose
        Mount k; k.valid = true;
        const float c = std::cos(30.0f * DEG2RAD), s = std::sin(30.0f * DEG2RAD);
        const float mm[9] = {c, s, 0, -s, c, 0, 0, 0, 1};
        for (int i = 0; i < 9; i++) k.m[i] = mm[i];
        boardAngles(k, y, p, r);
        CHECK(std::fabs(y - 30.0f) < 0.01f && std::fabs(p) < 0.01f && std::fabs(r) < 0.01f);
        // what the calibration finds equals the true mounting angles (sensor axes in the vehicle: Rz(yaw) Ry(pitch) Rx(roll))
        int checked = 0, good = 0;
        const double mounts[][3] = {{90, 45, 0}, {90, -45, 0}, {30, 40, 20}, {-20, 35, 100}, {60, -30, 15}, {45, 0, 0}, {0, 45, 0}, {80, 10, -170}};
        for (const auto& mt : mounts) for (int sp = -1; sp <= 1; sp += 2) for (int sr = -1; sr <= 1; sr += 2) {
            Rig g{mt[0], mt[1], mt[2], sp, sr, 1, 55.0};     // sa = +1: the mount's axes are (forward, left, up) like the rig's
            const Reading A = sense(g, vehicle(10, 0, 0)), B = sense(g, vehicle(10, 0, 12));
            Mount m;
            if (solve(poseOf(A), poseOf(B), Tilt::SIDE_RIGHT_DOWN, m) != Result::OK) continue;
            boardAngles(m, y, p, r);
            checked++;
            if (std::fabs(wrap(y - mt[2])) < 0.1 && std::fabs(p - mt[1]) < 0.1 && std::fabs(wrap(r - mt[0])) < 0.1) good++;
            else std::printf("  katy plytki: mount=%g,%g,%g -> yaw %.2f pitch %.2f roll %.2f\n", mt[0], mt[1], mt[2], y, p, r);
        }
        CHECK(checked >= 20 && good == checked);
    }

    // ----- 7c. angles typed by hand give the same mount as the calibration, and round-trip through boardAngles -----
    {
        int checked = 0, same = 0;
        const double mounts[][3] = {{90, 45, 0}, {30, 40, 20}, {-20, 35, 100}, {60, -30, 15}, {0, 45, 0}, {45, 0, 0}};
        for (const auto& mt : mounts) for (int sp = -1; sp <= 1; sp += 2) for (int sr = -1; sr <= 1; sr += 2) {
            Rig g{mt[0], mt[1], mt[2], sp, sr, 1, -20.0};
            const Reading A = sense(g, vehicle(10, 0, 0)), B = sense(g, vehicle(10, 0, 12));
            Mount solved, manual;
            if (solve(poseOf(A), poseOf(B), Tilt::SIDE_RIGHT_DOWN, solved) != Result::OK) continue;
            checked++;
            if (!fromBoardAngles((float)mt[2], (float)mt[1], (float)mt[0], solved.sp, solved.sr, solved.sa, manual)) continue;
            bool eq = true;
            for (int i = 0; i < 9; i++) if (std::fabs(manual.m[i] - solved.m[i]) > 2e-3f) eq = false;
            if (eq) same++;
        }
        CHECK(checked >= 12 && same == checked);
        float y, p, r; Mount k;
        CHECK(fromBoardAngles(20, 45, -30, 1, 1, 1, k) && k.valid);
        boardAngles(k, y, p, r);
        CHECK(std::fabs(y - 20) < 0.01f && std::fabs(p - 45) < 0.01f && std::fabs(r + 30) < 0.01f);
        CHECK(!fromBoardAngles(0, 80, 0, 1, 1, 1, k) && !k.valid);          // too close to standing on its edge
        CHECK(!fromBoardAngles(200, 10, 0, 1, 1, 1, k));                    // out of range
        CHECK(!fromBoardAngles(0, 100, 0, 1, 1, 1, k));
        CHECK(!fromBoardAngles(NAN, 10, 0, 1, 1, 1, k));
        CHECK(!fromBoardAngles(0, 10, 0, 0, 1, 1, k));                      // a sign must be +-1
    }

    // ----- 8. mountOk rejects broken matrices -----
    {
        Mount k; k.valid = true;
        CHECK(mountOk(k));
        k.m[4] = 0.5f; CHECK(!mountOk(k));
        k = Mount(); k.sp = 0; CHECK(!mountOk(k));
        k = Mount(); k.m[0] = NAN; CHECK(!mountOk(k));
    }

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
