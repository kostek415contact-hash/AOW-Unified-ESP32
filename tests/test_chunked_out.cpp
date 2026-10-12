// Testy bufora kawalkowego (web/chunked_out.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_chunked_out.cpp -o /tmp/test_chunked_out && /tmp/test_chunked_out
#include <cstdio>
#include <string>
#include <vector>
#include "../web/chunked_out.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

template <size_t CAP>
static void run(const std::vector<std::string>& parts, bool& okAll, size_t& chunks, size_t& total) {
    std::string got; size_t maxChunk = 0, minChunk = 1u << 30;
    auto sink = [&](const char* p, size_t n) { got.append(p, n); if (n > maxChunk) maxChunk = n; if (n < minChunk) minChunk = n; };
    ChunkedOut<CAP, decltype(sink)> o(sink);
    std::string want;
    for (auto& s : parts) { o.write(s.c_str(), s.size()); want += s; }
    o.flush();
    okAll = (got == want) && maxChunk <= CAP && (got.empty() || minChunk > 0) && o.total() == want.size();
    chunks = o.chunks(); total = o.total();
}

int main() {
    bool ok; size_t ch, tot;
    run<8>({}, ok, ch, tot);                                   CHECK(ok && ch == 0 && tot == 0);      // pusto: zero kawalkow
    run<8>({""}, ok, ch, tot);                                 CHECK(ok && ch == 0);
    run<8>({"abc"}, ok, ch, tot);                              CHECK(ok && ch == 1 && tot == 3);
    run<8>({"12345678"}, ok, ch, tot);                         CHECK(ok && ch == 1);                  // dokladnie CAP
    run<8>({"123456789"}, ok, ch, tot);                        CHECK(ok && ch == 2);                  // CAP + 1
    run<8>({"1234567", "8", "9"}, ok, ch, tot);                CHECK(ok && ch == 2 && tot == 9);
    run<8>({"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}, ok, ch, tot); CHECK(ok && ch == 6);      // 45 B / 8
    run<1>({"abcdef"}, ok, ch, tot);                           CHECK(ok && ch == 6);
    run<1024>({std::string(5000, 'x'), "{", "}"}, ok, ch, tot); CHECK(ok && ch == 5 && tot == 5002);
    // wiele malych kawalkow jak w /status ("klucz":wartosc,)
    { std::vector<std::string> v; for (int i = 0; i < 300; i++) v.push_back("\"k" + std::to_string(i) + "\":" + std::to_string(i * 7) + ",");
      run<64>(v, ok, ch, tot); CHECK(ok); run<1024>(v, ok, ch, tot); CHECK(ok && ch >= 1); }
    // zapis z napisu zakonczonego zerem i NULL
    { std::string got; auto sink = [&](const char* p, size_t n) { got.append(p, n); };
      ChunkedOut<4, decltype(sink)> o(sink); o.write("hello"); o.write((const char*)nullptr); o.write(" w"); o.flush(); o.flush();
      CHECK(got == "hello w" && o.total() == 7); }
    // wielokrotny flush nie dubluje
    { int calls = 0; auto sink = [&](const char*, size_t) { calls++; };
      ChunkedOut<16, decltype(sink)> o(sink); o.write("abc"); o.flush(); o.flush(); o.flush(); CHECK(calls == 1); }

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
