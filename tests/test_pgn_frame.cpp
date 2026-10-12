// Testy pomocnikow ramek PGN (connectivity/pgn_frame.h): suma kontrolna i odrzucanie komend sterujacych z bledna suma.
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_pgn_frame.cpp -o /tmp/test_pgn_frame && /tmp/test_pgn_frame
#include <cstdio>
#include <cstdint>
#include "../connectivity/pgn_frame.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

static void seal(uint8_t* b, int len) { uint8_t c = 0; for (int i = 2; i < len - 1; i++) c = (uint8_t)(c + b[i]); b[len - 1] = c; }

int main() {
    using namespace PgnFrame;
    // komenda steer PGN 254, 8 bajtow danych => ramka 14 B
    uint8_t steer[14] = {0x80, 0x81, 0x7F, 254, 8, 10, 0, 1, 0x20, 0x03, 5, 0xFF, 0x00, 0};
    seal(steer, 14);
    CHECK(crcCheck(steer, 14) == 1);
    CHECK(!shouldRejectOnCrc(254, crcCheck(steer, 14)));
    steer[8] ^= 0x01;                                   // uszkodzony bajt danych
    CHECK(crcCheck(steer, 14) == 0);
    CHECK(shouldRejectOnCrc(254, crcCheck(steer, 14)));
    CHECK(shouldRejectOnCrc(239, 0) && shouldRejectOnCrc(252, 0));
    CHECK(!shouldRejectOnCrc(200, 0) && !shouldRejectOnCrc(202, 0));   // Hello / Scan bez zmian
    CHECK(!shouldRejectOnCrc(215, 0));                                 // RTCM bez zmian
    CHECK(shouldRejectOnCrc(251, 0) && shouldRejectOnCrc(236, 0) && shouldRejectOnCrc(251, -1) && shouldRejectOnCrc(236, -1) && !shouldRejectOnCrc(236, 1));   // konfiguracja steer i piny tez sterujace
    CHECK(!shouldRejectOnCrc(254, 1) && !shouldRejectOnCrc(238, 1));   // poprawna suma: przyjmujemy
    CHECK(shouldRejectOnCrc(254, -1) && shouldRejectOnCrc(239, -1) && shouldRejectOnCrc(252, -1) && shouldRejectOnCrc(238, -1));  // brak bajtu CRC: nie do zweryfikowania
    CHECK(shouldRejectOnCrc(238, 0) && !shouldRejectOnCrc(200, -1) && !shouldRejectOnCrc(215, -1));  // 238 tez sterujacy; Hello i RTCM bez zmian
    CHECK(crcCheck(steer, 5) == -1);                                   // za krotka
    uint8_t shortFrame[8] = {0x80, 0x81, 0x7F, 254, 8, 0, 0, 0};
    CHECK(crcCheck(shortFrame, 8) == -1);                              // deklaruje 8 bajtow, jest mniej
    // zmiana samej sumy tez jest wykrywana
    uint8_t m[14] = {0x80, 0x81, 0x7F, 239, 8, 0, 80, 0, 0, 0, 0, 0xAA, 0x55, 0};
    seal(m, 14); m[13] = (uint8_t)(m[13] + 1);
    CHECK(shouldRejectOnCrc(239, crcCheck(m, 14)));
    // dlugosc RTCM w PGN 215 (istniejaca funkcja) - kontrola, ze nic nie zepsulem
    CHECK(pgn215PayloadLen(5 + 40 + 1, 40) == 40);
    CHECK(pgn215PayloadLen(5 + 300 + 1, (uint8_t)(300 & 0xFF)) == 300);
    // stan prowadzenia z bajtu stanu PGN 254 (dane z zrzutow: zielona kierownica = 0x01, wylaczona = 0x00)
    CHECK(!engagedFromStatus(0x00, 0x04) && !engagedFromStatus(0x01, 0x04));          // maska 0x04: samo 0x01 NIE wlacza
    CHECK(engagedFromStatus(0x04, 0x04) && engagedFromStatus(0x05, 0x04) && engagedFromStatus(0x0D, 0x04));
    CHECK(engagedFromStatus(0x01, 0x01) && engagedFromStatus(0x05, 0x01) && !engagedFromStatus(0x00, 0x01));
    CHECK(!engagedFromStatus(0x04, 0x01));                                            // tryb klasyczny: 0x04 samo nie wlacza
    CHECK(engagedFromStatus(0x01, 0x05) && engagedFromStatus(0x04, 0x05) && !engagedFromStatus(0x00, 0x05));
    CHECK(!engagedFromStatus(0x02, 0x05) && !engagedFromStatus(0x18, 0x05));          // work switch / GPS / naprowadzanie nie wlacza
    CHECK(sanitizeEngageMask(0) == 1 && sanitizeEngageMask(2) == 1 && sanitizeEngageMask(0xFF) == 1);   // zla maska -> domyslna 0x01
    CHECK(sanitizeEngageMask(1) == 1 && sanitizeEngageMask(4) == 4 && sanitizeEngageMask(5) == 5);
    CHECK(engagedFromStatus(0x01, 0x00) && engagedFromStatus(0x01, 0x80) && !engagedFromStatus(0x00, 0x80));   // maska 0 / smieci = domyslna 0x01
    // odpowiedz skanowania PGN 203: [9..11] = prefiks sieci (jak w AgOpenWeb PgnBuilder.TryParseScanReply), nie maska
    { const uint8_t ip[4] = {192, 168, 5, 126}; uint8_t f[13]; CHECK(buildScanReply(f, 126, ip) == 13);
      CHECK(f[0] == 0x80 && f[1] == 0x81 && f[2] == 126 && f[3] == 203 && f[4] == 7);
      CHECK(f[5] == 192 && f[6] == 168 && f[7] == 5 && f[8] == 126);
      CHECK(f[9] == 192 && f[10] == 168 && f[11] == 5);
      CHECK(crcCheck(f, 13) == 1 && f[12] == 0xA8); }          // suma kontrolna poprawna (AgOpenWeb liczy ja tak samo)
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
