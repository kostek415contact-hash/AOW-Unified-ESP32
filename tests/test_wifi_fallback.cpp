// Testy logiki awaryjnego restartu przy utracie WiFi (bez Arduino). Uruchomienie z katalogu glownego:
//     g++ -std=c++17 -Wall -Wextra tests/test_wifi_fallback.cpp -o /tmp/test_wifi_fallback && /tmp/test_wifi_fallback
#include <cstdio>
#include "../connectivity/wifi_fallback.h"
using namespace WifiFallback;
static int fails = 0, total = 0;
#define CHECK(cond, name) do { total++; if (!(cond)) { fails++; printf("  BLAD: %s\n", name); } } while (0)
static const unsigned long TO = 15000;   // WIFI_CONNECT_TIMEOUT_MS

int main() {
    // ---- licznik po starcie ----
    CHECK(countAtBoot(true, MAGIC, 1) == 1, "restart zlecony przez mechanizm: licznik przechodzi");
    CHECK(countAtBoot(false, MAGIC, 1) == 0, "start po panice/zasilaniu (nie SW): licznik 0");
    CHECK(countAtBoot(true, 0, 1) == 0, "zwykly restart programowy (np. z panelu, bez znacznika): licznik 0");
    CHECK(countAtBoot(true, 0xDEADBEEF, 2) == 0, "smieci w pamieci RTC po wlaczeniu zasilania: licznik 0");
    CHECK(shouldRestart(0) && shouldRestart(1) && !shouldRestart(2) && !shouldRestart(99), "shouldRestart: 0 i 1 tak, 2 i wiecej nie");

    // ---- przebieg: siec znika, restart, znika ponownie, restart, znika trzeci raz => poddanie sie ----
    { Monitor m; m.setBootCount(0); unsigned long t = 1000;
        for (int i = 0; i < 20; i++, t += 1000) CHECK(m.update(true, t, TO) == Monitor::NONE, "polaczone: nic sie nie dzieje");
        unsigned long lost = t;
        Monitor::Action a = Monitor::NONE; unsigned long when = 0;
        for (; t < lost + 60000; t += 100) { a = m.update(false, t, TO); if (a != Monitor::NONE) { when = t - lost; break; } }
        CHECK(a == Monitor::RESTART && when >= TO && when < TO + 200, "utrata sieci => RESTART po WIFI_CONNECT_TIMEOUT_MS (15 s), nie wczesniej");
        CHECK(m.count() == 0, "licznik w monitorze jeszcze 0 (zwieksza go network_manager przed restartem: count+1)");
        // --- po restarcie #1: nowy start, licznik 1 ---
        Monitor m2; m2.setBootCount(1); t = 1000; for (int i = 0; i < 10; i++, t += 1000) m2.update(true, t, TO);
        lost = t; a = Monitor::NONE; for (; t < lost + 60000; t += 100) { a = m2.update(false, t, TO); if (a != Monitor::NONE) break; }
        CHECK(a == Monitor::RESTART, "2. utrata (licznik 1) => nadal RESTART");
        // --- po restarcie #2: licznik 2 => poddanie sie ---
        Monitor m3; m3.setBootCount(2); t = 1000; for (int i = 0; i < 10; i++, t += 1000) m3.update(true, t, TO);
        lost = t; a = Monitor::NONE; for (; t < lost + 60000; t += 100) { a = m3.update(false, t, TO); if (a != Monitor::NONE) break; }
        CHECK(a == Monitor::GIVE_UP, "3. utrata (licznik 2) => GIVE_UP zamiast petli restartow");
        int extra = 0; for (; t < lost + 300000; t += 100) if (m3.update(false, t, TO) != Monitor::NONE) extra++;
        CHECK(extra == 0 && m3.gaveUp(), "po poddaniu sie: cisza (bez powtarzania wpisu w dzienniku i bez restartu)");
    }
    // ---- siec wraca i jest stabilna => pula restartow sie odnawia ----
    { Monitor m; m.setBootCount(2); unsigned long t = 1000;
        for (; t < 400000; t += 100) if (m.update(false, t, TO) != Monitor::NONE) break;   // poddaje sie
        CHECK(m.gaveUp(), "poddal sie");
        unsigned long back = t;
        for (; t < back + STABLE_MS - 1000; t += 100) m.update(true, t, TO);
        CHECK(m.count() == 2 && m.gaveUp(), "krotko po powrocie sieci (< 120 s) licznik NIE jest jeszcze zerowany");
        for (; t < back + STABLE_MS + 1000; t += 100) m.update(true, t, TO);
        CHECK(m.count() == 0 && !m.gaveUp(), "po 120 s ciaglej lacznosci: licznik 0 i pelna pula restartow");
        unsigned long lost = t; Monitor::Action a = Monitor::NONE;
        for (; t < lost + 60000; t += 100) { a = m.update(false, t, TO); if (a != Monitor::NONE) break; }
        CHECK(a == Monitor::RESTART, "kolejna utrata po odnowieniu puli => znowu RESTART");
    }
    // ---- niestabilna siec: rozlaczenia krotsze niz timeout nie wywoluja restartu ----
    { Monitor m; m.setBootCount(0); unsigned long t = 1000; int restarts = 0;
        for (int cycle = 0; cycle < 30; cycle++) {
            for (int i = 0; i < 100; i++, t += 100) if (m.update(false, t, TO) != Monitor::NONE) restarts++;   // 10 s bez sieci
            for (int i = 0; i < 50; i++, t += 100) m.update(true, t, TO);                                       // 5 s z siecia
        }
        CHECK(restarts == 0, "migotanie sieci (10 s bez / 5 s z, 30 cykli) nie wywoluje restartu");
    }
    // ---- migotanie z krotkimi powrotami resetuje odliczanie ----
    { Monitor m; unsigned long t = 1000; int restarts = 0;
        for (int i = 0; i < 140; i++, t += 100) if (m.update(false, t, TO) != Monitor::NONE) restarts++;   // 14 s bez sieci
        m.update(true, t, TO); t += 100;                                                                     // chwila polaczenia
        for (int i = 0; i < 140; i++, t += 100) if (m.update(false, t, TO) != Monitor::NONE) restarts++;   // znowu 14 s
        CHECK(restarts == 0, "krotki powrot sieci zeruje odliczanie (2 x 14 s to nie 15 s ciaglych)");
    }
    // ---- przekrecenie licznika millis() (co ok. 49 dni): odliczanie musi trwac PELNE 15 s ----
    { Monitor m; uint32_t t = 0xFFFFFFFFu - 5000u; Monitor::Action a = Monitor::NONE; uint32_t start = t, when = 0;
        for (int i = 0; i < 400; i++, t += 100) { a = m.update(false, t, TO); if (a != Monitor::NONE) { when = t - start; break; } }
        CHECK(a == Monitor::RESTART && when >= TO && when < TO + 200, "przekrecenie millis() w trakcie odliczania: restart po pelnych 15 s (nie wczesniej)");
        Monitor n; uint32_t u = 0xFFFFFFFFu - 60000u; n.setBootCount(1);
        for (int i = 0; i < 2000; i++, u += 100) n.update(true, u, TO);       // ok. 200 s ciaglej lacznosci, z przekreceniem po drodze
        CHECK(n.count() == 0, "przekrecenie millis() w trakcie stabilnej lacznosci: licznik i tak zerowany po 120 s");
    }
    // ---- sygnal "postep" (failover przelaczyl na inna siec): pelny czas dla nowej sieci, twardy limit 3 x ----
    { Monitor m; unsigned long t = 1000; unsigned long lost = t; Monitor::Action a = Monitor::NONE; unsigned long when = 0;
        for (; t < lost + 10000; t += 100) m.update(false, t, TO);
        m.noteProgress(t);                                                   // po 10 s przelaczono na inna siec
        for (; t < lost + 24000; t += 100) if (m.update(false, t, TO) != Monitor::NONE) a = Monitor::RESTART;
        CHECK(a == Monitor::NONE, "po sygnale postepu (w 10 s) brak restartu w 15 s od utraty - nowa siec dostaje pelne 15 s");
        for (; t < lost + 40000; t += 100) { a = m.update(false, t, TO); if (a != Monitor::NONE) { when = t - lost; break; } }
        CHECK(a == Monitor::RESTART && when >= 25000 && when < 25200, "restart dopiero 15 s PO sygnale postepu (25 s od utraty)");
    }
    { Monitor m; unsigned long t = 1000; unsigned long lost = t; Monitor::Action a = Monitor::NONE; unsigned long when = 0; int notes = 0;
        for (; t < lost + 120000; t += 100) {
            if ((t - lost) % 10000 == 0 && t != lost) { m.noteProgress(t); notes++; }     // ciagle "przelacza" co 10 s
            a = m.update(false, t, TO); if (a != Monitor::NONE) { when = t - lost; break; }
        }
        CHECK(a == Monitor::RESTART && when >= 45000 && when < 45200, "ciagle przelaczanie nie przedluza w nieskonczonosc: twardy limit 3 x 15 s = 45 s");
        CHECK(notes >= 3, "(w tym czasie zgloszono kilka sygnalow postepu)");
    }
    { Monitor m; unsigned long t = 1000;
        for (int i = 0; i < 50; i++, t += 100) m.update(true, t, TO);
        m.noteProgress(t);                                                   // przy polaczonej sieci sygnal nic nie znaczy
        unsigned long lost = t; Monitor::Action a = Monitor::NONE; unsigned long when = 0;
        for (; t < lost + 40000; t += 100) { a = m.update(false, t, TO); if (a != Monitor::NONE) { when = t - lost; break; } }
        CHECK(a == Monitor::RESTART && when >= TO && when < TO + 200, "sygnal postepu przy polaczonej sieci jest ignorowany (nie skraca ani nie wydluza kolejnego odliczania)");
    }
    printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
    return fails ? 1 : 0;
}
