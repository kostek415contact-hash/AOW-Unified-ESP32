// Testy nieblokujacej konfiguracji GPS (sensors/gps_config_fsm.h).
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_gps_config_fsm.cpp -o /tmp/test_gps_fsm && /tmp/test_gps_fsm
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../sensors/gps_config_fsm.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)
typedef GpsConfigFsm F;

struct Sim {
    F f; uint32_t t; std::vector<std::string> sent; int ubx = 0;
    bool quectelReplies, ver1Replies, protoReplies;
    uint32_t hz; uint8_t types; uint32_t ggaPeriod;   // symulowany strumien (types = jakie zdania nadaje; ggaPeriod=0 -> cisza)
    uint32_t nextGga;
    Sim(uint32_t start = 1000) : t(start), quectelReplies(true), ver1Replies(true), protoReplies(true), hz(10), types(0x3F), ggaPeriod(100), nextGga(start) {}
    void begin() { f.start(t, hz); nextGga = t; }
    // jeden krok 1 ms
    void step() {
        if (ggaPeriod && t >= nextGga) {
            nextGga += ggaPeriod;
            f.onNmea(F::T_GGA, t);
            for (uint8_t b = 2; b <= 32; b <<= 1) if (types & b) f.onNmea(b, t);
        }
        F::Action a = f.tick(t);
        if (a.type == F::A_SEND) {
            sent.push_back(a.cmd);
            if (!strcmp(a.cmd, "PQTMVERNO") && quectelReplies && ver1Replies) f.onLine("$PQTMVERNO,LG290P03AANR01A03S,2022/10/28,17:25:10*5A");
            if (!strcmp(a.cmd, "PQTMVER") && quectelReplies && !ver1Replies) f.onLine("$PQTMVER,ERROR,3*4F");
            if (!strncmp(a.cmd, "PQTMCFGPROT", 11) && protoReplies) f.onLine("$PQTMCFGPROT,OK,1,3,1,0*00");
        } else if (a.type == F::A_UBX) ubx++;
        t++;
    }
    void run(uint32_t ms) { for (uint32_t i = 0; i < ms && !f.done(); i++) step(); }
    bool sentHas(const char* s) const { for (auto& c : sent) if (c == s) return true; return false; }
    int count(const char* pfx) const { int n = 0; for (auto& c : sent) if (!c.compare(0, strlen(pfx), pfx)) n++; return n; }
};

static std::vector<F::Event> events(F& f) { std::vector<F::Event> v; F::Event e; while ((e = f.popEvent()) != F::EV_NONE) v.push_back(e); return v; }

int main() {
    {   // Odbiornik juz skonfigurowany: nic nie zapisujemy
        Sim s; s.begin(); s.run(10000);
        CHECK(s.f.done() && s.f.result() == F::R_MATCHED && s.f.quectel());
        CHECK(s.count("PQTMCFGMSGRATE") == 0 && !s.sentHas("PQTMSAVEPAR") && s.count("PQTMCFGFIXRATE") == 0);
        CHECK(s.sentHas("PQTMVERNO") && s.count("PQTMCFGPROT") == 1 && s.ubx == 0);
        CHECK(s.f.streamMatches() && s.f.observedRateHz() > 9.0f && s.f.observedRateHz() < 11.0f);
        CHECK(std::strstr(s.f.verLine(), "LG290P03") != nullptr && std::strstr(s.f.protoLine(), "CFGPROT") != nullptr);
        std::vector<F::Event> e = events(s.f);
        CHECK(e.size() == 4 && e[0] == F::EV_OBSERVED_MATCH && e[1] == F::EV_QUECTEL && e[2] == F::EV_PROTO_REPLY && e[3] == F::EV_DONE);
    }
    {   // Inna czestotliwosc (1 Hz zamiast 10): zapis + SAVEPAR dokladnie raz, w kolejnosci
        Sim s; s.ggaPeriod = 1000; s.begin(); s.run(15000);
        CHECK(s.f.done() && s.f.result() == F::R_WRITTEN && !s.f.streamMatches());
        CHECK(s.count("PQTMSAVEPAR") == 1 && s.sent.size() >= 10);
        CHECK(s.sentHas("PQTMCFGMSGRATE,W,GGA,1") && s.sentHas("PQTMCFGMSGRATE,W,GSV,10") && s.sentHas("PQTMCFGMSGRATE,W,GSA,10"));
        CHECK(s.sentHas("PQTMCFGFIXRATE,W,100"));
        size_t iFix = 0, iSave = 0, iVer = 0, iProto = 0;
        for (size_t i = 0; i < s.sent.size(); i++) {
            if (s.sent[i] == "PQTMCFGFIXRATE,W,100") iFix = i;
            if (s.sent[i] == "PQTMSAVEPAR") iSave = i;
            if (s.sent[i] == "PQTMVERNO") iVer = i;
            if (!s.sent[i].compare(0, 11, "PQTMCFGPROT")) iProto = i;
        }
        CHECK(iVer == 0 && iFix < iSave && iSave < iProto);
        std::vector<F::Event> e = events(s.f);
        CHECK(e.size() == 5 && e[0] == F::EV_OBSERVED_DIFFERS && e[1] == F::EV_QUECTEL && e[2] == F::EV_WROTE);
    }
    {   // Brakuje zdania (GSA): rozni sie -> zapis
        Sim s; s.types = 0x3F & ~F::T_GSA; s.begin(); s.run(10000);
        CHECK(s.f.done() && s.f.result() == F::R_WRITTEN && s.sentHas("PQTMSAVEPAR"));
    }
    {   // Odbiornik milczy: czekamy max 3.5 s, potem pytamy o wersje; brak odpowiedzi -> UBX
        Sim s; s.ggaPeriod = 0; s.quectelReplies = false; s.begin();
        s.run(3400); CHECK(s.f.phase() == F::OBSERVE && s.sent.empty());
        s.run(10000);
        CHECK(s.f.done() && s.f.result() == F::R_OTHER && !s.f.quectel() && s.ubx == 1);
        CHECK(s.count("PQTMVER") == 2 && s.count("PQTMSAVEPAR") == 0);
    }
    {   // PQTMVERNO nieznane, dopiero PQTMVER (nawet z ERROR) -> Quectel
        Sim s; s.ver1Replies = false; s.begin(); s.run(10000);
        CHECK(s.f.done() && s.f.quectel() && s.ubx == 0 && s.f.result() == F::R_MATCHED);
        CHECK(s.count("PQTMVER") == 2);   // PQTMVERNO i PQTMVER
    }
    {   // Brak odpowiedzi na zapytanie o protokol: konczymy po limicie czasu
        Sim s; s.protoReplies = false; s.begin(); s.run(10000);
        std::vector<F::Event> e = events(s.f);
        CHECK(s.f.done() && e.size() == 4 && e[2] == F::EV_PROTO_NONE);
    }
    {   // Przekrecenie millis() przez 2^32 w srodku sekwencji
        Sim s(0xFFFFFFFFu - 2000u); s.ggaPeriod = 1000; s.begin(); s.run(15000);
        CHECK(s.f.done() && s.f.result() == F::R_WRITTEN && s.count("PQTMSAVEPAR") == 1);
    }
    {   // Nie blokuje: kazdy tick zwraca najwyzej jedna komende; ponowny start czysci stan
        Sim s; s.begin(); s.run(10000);
        s.f.start(s.t, 20);
        CHECK(s.f.running() && s.f.result() == F::R_NONE && s.f.intervalMs() == 50);
        CHECK(s.f.popEvent() == F::EV_NONE);
    }
    {   // Zdarzenia przed OBSERVE sa ignorowane, a onLine poza fazami odpowiedzi nic nie psuje
        F f; f.onNmea(F::T_GGA, 5); f.onLine("$PQTMVERNO,x*00");
        CHECK(!f.running() && !f.quectel() && f.observedGga() == 0);
    }
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
