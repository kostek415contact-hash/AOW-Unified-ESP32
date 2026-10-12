// Testy arbitra zrodel RTCM (RtcmArbiter) i funkcji ramek PGN (PgnFrame) - czysty C++.
// Uruchomienie z katalogu glownego projektu:
//     g++ -std=c++17 -Wall -Wextra tests/test_rtcm_arbiter.cpp -o /tmp/tra && /tmp/tra
// Makra z Arduino.h, ktore na ESP32 psuja nazwy w naszych nagłówkach (test ma je wychwycic).
#define EXTERNAL 0
#define INTERNAL 3
#define DEFAULT 1
#define INPUT 1
#define OUTPUT 3
#define HIGH 1
#define LOW 0
#define DISABLED 0
#define ENABLED 1
#define CHANGE 2
#define RISING 3
#define FALLING 4
#include <cstdio>
#include <vector>
#include "../ntrip/rtcm_arbiter.h"
#include "../connectivity/pgn_frame.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)

static std::vector<uint8_t> frame(uint8_t pgn, int payload, bool goodCrc = true, bool wrapLen = true) {
    std::vector<uint8_t> f(5 + payload + 1, 0);
    f[0] = 0x80; f[1] = 0x81; f[2] = 0x7F; f[3] = pgn; f[4] = wrapLen ? (uint8_t)(payload & 0xFF) : (uint8_t)payload;
    for (int i = 0; i < payload; i++) f[5 + i] = (uint8_t)(i * 7 + 1);
    uint8_t c = 0; for (size_t i = 2; i < f.size() - 1; i++) c += f[i];
    f.back() = goodCrc ? c : (uint8_t)(c + 1);
    return f;
}
// Wzor ze starego kodu (network_manager.h przed zmiana) - do porownania w przypadkach niezawinietych
static int oldLen(int len, uint8_t declared) { return (declared > 0 && len >= 5 + declared) ? declared : (len - 5); }

int main() {
    // ---- RtcmArbiter ----
    CHECK(RtcmArbiter::activeSource(1000) == RtcmArbiter::SRC_NONE, "start: brak zrodla");
    CHECK(RtcmArbiter::filterExternal(1000, 100), "wlasny klient milczy => zewnetrzne przepuszczone");
    CHECK(RtcmArbiter::activeSource(1500) == RtcmArbiter::SRC_AOG, "po przepuszczeniu: zrodlo = zewnetrzne");
    CHECK(RtcmArbiter::forwardedBytes == 100 && RtcmArbiter::forwardedPackets == 1 && RtcmArbiter::droppedPackets == 0, "liczniki przepuszczonych");
    RtcmArbiter::noteOwn(2000);
    CHECK(!RtcmArbiter::filterExternal(2100, 200), "wlasny klient dostarcza => zewnetrzne ODRZUCONE");
    CHECK(RtcmArbiter::droppedBytes == 200 && RtcmArbiter::droppedPackets == 1, "liczniki odrzuconych");
    CHECK(RtcmArbiter::activeSource(2100) == RtcmArbiter::SRC_OWN, "zrodlo = wlasny NTRIP");
    RtcmArbiter::noteOwn(2900);
    CHECK(!RtcmArbiter::filterExternal(5800, 50), "2,9 s po ostatnich danych wlasnych nadal odrzucane (okno 3 s)");
    CHECK(RtcmArbiter::filterExternal(5901, 50), "po > 3 s ciszy wlasnego klienta zewnetrzne wraca");
    CHECK(RtcmArbiter::activeSource(6000) == RtcmArbiter::SRC_AOG, "zrodlo znow zewnetrzne");
    CHECK(RtcmArbiter::activeSource(20000) == RtcmArbiter::SRC_NONE, "dlugo bez danych: brak zrodla");
    RtcmArbiter::noteOwn(0);
    CHECK(RtcmArbiter::lastOwnMs == 1 && RtcmArbiter::ownActive(1), "noteOwn(0) nie jest mylone z 'nigdy'");

    // ---- PgnFrame::crcCheck ----
    auto ok = frame(254, 8); CHECK(PgnFrame::crcCheck(ok.data(), (int)ok.size()) == 1, "poprawne CRC => 1");
    auto bad = frame(254, 8, false); CHECK(PgnFrame::crcCheck(bad.data(), (int)bad.size()) == 0, "zle CRC => 0");
    CHECK(PgnFrame::crcCheck(ok.data(), (int)ok.size() - 1) == -1, "ramka bez ostatniego bajtu => -1 (niekompletna)");
    CHECK(PgnFrame::crcCheck(ok.data(), 4) == -1, "za krotka => -1");
    auto big = frame(254, 8); big.resize(64, 0xEE);
    CHECK(PgnFrame::crcCheck(big.data(), (int)big.size()) == 1, "datagram dluzszy niz ramka (smieci za CRC) - oceniana tylko ramka");
    // zgodnosc z funkcja wysylajaca (nmCalculateCRC): CRC = suma bajtow od 2 do length-2
    uint8_t hello[13] = {0x80, 0x81, 0x7F, 203, 7, 1, 2, 3, 4, 5, 6, 7, 0}; uint8_t c = 0; for (int i = 2; i < 12; i++) c += hello[i]; hello[12] = c;
    CHECK(PgnFrame::crcCheck(hello, 13) == 1, "zgodne z nmCalculateCRC() uzywanym przy wysylaniu");

    // ---- PgnFrame::pgn215PayloadLen ----
    auto f100 = frame(215, 100); CHECK(PgnFrame::pgn215PayloadLen((int)f100.size(), 100) == 100, "100 B z CRC, deklarowana = 100");
    auto f300 = frame(215, 300); CHECK(f300[4] == 44, "(kontrola testu) 300 B => deklarowana zawinieta do 44");
    CHECK(PgnFrame::pgn215PayloadLen((int)f300.size(), f300[4]) == 300, "300 B: zawinieta dlugosc 44 NIE ucina danych");
    CHECK(oldLen((int)f300.size(), f300[4]) == 44, "(dowod) stary kod ucinal 300 B do 44");
    auto f600 = frame(215, 600); CHECK(PgnFrame::pgn215PayloadLen((int)f600.size(), f600[4]) == 600, "600 B: rowniez w calosci");
    CHECK(PgnFrame::pgn215PayloadLen(5 + 40, 0) == 40, "deklarowana 0: wszystko od bajtu 5 (jak dotad)");
    CHECK(PgnFrame::pgn215PayloadLen(5 + 40, 40) == 40, "bez CRC i deklarowana = reszta: 40 (jak stary kod)");
    CHECK(PgnFrame::pgn215PayloadLen(5 + 30, 50) == 30, "deklarowana wieksza niz datagram: reszta (jak stary kod)");
    CHECK(PgnFrame::pgn215PayloadLen(5, 0) == 0 && PgnFrame::pgn215PayloadLen(3, 9) == 0, "za krotkie: 0");
    bool same = true;   // dla ramek do 255 B nowa funkcja daje to samo co stara (poza usunieciem bajtu CRC przy zgodnej dlugosci)
    for (int payload = 0; payload <= 250; payload += 5) for (int extra = 0; extra <= 3; extra++) {
        int len = 5 + payload + extra; uint8_t decl = (uint8_t)payload; int n = PgnFrame::pgn215PayloadLen(len, decl), o = oldLen(len, decl);
        if (n != o && !(extra == 1 && n == payload)) same = false;   // jedyna dozwolona roznica: CRC
        if (n > len - 5) same = false;                                // nigdy wiecej niz jest w datagramie
    }
    CHECK(same, "dla ramek <= 255 B zgodne ze starym kodem; nigdy poza datagram");
    printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
    return fails ? 1 : 0;
}
