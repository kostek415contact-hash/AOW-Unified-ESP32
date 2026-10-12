// Host test of sensors/gps_baud_retry.h (plain C++).
//     g++ -std=c++17 -Wall -Wextra tests/test_gps_baud_retry.cpp -o /tmp/tgbr && /tmp/tgbr
#include <cstdio>
#include "../sensors/gps_baud_retry.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
typedef GpsBaudRetry R;
int main() {
  { R r; r.restartClock(1000);
    CHECK(r.tick(1000, false).act == R::A_NONE && r.tick(15999, false).act == R::A_NONE, "retry: pierwsza runda dopiero po 15 s od startu");
    R::Out o = r.tick(16000, false);
    CHECK(o.act == R::A_SWITCH && o.index == 0 && r.active() && r.attempts() == 1, "retry: po 15 s start rundy, predkosc 0");
    CHECK(r.tick(16100, false).act == R::A_NONE && r.tick(16149, false).act == R::A_NONE, "retry: w oknie 150 ms nic sie nie dzieje (bez czekania w petli)");
    o = r.tick(16150, false);
    CHECK(o.act == R::A_SWITCH && o.index == 1, "retry: po 150 ms nastepna predkosc");
    for (uint8_t i = 2; i < 7; i++) { o = r.tick(16150 + 150u * (i - 1), false); CHECK(o.act == R::A_SWITCH && o.index == i, "retry: kolejne predkosci po kolei"); }
    o = r.tick(16150 + 150u * 6, false);
    CHECK(o.act == R::A_FALLBACK && !r.active(), "retry: po wyczerpaniu kandydatow powrot do predkosci awaryjnej");
    CHECK(r.tick(17000, false).act == R::A_NONE && r.tick(30999, false).act == R::A_NONE, "retry: po rundzie cisza do nastepnej");
    CHECK(r.tick(31000, false).act == R::A_SWITCH && r.attempts() == 2, "retry: nastepna runda 15 s od POCZATKU poprzedniej"); }
  { R r; r.restartClock(0); r.tick(15000, false); r.tick(15150, false); r.tick(15300, false);
    R::Out o = r.tick(15310, true);
    CHECK(o.act == R::A_SUCCESS && o.index == 2 && !r.active(), "retry: poprawna linia w trakcie rundy = sukces dla biezacej predkosci");
    CHECK(r.tick(15320, true).act == R::A_NONE && r.tick(60000, true).act == R::A_NONE, "retry: po sukcesie koniec, rundy sie nie powtarzaja"); }
  { R r; r.restartClock(0);
    CHECK(r.tick(20000, true).act == R::A_NONE, "retry: poprawna linia bez trwajacej rundy nic nie robi"); }
  { R r; r.restartClock(0); r.tick(15000, false);
    r.restartClock(15100);
    CHECK(!r.active() && r.tick(15200, false).act == R::A_NONE, "retry: restartClock przerywa runde"); }
  { R r; r.restartClock(4294960000u);
    CHECK(r.tick(4294960000u + 14999u, false).act == R::A_NONE, "retry: zawiniecie millis(): przed 15 s");
    CHECK(r.tick(4294960000u + 15000u, false).act == R::A_SWITCH, "retry: zawiniecie millis(): po 15 s przez zawiniecie"); }
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
