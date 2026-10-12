// Test zabezpieczen silnika (actuators/motor_safety.h): dead-man i uzbrojenie po restarcie - czysty C++.
//     g++ -std=c++17 -Wall -Wextra tests/test_motor_safety.cpp -o /tmp/tms && /tmp/tms
#include <cstdio>
#include "../actuators/motor_safety.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
int main() {
  { MotorDeadman d;
    CHECK(!d.poll(100000, 300), "dead-man: nieuzbrojony przed pierwszym feed (opoznienia startu nie liczą się)");
    d.feed(1000);
    { MotorDeadman e; e.feed(5000); CHECK(!e.poll(4990, 300), "dead-man: feed tuz po odczycie czasu nie odcina"); }
    CHECK(!d.poll(1200, 300) && !d.poll(1300, 300), "dead-man: do limitu wlacznie brak odciecia");
    CHECK(d.poll(1301, 300), "dead-man: po przekroczeniu limitu odcina");
    CHECK(!d.poll(1400, 300) && !d.poll(5000, 300), "dead-man: odcina tylko RAZ na zawieszenie");
    CHECK(d.trips() == 1, "dead-man: licznik odciec = 1");
    d.feed(5100);
    CHECK(!d.poll(5200, 300), "dead-man: po wznowieniu petli znow gotowy");
    CHECK(d.poll(5500, 300) && d.trips() == 2, "dead-man: drugie zawieszenie odcina ponownie"); }
  { MotorDeadman d; d.feed(4294967000u);
    CHECK(!d.poll(4294967200u, 300), "dead-man: zawiniecie millis(): w limicie");
    CHECK(!d.poll(100u, 600), "dead-man: zawiniecie millis(): 396 ms przy limicie 600");
    CHECK(d.poll(100u, 300), "dead-man: zawiniecie millis(): 396 ms przy limicie 300 odcina"); }
  { MotorDeadman d; d.feed(0);
    CHECK(!d.poll(300) && d.poll(301), "dead-man: domyslny limit 300 ms, feed(0) jest poprawny"); }
  { SteerArming a;
    CHECK(!a.armed(), "uzbrojenie: po starcie nieuzbrojone");
    a.update(false, false);
    CHECK(!a.armed(), "uzbrojenie: brak swiezej ramki nie uzbraja");
    a.update(true, true);
    CHECK(!a.armed() && a.blocking(true), "uzbrojenie: ramka 'wlaczone' po restarcie nie uzbraja i blokuje");
    a.update(true, false);
    CHECK(a.armed() && !a.blocking(true), "uzbrojenie: swieza ramka 'wylaczone' uzbraja");
    a.update(true, true);
    CHECK(a.armed(), "uzbrojenie: pozostaje uzbrojone przy kolejnych wlaczeniach");
    a.update(false, true);
    CHECK(a.armed(), "uzbrojenie: krotka utrata lacznosci nie rozbraja"); }
  { DirGuard g;
    CHECK(g.filter(40, 1000) == 40 && g.filter(60, 1005) == 60, "dirguard: ten sam kierunek bez opoznienia");
    CHECK(g.filter(-30, 1010) == 0, "dirguard: bezposrednie odwrocenie -> najpierw stop");
    CHECK(g.filter(-30, 1020) == 0 && g.filter(-30, 1029) == 0, "dirguard: stop trwa HOLD_MS");
    CHECK(g.filter(-30, 1030) == -30, "dirguard: po przerwie jedzie w nowa strone");
    CHECK(g.filter(-35, 1031) == -35 && g.filter(20, 1032) == 0, "dirguard: kolejne odwrocenie znow przez stop");
    CHECK(g.filter(0, 1040) == 0 && g.filter(25, 1041) == 0, "dirguard: odwrocenie w trakcie przerwy nie omija jej");
    CHECK(g.filter(25, 1052) == 25, "dirguard: po uplywie przerwy rusza"); }
  { DirGuard g; g.filter(50, 100); CHECK(g.filter(0, 101) == 0 && g.filter(-50, 102) == -50, "dirguard: przejscie przez zero (wyjscie 0) nie jest opozniane"); }
  { DirGuard g; g.filter(50, 100); g.filter(-50, 101); g.reset(); CHECK(g.filter(-50, 102) == -50, "dirguard: reset kasuje przerwe"); }
  { SteerArming a; a.update(true, false);
    CHECK(a.armed(), "uzbrojenie: po swiezej ramce z wylaczonym prowadzeniem uzbrojone");
    a.reset();
    CHECK(!a.armed() && a.blocking(true), "uzbrojenie: reset (funkcja wylaczona) kasuje uzbrojenie i znowu blokuje");
    a.update(true, true);
    CHECK(!a.armed(), "uzbrojenie: po resecie ramka 'wlaczone' nie uzbraja");
    a.update(true, false);
    CHECK(a.armed(), "uzbrojenie: po resecie uzbraja dopiero ramka z wylaczonym prowadzeniem"); }
  { DirGuard g; uint32_t t = 4294967290u; g.filter(50, t); CHECK(g.filter(-50, t + 3) == 0 && g.filter(-50, t + 30) == -50, "dirguard: zawiniecie millis()"); }
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
