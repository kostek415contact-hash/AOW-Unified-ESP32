// Testy decyzji o ponownym uruchomieniu ArduinoOTA/mDNS (ota/ota_keeper.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_ota_keeper.cpp -o /tmp/test_ota_keeper && /tmp/test_ota_keeper
#include <cstdio>
#include <cstdint>
#include "../ota/ota_keeper.h"
using namespace OtaKeeper;

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

int main() {
    const uint32_t IP1 = 0xC0A8010A, IP2 = 0xC0A8010B;
    { State s; CHECK(decide(s, 0, 1000, false) == NONE);                       // brak sieci: nic
      CHECK(decide(s, IP1, 1000, false) == FIRST_IP);                          // pierwszy adres: start
      CHECK(decide(s, IP1, 1000, true) == NONE); }                             // zablokowane: nic
    { State s; markInit(s, IP1, 1000);
      CHECK(decide(s, IP1, 1500, false) == NONE);                              // swiezo po starcie
      CHECK(decide(s, IP2, 1500, false) == NONE);                              // zmiana IP, ale za wczesnie (RETRY)
      CHECK(decide(s, IP2, 1000 + RETRY_MS, false) == IP_CHANGED);
      CHECK(decide(s, 0, 50000, false) == NONE);                               // utrata sieci: czekamy na adres
      CHECK(decide(s, IP1, 1000 + REFRESH_MS - 1, false) == NONE);             // ten sam adres, przed odswiezeniem
      CHECK(decide(s, IP1, 1000 + REFRESH_MS, false) == PERIODIC);
      CHECK(decide(s, IP1, 1000 + REFRESH_MS, true) == NONE); }                // wgrywanie/prowadzenie: nie ruszamy
    { State s; markInit(s, IP1, 0xFFFFFF00u);                                  // przejscie przez zero millis()
      CHECK(decide(s, IP1, 0xFFFFFF00u + 1000, false) == NONE);
      CHECK(decide(s, IP1, 0xFFFFFF00u + REFRESH_MS, false) == PERIODIC); }
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
