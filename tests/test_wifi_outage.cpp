// Testy pomiaru zaniku WiFi (diagnostics/wifi_outage.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_wifi_outage.cpp -o /tmp/test_wifi_outage && /tmp/test_wifi_outage
#include <cstdio>
#include "../diagnostics/wifi_outage.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

int main() {
    {   WifiOutage o;                                    // start bez sieci to jeszcze nie zanik
        CHECK(!o.update(false, 100) && !o.update(false, 5000) && !o.inOutage() && o.count() == 0);
        CHECK(!o.update(true, 9000) && o.count() == 0);   // pierwsze polaczenie
    }
    {   WifiOutage o; o.update(true, 1000);
        CHECK(!o.update(true, 2000));
        CHECK(!o.update(false, 3000) && o.inOutage() && o.outageSoFarMs(4500) == 1500);
        CHECK(!o.update(false, 4000));
        CHECK(o.update(true, 5200) && !o.inOutage());       // powrot: 2200 ms
        CHECK(o.count() == 1 && o.lastMs() == 2200 && o.maxMs() == 2200 && o.avgMs() == 2200);
        CHECK(!o.update(true, 6000));                        // kolejne obiegi nie licza powrotu drugi raz
        o.update(false, 7000); CHECK(o.update(true, 7400));  // krotki zanik 400 ms
        CHECK(o.count() == 2 && o.lastMs() == 400 && o.maxMs() == 2200 && o.avgMs() == 1300);
        CHECK(o.outageSoFarMs(9999) == 0);
    }
    {   WifiOutage o; o.update(true, 0xFFFFFF00u);          // przekrecenie millis()
        o.update(false, 0xFFFFFFF0u); CHECK(o.update(true, 0x00000200u));
        CHECK(o.lastMs() == 0x210u);
    }
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
