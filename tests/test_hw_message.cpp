// Testy komunikatu sprzętowego PGN 221 (connectivity/hw_message.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_hw_message.cpp -o /tmp/test_hw_message && /tmp/test_hw_message
#include <cstdio>
#include <cstring>
#include "../connectivity/hw_message.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

int main() {
    // 1. Ramka: naglowek, PGN, dlugosc = tekst + 2, sekundy, kolor 0 = ostrzezenie, tekst, suma kontrolna
    {   uint8_t f[80];
        size_t n = HwMessage::build(f, sizeof(f), 126, "IMU lost", 8, true);
        CHECK(n == 5 + 2 + 8 + 1);
        CHECK(f[0] == 0x80 && f[1] == 0x81 && f[2] == 126 && f[3] == 221 && f[4] == 10);
        CHECK(f[5] == 8 && f[6] == 0 && memcmp(f + 7, "IMU lost", 8) == 0);
        uint8_t crc = 0; for (size_t i = 2; i < n - 1; i++) crc = (uint8_t)(crc + f[i]);
        CHECK(f[n - 1] == crc);
        // tak jak czyta AgOpenWeb: n = min(len-2, dlugosc-8)
        int tn = f[4] - 2; if (tn > (int)n - 8) tn = (int)n - 8;
        CHECK(tn == 8);
        size_t m = HwMessage::build(f, sizeof(f), 126, "ok", 3, false);
        CHECK(m == 10 && f[6] == 1);
    }
    // 2. Za dlugi tekst jest ucinany, za maly bufor i pusty tekst = 0
    {   uint8_t f[100]; char big[200]; memset(big, 'x', sizeof(big)); big[199] = 0;
        size_t n = HwMessage::build(f, sizeof(f), 126, big, 5, true);
        CHECK(n == 5 + 2 + HwMessage::MAX_TEXT + 1 && f[4] == HwMessage::MAX_TEXT + 2);
        uint8_t small[8];
        CHECK(HwMessage::build(small, sizeof(small), 126, "abcdef", 5, true) == 0);
        CHECK(HwMessage::build(f, sizeof(f), 126, "", 5, true) == 0 && HwMessage::build(f, sizeof(f), 126, nullptr, 5, true) == 0);
    }
    // 3. Bramka zbocza: raz przy starcie warunku, powtorka co repeatMs, zwolnienie po ustaniu
    {   HwMessage::AlertGate g;
        CHECK(!g.update(false, 0, 1000));
        CHECK(g.update(true, 100, 1000));            // zbocze
        CHECK(!g.update(true, 600, 1000));           // jeszcze za wczesnie
        CHECK(g.update(true, 1100, 1000));           // powtorka
        CHECK(!g.update(true, 1500, 1000));
        CHECK(!g.update(false, 1600, 1000));         // warunek ustal
        CHECK(g.update(true, 1700, 1000));           // nowe zbocze od razu
    }
    // 4. Powtorka dziala tez przy przekroczeniu zakresu millis()
    {   HwMessage::AlertGate g;
        CHECK(g.update(true, 4294967000u, 1000));
        CHECK(!g.update(true, 4294967290u, 1000));
        CHECK(g.update(true, 800u, 1000));           // 1096 ms pozniej (przejscie przez 0)
    }
    // 5. „Bylo dobrze, teraz zepsute”: sprzet, ktory nigdy nie dzialal, nie alarmuje
    {   HwMessage::LostAfterOk l;
        CHECK(!l.update(false) && !l.update(false));
        CHECK(!l.update(true));
        CHECK(l.update(false) && l.update(false));
        CHECK(!l.update(true));
    }
    // 7. Komunikat po nienormalnym restarcie: tylko awaria / watchdog / spadek napiecia; zwykly start milczy; tekst miesci sie w ramce
    {   CHECK(HwMessage::bootAlertText(HwMessage::BK_NORMAL) == nullptr);
        for (int k = 1; k <= 3; k++) {
            const char* t = HwMessage::bootAlertText((HwMessage::BootKind)k);
            CHECK(t != nullptr && strlen(t) <= HwMessage::MAX_TEXT);
            uint8_t f[80]; CHECK(HwMessage::build(f, sizeof(f), 126, t, 10, true) == 5 + 2 + strlen(t) + 1);
        }
    }
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
