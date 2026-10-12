// Testy detektora ucieczki silnika (RunawayGuard): kierunek, brak ruchu, okna, zatrzask, parametry.
// Czysty C++, z makrami Arduino (wylapywanie kolizji nazw):
//     g++ -std=c++17 -Wall -Wextra tests/test_runaway.cpp -o /tmp/trw && /tmp/trw
#define EXTERNAL 0
#define INTERNAL 3
#define DEFAULT 1
#define INPUT 1
#define OUTPUT 3
#define HIGH 1
#define LOW 0
#include <cstdio>
#include <climits>
#include <cmath>
#include "../actuators/runaway_guard.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)

// Symulator: kat zmienia sie z zadana predkoscia [deg/s]; co 10 ms wolamy update. Zwraca czas zadzialania [ms od startu] albo -1.
struct Sim {
  RunawayGuard g; unsigned long t = 100000; float angle = 0.0f;
  long run(int ms, int pwm, float rateDegS, float err = 10.0f, bool driving = true, float noise = 0.0f) {
    unsigned long t0 = t; int n = 0;
    for (int i = 0; i < ms / 10; i++) {
      t += 10; angle += rateDegS * 0.01f; n++;
      float meas = angle + (noise != 0.0f ? ((n % 2) ? noise : -noise) : 0.0f);
      g.update(t, driving, pwm, err, meas);
      if (g.isTripped()) return (long)(t - t0);
    }
    return -1;
  }
};

int main() {
  // ---------- prawidlowa praca ----------
  { Sim s; long r = s.run(5000, +100, +5.0f);
    CHECK(r < 0 && !s.g.isTripped() && s.g.state() == RunawayGuard::S_OK, "kolo jedzie w zadana strone 5 deg/s przez 5 s: bez zadzialania"); }
  { Sim s; long r = s.run(5000, -100, -5.0f);
    CHECK(r < 0, "to samo w strone ujemna"); }
  { Sim s; long r = s.run(6000, +100, +1.2f);
    CHECK(r < 0, "wolny, ale wystarczajacy ruch 1,2 deg/s (>= 1,0 deg w oknie 1 s): OK, okna sie przesuwaja"); }

  // ---------- odwrocony kierunek ----------
  { Sim s; long r = s.run(3000, +100, -10.0f);
    CHECK(r >= 200 && r <= 300, "pwm dodatni, kat maleje 10 deg/s: zadzialanie po 200-300 ms");
    CHECK(s.g.state() == RunawayGuard::S_WRONG_DIR && s.g.isTripped(), "przyczyna: ODWROCONY KIERUNEK");
    CHECK(s.g.lastMoveDeg() <= -1.5f && s.g.trips() == 1, "zapamietany ruch <= -1,5 deg i 1 zadzialanie"); }
  { Sim s; long r = s.run(3000, -100, +10.0f);
    CHECK(r >= 200 && r <= 300 && s.g.state() == RunawayGuard::S_WRONG_DIR, "pwm ujemny, kat rosnie: odwrocony kierunek rowniez wykryty"); }
  { Sim s; long r = s.run(3000, +100, -1.0f);
    CHECK(r >= 1000 && r <= 1100 && s.g.state() == RunawayGuard::S_NO_PROGRESS, "wolny ruch W ZLA strone (1 deg/s) nie przekracza -1,5 deg w 1 s: lapany jako brak postepu po oknie"); }
  { Sim s; long r = s.run(3000, +100, -10.0f);   // tempo ucieczki: zatrzymany zanim kolo przejedzie wiecej niz kilka stopni
    CHECK(std::fabs(s.angle) < 4.0f, "kolo uciekalo w zla strone przez ulamek sekundy: < 4 deg (nie do ogranicznika)"); (void)r; }

  // ---------- brak ruchu ----------
  { Sim s; long r = s.run(3000, +100, 0.0f);
    CHECK(r >= 1000 && r <= 1010, "silnik napedzany, kat stoi: zadzialanie po 1 s okna");
    CHECK(s.g.state() == RunawayGuard::S_NO_PROGRESS, "przyczyna: BRAK POSTEPU"); }
  { Sim s; long r = s.run(990, +100, 0.0f); CHECK(r < 0, "po 990 ms jeszcze bez zadzialania (okno 1000 ms)"); }
  { Sim s; long r = s.run(3000, +100, +0.8f);
    CHECK(r >= 1000 && r <= 1010, "ruch 0,8 deg/s < 1,0 deg/okno: brak postepu"); }
  { Sim s; long r = s.run(3000, +100, 0.0f, 10.0f, true, 0.2f);
    CHECK(r >= 1000 && r <= 1010, "szum czujnika +-0,2 deg przy zablokowanym kole NIE udaje ruchu: zadzialanie"); }
  { Sim s; long r = s.run(5000, +100, +5.0f, 10.0f, true, 0.2f);
    CHECK(r < 0, "szum +-0,2 deg przy prawdziwym ruchu nie psuje detekcji: bez zadzialania"); }
  { Sim s; s.run(2000, +100, +5.0f); long r = s.run(3000, +100, 0.0f);
    CHECK(r >= 1000 && r <= 1100, "najpierw dobry ruch (okna sie przesuwaja), potem kolo staje: zadzialanie do ok. 1 s od zatrzymania"); }

  // ---------- kiedy NIE oceniamy ----------
  { Sim s; long r = s.run(10000, +100, 0.0f, 2.9f); CHECK(r < 0, "blad 2,9 deg < 3,0 (blisko celu): stojace kolo nie wyzwala, regulator dreptuje"); }
  { Sim s; long r = s.run(3000, +100, 0.0f, 3.0f); CHECK(r >= 1000, "blad dokladnie 3,0 deg: ocena aktywna"); }
  { Sim s; long r = s.run(10000, +100, 0.0f, 10.0f, false); CHECK(r < 0, "silnik NIE napedzany (driving=false): brak oceny"); }
  { Sim s; long r = s.run(10000, 0, 0.0f); CHECK(r < 0, "pwm = 0: brak oceny"); }
  { Sim s; s.run(600, +100, 0.0f); long r = s.run(900, -100, 0.0f);
    CHECK(r < 0, "zmiana kierunku napedu po 600 ms zaczyna nowe okno: po kolejnych 900 ms jeszcze bez zadzialania");
    long r2 = s.run(300, -100, 0.0f); CHECK(r2 >= 0, "nowe okno minelo 1000 ms => zadzialanie"); }
  { Sim s; s.run(800, +100, 0.0f); s.run(100, +100, 0.0f, 0.5f); long r = s.run(500, +100, 0.0f, 10.0f);
    CHECK(r < 0, "blad spadl pod prog na chwile: okno zaczyna sie od zera (ostatnie 500 ms to za malo)"); }
  { Sim s; s.g.update(s.t, true, +100, NAN, 0.0f); s.g.update(s.t + 2000, true, +100, 10.0f, NAN);
    CHECK(!s.g.isTripped(), "NaN w bledzie/kacie: brak oceny, brak zadzialania, brak wyjatku"); }

  // ---------- zatrzask ----------
  { Sim s; s.run(2000, +100, 0.0f); CHECK(s.g.isTripped(), "(kontrola) zadzialalo");
    unsigned long t0 = s.t; s.g.tick(t0, true); s.g.tick(t0 + 5000, true);
    CHECK(s.g.isTripped(), "prowadzenie WLACZONE: zatrzask trzyma");
    s.g.tick(t0 + 6000, false); s.g.tick(t0 + 6999, false); CHECK(s.g.isTripped(), "prowadzenie wylaczone 999 ms: jeszcze zatrzask");
    s.g.tick(t0 + 7000, false); CHECK(!s.g.isTripped() && s.g.state() == RunawayGuard::S_OK, "wylaczone 1000 ms: zatrzask zdjety");
    long r = s.run(2000, +100, 0.0f); CHECK(r >= 1000 && s.g.trips() == 2, "po zdjeciu zatrzasku nadzor dziala dalej: drugie zadzialanie, licznik 2"); }
  { Sim s; s.run(2000, +100, 0.0f); unsigned long t0 = s.t; s.g.tick(t0, false); s.g.tick(t0 + 800, false); s.g.tick(t0 + 900, true); s.g.tick(t0 + 1500, false); s.g.tick(t0 + 2400, false);
    CHECK(s.g.isTripped(), "chwilowe ponowne wlaczenie prowadzenia zeruje odliczanie 1 s");
    s.g.tick(t0 + 2500, false); CHECK(!s.g.isTripped(), "pelna sekunda ciaglego wylaczenia zdejmuje"); }
  { Sim s; s.run(2000, +100, 0.0f); s.g.reset(); CHECK(!s.g.isTripped(), "reczny reset zdejmuje zatrzask od razu"); }
  { Sim s; s.run(2000, +100, 0.0f); for (int i = 0; i < 300; i++) { s.t += 10; s.angle -= 0.1f; s.g.update(s.t, true, +100, 10.0f, s.angle); }   // 3 s ucieczki przy aktywnym zatrzasku (petla wprost, bez run())
    CHECK(s.g.trips() == 1 && s.g.state() == RunawayGuard::S_NO_PROGRESS, "gdy zatrzask aktywny, dalsze update (nawet z ucieczka) nie mnozy zadzialan ani nie zmienia przyczyny"); }

  // ---------- parametry i wylaczenie ----------
  { Sim s; s.g.configure({true, 500, 0.5f, 1.0f}); long r = s.run(2000, +100, 0.0f);
    CHECK(r >= 500 && r <= 510, "okno 500 ms: zadzialanie po 500 ms"); }
  { Sim s; s.g.configure({true, 500, 0.5f, 1.0f}); long r = s.run(4000, +100, +1.2f);
    CHECK(r < 0, "minMove 0,5 deg/500 ms: ruch 1,2 deg/s wystarcza"); }
  { Sim s; s.g.configure({false, 1000, 1.0f, 3.0f}); long r = s.run(10000, +100, -10.0f);
    CHECK(r < 0 && s.g.state() == RunawayGuard::S_OFF, "detektor wylaczony: nie zadziala nawet przy ucieczce; stan OFF"); }
  { Sim s; s.run(800, +100, 0.0f); s.g.configure({false, 1000, 1.0f, 3.0f}); s.g.configure({true, 1000, 1.0f, 3.0f}); long r = s.run(900, +100, 0.0f);
    CHECK(r < 0, "wylaczenie i ponowne wlaczenie w trakcie okna zaczyna okno od nowa"); }

  // ---------- zegar ----------
  { Sim s; s.t = ULONG_MAX - 300; long r = s.run(3000, +100, 0.0f);
    CHECK(s.t < 5000 && r >= 1000 && r <= 1010, "okno przechodzi przez zawiniecie zegara liczby bez znaku"); }
  { Sim s; s.t = ULONG_MAX - 100; long r = s.run(2000, +100, -10.0f);
    CHECK(r >= 200 && r <= 300, "odwrocony kierunek wykryty poprawnie mimo zawiniecia zegara"); }

  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
