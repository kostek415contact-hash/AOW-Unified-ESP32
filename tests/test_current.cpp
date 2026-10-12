// Testy logiki czujnika pradu ACS71240: przeliczenia, kalibracja zera, ochrona silnika (kickout).
// Czysty C++, z makrami Arduino, zeby wylapac kolizje nazw:
//     g++ -std=c++17 -Wall -Wextra tests/test_current.cpp -o /tmp/tc && /tmp/tc
#define EXTERNAL 0
#define INTERNAL 3
#define DEFAULT 1
#define INPUT 1
#define OUTPUT 3
#define HIGH 1
#define LOW 0
#include <cstdio>
#include <string>
#include <climits>
#include <cmath>
#include "../sensors/current_math.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
static float vA(float amps, float zero = 1.65f) { return zero + amps * CurrentMath::SENS_V_PER_A; }   // napiecie dla zadanego pradu
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
  using CurrentMath::voltsToAmps;
  // ---------- przeliczenia (dane z dokumentacji: 44 mV/A, zero 1,65 V) ----------
  CHECK(near(voltsToAmps(1.65f, 1.65f), 0.0f, 1e-4f), "1,65 V = 0 A");
  CHECK(near(voltsToAmps(1.65f + 0.44f, 1.65f), 10.0f, 1e-3f), "+0,44 V = +10 A");
  CHECK(near(voltsToAmps(1.65f - 0.66f, 1.65f), -15.0f, 1e-3f), "-0,66 V = -15 A (kierunek ujemny)");
  CHECK(near(voltsToAmps(1.68f, 1.68f), 0.0f, 1e-4f), "zero skalibrowane na 1,68 V: odczyt 1,68 V = 0 A");
  CHECK(CurrentMath::voltsPlausible(vA(30.0f)) && CurrentMath::voltsPlausible(vA(-30.0f)), "pelny zakres +-30 A (0,33-2,97 V) jest wiarygodny");
  CHECK(!CurrentMath::voltsPlausible(0.0f) && !CurrentMath::voltsPlausible(3.3f) && !CurrentMath::voltsPlausible(NAN) && !CurrentMath::voltsPlausible(INFINITY), "0 V, 3,3 V, NaN, Inf = niewiarygodne");
  CHECK(near(CurrentMath::SENS_V_PER_A, 0.044f, 1e-6f), "czulosc 44 mV/A (NIE 66 mV/A z dawnego komentarza - to wariant 20 A)");

  // ---------- kalibracja zera ----------
  { ZeroCalibrator c; c.start(64); bool done = false; for (int i = 0; i < 64; i++) done = c.add(1.652f + ((i % 3) - 1) * 0.002f);
    CHECK(done && c.status() == ZeroCalibrator::Z_DONE && near(c.result(), 1.652f, 0.002f), "64 stabilne probki => zero ~1,652 V"); }
  { ZeroCalibrator c; c.start(10); for (int i = 0; i < 9; i++) CHECK(!c.add(1.65f), "przed ostatnia probka brak wyniku");
    CHECK(c.progressPercent() == 90, "postep 90% po 9/10"); CHECK(c.add(1.65f) && c.progressPercent() == 100, "ostatnia probka konczy, 100%"); CHECK(!c.add(1.65f), "po zakonczeniu kolejne probki ignorowane"); }
  { ZeroCalibrator c; c.start(20); bool done = false; for (int i = 0; i < 20; i++) done = c.add(i % 2 ? 1.62f : 1.70f);
    CHECK(done && c.status() == ZeroCalibrator::Z_FAILED && std::string(c.failReason()).find("unstable") != std::string::npos, "rozrzut 80 mV p-p (plynie prad?) => odrzucone"); }
  { ZeroCalibrator c; c.start(20); bool done = false; for (int i = 0; i < 20; i++) done = c.add(1.90f);
    CHECK(done && c.status() == ZeroCalibrator::Z_FAILED && std::string(c.failReason()).find("out of range") != std::string::npos, "stale 1,90 V (5,7 A) => zero poza zakresem, odrzucone"); }
  { ZeroCalibrator c; c.start(20); bool done = c.add(0.0f);
    CHECK(done && c.status() == ZeroCalibrator::Z_FAILED && std::string(c.failReason()).find("not connected") != std::string::npos, "0 V (niepodlaczony) => odrzucone od razu"); }
  { ZeroCalibrator c; CHECK(c.status() == ZeroCalibrator::Z_IDLE && !c.add(1.65f), "bez start() nic nie robi"); }

  // ---------- ochrona: praca normalna i brak danych ----------
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000;
    CHECK(g.tripped(t) && g.state(t) == CurrentGuard::S_NO_DATA, "przed pierwsza probka: brak danych, silnik zablokowany (bezpiecznie)");
    for (int i = 0; i < 20; i++) { t += 20; g.onSample(t, vA(0.3f), 1.65f); g.update(t, true); }
    CHECK(!g.tripped(t) && g.state(t) == CurrentGuard::S_OK, "0,3 A: OK, silnik moze pracowac");
    CHECK(near(g.amps(), 0.3f, 0.01f) && g.ageMs(t) == 0, "odczyt i wiek probki");
    CHECK(!g.tripped(t + 250) && g.tripped(t + 251) && g.state(t + 300) == CurrentGuard::S_NO_DATA, "po > 250 ms bez probki: brak danych => zatrzymanie"); }

  // ---------- prog miekki: prad przekroczony przez okreslony czas ----------
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(1.0f), 1.65f);
    for (int i = 0; i < 10; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); }   // 200 ms powyzej progu
    for (int i = 0; i < 10; i++) { t += 20; g.onSample(t, vA(2.0f), 1.65f); }   // spadek = brak wyzwolenia
    CHECK(!g.isLatched(), "krotki pik 9 A przez 200 ms (rozruch) NIE wyzwala");
    for (int i = 0; i < 25; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); if (i == 18) CHECK(!g.isLatched(), "po ~380 ms jeszcze nie"); }
    CHECK(g.isLatched() && g.tripped(t) && g.state(t) == CurrentGuard::S_OVERCURRENT, "9 A nieprzerwanie >= 400 ms => zatrzask nadpradowy");
    CHECK(near(g.latchedAmps_(), 9.0f, 0.05f), "zapamietany prad wyzwolenia"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 40; i++) { t += 20; g.onSample(t, vA(i % 2 ? 9.0f : 7.0f), 1.65f); }   // schodzi pod prog co druga probke
    CHECK(!g.isLatched(), "prad faluje wokol progu (kazdy drugi odczyt pod nim) - odliczanie czasu zaczyna sie od nowa"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 25; i++) { t += 20; g.onSample(t, vA(-9.0f), 1.65f); }
    CHECK(g.isLatched(), "kierunek ujemny (-9 A) dziala tak samo"); }

  // ---------- prog twardy ----------
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    t += 20; g.onSample(t, vA(16.0f), 1.65f); CHECK(!g.isLatched(), "pojedyncza probka 16 A (glitch) nie wyzwala");
    t += 20; g.onSample(t, vA(1.0f), 1.65f); t += 20; g.onSample(t, vA(16.0f), 1.65f); CHECK(!g.isLatched(), "16 A, przerwa, 16 A - to nie dwie KOLEJNE");
    t += 20; g.onSample(t, vA(17.0f), 1.65f); CHECK(g.isLatched() && g.tripped(t), "dwie kolejne probki >= 15 A => natychmiastowy zatrzask"); }

  // ---------- blad czujnika (niepodlaczony, zwarcie) ----------
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000;
    for (int i = 0; i < 5; i++) { t += 20; g.onSample(t, vA(0.0f), 1.65f); }
    t += 20; g.onSample(t, 0.0f, 1.65f); t += 20; g.onSample(t, 0.0f, 1.65f);
    CHECK(!g.tripped(t) && g.state(t) == CurrentGuard::S_OK, "dwie zle probki z rzedu to jeszcze nie blad (zaklocenie)");
    t += 20; g.onSample(t, 0.0f, 1.65f);
    CHECK(g.tripped(t) && g.state(t) == CurrentGuard::S_FAULT, "trzecia zla probka => BLAD CZUJNIKA, silnik zablokowany");
    CHECK(!g.isLatched(), "0 V (= -37 A) NIE jest brane za nadprad: to blad czujnika, nie zatrzask");
    for (int i = 0; i < 9; i++) { t += 20; g.onSample(t, vA(0.0f), 1.65f); }
    CHECK(g.state(t) == CurrentGuard::S_FAULT, "9 dobrych probek: blad jeszcze trwa");
    t += 20; g.onSample(t, vA(0.0f), 1.65f);
    CHECK(g.state(t) == CurrentGuard::S_OK && !g.tripped(t), "10 dobrych z rzedu: blad schodzi"); }
  { CurrentGuard g; unsigned long t = 1000; for (int i = 0; i < 4; i++) { t += 20; g.onSample(t, 3.3f, 1.65f); }
    CHECK(g.state(t) == CurrentGuard::S_FAULT, "3,3 V (zwarcie do zasilania) => blad czujnika"); }
  { CurrentGuard g; unsigned long t = 1000; for (int i = 0; i < 4; i++) { t += 20; g.onSample(t, NAN, 1.65f); }
    CHECK(g.state(t) == CurrentGuard::S_FAULT, "NaN z ADC => blad czujnika, bez wyjatku"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 20; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); }
    for (int i = 0; i < 3; i++) { t += 20; g.onSample(t, 0.0f, 1.65f); }
    for (int i = 0; i < 20; i++) { t += 20; g.onSample(t, vA(0.0f), 1.65f); }
    CHECK(!g.isLatched(), "zle probki przerywaja odliczanie nadpradu (nie liczymy smieci jako pradu)"); }

  { // podczas bledu czujnika (silnik i tak stoi) prad nie jest oceniany, ale po zejsciu bledu ocena wraca
    CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 3; i++) { t += 20; g.onSample(t, 0.0f, 1.65f); }
    CHECK(g.state(t) == CurrentGuard::S_FAULT, "(kontrola) blad czujnika");
    for (int i = 0; i < 9; i++) { t += 20; g.onSample(t, vA(20.0f), 1.65f); }
    CHECK(g.state(t) == CurrentGuard::S_FAULT && !g.isLatched(), "9 probek 20 A podczas powrotu po bledzie: dalej BLAD, jeszcze bez zatrzasku nadpradowego");
    for (int i = 0; i < 3; i++) { t += 20; g.onSample(t, vA(20.0f), 1.65f); }
    CHECK(g.isLatched() && g.state(t) != CurrentGuard::S_FAULT, "po zejsciu bledu ocena wraca: utrzymujacy sie nadprad 20 A wyzwala zatrzask"); }

  // ---------- zatrzask: zdejmowanie ----------
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 25; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); } 
    for (int i = 0; i < 5; i++) { t += 20; g.onSample(t, vA(0.0f), 1.65f); }   // prad wrocil do zera
    for (int i = 0; i < 100; i++) { t += 20; g.update(t, true); g.onSample(t, vA(0.0f), 1.65f); }
    CHECK(g.isLatched(), "zatrzask trzyma mimo spadku pradu, dopoki AgOpenWeb ma prowadzenie WLACZONE (2 s)");
    unsigned long t0 = t;
    auto tick = [&](unsigned long now, bool eng, float amps) { g.update(now, eng); g.onSample(now, vA(amps), 1.65f); };   // jak w firmware: probki ida non stop
    for (unsigned long x = 0; x < 999; x += 20) tick(t0 + x, false, 0.0f);
    CHECK(g.isLatched(), "prowadzenie wylaczone i prad 0 przez 979 ms: jeszcze zatrzask");
    for (unsigned long x = 1000; x <= 1040; x += 20) tick(t0 + x, false, 0.0f);
    CHECK(!g.isLatched(), "prowadzenie wylaczone i prad 0 przez ponad 1 s: zatrzask zdjety sam");
    // timer zeruje sie, gdy prowadzenie wroci
    CurrentGuard h; h.configure({8.0f, 400, 15.0f}); unsigned long u = 1000; h.onSample(u, vA(0.0f), 1.65f);
    for (int i = 0; i < 25; i++) { u += 20; h.onSample(u, vA(9.0f), 1.65f); }
    unsigned long u0 = u; auto tk = [&](unsigned long now, bool eng) { h.update(now, eng); h.onSample(now, vA(0.0f), 1.65f); };
    for (unsigned long x = 0; x <= 800; x += 20) tk(u0 + x, false);          // 0,8 s wylaczone - jeszcze nie zeszlo
    tk(u0 + 900, true);                                                        // prowadzenie wraca: odliczanie od zera
    for (unsigned long x = 920; x <= 1880; x += 20) tk(u0 + x, false);        // znow wylaczone, od 920 do 1880 ms = 0,96 s
    CHECK(h.isLatched(), "chwilowe ponowne wlaczenie prowadzenia zeruje odliczanie: 0,96 s od powrotu to jeszcze za malo");
    for (unsigned long x = 1900; x <= 1940; x += 20) tk(u0 + x, false);       // przekroczona pelna 1 s od 920 ms
    CHECK(!h.isLatched(), "dopiero pelna 1 s ciaglego wylaczenia (od ostatniego wlaczenia) zdejmuje zatrzask"); }
  // ---------- zatrzask NIE schodzi, dopoki przyczyna trwa (przypadek z logu: niepodlaczony czujnik, stale -24 A) ----------
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, 0.59f, 1.65f);
    for (int i = 0; i < 400; i++) { t += 20; g.update(t, false); g.onSample(t, 0.59f, 1.65f); }      // 8 s, prowadzenie wylaczone, prad stale ok. -24 A
    CHECK(g.isLatched() && g.trips() == 1, "prad nadal -24 A przy wylaczonym prowadzeniu: zatrzask TRZYMA 8 s, jedno zadzialanie (bez flapowania)");
    CHECK(g.tripped(t) && g.state(t) == CurrentGuard::S_OVERCURRENT, "silnik caly czas zablokowany (brak okien bez blokady)"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 25; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); }                           // zatrzask
    for (int i = 0; i < 100; i++) { t += 20; g.update(t, false); g.onSample(t, vA(7.9f), 1.65f); }     // 7,9 A: tuz pod progiem 8 A
    CHECK(!g.isLatched(), "prad spada tuz pod prog miekki (7,9 A) i prowadzenie wylaczone: zatrzask schodzi");
    CurrentGuard h; h.configure({8.0f, 400, 15.0f}); unsigned long u = 1000; h.onSample(u, vA(0.0f), 1.65f);
    for (int i = 0; i < 25; i++) { u += 20; h.onSample(u, vA(9.0f), 1.65f); }
    for (int i = 0; i < 100; i++) { u += 20; h.update(u, false); h.onSample(u, vA(8.05f), 1.65f); }    // 8,05 A: tuz NAD progiem, przyczyna trwa
    CHECK(h.isLatched() && h.trips() == 1, "prad tuz nad progiem (8,05 A) przy wylaczonym prowadzeniu: zatrzask trzyma i NIE flapuje (jedno zadzialanie przez 2 s)"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 25; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); }
    for (int i = 0; i < 3; i++) { t += 20; g.onSample(t, vA(0.0f), 1.65f); }                           // prad spadl, potem CISZA (brak probek)
    unsigned long t0 = t; for (unsigned long x = 0; x < 5000; x += 20) g.update(t0 + x, false);
    CHECK(g.isLatched(), "brak swiezych probek (nie da sie potwierdzic, ze przyczyna zniknela): zatrzask trzyma mimo wylaczonego prowadzenia"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 25; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); }
    for (int i = 0; i < 3; i++) { t += 20; g.onSample(t, vA(0.5f), 1.65f); g.update(t, true); }        // prad spadl do 0,5 A (prowadzenie jeszcze wlaczone)
    for (int i = 0; i < 100; i++) { t += 20; g.update(t, false); g.onSample(t, 0.0f, 1.65f); }         // a potem czujnik zawodzi (0 V): to NIE jest potwierdzenie spadku
    CHECK(g.isLatched() && g.state(t) == CurrentGuard::S_FAULT, "prad spadl, ale czujnik zawiodl i prowadzenie wylaczone przez 2 s: zatrzask trzyma (blad czujnika)"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 25; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); }
    for (int i = 0; i < 100; i++) { t += 20; g.update(t, false); g.onSample(t, 0.0f, 1.65f); }         // czujnik niewiarygodny (0 V) zamiast prawdziwego spadku
    CHECK(g.isLatched() && g.state(t) == CurrentGuard::S_FAULT, "blad czujnika zamiast spadku pradu: zatrzask trzyma (to nie jest potwierdzenie), stan BLAD CZUJNIKA"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 25; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); }
    CHECK(g.isLatched() && g.peakAmps() > 8.9f, "zatrzask + szczyt 9 A"); g.reset();
    CHECK(!g.isLatched() && g.peakAmps() == 0.0f, "reczny reset: zatrzask i szczyt wyzerowane"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 25; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); } g.reset();
    bool again = false; for (int i = 0; i < 30 && !again; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); again = g.isLatched(); }
    CHECK(again, "po resecie dalszy nadprad (np. silnik wciaz zablokowany) znow wyzwala po kickoutMs"); }

  // ---------- konfiguracja, zero, zegar ----------
  { CurrentGuard g; g.configure({2.0f, 100, 5.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 8; i++) { t += 20; g.onSample(t, vA(3.0f), 1.65f); } CHECK(g.isLatched(), "nizsze progi (2 A / 100 ms) wyzwalaja przy 3 A"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; for (int i = 0; i < 30; i++) { t += 20; g.onSample(t, 1.70f, 1.70f); }
    CHECK(near(g.amps(), 0.0f, 0.01f) && !g.tripped(t), "zero skalibrowane na 1,70 V: odczyt 1,70 V = 0 A"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = ULONG_MAX - 200; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 40; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); }
    CHECK(t < 1000 && g.isLatched(), "zegar przekracza zawiniecie liczby bez znaku w trakcie odliczania - nadal wyzwala poprawnie");
    CHECK(g.tripped(t), "zatrzask aktywny po zawinieciu"); }
  { CurrentGuard g; unsigned long t = 1000; for (int i = 0; i < 10; i++) { t += 20; g.onSample(t, vA(i % 2 ? 4.0f : -6.0f), 1.65f); }
    CHECK(near(g.peakAmps(), 6.0f, 0.05f), "szczyt = najwieksza wartosc bezwzgledna (6 A)"); g.resetPeak(); CHECK(g.peakAmps() == 0.0f, "resetPeak"); }
  // ---------- liczniki i szczyt z ostatniej sekundy (dla Diagnostyki, Statusu, wykresow i historii) ----------
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000;
    CHECK(g.sampleCount() == 0 && g.trips() == 0 && g.recentPeakA(t) == 0.0f, "start: 0 probek, 0 zadzialan, szczyt 0");
    for (int i = 0; i < 50; i++) { t += 20; g.onSample(t, vA(1.0f), 1.65f); }
    CHECK(g.sampleCount() == 50, "licznik probek = 50");
    g.onSample(t + 1, 0.0f, 1.65f); CHECK(g.sampleCount() == 51, "zla probka (0 V) tez liczona jako probka");
    CHECK(near(g.recentPeakA(t), 1.0f, 0.05f), "szczyt z ostatniej sekundy = 1 A"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 10000; g.onSample(t, vA(0.0f), 1.65f);
    t += 20; g.onSample(t, vA(-6.0f), 1.65f);                                        // krotki pik -6 A (kierunek ujemny)
    for (int i = 0; i < 10; i++) { t += 20; g.onSample(t, vA(0.5f), 1.65f); }
    CHECK(near(g.recentPeakA(t), 6.0f, 0.05f), "pik -6 A widoczny jako 6 A (wartosc bezwzgledna)");
    unsigned long t0 = t; bool gone = false; unsigned long goneAt = 0;
    for (int i = 0; i < 100; i++) { t += 20; g.onSample(t, vA(0.5f), 1.65f); if (!gone && g.recentPeakA(t) < 1.0f) { gone = true; goneAt = t - t0; } }
    CHECK(gone && goneAt >= 800 && goneAt <= 1700, "pik znika z okna po ok. 1,2-1,6 s (nie wczesniej niz 0,8 s, nie pozniej niz 1,7 s)");
    CHECK(near(g.peakAmps(), 6.0f, 0.05f), "peakAmps (od skasowania) nadal pamieta 6 A - to inna wielkosc niz szczyt z sekundy"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 3; i++) { t += 20; g.onSample(t, 0.0f, 1.65f); }
    CHECK(g.recentPeakA(t) == 0.0f, "zle probki (blad czujnika) nie wnosza 'pradu' do szczytu"); }
  { CurrentGuard g; g.configure({8.0f, 400, 15.0f}); unsigned long t = 1000; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 300; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); }      // zatrzask (raz), prad nadal wysoki
    CHECK(g.trips() == 1, "zatrzask zalozony RAZ mimo 300 probek nadpradu (licznik nie rosnie z kazda probka)");
    g.reset(); for (int i = 0; i < 40; i++) { t += 20; g.onSample(t, vA(9.0f), 1.65f); }
    CHECK(g.trips() == 2 && g.sampleCount() == 341, "po resecie i ponownym zadzialaniu: 2 zadzialania; licznik probek nie jest zerowany resetem"); }
  { CurrentGuard g; unsigned long t = ULONG_MAX - 500; g.onSample(t, vA(0.0f), 1.65f);
    for (int i = 0; i < 60; i++) { t += 20; g.onSample(t, vA(3.0f), 1.65f); }      // zegar zawija sie w trakcie
    CHECK(g.recentPeakA(t) >= 2.9f, "szczyt dziala przez zawiniecie zegara"); }
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
