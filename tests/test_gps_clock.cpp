// Testy zegara UTC z GPS (sensors/gps_clock.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_gps_clock.cpp -o /tmp/test_gps_clock && /tmp/test_gps_clock
#include <cstdio>
#include <cstdint>
#include "../sensors/gps_clock.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

int main() {
    // wartosci referencyjne policzone niezaleznie (calendar.timegm)
    CHECK(GpsClock::toEpoch("113215", "091026") == 1791545535LL);
    CHECK(GpsClock::toEpoch("113215.00", "091026") == 1791545535LL);                 // czesci ulamkowe sekund
    CHECK(GpsClock::toEpoch("235959", "290228") == 1835481599LL);                    // rok przestepny 2028, 29 lutego
    CHECK(GpsClock::toEpoch("000000", "010124") == 1704067200LL);
    CHECK(GpsClock::toEpoch("235959", "311299") == 4102444799LL);
    CHECK(GpsClock::toEpoch("000000", "010326") == 1772323200LL);                    // 1 marca 2026 (luty 28 dni)

    // bledne dane
    CHECK(GpsClock::toEpoch("", "091026") == -1 && GpsClock::toEpoch("113215", "") == -1);
    CHECK(GpsClock::toEpoch(nullptr, "091026") == -1 && GpsClock::toEpoch("113215", nullptr) == -1);
    CHECK(GpsClock::toEpoch("246015", "091026") == -1 && GpsClock::toEpoch("116015", "091026") == -1);      // godzina 24 / minuta 60
    CHECK(GpsClock::toEpoch("113215", "320126") == -1 && GpsClock::toEpoch("113215", "300226") == -1);       // 32 stycznia, 30 lutego
    CHECK(GpsClock::toEpoch("113215", "290226") == -1);                                                      // 29.02.2026 - nie przestepny
    CHECK(GpsClock::toEpoch("113215", "011380") == -1 && GpsClock::toEpoch("113215", "000126") == -1);       // miesiac 13; dzien 0
    CHECK(GpsClock::toEpoch("113215", "060123") == -1);                                                      // rok 2023 (przed MIN_YEAR) odrzucony
    CHECK(GpsClock::toEpoch("113215", "060100") == -1);                                                      // rok 2000 (odbiornik bez daty) odrzucony
    CHECK(GpsClock::toEpoch("11a215", "091026") == -1 && GpsClock::toEpoch("113215x", "091026") == -1);      // znaki nie-cyfry / smiec po czasie
    CHECK(GpsClock::toEpoch("113215", "0910260") == -1 && GpsClock::toEpoch("11321", "091026") == -1);       // zla dlugosc

    // zegar: brak -> 0, ustawienie, ekstrapolacja, wygasniecie, odrzucenie zlej ramki nie psuje poprzedniego czasu
    GpsClock::reset();
    CHECK(GpsClock::epochNow(5000) == 0);
    CHECK(GpsClock::noteRmc("113215", "091026", 10000));
    CHECK(GpsClock::epochNow(10000) == 1791545535UL);
    CHECK(GpsClock::epochNow(10999) == 1791545535UL && GpsClock::epochNow(11000) == 1791545536UL);
    CHECK(GpsClock::epochNow(10000 + 3600000UL) == 1791545535UL + 3600);
    CHECK(!GpsClock::noteRmc("999999", "999999", 20000) && GpsClock::epochNow(20000) == 1791545535UL + 10);   // zla ramka ignorowana
    CHECK(GpsClock::epochNow(10000 + GpsClock::MAX_AGE_MS + 1) == 0);                                          // za stare
    CHECK(GpsClock::noteRmc("120000", "091026", 50000) && GpsClock::epochNow(50000) == 1791547200UL);          // nowy RMC odswieza

    // przejscie millis() przez zero (49,7 dnia)
    GpsClock::reset();
    CHECK(GpsClock::noteRmc("113215", "091026", 0xFFFFFF00u) && GpsClock::epochNow(0xFFFFFF00u + 2000u) == 1791545537UL);

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
