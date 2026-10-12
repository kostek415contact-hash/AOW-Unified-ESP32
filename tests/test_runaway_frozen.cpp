// Testy wykrywania zamrozonego czujnika kata w detektorze ucieczki (actuators/runaway_guard.h, stan S_FROZEN).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_runaway_frozen.cpp -o /tmp/test_runaway_frozen && /tmp/test_runaway_frozen
#include <cstdio>
#include "../actuators/runaway_guard.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c, msg) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, msg); } while (0)

static const unsigned long STEP_MS = 30;   // probkowanie czujnika kata jak w firmware

int main() {
    using G = RunawayGuard;
    const RunawayGuard::Params P{true, 1000, 1.0f, 3.0f};

    // 1) zamrozony odczyt przy napedzie: stop szybciej niz okno 1000 ms
    {
        G g; g.configure(P);
        unsigned long t = 1000; uint32_t seq = 0; unsigned long trippedAt = 0;
        for (int i = 0; i < 60 && !g.isTripped(); i++) {
            t += STEP_MS; seq++;
            g.update(t, true, 100, 10.0f, 0.0f, seq, 12345);
            if (g.isTripped()) trippedAt = t - 1000;
        }
        CHECK(g.isTripped() && g.state() == G::S_FROZEN, "zamrozony odczyt => S_FROZEN");
        CHECK(trippedAt >= 300 && trippedAt <= 450, "zadzialanie po ok. 0,4 s (13 probek co 30 ms)");
        CHECK(trippedAt < P.windowMs, "szybciej niz okno braku ruchu");
        CHECK(g.lastReason() == G::S_FROZEN && g.trips() == 1, "przyczyna ostatniego zadzialania zapamietana");
    }
    // 2) zywy sygnal z szumem +-1 LSB nigdy nie daje S_FROZEN (kat tez stoi => dopiero okno 1 s da BRAK POSTEPU)
    {
        G g; g.configure(P);
        unsigned long t = 1000; uint32_t seq = 0; bool frozen = false;
        for (int i = 0; i < 100; i++) {
            t += STEP_MS; seq++;
            int16_t raw = (int16_t)(12345 + ((i % 3) - 1));
            g.update(t, true, 100, 10.0f, 0.0f, seq, raw);
            if (g.state() == G::S_FROZEN) frozen = true;
        }
        CHECK(!frozen, "szum +-1 LSB nie jest zamrozeniem");
        CHECK(g.state() == G::S_NO_PROGRESS, "stojacy kat z szumem: dalej dziala stare okno braku ruchu");
    }
    // 3) naprzemienne dwie wartosci tez nie sa zamrozeniem
    {
        G g; g.configure(P);
        unsigned long t = 1000; uint32_t seq = 0;
        for (int i = 0; i < 12 * 3; i++) { t += STEP_MS; seq++; g.update(t, true, 100, 10.0f, 0.0f, seq, (int16_t)((i & 1) ? 100 : 101)); }
        CHECK(g.state() != G::S_FROZEN, "naprzemienne wartosci nie sa zamrozeniem");
    }
    // 4) identyczny odczyt PRZED napedem (stojace kolo) nie liczy sie
    {
        G g; g.configure(P);
        unsigned long t = 1000; uint32_t seq = 0;
        for (int i = 0; i < 200; i++) { t += STEP_MS; seq++; g.update(t, false, 0, 0.0f, 0.0f, seq, 500); }
        bool tripped = false;
        for (int i = 0; i < 10; i++) {   // start napedu, odczyt zaczyna sie ruszac juz od 2. probki
            t += STEP_MS; seq++; g.update(t, true, 100, 10.0f, 0.0f, seq, (int16_t)(500 + i * 3));
            if (g.isTripped()) tripped = true;
        }
        CHECK(!tripped, "dlugi bezruch przed wlaczeniem napedu nie powoduje natychmiastowego STOP");
    }
    // 5) bez numeru probki (sampleSeq = 0) zamrozenie nie jest sprawdzane (zgodnosc wsteczna)
    {
        G g; g.configure(P);
        unsigned long t = 1000;
        for (int i = 0; i < 20; i++) { t += STEP_MS; g.update(t, true, 100, 10.0f, 0.0f); }
        CHECK(g.state() != G::S_FROZEN && !g.isTripped(), "bez numeru probki brak kontroli zamrozenia (a okno 1 s jeszcze nie minelo)");
    }
    // 6) ten sam numer probki wolany wiele razy (petla kreci sie szybciej niz ADC) nie liczy sie jako kolejne identyczne probki
    {
        G g; g.configure(P);
        unsigned long t = 1000;
        for (int i = 0; i < 90; i++) { t += 5; g.update(t, true, 100, 10.0f, 0.0f, 7, 4242); }   // 450 ms, ciagle ta sama probka
        CHECK(g.state() != G::S_FROZEN, "powtarzanie tej samej probki nie jest zamrozeniem");
    }
    // 7) przerwa w napedzie zeruje serie
    {
        G g; g.configure(P);
        unsigned long t = 1000; uint32_t seq = 0;
        for (int i = 0; i < 8; i++) { t += STEP_MS; seq++; g.update(t, true, 100, 10.0f, 0.0f, seq, 900); }
        t += STEP_MS; seq++; g.update(t, true, 0, 10.0f, 0.0f, seq, 900);            // pwm = 0: nie napedzamy
        for (int i = 0; i < 8; i++) { t += STEP_MS; seq++; g.update(t, true, 100, 10.0f, 0.0f, seq, 900); }
        CHECK(!g.isTripped(), "8 + przerwa + 8 identycznych probek: seria sie zerowala");
    }
    // 8) blisko celu (blad < minErrorDeg) nie oceniamy
    {
        G g; g.configure(P);
        unsigned long t = 1000; uint32_t seq = 0;
        for (int i = 0; i < 40; i++) { t += STEP_MS; seq++; g.update(t, true, 100, 1.0f, 0.0f, seq, 77); }
        CHECK(!g.isTripped(), "blad ponizej progu - brak oceny");
    }
    // 9) zatrzask: trzyma, schodzi po wylaczeniu prowadzenia (RELEASE_MS) i po resecie; potem seria liczy sie od zera
    {
        G g; g.configure(P);
        unsigned long t = 1000; uint32_t seq = 0;
        for (int i = 0; i < 20; i++) { t += STEP_MS; seq++; g.update(t, true, 100, 10.0f, 0.0f, seq, 5); }
        CHECK(g.state() == G::S_FROZEN, "zatrzasniete");
        g.tick(t, true); CHECK(g.isTripped(), "przy wlaczonym prowadzeniu zatrzask trzyma");
        g.tick(t + 10, false); g.tick(t + 10 + G::RELEASE_MS, false);
        CHECK(!g.isTripped(), "po 1 s bez prowadzenia zatrzask schodzi");
        // po zdjeciu zatrzasku te same identyczne probki: potrzeba pelnej serii od nowa
        bool early = false;
        for (int i = 0; i < 5; i++) { t += STEP_MS; seq++; g.update(t, true, 100, 10.0f, 0.0f, seq, 5); if (g.isTripped()) early = true; }
        CHECK(!early, "seria po zdjeciu zatrzasku liczy sie od zera");
        g.reset(); CHECK(!g.isTripped(), "reset z panelu");
    }
    // 10) wylaczony detektor: nic nie robi
    {
        G g; RunawayGuard::Params off = P; off.enabled = false; g.configure(off);
        unsigned long t = 1000; uint32_t seq = 0;
        for (int i = 0; i < 40; i++) { t += STEP_MS; seq++; g.update(t, true, 100, 10.0f, 0.0f, seq, 5); }
        CHECK(!g.isTripped() && g.state() == G::S_OFF, "wylaczony detektor nie zatrzymuje silnika");
    }

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
