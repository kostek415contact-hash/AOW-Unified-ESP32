// Test agregacji powtarzajacych sie logow (CountingThrottle) - czysty C++.
//     g++ -std=c++17 -Wall -Wextra tests/test_log_throttle.cpp -o /tmp/tlt && /tmp/tlt
#include <cstdio>
#include "../diagnostics/log_throttle.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
int main() {
  CountingThrottle t(5000); uint32_t c; unsigned long s;
  CHECK(t.note(1000, c, s) && c == 1 && s == 0, "pierwsze zdarzenie wypisane od razu (1 szt., okno 0)");
  int printed = 0; uint32_t sum = 0;
  for (int i = 1; i <= 90; i++) { unsigned long now = 1000 + i * 111; if (t.note(now, c, s)) { printed++; sum += c; } }   // ~9/s przez ok. 10 s
  CHECK(printed == 1 || printed == 2, "przez ~10 s przy 9 zdarzeniach/s wypisano tylko 1-2 linie zamiast 90");
  CHECK(sum >= 40 && sum <= 90, "liczby w liniach sumuja sie do rzeczywistej liczby zdarzen");
  CountingThrottle u(5000);
  u.note(0, c, s); for (int i = 0; i < 10; i++) CHECK(!u.note(100 + i * 100, c, s), "w oknie 5 s nic nie jest wypisywane");
  CHECK(u.note(5000, c, s) && c == 11 && s == 5000, "po 5 s: 11 zdarzen (10 + to jedno) w oknie 5000 ms");
  CHECK(!u.note(5100, c, s), "nowe okno zaczyna sie od zera");
  CHECK(u.note(10000, c, s) && c == 2 && s == 5000, "kolejne okno: 2 zdarzenia");
  CountingThrottle w(1000); w.note(4294967000UL, c, s);
  CHECK(!w.note(4294967100UL, c, s), "przed zawinieciem millis(): cisza");
  CHECK(w.note((unsigned long)(4294967000UL + 1200UL), c, s) && s == 1200, "duze wartosci millis() (na ESP32 32-bitowe zawinieciem rzadzi ta sama arytmetyka bez znaku)");
  // ---- IntervalThrottle (komendy z AgOpenWeb: liczba + odstepy min/sr/max) ----
  {
    IntervalThrottle it(5000); IntervalReport r;
    CHECK(it.note(1000, 0, r) && r.count == 1 && r.spanMs == 0 && r.minDt == 0 && r.maxDt == 0, "pierwsza ramka: zgloszona od razu, bez odstepow");
    int printed2 = 0; IntervalReport last{0,0,0,0,0}; uint32_t total2 = 0;
    unsigned long now = 1000;
    for (int i = 0; i < 1200; i++) { uint32_t dt = (i == 400) ? 25 : 5; now += dt; if (it.note(now, dt, r)) { printed2++; last = r; total2 += r.count; } }   // ok. 6,2 s po 5 ms
    CHECK(printed2 == 1, "6 s strumienia 200 ramek/s daje JEDEN raport zamiast 1200 linii");
    CHECK(last.count >= 990 && last.count <= 1010 && last.minDt == 5 && last.avgDt >= 5 && last.avgDt <= 6 && last.maxDt == 25, "raport: ok. 1000 ramek, odstep min 5, sr ok. 5, max 25 ms");
    CHECK(last.spanMs >= 5000 && last.spanMs <= 5010, "okno raportu ok. 5 s");
    IntervalThrottle jt(5000); jt.note(0, 0, r);
    for (int i = 1; i <= 10; i++) CHECK(!jt.note(i * 100, 100, r), "w oknie 5 s cisza");
    CHECK(jt.note(5000, 4000, r) && r.count == 11 && r.minDt == 100 && r.maxDt == 4000, "min/max liczone tez dla pojedynczych odstepow");
    CHECK(!jt.note(5100, 100, r), "po raporcie statystyka zaczyna od zera");
    CHECK(jt.note(10000, 100, r) && r.count == 2 && r.minDt == 100 && r.maxDt == 100 && r.avgDt == 100, "kolejny raport liczy tylko swoje okno");
  }
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total); return fails ? 1 : 0;
}
