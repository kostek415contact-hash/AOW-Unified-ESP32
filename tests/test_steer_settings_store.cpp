// Test zapisu ustawien sterowania z AgOpenWeb (SteerSettingsSaveGate) - czysty C++.
//     g++ -std=c++17 -Wall -Wextra tests/test_steer_settings_store.cpp -o /tmp/tss && /tmp/tss
#include <cstdio>
#include "../connectivity/steer_settings_store.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
static SteerSettingsData mk(uint8_t g, uint8_t h, uint8_t m, int16_t off = 0) { SteerSettingsData d; d.gainP = g; d.highPWM = h; d.minPWM = m; d.lowPWM = 10; d.countsPerDeg = 100; d.steerOffset = off; d.ackermanFix = 100; return d; }
int main() {
  { uint8_t b[SteerSettingsData::BYTES]; SteerSettingsData d = mk(40, 150, 25, -1234); d.pack(b);
    CHECK(SteerSettingsData::unpack(b) == d, "pack/unpack: te same wartosci (takze ujemny steerOffset)");
    SteerSettingsData e = mk(40, 150, 25, 32767); e.pack(b); CHECK(SteerSettingsData::unpack(b).steerOffset == 32767, "steerOffset max");
    e = mk(1, 2, 3, -32768); e.pack(b); CHECK(SteerSettingsData::unpack(b).steerOffset == -32768, "steerOffset min"); }
  { SteerSettingsSaveGate g(3000);
    g.onReceived(mk(40, 150, 25), 1000);
    CHECK(g.hasPending() && !g.shouldSave(1000) && !g.shouldSave(3999), "pierwsze odebrane (nic zapisanego): zapis dopiero po 3 s bez zmian");
    CHECK(g.shouldSave(4000), "po 3 s ustalonych wartosci: zapisz");
    CHECK(g.pendingValues() == mk(40, 150, 25), "do zapisu idzie to, co odebrano");
    g.markSaved();
    CHECK(!g.hasPending() && !g.shouldSave(100000), "po zapisie nic nie czeka");
    g.onReceived(mk(40, 150, 25), 100000);
    CHECK(!g.hasPending(), "te same wartosci co zapisane: nie zapisuj ponownie (zero zuzycia flasha)"); }
  { SteerSettingsSaveGate g(3000);   // przesuwanie suwaka: wartosci zmieniaja sie co sekunde
    for (int i = 0; i < 10; i++) { g.onReceived(mk(30 + i, 150, 25), 1000 + i * 1000); CHECK(!g.shouldSave(1000 + i * 1000 + 2999), "suwak w ruchu: bez zapisu"); }
    CHECK(g.shouldSave(10000 + 3000 + 0) || g.shouldSave(13000), "po zatrzymaniu suwaka: jeden zapis ostatniej wartosci");
    CHECK(g.pendingValues().gainP == 39, "zapisana ostatnia wartosc suwaka"); }
  { SteerSettingsSaveGate g(3000); g.setStored(mk(40, 150, 25));
    g.onReceived(mk(40, 150, 25), 5);
    CHECK(!g.hasPending(), "po starcie AgOpenWeb wysyla te same ustawienia: bez zapisu");
    g.onReceived(mk(45, 150, 25), 10);
    CHECK(g.hasPending() && g.shouldSave(3010), "zmiana wzgledem zapisanych: zapis po 3 s");
    g.onReceived(mk(40, 150, 25), 1000);
    CHECK(!g.hasPending(), "powrot do zapisanych wartosci przed zapisem: anulowanie"); }
  { SteerSettingsSaveGate g(1000); g.onReceived(mk(1, 2, 3), 4294967000UL);
    CHECK(!g.shouldSave(4294967500UL) && g.shouldSave((unsigned long)(4294967000UL + 1200UL)), "arytmetyka bez znaku przy zawinieciu millis()"); }
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total); return fails ? 1 : 0;
}
