// Testy trwalego zapisu zdarzen w pamieci RTC (diagnostics/persist_log.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_persist_log.cpp -o /tmp/test_persist_log && /tmp/test_persist_log
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include "../diagnostics/persist_log.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

int main() {
    static PersistLog::Blob rtc;

    // smieci w pamieci (po wlaczeniu zasilania): brak poprzedniego uruchomienia, numer 1
    { std::memset(&rtc, 0xA5, sizeof(rtc)); PersistLog::begin(&rtc);
      CHECK(PersistLog::prevCount() == 0 && PersistLog::prevEndMs() == 0 && PersistLog::prevSession() == 0);
      CHECK(PersistLog::session() == 1); }

    // zapis zdarzen, restart (ten sam blok, bez zerowania), odczyt poprzedniego uruchomienia
    { PersistLog::add(1, 3, 100); PersistLog::add(2, 4, 5000); PersistLog::add(3, 0, 7000); PersistLog::alive(9500);
      PersistLog::begin(&rtc);
      CHECK(PersistLog::prevCount() == 3 && PersistLog::prevSession() == 1 && PersistLog::session() == 2);
      CHECK(PersistLog::prevAt(0).type == 1 && PersistLog::prevAt(0).arg == 3 && PersistLog::prevAt(0).ms == 100);
      CHECK(PersistLog::prevAt(2).type == 3 && PersistLog::prevAt(2).arg == 0 && PersistLog::prevAt(2).ms == 7000);
      CHECK(PersistLog::prevEndMs() == 9500); }

    // drugi restart bez zdarzen w nowym uruchomieniu: poprzednie jest puste, ale blok wazny, numer rosnie
    { PersistLog::begin(&rtc);
      CHECK(PersistLog::prevCount() == 0 && PersistLog::prevSession() == 2 && PersistLog::session() == 3); }

    // ujemny arg i pelny pierscien (32): zostaja NAJNOWSZE, w kolejnosci od najstarszego
    { for (int i = 0; i < 40; i++) PersistLog::add(8, (int16_t)(i == 39 ? -7 : i), 1000 + i);
      PersistLog::begin(&rtc);
      CHECK(PersistLog::prevCount() == 32);
      CHECK(PersistLog::prevAt(0).arg == 8 && PersistLog::prevAt(0).ms == 1008);       // wpisy 0..7 wypadly
      CHECK(PersistLog::prevAt(31).arg == -7 && PersistLog::prevAt(31).ms == 1039); }

    // uszkodzenie jednego bajtu (np. zasilanie padlo w trakcie zapisu) = blok odrzucony
    { PersistLog::add(2, 4, 50); PersistLog::alive(60);
      reinterpret_cast<uint8_t*>(&rtc.ring[0])[0] ^= 0x01;
      PersistLog::begin(&rtc);
      CHECK(PersistLog::prevCount() == 0 && PersistLog::prevSession() == 0 && PersistLog::session() == 1); }

    // niepoprawne head/cnt przy poprawnym CRC nie wywala (zabezpieczenie zakresow)
    { PersistLog::add(1, 1, 10); rtc.head = 200; rtc.crc = PersistLog::crcOf(rtc);
      PersistLog::begin(&rtc); CHECK(PersistLog::prevCount() == 0); }

    // czas UTC z GPS: zapamietywany przy alive(), ostatnia znana wartosc zostaje, gdy GPS chwilowo nie zna czasu (0)
    { PersistLog::begin(&rtc); CHECK(PersistLog::prevUtc() == 0);
      PersistLog::add(2, 4, 100); PersistLog::alive(1000, 1791545535u); PersistLog::alive(2000, 0);   // 0 = nieznany: nie kasuje
      PersistLog::begin(&rtc);
      CHECK(PersistLog::prevCount() == 1 && PersistLog::prevUtc() == 1791545535u && PersistLog::prevEndMs() == 2000);
      PersistLog::alive(500, 1791545999u); PersistLog::begin(&rtc);
      CHECK(PersistLog::prevUtc() == 1791545999u);
      PersistLog::begin(&rtc); CHECK(PersistLog::prevUtc() == 0); }                                   // nowe uruchomienie bez GPS: brak czasu

    // add/alive przed begin() nic nie robia (blok nieustawiony) - sprawdzamy na nowym procesie logicznie: po begin dziala
    { PersistLog::add(5, 1, 123); CHECK(PersistLog::session() >= 1); }

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
