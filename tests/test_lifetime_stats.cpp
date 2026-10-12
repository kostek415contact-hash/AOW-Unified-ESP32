// Tests of the counters that outlive restarts (diagnostics/lifetime_stats.h).
//     g++ -std=c++17 -Wall -Wextra tests/test_lifetime_stats.cpp -o /tmp/tls && /tmp/tls
#include <cstdio>
#include "../diagnostics/lifetime_stats.h"
static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

int main() {
    // 1. Start: counted, the first save is urgent but only when a flash write is acceptable
    { LifetimeStats s; LifetimeStats::Data d; d.boots = 41; d.motorRunSec = 1000; s.load(d);
      CHECK(!s.dirty() && !s.shouldSave(10, true));
      s.noteBoot(false); CHECK(s.data().boots == 42 && s.data().abnormalBoots == 0);
      CHECK(!s.shouldSave(10, false));                        // steering engaged: no flash write
      CHECK(s.shouldSave(10, true));
      s.markSaved(10); CHECK(!s.shouldSave(20, true));
      s.noteBoot(true); CHECK(s.data().abnormalBoots == 1); }
    // 2. Motor time counts only while driven, in whole seconds, and a stalled loop (gap >= 5 s) is not motor time
    { LifetimeStats s; uint32_t t = 1000;
      for (int i = 0; i < 301; i++) { t += 10; s.tick(t, true, 0, 0, 0); }       // 3.0 s driven (the first pass only sets the reference)
      CHECK(s.data().motorRunSec == 3);
      for (int i = 0; i < 300; i++) { t += 10; s.tick(t, false, 0, 0, 0); }      // 3 s idle
      CHECK(s.data().motorRunSec == 3);
      t += 60000; s.tick(t, true, 0, 0, 0);                                      // the loop hung for a minute
      CHECK(s.data().motorRunSec == 3); }
    // 3. Trips: only the GROWTH of the session counters is added (a counter that is already 2 at the first pass is 2 new trips)
    { LifetimeStats s; s.tick(100, false, 2, 0, 0); CHECK(s.data().runawayTrips == 2);
      s.tick(110, false, 2, 1, 0); CHECK(s.data().runawayTrips == 2 && s.data().kickoutTrips == 1);
      s.tick(120, false, 3, 1, 4); CHECK(s.data().runawayTrips == 3 && s.data().deadmanTrips == 4);
      CHECK(s.shouldSave(125, true) && !s.shouldSave(125, false)); }                 // a trip is saved at once, when it is safe
    // 3b. A tripping sensor must not write the flash on every trip: the next urgent save waits TRIP_MIN_GAP_MS
    { LifetimeStats s; s.tick(100, false, 1, 0, 0); CHECK(s.shouldSave(110, true)); s.markSaved(110);
      s.tick(200, false, 2, 0, 0); CHECK(s.dirty() && !s.shouldSave(300, true));
      CHECK(s.shouldSave(110 + LifetimeStats::TRIP_MIN_GAP_MS, true)); }
    // 4. Running time alone is saved only every SAVE_EVERY_MS
    { LifetimeStats s; uint32_t t = 1; s.tick(t, true, 0, 0, 0);
      for (int i = 0; i < 200; i++) { t += 10; s.tick(t, true, 0, 0, 0); }
      CHECK(s.dirty() && !s.shouldSave(t, true));
      CHECK(s.shouldSave(t + LifetimeStats::SAVE_EVERY_MS, true)); }
    // 5. millis() wrap-around does not invent motor time
    { LifetimeStats s; s.tick(4294967290u, true, 0, 0, 0); s.tick(4294967295u, true, 0, 0, 0); s.tick(4u, true, 0, 0, 0);
      CHECK(s.data().motorRunSec == 0); }
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
