// Test impulsu podnosnika (LiftPulse) - czysty C++.
//     g++ -std=c++17 -Wall -Wextra tests/test_lift_pulse.cpp -o /tmp/tlp && /tmp/tlp
#include <cstdio>
#include "../actuators/lift_pulse.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
int main() {
  { LiftPulse p;
    CHECK(!p.update(false, 0, 3), "bez komendy wyjscie wylaczone");
    CHECK(p.update(true, 1000, 3), "zbocze narastajace: impuls startuje");
    CHECK(p.update(true, 3999, 3), "tuz przed koncem impulsu nadal wlaczone");
    CHECK(!p.update(true, 4000, 3), "po raiseTime wyjscie puszczone mimo trwajacej komendy");
    CHECK(!p.update(true, 9000, 3), "komenda trwa, a impuls sie nie powtarza");
    CHECK(!p.update(false, 9100, 3), "zdjecie komendy: wylaczone");
    CHECK(p.update(true, 10000, 3), "nowa komenda: nowy impuls"); }
  { LiftPulse p; p.update(true, 0, 5);
    CHECK(!p.update(false, 1000, 5), "komenda znika w trakcie impulsu: wyjscie opada od razu");
    CHECK(p.update(true, 1500, 5), "i kolejne zbocze znow daje impuls"); }
  { LiftPulse p;
    CHECK(p.update(true, 0, 0) && p.update(true, 100000, 0), "raiseTime = 0: wyjscie podaza za komenda (poziom)");
    CHECK(!p.update(false, 100001, 0), "raiseTime = 0: zdjecie komendy wylacza"); }
  { LiftPulse p; p.update(true, 0, 2);
    CHECK(p.update(true, 1999, 2) && !p.update(true, 2000, 2), "granica: 2 s = 2000 ms"); }
  { LiftPulse p; unsigned long t0 = 4294967000UL; p.update(true, t0, 2);
    CHECK(p.update(true, t0 + 1000UL, 2), "zawiniecie millis(): w trakcie impulsu");
    CHECK(!p.update(true, (unsigned long)(t0 + 2500UL), 2), "zawiniecie millis(): po impulsie"); }
  { LiftPulse p; p.update(true, 0, 255);
    CHECK(p.update(true, 254999, 255) && !p.update(true, 255000, 255), "maksymalny raiseTime 255 s"); }
  { LiftPulse p; p.update(true, 0, 3); p.reset();
    CHECK(!p.update(false, 10, 3) && p.update(true, 20, 3), "reset czysci stan"); }
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total); return fails ? 1 : 0;
}
