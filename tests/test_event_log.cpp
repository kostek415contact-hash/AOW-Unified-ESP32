// Testy dziennika zdarzen i detektora (diagnostics/event_log.h, event_detector.h). Czysty C++.
// Kompilacja: g++ -std=c++17 -Wall -Wextra tests/test_event_log.cpp -o /tmp/test_event_log && /tmp/test_event_log
#include <cstdio>
#include "../diagnostics/event_detector.h"

static int g_ok = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_ok++; } else { g_fail++; std::printf("BLAD: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)

static int lockCalls = 0, unlockCalls = 0;
static void lk() { lockCalls++; }
static void ul() { unlockCalls++; }

static EventInputs base() { return EventInputs{4, true, true, true, true, 0, false, false, 0}; }
static uint8_t snap(EventLog::Event* e) { return EventLog::snapshot(e, EventLog::MAX); }

int main() {
    using E = EventLog;
    EventLog::Event ev[E::MAX];

    // dziennik: kolejnosc, ring, blokada
    E::reset(); E::setLock(lk, ul);
    E::add(E::EV_BOOT, 1, 100);
    E::add(E::EV_FIX, 4, 200);
    uint8_t n = snap(ev);
    CHECK(n == 2 && ev[0].type == E::EV_BOOT && ev[0].ms == 100 && ev[1].type == E::EV_FIX && ev[1].arg == 4, "kolejnosc od najstarszego");
    CHECK(lockCalls >= 3 && lockCalls == unlockCalls, "blokada zakladana i zwalniana w parach");
    for (int i = 0; i < 100; i++) E::add(E::EV_NET, i & 1, 1000 + i);
    n = snap(ev);
    CHECK(n == E::MAX && E::totalAdded() == 102, "pierscien: max 64, licznik lacznie 102");
    CHECK(ev[E::MAX - 1].ms == 1099 && ev[0].ms == 1000 + 100 - E::MAX, "pierscien: zostaja najnowsze, kolejnosc zachowana");
    CHECK(E::snapshot(ev, 5) == 5 && ev[4].ms == 1099, "snapshot z limitem: 5 najnowszych");
    E::setLock(nullptr, nullptr);

    // detektor: stan wyjsciowy bez zdarzen
    E::reset(); EventDetector d; EventInputs in = base();
    d.update(1000, in);
    CHECK(snap(ev) == 0, "pierwsze wywolanie: tylko stan wyjsciowy, bez zdarzen");
    d.update(2000, in); d.update(3000, in);
    CHECK(snap(ev) == 0, "bez zmian: bez zdarzen");

    // fix: potwierdzenie przez 2 probki
    in.fixLevel = 3; d.update(4000, in);
    CHECK(snap(ev) == 0, "fix: jedna probka FLOAT jeszcze nie zdarzenie");
    in.fixLevel = 4; d.update(5000, in);
    CHECK(snap(ev) == 0, "fix: migniecie 4->3->4 nie tworzy zdarzenia");
    in.fixLevel = 3; d.update(6000, in); d.update(7000, in);
    n = snap(ev);
    CHECK(n == 1 && ev[0].type == E::EV_FIX && ev[0].arg == 3 && ev[0].ms == 7000, "fix: FLOAT utrzymany 2 s = zdarzenie z czasem drugiej probki");
    in.fixLevel = 0; d.update(8000, in); in.fixLevel = 3; d.update(9000, in);
    CHECK(snap(ev) == 1, "fix: zmienna wartosc (0 potem 3) nie potwierdza zadnej zmiany");
    in.fixLevel = 4; d.update(10000, in); d.update(11000, in);
    n = snap(ev);
    CHECK(n == 2 && ev[1].arg == 4, "fix: powrot do FIX po 2 s");
    in.fixLevel = -1; d.update(12000, in); d.update(13000, in);
    CHECK(snap(ev) == 2, "fix nieznany (-1) pomijany");

    // NTRIP
    E::reset(); in = base(); EventDetector d2; d2.update(0, in);
    in.ntripStreaming = false; d2.update(1000, in);
    n = snap(ev);
    CHECK(n == 1 && ev[0].type == E::EV_NTRIP && ev[0].arg == 0, "NTRIP utracony");
    in.ntripStreaming = true; d2.update(2000, in);
    n = snap(ev);
    CHECK(n == 2 && ev[1].arg == 1, "NTRIP wrocil");
    in.ntripEnabled = false; in.ntripStreaming = false; d2.update(3000, in);
    CHECK(snap(ev) == 2, "wylaczenie NTRIP w ustawieniach nie jest utrata");
    in.ntripEnabled = true; d2.update(4000, in);
    CHECK(snap(ev) == 2, "samo wlaczenie NTRIP w ustawieniach (strumien jeszcze nie plynie) nie jest zdarzeniem");
    in.ntripStreaming = true; d2.update(5000, in);
    n = snap(ev);
    CHECK(n == 3 && ev[2].arg == 1, "po wlaczeniu strumien ruszyl = zdarzenie 'NTRIP plynie'");
    in.ntripStreaming = false; d2.update(6000, in);
    CHECK(snap(ev) == 4, "po ponownym wlaczeniu utrata liczy sie normalnie");

    // siec, AgOpenWeb, prowadzenie
    E::reset(); in = base(); EventDetector d3; d3.update(0, in);
    in.netUp = false; d3.update(1000, in); in.netUp = true; d3.update(2000, in);
    in.agioActive = false; d3.update(3000, in); in.agioActive = true; d3.update(4000, in);
    in.agioSwitches = 1; d3.update(5000, in);
    in.steerOn = true; d3.update(6000, in); in.steerOn = false; d3.update(7000, in);
    n = snap(ev);
    CHECK(n == 7, "siec x2, AgOpenWeb x2, przelaczenie nadawcy, prowadzenie x2 = 7 zdarzen");
    CHECK(ev[0].type == E::EV_NET && ev[0].arg == 0 && ev[1].type == E::EV_NET && ev[1].arg == 1, "siec: utrata, powrot");
    CHECK(ev[2].type == E::EV_AGIO && ev[2].arg == 0 && ev[3].type == E::EV_AGIO && ev[3].arg == 1, "AgOpenWeb: zamilkl, wrocil");
    CHECK(ev[4].type == E::EV_AGIO_SWITCH, "zmiana nadawcy");
    CHECK(ev[5].type == E::EV_STEER && ev[5].arg == 1 && ev[6].type == E::EV_STEER && ev[6].arg == 0, "prowadzenie wlaczone, wylaczone");

    // silnik: zatrzask raz na wlaczenie, licznik ucieczek
    E::reset(); in = base(); EventDetector d4; d4.update(0, in);
    in.currentLatched = true; d4.update(1000, in); d4.update(2000, in); d4.update(3000, in);
    n = snap(ev);
    CHECK(n == 1 && ev[0].type == E::EV_MOTOR && ev[0].arg == 1, "zatrzask nadpradowy: jedno zdarzenie, nie co sekunde");
    in.currentLatched = false; d4.update(4000, in); in.currentLatched = true; d4.update(5000, in);
    CHECK(snap(ev) == 2, "ponowne zadzialanie zatrasku = nowe zdarzenie");
    in.runawayTrips = 1; d4.update(6000, in); d4.update(7000, in);
    n = snap(ev);
    CHECK(n == 3 && ev[2].arg == 2, "ucieczka: jedno zdarzenie na jedno zadzialanie");
    in.runawayTrips = 0; d4.update(8000, in);
    CHECK(snap(ev) == 3, "wyzerowanie licznika (po zmianie ustawien) nie jest zdarzeniem");
    in.runawayTrips = 1; d4.update(9000, in);
    CHECK(snap(ev) == 4, "po wyzerowaniu kolejne zadzialanie liczy sie");

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_ok + g_fail);
    return g_fail == 0 ? 0 : 1;
}
