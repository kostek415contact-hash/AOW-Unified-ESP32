// Testy sygnalu kickout do AgOpenWeb (connectivity/kickout_signal.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_kickout_signal.cpp -o /tmp/test_kickout_signal && /tmp/test_kickout_signal
#include <cstdio>
#include <cstdint>
#include <cmath>
#include "../connectivity/kickout_signal.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

int main() {
    // start: brak sygnalu, zero
    { KickoutSignal k; CHECK(!k.active(0)); CHECK(k.sensorByte(0) == 0); CHECK(k.count() == 0);
      CHECK(!k.active(100000)); }   // nigdy nie zadzialalo -> brak sygnalu, nawet przy now > HOLD_MS

    // obciazenie skaluje sie do maks. 150, nigdy nie dociaga do progu kickout
    { KickoutSignal k; k.update(1000, false, 0.5f); CHECK(k.sensorByte(1000) == 75);
      k.update(1040, false, 1.0f); CHECK(k.sensorByte(1040) == 150);
      k.update(1080, false, 7.0f); CHECK(k.sensorByte(1080) == 150);        // obciecie do 1
      k.update(1120, false, -3.0f); CHECK(k.sensorByte(1120) == 0);
      k.update(1160, false, NAN); CHECK(k.sensorByte(1160) == 0);
      CHECK(KickoutSignal::LOAD_MAX_BYTE * 100 / 255 < 80); }               // normalna praca < zalecany prog 80%

    // zadzialanie: od razu 255, niezaleznie od obciazenia
    { KickoutSignal k; k.update(5000, true, 0.0f);
      CHECK(k.active(5000)); CHECK(k.sensorByte(5000) == 255); CHECK(k.count() == 1);
      CHECK(255 * 100.0 / 255 >= 100.0); }

    // trzymanie po ustaniu przyczyny: od razu po zwolnieniu nadal 255
    { KickoutSignal k; k.update(1000, true, 0.2f); k.update(1100, false, 0.2f);
      CHECK(k.active(1100)); CHECK(k.sensorByte(1100) == 255); }

    // precyzyjna granica okna: ostatnie tripped = 2000
    { KickoutSignal k; k.update(2000, true, 0.0f); k.update(2040, false, 0.4f);
      CHECK(k.active(2000 + KickoutSignal::HOLD_MS - 1));
      CHECK(!k.active(2000 + KickoutSignal::HOLD_MS));
      CHECK(k.sensorByte(2000 + KickoutSignal::HOLD_MS) == 60); }           // 0,4*150

    // zbocza: dlugie zadzialanie liczy sie raz, kolejne po zwolnieniu osobno
    { KickoutSignal k; for (uint32_t t = 0; t < 1000; t += 40) k.update(t, true, 0.f);
      CHECK(k.count() == 1);
      k.update(1000, false, 0.f); k.update(1040, true, 0.f); CHECK(k.count() == 2);
      k.update(1080, true, 0.f); CHECK(k.count() == 2); }

    // ponowne zadzialanie w oknie przedluza trzymanie
    { KickoutSignal k; k.update(0, true, 0.f); k.update(40, false, 0.f); k.update(2500, true, 0.f); k.update(2540, false, 0.f);
      CHECK(k.active(2540 + 2000)); CHECK(!k.active(2500 + KickoutSignal::HOLD_MS)); }

    // przepelnienie millis() (uint32): trzymanie dziala przez zawiniecie
    { KickoutSignal k; uint32_t t0 = 0xFFFFFF00u; k.update(t0, true, 0.f); k.update(t0 + 40, false, 0.f);
      CHECK(k.active(t0 + 1000));                 // po zawinieciu: (uint32_t)(now - last) = 1000 < 3000
      CHECK(k.active(t0 + 2999)); CHECK(!k.active(t0 + 3000)); }

    // PRZYCZYNA zadzialania: priorytet nadzoru ruchu, potem czujnik pradu; brak danych tuz po starcie nie liczy sie
    CHECK(kickoutCause(0, 0, 100000) == KC_NONE && kickoutCause(1, 2, 100000) == KC_NONE);       // wszystko OK / wylaczone
    CHECK(kickoutCause(2, 2, 100000) == KC_RUN_NOPROGRESS && kickoutCause(3, 2, 100000) == KC_RUN_REVERSED && kickoutCause(4, 2, 100000) == KC_RUN_FROZEN);
    CHECK(kickoutCause(1, 3, 100000) == KC_CURRENT_LATCH && kickoutCause(1, 4, 100000) == KC_CURRENT_ERROR && kickoutCause(1, 1, 100000) == KC_CURRENT_NODATA);
    CHECK(kickoutCause(2, 3, 100000) == KC_RUN_NOPROGRESS);                                       // jednoczesnie: wygrywa nadzor ruchu
    CHECK(kickoutCause(1, 1, 0) == KC_NONE && kickoutCause(1, 1, KICKOUT_STARTUP_GRACE_MS - 1) == KC_NONE);   // brak probek tuz po starcie
    CHECK(kickoutCause(1, 1, KICKOUT_STARTUP_GRACE_MS) == KC_CURRENT_NODATA);
    CHECK(kickoutCause(1, 4, 0) == KC_CURRENT_ERROR && kickoutCause(1, 3, 0) == KC_CURRENT_LATCH);    // prawdziwe awarie od razu, takze w oknie startu
    for (uint8_t c = 0; c < KC_COUNT; c++) CHECK(kickoutCauseText(c)[0] != 0);
    CHECK(kickoutCauseText(200)[0] != 0);
    { KickoutSignal k; CHECK(k.lastCause() == 0); k.noteCause(KC_CURRENT_ERROR); CHECK(k.lastCause() == KC_CURRENT_ERROR);
      k.noteCause(KC_NONE); CHECK(k.lastCause() == KC_CURRENT_ERROR); }                         // zero nie kasuje ostatniej przyczyny
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
