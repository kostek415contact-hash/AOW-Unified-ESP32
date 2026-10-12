// Testy normalizacji kursu do $PANDA (sensors/heading_math.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_heading_math.cpp -o /tmp/test_heading_math && /tmp/test_heading_math
#include <cstdio>
#include <cmath>
#include <limits>
#include "../sensors/heading_math.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)
static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

int main() {
    using namespace HeadingMath;
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Inf = std::numeric_limits<float>::infinity();

    CHECK(near(normalize360(0.0f), 0.0f));
    CHECK(near(normalize360(90.0f), 90.0f));
    CHECK(near(normalize360(359.9f), 359.9f));
    CHECK(near(normalize360(360.0f), 0.0f));
    CHECK(near(normalize360(-90.0f), 270.0f));
    CHECK(near(normalize360(-180.0f), 180.0f));
    CHECK(near(normalize360(-0.1f), 359.9f));
    CHECK(near(normalize360(725.0f), 5.0f));
    CHECK(near(normalize360(-725.0f), 355.0f));
    CHECK(normalize360(NaN) < 0.0f && normalize360(Inf) < 0.0f && normalize360(-Inf) < 0.0f);

    CHECK(toX10(0.0f) == 0);
    CHECK(toX10(90.0f) == 900);
    CHECK(toX10(-90.0f) == 2700);                 // wczesniej poszloby -900
    CHECK(toX10(-180.0f) == 1800);
    CHECK(toX10(179.99f) == 1799);
    CHECK(toX10(360.0f) == 0);                    // wczesniej 3600
    CHECK(toX10(359.99f) == 3599);
    CHECK(toX10(-0.01f) == 3599);
    CHECK(toX10(-1e-9f) >= 0 && toX10(-1e-9f) <= 3599);
    CHECK(toX10(NaN) == NO_HEADING_X10);
    CHECK(toX10(Inf) == NO_HEADING_X10);

    bool range = true, symmetric = true;
    for (float d = -1000.0f; d < 1000.0f; d += 0.173f) {
        int v = toX10(d);
        if (v < 0 || v > 3599) range = false;
        float a = normalize360(d), b = normalize360(d + 360.0f);
        if (std::fabs(a - b) > 0.01f && std::fabs(std::fabs(a - b) - 360.0f) > 0.01f) symmetric = false;
    }
    CHECK(range);
    CHECK(symmetric);

    // kurs GPS do PANDA tylko w ruchu (AgOpenWeb traktuje kazda wartosc != 65535 jako zywy IMU)
    CHECK(HeadingMath::courseX10(123.4f, 5.0f, true) == 1234);
    CHECK(HeadingMath::courseX10(123.4f, 0.0f, true) == HeadingMath::NO_HEADING_X10);
    CHECK(HeadingMath::courseX10(123.4f, 5.0f, false) == HeadingMath::NO_HEADING_X10);
    CHECK(HeadingMath::courseX10(360.0f, 2.0f, true) == 0);
    CHECK(HeadingMath::courseX10(10.0f, NAN, true) == HeadingMath::NO_HEADING_X10);
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
