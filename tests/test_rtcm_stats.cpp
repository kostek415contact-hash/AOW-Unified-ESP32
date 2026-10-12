// Testy parsera ramek RTCM3 (ntrip/rtcm_stats.h), z weryfikacja CRC24Q.
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_rtcm_stats.cpp -o /tmp/test_rtcm_stats && /tmp/test_rtcm_stats
#include <cstdio>
#include <vector>
#include <cstdint>
#include "../ntrip/rtcm_stats.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

// Niezalezna implementacja CRC24Q (po bitach, MSB first, wielomian 0x864CFB) - nie ta z testowanego pliku.
static uint32_t crcRef(const std::vector<uint8_t>& d) {
    uint32_t c = 0;
    for (uint8_t b : d) { for (int i = 7; i >= 0; i--) { bool top = (c >> 23) & 1; bool in = (b >> i) & 1; c = (c << 1) & 0xFFFFFF; if (top ^ in) c ^= 0x864CFB; } }
    return c;
}
// Ramka z podanym typem i dlugoscia payloadu (>= 2); reszta payloadu wypelniona deterministycznie.
static std::vector<uint8_t> frame(uint16_t type, size_t payloadLen, uint8_t fill = 0x5A) {
    std::vector<uint8_t> f = {0xD3, (uint8_t)((payloadLen >> 8) & 3), (uint8_t)(payloadLen & 0xFF)};
    f.push_back((uint8_t)(type >> 4)); f.push_back((uint8_t)((type & 0xF) << 4));
    for (size_t i = 2; i < payloadLen; i++) f.push_back((uint8_t)(fill + i));
    uint32_t c = crcRef(f);
    f.push_back((uint8_t)(c >> 16)); f.push_back((uint8_t)(c >> 8)); f.push_back((uint8_t)c);
    return f;
}
static void feedAll(const std::vector<uint8_t>& v) { RtcmMessageStats::feed(v.data(), v.size()); }
static uint32_t countOf(uint16_t t) { for (uint8_t i = 0; i < RtcmMessageStats::getTypeCount(); i++) if (RtcmMessageStats::getTypeAt(i) == t) return RtcmMessageStats::getCountAt(i); return 0; }

int main() {
    using S = RtcmMessageStats;
    // pewnosc, ze moj "niezalezny" CRC to CRC24Q: znany wektor kontrolny "123456789" -> 0xCDE703
    { std::vector<uint8_t> v = {'1','2','3','4','5','6','7','8','9'}; CHECK(crcRef(v) == 0xCDE703); }

    // pojedyncze ramki, kilka typow
    S::reset();
    feedAll(frame(1005, 19)); feedAll(frame(1077, 120)); feedAll(frame(1077, 130)); feedAll(frame(1087, 60));
    CHECK(S::getFramesParsed() == 4); CHECK(S::getCrcErrors() == 0);
    CHECK(countOf(1005) == 1 && countOf(1077) == 2 && countOf(1087) == 1 && S::getTypeCount() == 3);

    // bajt po bajcie (jak z TCP) i duza ramka 1023 B (maks. dlugosc)
    S::reset();
    { auto f = frame(1230, 1023); for (uint8_t b : f) S::feedByte(b); }
    CHECK(S::getFramesParsed() == 1 && countOf(1230) == 1 && S::getCrcErrors() == 0);
    // najmniejsza dopuszczalna: payload 2 B
    S::reset(); feedAll(frame(4095, 2)); CHECK(countOf(4095) == 1);
    S::reset(); feedAll(frame(0, 2)); CHECK(S::getFramesParsed() == 1 && countOf(0) == 1);

    // smieci przed ramka i miedzy ramkami (bez 0xD3)
    S::reset();
    { std::vector<uint8_t> v = {0x00, 0xFF, 0x12, 0x34}; auto a = frame(1005, 19); v.insert(v.end(), a.begin(), a.end());
      v.push_back(0x77); v.push_back(0x01); auto b = frame(1077, 40); v.insert(v.end(), b.begin(), b.end()); feedAll(v); }
    CHECK(S::getFramesParsed() == 2 && S::getCrcErrors() == 0);

    // uszkodzony bajt payloadu -> blad CRC, typ NIE jest liczony
    S::reset();
    { auto f = frame(1077, 50); f[10] ^= 0x04; feedAll(f); }
    CHECK(S::getFramesParsed() == 0 && S::getCrcErrors() == 1 && S::getTypeCount() == 0);
    // uszkodzony bajt sumy
    S::reset(); { auto f = frame(1077, 50); f.back() ^= 0x01; feedAll(f); }
    CHECK(S::getCrcErrors() == 1 && S::getFramesParsed() == 0);
    // uszkodzony typ (bity w payloadzie) wykryty tak samo
    S::reset(); { auto f = frame(1005, 19); f[3] ^= 0x80; feedAll(f); } CHECK(S::getCrcErrors() == 1 && countOf(1005) == 0);
    // po bledzie nastepna dobra ramka jest liczona
    S::reset(); { auto bad = frame(1077, 50); bad[12] ^= 1; feedAll(bad); feedAll(frame(1005, 19)); }
    CHECK(S::getCrcErrors() == 1 && S::getFramesParsed() == 1 && countOf(1005) == 1);

    // zarezerwowane bity dlugosci != 0: to nie naglowek; bajt jest ponownie oceniany jako poczatek ramki
    S::reset();
    { std::vector<uint8_t> v = {0xD3, 0xD3}; auto f = frame(1005, 19); v.insert(v.end(), f.begin(), f.end());
      // pierwszy 0xD3 + (0xD3 & 0xFC != 0) -> drugi 0xD3 zaczyna pelna ramke? nie: ramka zaczyna sie wlasnym 0xD3 z f
      feedAll(v); }
    CHECK(S::getFramesParsed() == 1 && S::getCrcErrors() == 0);
    // 0xD3 tuz przed prawdziwa ramka (bez fałszywej dlugosci): dluzszy ogon 0xD3 nie psuje
    S::reset(); { std::vector<uint8_t> v(5, 0xD3); auto f = frame(1005, 19); v.insert(v.end(), f.begin(), f.end()); feedAll(v); }
    CHECK(S::getFramesParsed() == 1);
    // dlugosc 0 i 1 odrzucone (payload < 2 B nie ma typu)
    S::reset(); { std::vector<uint8_t> v = {0xD3, 0x00, 0x00, 0xD3, 0x00, 0x01}; feedAll(v); }
    CHECK(S::getFramesParsed() == 0 && S::getCrcErrors() == 0);

    // zlaczone ramki w jednym buforze, wiele rund
    S::reset();
    { std::vector<uint8_t> v; for (int i = 0; i < 200; i++) { auto f = frame(i % 2 ? 1077 : 1087, 20 + (i % 50), (uint8_t)i); v.insert(v.end(), f.begin(), f.end()); } feedAll(v); }
    CHECK(S::getFramesParsed() == 200 && countOf(1077) == 100 && countOf(1087) == 100 && S::getCrcErrors() == 0);

    // limit 24 roznych typow; reszta w overflow
    S::reset();
    for (uint16_t t = 1000; t < 1030; t++) feedAll(frame(t, 10));
    CHECK(S::getTypeCount() == 24 && S::getOverflowDropped() == 6 && S::getFramesParsed() == 30);

    // reset zeruje wszystko, takze w polowie ramki
    S::reset(); { auto f = frame(1005, 19); S::feed(f.data(), 7); S::reset(); S::feed(f.data() + 7, f.size() - 7); }
    CHECK(S::getFramesParsed() == 0 && S::getTypeCount() == 0 && S::getCrcErrors() == 0);
    S::reset(); feedAll(frame(1005, 19)); CHECK(S::getFramesParsed() == 1);

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
