// Testy licznika przerw (diagnostics/gap_tracker.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_gap_tracker.cpp -o /tmp/test_gap_tracker && /tmp/test_gap_tracker
#include <cstdio>
#include "../diagnostics/gap_tracker.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

int main() {
    {   GapTracker g;
        CHECK(g.maxMs() == 0 && g.countOver1s() == 0 && g.countOver2s() == 0 && g.msSinceLastBigGap(5000) == -1);
        g.note(1000);                      // punkt odniesienia, jeszcze zadna przerwa
        CHECK(g.maxMs() == 0 && g.lastMs() == 0);
        for (unsigned long t = 1100; t <= 2000; t += 100) g.note(t);   // rowne 100 ms
        CHECK(g.maxMs() == 100 && g.countOver1s() == 0);
        g.note(2000 + 1000);               // dokladnie 1000 ms: NIE jest "wieksze niz 1 s"
        CHECK(g.countOver1s() == 0 && g.maxMs() == 1000);
        g.note(3000 + 1500);               // 1500 ms: duza, ale nie ogromna
        CHECK(g.countOver1s() == 1 && g.countOver2s() == 0 && g.lastBigGapMs() == 1500 && g.maxMs() == 1500);
        CHECK(g.msSinceLastBigGap(4500 + 700) == 700);
        g.note(4500 + 2600);               // 2600 ms: przekracza 2 s
        CHECK(g.countOver1s() == 2 && g.countOver2s() == 1 && g.lastBigGapMs() == 2600 && g.maxMs() == 2600);
        g.note(7100 + 100);                // powrot do normy nie kasuje historii
        CHECK(g.lastMs() == 100 && g.maxMs() == 2600 && g.countOver2s() == 1);
        g.reset();
        CHECK(g.maxMs() == 0 && g.countOver1s() == 0 && g.lastBigGapMs() == 0);
    }
    {   // zdarzenie w czasie 0 nie psuje punktu odniesienia
        GapTracker g; g.note(0); g.note(150);
        CHECK(g.lastMs() == 149 || g.lastMs() == 150);
    }
    {   // przekrecenie millis() (uint32): roznica unsigned long nadal poprawna na 32-bitowym ESP32; tu sprawdzamy dla 64-bit bez przekrecenia
        GapTracker g; g.note(4294960000UL); g.note(4294960000UL + 2500UL);
        CHECK(g.countOver2s() == 1);
    }
    {   // trzy liczniki sa niezalezne
        GapStats::gga.note(100); GapStats::gga.note(3000);
        CHECK(GapStats::gga.countOver2s() == 1 && GapStats::pandaTry.countOver2s() == 0 && GapStats::pandaOk.maxMs() == 0);
    }
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
