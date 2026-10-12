// Testy monitora trendu wolnego RAM (diagnostics/heap_trend.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_heap_trend.cpp -o /tmp/test_heap_trend && /tmp/test_heap_trend
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "../diagnostics/heap_trend.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

// Symuluje 'hours' godzin pracy: probka co 1 s, wolny RAM = f(godzina) + szum +-noise, od momentu t0Ms.
template <class F> static void run(HeapTrend& h, double hours, F f, uint32_t noise, uint32_t t0Ms = 1000) {
    std::srand(7);
    uint32_t steps = (uint32_t)(hours * 3600.0);
    for (uint32_t s = 0; s <= steps; s++) {
        double hr = s / 3600.0;
        int32_t n = noise ? (int32_t)(std::rand() % (2 * noise + 1)) - (int32_t)noise : 0;
        h.sample(t0Ms + s * 1000u, (uint32_t)((int64_t)f(hr) + n));
    }
}

int main() {
    {   HeapTrend h;                                           // brak danych
        CHECK(!h.result().valid && !h.leakSuspect() && h.result().buckets == 0);
    }
    {   HeapTrend h; run(h, 1.5, [](double) { return 150000.0; }, 0);   // za krotko: < 2 h
        CHECK(!h.result().valid && !h.leakSuspect());
    }
    {   HeapTrend h; run(h, 4.0, [](double) { return 150000.0; }, 800);  // stabilnie z szumem
        HeapTrend::Result r = h.result();
        CHECK(r.valid && std::fabs(r.slopeBph) < 300.0f && !h.leakSuspect());
    }
    {   HeapTrend h; run(h, 4.0, [](double hr) { return 160000.0 - 5000.0 * hr; }, 800);   // wyciek 5 KB/h z szumem
        HeapTrend::Result r = h.result();
        CHECK(r.valid && r.slopeBph < -4500.0f && r.slopeBph > -5500.0f && r.r2 > 0.9f && h.leakSuspect());
    }
    {   HeapTrend h; run(h, 4.0, [](double hr) { return 160000.0 - 1000.0 * hr; }, 0);   // wolny spadek 1 KB/h: ponizej progu
        CHECK(h.result().valid && h.result().slopeBph < -900.0f && !h.leakSuspect());
    }
    {   HeapTrend h; run(h, 4.0, [](double hr) { return 120000.0 + 6000.0 * hr; }, 500);  // pamiec ROSNIE (np. po zwolnieniu bufora)
        CHECK(h.result().valid && h.result().slopeBph > 5000.0f && !h.leakSuspect());
    }
    {   HeapTrend h; run(h, 4.0, [](double hr) { return (int)(hr * 12) % 2 ? 150000.0 : 100000.0; }, 0);   // duze wahania bez trendu: R^2 niskie
        CHECK(h.result().valid && h.result().r2 < 0.2f && !h.leakSuspect());
    }
    {   HeapTrend h;                                           // chwilowy dolek (alokacja w jednym kubelku) nie robi wycieku: minimum kubelka, ale jeden kubelek
        run(h, 4.0, [](double) { return 150000.0; }, 0);
        for (int i = 0; i < 400; i++) h.sample(1000u + 14400u * 1000u + (uint32_t)i * 1000u, 90000);
        CHECK(!h.leakSuspect());
    }
    {   HeapTrend h; run(h, 9.0, [](double hr) { return 200000.0 - 4000.0 * hr; }, 300);   // dluzej niz 6 h: okno przesuwa sie, nachylenie nadal ok
        HeapTrend::Result r = h.result();
        CHECK(r.valid && r.buckets == HeapTrend::MAX_BUCKETS && r.slopeBph < -3500.0f && r.slopeBph > -4500.0f && h.leakSuspect());
    }
    {   HeapTrend h; run(h, 4.0, [](double hr) { return 160000.0 - 5000.0 * hr; }, 0, 4294960000u);   // zawiniecie millis() (po ok. 49 dniach)
        CHECK(h.leakSuspect());
    }
    {   HeapTrend h; run(h, 4.0, [](double hr) { return 160000.0 - 5000.0 * hr; }, 0);
        h.reset(); CHECK(!h.result().valid && h.result().buckets == 0);
    }
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
