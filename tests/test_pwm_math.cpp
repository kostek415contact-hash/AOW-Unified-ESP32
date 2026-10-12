// Testy regulatora P silnika (actuators/pwm_math.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_pwm_math.cpp -o /tmp/test_pwm_math && /tmp/test_pwm_math
#include <cstdio>
#include <cmath>
#include <limits>
#include <initializer_list>
#include "../actuators/pwm_math.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

int main() {
    using PwmMath::compute;
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Inf = std::numeric_limits<float>::infinity();

    // zwykle P
    CHECK(compute(2.0f, 10.0f, 200, 0) == 20);
    CHECK(compute(-2.0f, 10.0f, 200, 0) == -20);
    CHECK(compute(0.0f, 10.0f, 200, 30) == 0);            // zero bledu = zero PWM, nawet z minPwm
    // limit highPwm
    CHECK(compute(50.0f, 10.0f, 200, 0) == 200);
    CHECK(compute(-50.0f, 10.0f, 200, 0) == -200);
    // minPwm: maly niezerowy wynik podciagany, znak zachowany
    CHECK(compute(0.5f, 10.0f, 200, 40) == 40);            // 5 -> 40
    CHECK(compute(-0.5f, 10.0f, 200, 40) == -40);
    CHECK(compute(0.05f, 10.0f, 200, 40) == 0);            // 0,5 obciete do 0 -> bez podciagania (jak dotychczas)
    CHECK(compute(10.0f, 10.0f, 200, 40) == 100);          // powyzej minPwm bez zmian
    // minPwm wieksze od highPwm nie lamie limitu
    CHECK(compute(0.5f, 10.0f, 50, 120) == 50);
    CHECK(compute(-0.5f, 10.0f, 50, 120) == -50);
    CHECK(compute(50.0f, 10.0f, 50, 120) == 50);
    // brak ustawien z AgOpenWeb (highPwm = 0) => zawsze 0
    CHECK(compute(5.0f, 10.0f, 0, 0) == 0);
    CHECK(compute(5.0f, 10.0f, 0, 30) == 0);
    CHECK(compute(5.0f, 0.0f, 200, 0) == 0);               // gainP = 0
    // wartosci nieliczbowe
    CHECK(compute(NaN, 10.0f, 200, 30) == 0);
    CHECK(compute(Inf, 10.0f, 200, 30) == 0);
    CHECK(compute(-Inf, 10.0f, 200, 30) == 0);
    CHECK(compute(1.0f, NaN, 200, 30) == 0);
    // wartosci poza zakresem bajtu
    CHECK(compute(100.0f, 255.0f, 1000, 0) == 255);
    CHECK(compute(1.0f, 1.0f, -5, 0) == 0);
    CHECK(compute(1.0f, 10.0f, 200, -5) == 10);
    // symetria dla wielu bledow
    bool sym = true;
    for (float e = 0.0f; e < 30.0f; e += 0.37f) for (int mn : {0, 20, 90, 250}) {
        int p = compute(e, 7.0f, 120, mn), n = compute(-e, 7.0f, 120, mn);
        if (p != -n || p > 120 || p < 0) sym = false;
    }
    CHECK(sym);
    // wartosc PWM dla wykresu w AgOpenWeb (PGN 253, bajt 12): modul, 0..255
    CHECK(PwmMath::displayByte(0) == 0 && PwmMath::displayByte(77) == 77 && PwmMath::displayByte(-77) == 77);
    CHECK(PwmMath::displayByte(255) == 255 && PwmMath::displayByte(-255) == 255 && PwmMath::displayByte(1000) == 255 && PwmMath::displayByte(-1000) == 255);

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
