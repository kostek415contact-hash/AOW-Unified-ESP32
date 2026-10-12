// Testy logiki klienta NTRIP (ntrip/ntrip_logic.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_ntrip_logic.cpp -o /tmp/test_ntrip_logic && /tmp/test_ntrip_logic
#include <cstdio>
#include <cstring>
#include <strings.h>
#include "../ntrip/ntrip_logic.h"
using namespace NtripLogic;

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)
static Reply P(const char* s) { return parse(s, strlen(s)); }

int main() {
    // zwykle odpowiedzi
    CHECK(P("ICY 200 OK\r\n\r\n").kind == NR_OK);
    CHECK(P("HTTP/1.1 200 OK\r\nServer: x\r\nContent-Type: gnss/data\r\n\r\n").kind == NR_OK);
    CHECK(P("HTTP/1.0 200 OK\r\n\r\n").code == 200);
    CHECK(P("ICY 200 OK\r\n").kind == NR_PENDING);                   // brak pustej linii - czekamy jak dotad
    CHECK(P("ICY 20").kind == NR_PENDING);
    CHECK(P("").kind == NR_PENDING);
    // bledy
    CHECK(P("ICY 401 Unauthorized\r\n").kind == NR_AUTH);            // od razu po pierwszej linii
    CHECK(P("HTTP/1.1 403 Forbidden\r\n\r\n").kind == NR_AUTH);
    CHECK(P("HTTP/1.1 404 Not Found\r\n\r\n").kind == NR_NOT_FOUND);
    CHECK(P("SOURCETABLE 200 OK\r\nServer: x\r\n").kind == NR_SOURCETABLE);
    CHECK(P("HTTP/1.1 500 Internal\r\n\r\n").kind == NR_OTHER);
    CHECK(P("smieci\r\n\r\n").kind == NR_OTHER);
    CHECK(P("HTTP/1.1 200 OK\r\n\r\n").kind == NR_OK && P("HTTP/1.1 200 OK\r\n\r\n").code == 200);
    // przekierowania
    { Reply r = P("HTTP/1.1 302 Found\r\nLocation: http://caster2.example.pl:2101/MOUNT\r\n\r\n");
      CHECK(r.kind == NR_REDIRECT); CHECK(strcmp(r.host, "caster2.example.pl") == 0); CHECK(r.port == 2101); }
    { Reply r = P("HTTP/1.1 301 Moved\r\nlocation: http://10.0.0.5/x\r\n\r\n");     // mala litera, bez portu
      CHECK(r.kind == NR_REDIRECT); CHECK(strcmp(r.host, "10.0.0.5") == 0); CHECK(r.port == 80); }
    { Reply r = P("HTTP/1.1 307 T\r\nServer: a\r\nLocation:   http://h.pl:8086\r\n\r\n");
      CHECK(r.kind == NR_REDIRECT); CHECK(strcmp(r.host, "h.pl") == 0); CHECK(r.port == 8086); }
    CHECK(P("HTTP/1.1 302 Found\r\nLocation: https://secure.pl/\r\n\r\n").kind == NR_OTHER);   // https nieobslugiwane
    CHECK(P("HTTP/1.1 302 Found\r\n\r\n").kind == NR_OTHER);                                    // brak Location
    CHECK(P("HTTP/1.1 302 Found\r\nLocation: http://h.pl:99999/\r\n\r\n").kind == NR_OTHER);   // port poza zakresem
    CHECK(P("HTTP/1.1 302 Found\r\nLocation: http://h.pl:/\r\n\r\n").kind == NR_OTHER);        // pusty port
    CHECK(P("HTTP/1.1 302 Found\r\nLocation: http://:2101/\r\n\r\n").kind == NR_OTHER);        // pusty host
    CHECK(P("HTTP/1.1 302 Found\r\nLocation: http://h.pl\r\n").kind == NR_PENDING);            // naglowki niekompletne
    { char big[200]; memset(big, 'a', 150); big[150] = 0;
      char req[400]; snprintf(req, sizeof req, "HTTP/1.1 302 F\r\nLocation: http://%s:1/\r\n\r\n", big);
      CHECK(P(req).kind == NR_OTHER); }                                                         // host dluzszy niz bufor

    // przerwy: siec
    { Backoff b; CHECK(b.delayMs() == 5000); CHECK(b.fails() == 0);
      uint32_t exp[] = {5000, 10000, 20000, 40000, 80000, 120000, 120000, 120000};
      for (uint32_t e : exp) { b.onFailure(NR_PENDING); CHECK(b.delayMs() == e); }
      for (int i = 0; i < 400; i++) b.onFailure(NR_PENDING);                 // bez przepelnienia licznika
      CHECK(b.delayMs() == 120000 && b.fails() >= 200); }
    // zle haslo
    { Backoff b; b.onFailure(NR_AUTH); CHECK(b.delayMs() == 120000); b.onFailure(NR_AUTH); CHECK(b.delayMs() == 300000);
      b.onFailure(NR_AUTH); CHECK(b.delayMs() == 300000); }
    // zly mountpoint / inne
    { Backoff b; b.onFailure(NR_NOT_FOUND); CHECK(b.delayMs() == 60000);
      b.onFailure(NR_SOURCETABLE); CHECK(b.delayMs() == 60000); b.onFailure(NR_OTHER); CHECK(b.delayMs() == 60000); }
    // stabilny strumien zeruje serie, krotki nie
    { Backoff b; b.onFailure(NR_PENDING); b.onFailure(NR_PENDING); b.onFailure(NR_PENDING);
      b.onStreamEnded(Backoff::STABLE_MS); CHECK(b.fails() == 0 && b.delayMs() == 5000);
      b.onStreamEnded(2000); CHECK(b.fails() == 1); b.onStreamEnded(2000); CHECK(b.delayMs() == 10000);
      b.onStreamEnded(Backoff::STABLE_MS - 1); CHECK(b.fails() == 3);
      b.reset(); CHECK(b.fails() == 0 && b.lastKind() == NR_PENDING); }
    // po zlym hasle i poprawieniu: zerwanie strumienia wraca do przerw sieciowych
    { Backoff b; b.onFailure(NR_AUTH); b.onStreamEnded(100); CHECK(b.lastKind() == NR_PENDING && b.delayMs() == 10000); }

    // StallWatch: martwy strumien (TCP otwarty, brak danych RTCM)
    { StallWatch w; w.start(1000);
      CHECK(!w.stalled(1000)); CHECK(!w.stalled(1000 + StallWatch::STALL_MS - 1)); CHECK(w.stalled(1000 + StallWatch::STALL_MS));
      w.onData(15000); CHECK(!w.stalled(15000 + StallWatch::STALL_MS - 1)); CHECK(w.stalled(15000 + StallWatch::STALL_MS)); }
    // przejscie przez zawiniecie millis()
    { StallWatch w; w.start(0xFFFFFFF0u); CHECK(!w.stalled(0xFFFFFFF0u + 5000u)); CHECK(w.stalled(0xFFFFFFF0u + StallWatch::STALL_MS)); }

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
