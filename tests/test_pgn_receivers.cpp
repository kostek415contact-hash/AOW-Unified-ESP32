// Test parserow PGN (connectivity/pgn_receivers.h) z minimalnym zamiennikiem rdzenia Arduino (tests/mocks).
//     g++ -std=c++17 -Wall -Wextra -Itests/mocks tests/test_pgn_receivers.cpp -o /tmp/tpr && /tmp/tpr
// Stan odbiornikow jest statyczny (jak w firmware), wiec bloki ponizej zalezą od kolejnosci.
#pragma GCC diagnostic ignored "-Wunused-variable"
#include <cstdio>
#include <cstring>
#include "../connectivity/pgn_receivers.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
static bool near_(float a, float b) { return (a > b ? a - b : b - a) < 0.005f; }

// Ramka: 0x80 0x81 src pgn len(8) + 8 bajtow danych + CRC; p[i] to bajt o indeksie 5+i.
static void frame(uint8_t* f, uint8_t pgn, const uint8_t* p) {
  f[0] = 0x80; f[1] = 0x81; f[2] = 0x7F; f[3] = pgn; f[4] = 8;
  memcpy(f + 5, p, 8); f[13] = 0;
}

int main() {
  LogBuffer::begin();
  uint8_t f[14];

  // ---------- PGN 254: komenda steer ----------
  { const IPAddress ip(192, 168, 1, 10);
    CHECK(!SteerCommandReceiver::isFresh() && !SteerCommandReceiver::isAutoSteerActive(), "steer: przed pierwsza ramka brak swiezosci");
    const uint8_t p[8] = {123, 0, 0x01 | 0x02 | 0x08 | 0x10, 0x2E, 0xFB, 0x05, 0x0F, 0xA0};
    // speed=123 (12.3 km/h), status=0x1B, kat = 0xFB2E = -1234 (-12.34), xte=5, sekcje 0x0F / 0xA0
    frame(f, 0xFE, p);
    SteerCommandReceiver::parse(f, 3, ip);   // za krotka (len<8) - ignorowana
    CHECK(SteerCommandReceiver::getRxCount() == 0, "steer: len<8 odrzucone");
    SteerCommandReceiver::parse(f, 8, ip);
    CHECK(SteerCommandReceiver::getRxCount() == 1, "steer: ramka przyjeta");
    CHECK(near_(SteerCommandReceiver::getSpeedKmh(), 12.3f), "steer: predkosc /10");
    CHECK(near_(SteerCommandReceiver::getTargetAngleDeg(), -12.34f), "steer: kat ze znakiem /100");
    CHECK(SteerCommandReceiver::isAutoSteerEngaged() && SteerCommandReceiver::isAutoSteerActive(), "steer: bit 0x01 = wlaczone (maska domyslna)");
    CHECK(SteerCommandReceiver::getLastStatusByte() == 0x1B, "steer: surowy bajt statusu");
    CHECK(SteerCommandReceiver::getSections1to8() == 0x0F && SteerCommandReceiver::getSections9to16() == 0xA0, "steer: bajty sekcji");
    uint8_t raw[16]; uint8_t n = SteerCommandReceiver::copyLastRaw(raw, sizeof raw);
    CHECK(n == 13 && raw[0] == 0x80 && raw[3] == 0xFE && raw[7] == 0x1B, "steer: surowa ramka do diagnostyki");
    CHECK(SteerCommandReceiver::copyLastRaw(raw, 4) == 4, "steer: copyLastRaw nie przekracza bufora");

    // swiezosc: AgOpenWeb wysyla cyklicznie; brak ramek => sterowanie NIE jest aktywne mimo flagi
    mockAdvance(400);
    CHECK(SteerCommandReceiver::isFresh() && SteerCommandReceiver::isAutoSteerActive(), "steer: 400 ms - nadal swieze");
    CHECK(!SteerCommandReceiver::isDisengagedConfirmed(), "steer: wlaczone i swieze - wylaczenie NIE potwierdzone");
    mockAdvance(200);
    CHECK(!SteerCommandReceiver::isFresh(), "steer: 600 ms - nieswieze");
    CHECK(SteerCommandReceiver::isAutoSteerEngaged() && !SteerCommandReceiver::isAutoSteerActive(), "steer: flaga zostaje, ale isAutoSteerActive() = false (utrata lacznosci)");
    CHECK(!SteerCommandReceiver::isDisengagedConfirmed(), "steer: utrata lacznosci przy fladze wlaczone - wylaczenie NIE potwierdzone");

    // odstepy miedzy ramkami
    SteerCommandReceiver::parse(f, 8, ip);   // przerwa 600 ms
    mockAdvance(20); SteerCommandReceiver::parse(f, 8, ip);
    CHECK(SteerCommandReceiver::getLastDeltaMs() == 20 && SteerCommandReceiver::getMaxDeltaMs() == 600, "steer: ostatni i maksymalny odstep");

    // maski bitu "wlaczone"
    uint8_t p4[8] = {0, 0, 0x04, 0, 0, 0, 0, 0};
    SteerCommandReceiver::setEngagedMask(0x04); frame(f, 0xFE, p4); SteerCommandReceiver::parse(f, 8, ip);
    CHECK(SteerCommandReceiver::getEngagedMask() == 0x04 && SteerCommandReceiver::isAutoSteerEngaged(), "steer: maska 0x04 - bit 0x04 wlacza");
    uint8_t p1[8] = {0, 0, 0x01, 0, 0, 0, 0, 0};
    frame(f, 0xFE, p1); SteerCommandReceiver::parse(f, 8, ip);
    CHECK(!SteerCommandReceiver::isAutoSteerEngaged(), "steer: maska 0x04 - bit 0x01 nie wlacza");
    SteerCommandReceiver::setEngagedMask(0x05); SteerCommandReceiver::parse(f, 8, ip);
    CHECK(SteerCommandReceiver::isAutoSteerEngaged(), "steer: maska 0x05 - obie wersje");
    SteerCommandReceiver::setEngagedMask(0x77);
    CHECK(SteerCommandReceiver::getEngagedMask() == 0x01, "steer: smieciowa maska wraca do 0x01");

    // wylaczenie: status 0 => zdjete
    uint8_t p0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    frame(f, 0xFE, p0); SteerCommandReceiver::parse(f, 8, ip);
    CHECK(!SteerCommandReceiver::isAutoSteerEngaged() && !SteerCommandReceiver::isAutoSteerActive(), "steer: status 0 - wylaczone");
    CHECK(SteerCommandReceiver::isDisengagedConfirmed(), "steer: swieza ramka ze statusem 0 - wylaczenie potwierdzone");
    mockAdvance(600);
    CHECK(!SteerCommandReceiver::isAutoSteerEngaged() && !SteerCommandReceiver::isDisengagedConfirmed(), "steer: brak ramek - stan nieznany, wylaczenie NIE potwierdzone (zatrzask nie schodzi)");
    SteerCommandReceiver::parse(f, 8, ip);   // wznowienie lacznosci, nadal status 0

    // kat skrajny i dodatni; zawiniecie millis()
    uint8_t pa[8] = {0, 0, 0x01, 0xFF, 0x7F, 0, 0, 0};   // +32767 = +327.67
    frame(f, 0xFE, pa); SteerCommandReceiver::parse(f, 8, ip);
    CHECK(near_(SteerCommandReceiver::getTargetAngleDeg(), 327.67f), "steer: maksymalny kat dodatni");
    g_mockMillis = 4294967290UL; SteerCommandReceiver::parse(f, 8, ip);
    mockAdvance(100);   // zawija sie przez 0
    CHECK(SteerCommandReceiver::isFresh(), "steer: swiezosc poprawna przez zawiniecie millis()");
    mockAdvance(1000);
    CHECK(!SteerCommandReceiver::isFresh(), "steer: po zawinieciu i 1100 ms - nieswieze");

    // gałąź diagnostyczna RAW (formatowanie, toString, bufor 14 bajtow) nie moze sie wywrocic
    DEBUG_STEER_COMMAND_RAW = true; DEBUG_STEER_COMMAND = true;
    for (int i = 0; i < 3; i++) { mockAdvance(10); SteerCommandReceiver::parse(f, 8, ip); }
    DEBUG_STEER_COMMAND_RAW = false; DEBUG_STEER_COMMAND = false;
    CHECK(true, "steer: tryb RAW dziala bez bledu");
    g_mockMillis = 100000;
  }

  // ---------- PGN 252: ustawienia steer ----------
  { CHECK(!SteerSettingsReceiver::hasReceived() && SteerSettingsReceiver::getCountsPerDeg() == 1 && SteerSettingsReceiver::getAckermanFix() == 100,
          "settings: bezpieczne wartosci przed odbiorem (countsPerDeg=1, ackerman=100)");
    const uint8_t p[8] = {40, 255, 20, 30, 50, 0xD4, 0xFE, 110};   // steerOffset = 0xFED4 = -300
    frame(f, 0xFC, p);
    SteerSettingsReceiver::parse(f, 4);
    CHECK(!SteerSettingsReceiver::hasReceived(), "settings: len<8 odrzucone");
    SteerSettingsReceiver::parse(f, 8);
    CHECK(SteerSettingsReceiver::hasReceived() && !SteerSettingsReceiver::isFromStorage(), "settings: odebrane z sieci");
    CHECK(SteerSettingsReceiver::getGainP() == 40 && SteerSettingsReceiver::getHighPWM() == 255 && SteerSettingsReceiver::getLowPWM() == 20 && SteerSettingsReceiver::getMinPWM() == 30,
          "settings: gainP / highPWM / lowPWM / minPWM");
    CHECK(SteerSettingsReceiver::getCountsPerDeg() == 50 && SteerSettingsReceiver::getSteerOffset() == -300 && SteerSettingsReceiver::getAckermanFix() == 110,
          "settings: countsPerDeg / steerOffset ze znakiem / ackermanFix");
    CHECK(SteerSettingsReceiver::gate().hasPending(), "settings: nowe wartosci czekaja na zapis do NVS");
    SteerSettingsData s = SteerSettingsReceiver::snapshot();
    CHECK(s.gainP == 40 && s.steerOffset == -300 && s.ackermanFix == 110, "settings: snapshot odpowiada odebranym");

    // przywracanie z pamieci (start): countsPerDeg=0 nie moze zostac dzielnikiem
    SteerSettingsData d = s; d.countsPerDeg = 0; d.gainP = 7;
    SteerSettingsReceiver::restore(d);
    CHECK(SteerSettingsReceiver::getCountsPerDeg() == 1 && SteerSettingsReceiver::getGainP() == 7, "settings: restore zamienia countsPerDeg=0 na 1");
    CHECK(!SteerSettingsReceiver::gate().hasPending(), "settings: po restore nic nie czeka na zapis");
    SteerSettingsReceiver::parse(f, 8);
    CHECK(SteerSettingsReceiver::getGainP() == 40 && SteerSettingsReceiver::gate().hasPending(), "settings: odebrane pozniej nadpisuje przywrocone i wymaga zapisu");
  }

  // ---------- PGN 239: maszyna ----------
  { CHECK(!MachineCommandReceiver::isFresh() && !MachineCommandReceiver::isSectionOn(1), "machine: przed ramka sekcje wylaczone");
    const uint8_t p[8] = {2, 55, 1, 3, 1, 0, 0x05, 0x80};
    // uturn=2, speed 5.5, hydLift=1, tram=3, geoStop=1, sekcje 1,3 oraz 16
    frame(f, 0xEF, p);
    MachineCommandReceiver::parse(f, 7);
    CHECK(!MachineCommandReceiver::isFresh(), "machine: len<8 odrzucone");
    MachineCommandReceiver::parse(f, 8);
    CHECK(MachineCommandReceiver::getUturnState() == 2 && near_(MachineCommandReceiver::getSpeedKmh(), 5.5f) &&
          MachineCommandReceiver::getHydLift() == 1 && MachineCommandReceiver::getTramState() == 3 && MachineCommandReceiver::getGeoStop() == 1,
          "machine: uturn / predkosc / hydLift / tram / geoStop");
    CHECK(MachineCommandReceiver::getSectionsMask() == 0x8005, "machine: maska 16 sekcji");
    CHECK(MachineCommandReceiver::isSectionOn(1) && !MachineCommandReceiver::isSectionOn(2) && MachineCommandReceiver::isSectionOn(3) && MachineCommandReceiver::isSectionOn(16),
          "machine: isSectionOn numeruje od 1, w tym sekcja 16");
    CHECK(!MachineCommandReceiver::isSectionOn(0) && !MachineCommandReceiver::isSectionOn(17), "machine: numery poza 1..16 = wylaczone");
    mockAdvance(7999);
    CHECK(MachineCommandReceiver::isSectionOn(1), "machine: 7999 ms - jeszcze swieze");
    mockAdvance(2);
    CHECK(!MachineCommandReceiver::isSectionOn(1) && !MachineCommandReceiver::isSectionOn(16), "machine: po 8 s bez ramki sekcje gasna (bezpieczny stan)");
    DEBUG_MACHINE_COMMAND = true; mockAdvance(1); MachineCommandReceiver::parse(f, 8); DEBUG_MACHINE_COMMAND = false;
    CHECK(MachineCommandReceiver::isSectionOn(1), "machine: nowa ramka znow wlacza");
  }

  // ---------- PGN 238: konfiguracja maszyny ----------
  { CHECK(!MachineConfigReceiver::hasReceived(), "config: przed ramka brak");
    const uint8_t p[8] = {5, 3, 1, 0, 0, 0, 0, 0};
    frame(f, 0xEE, p);
    MachineConfigReceiver::parse(f, 6);
    CHECK(!MachineConfigReceiver::hasReceived(), "config: len<8 odrzucone");
    MachineConfigReceiver::parse(f, 8);
    CHECK(MachineConfigReceiver::hasReceived() && MachineConfigReceiver::getRaiseTime() == 5 && MachineConfigReceiver::getLowerTime() == 3 &&
          MachineConfigReceiver::getEnableToolLift() == 1, "config: raiseTime / lowerTime / enableToolLift");
    MachineConfigReceiver::parse(f, 8);
    CHECK(MachineConfigReceiver::getRxCount() == 2, "config: licznik ramek");
    const uint8_t q[8] = {255, 0, 0, 1, 0, 0, 0, 0};
    frame(f, 0xEE, q); MachineConfigReceiver::parse(f, 8);
    CHECK(MachineConfigReceiver::getRaiseTime() == 255 && MachineConfigReceiver::getLowerTime() == 0 && MachineConfigReceiver::getEnableToolLift() == 0,
          "config: wartosc 255 i zero bez znaku");
  }

  // ---------- PGN 238: Invert relay (set0 bit 0) i wartosci User 1-4 ----------
  { // (poprzedni blok skonczyl na set0 = 1, wiec Invert relay jest juz wlaczony)
    const uint8_t p[8] = {2, 4, 1, 0x01, 11, 22, 33, 44};
    frame(f, 0xEE, p); MachineConfigReceiver::parse(f, 8);
    CHECK(MachineConfigReceiver::getInvertRelay(), "config: set0 bit 0 = Invert relay");
    CHECK(MachineConfigReceiver::getUser(0) == 11 && MachineConfigReceiver::getUser(1) == 22 && MachineConfigReceiver::getUser(2) == 33 &&
          MachineConfigReceiver::getUser(3) == 44 && MachineConfigReceiver::getUser(4) == 0, "config: User 1-4");
    CHECK(MachineConfigReceiver::invertNeedsSave(), "config: zmiana polaryzacji czeka na zapis");
    MachineConfigReceiver::markInvertSaved();
    CHECK(!MachineConfigReceiver::invertNeedsSave(), "config: po zapisie nic nie czeka");
    const uint8_t q[8] = {2, 4, 1, 0x02, 0, 0, 0, 0};   // inny bit set0: Invert relay wylaczony
    frame(f, 0xEE, q); MachineConfigReceiver::parse(f, 8);
    CHECK(!MachineConfigReceiver::getInvertRelay() && MachineConfigReceiver::invertNeedsSave(), "config: wylaczenie Invert relay");
    MachineConfigReceiver::restoreInvertRelay(true);
    CHECK(MachineConfigReceiver::getInvertRelay() && !MachineConfigReceiver::invertNeedsSave(), "config: odtworzenie z pamieci modulu");
  }

  // ---------- PGN 236: konfiguracja pinow maszyny ----------
  { CHECK(!MachinePinConfigReceiver::hasReceived() && MachinePinConfigReceiver::map().fn[0] == 1 && MachinePinConfigReceiver::map().fn[15] == 16,
          "piny: przed ramka uklad fabryczny");
    uint8_t g[30]; memset(g, 0, sizeof(g)); g[0] = 0x80; g[1] = 0x81; g[2] = 0x7F; g[3] = 0xEC; g[4] = 24;
    for (int i = 0; i < 24; i++) g[5 + i] = (uint8_t)(i < 16 ? 16 - i : 0);   // pin1 = Sec16 ... pin16 = Sec1
    MachinePinConfigReceiver::parse(g, 10);
    CHECK(!MachinePinConfigReceiver::hasReceived(), "piny: len<24 odrzucone");
    MachinePinConfigReceiver::parse(g, 24);
    CHECK(MachinePinConfigReceiver::hasReceived() && MachinePinConfigReceiver::map().fn[0] == 16 && MachinePinConfigReceiver::map().fn[15] == 1, "piny: mapa z ramki");
    CHECK(MachinePinConfigReceiver::needsSave(), "piny: zmiana czeka na zapis");
    MachinePinConfigReceiver::markSaved();
    CHECK(!MachinePinConfigReceiver::needsSave(), "piny: po zapisie nic nie czeka");
    MachinePinConfigReceiver::parse(g, 24);
    CHECK(!MachinePinConfigReceiver::needsSave() && MachinePinConfigReceiver::getRxCount() == 2, "piny: ta sama ramka nie wymaga zapisu");
    g[5] = 99; MachinePinConfigReceiver::parse(g, 24);
    CHECK(MachinePinConfigReceiver::map().fn[0] == 0, "piny: nieznany kod funkcji = brak");
  }

  // ---------- PGN 251: konfiguracja steer (Invert motor) ----------
  { CHECK(!SteerConfigReceiver::hasReceived() && !SteerConfigReceiver::invertMotor(), "steercfg: przed ramka brak, silnik normalnie");
    const uint8_t p[8] = {0x04 | 0x10, 77, 5, 0x04, 0, 0, 0, 0};   // set0: Invert motor + Cytron; trip 77; minSpeed 5; set1: prad
    frame(f, 0xFB, p); f[4] = 5;
    SteerConfigReceiver::parse(f, 3);
    CHECK(!SteerConfigReceiver::hasReceived(), "steercfg: len<5 odrzucone");
    SteerConfigReceiver::parse(f, 5);
    CHECK(SteerConfigReceiver::hasReceived() && SteerConfigReceiver::getSet0() == 0x14 && SteerConfigReceiver::getSet1() == 0x04 &&
          SteerConfigReceiver::getTrip() == 77 && SteerConfigReceiver::getMinSpeed10() == 5, "steercfg: pola");
    CHECK(SteerConfigReceiver::invertMotorRequested() && !SteerConfigReceiver::invertMotor(), "steercfg: zadanie zapisane, jeszcze nie zastosowane");
    SteerConfigReceiver::tick(true);
    CHECK(!SteerConfigReceiver::invertMotor(), "steercfg: przy zalaczonym sterowaniu bez zmian");
    SteerConfigReceiver::tick(false);
    CHECK(SteerConfigReceiver::invertMotor() && SteerConfigReceiver::invertNeedsSave(), "steercfg: po wylaczeniu sterowania zastosowane i czeka na zapis");
    SteerConfigReceiver::markInvertSaved();
    const uint8_t q[8] = {0x10, 77, 5, 0x04, 0, 0, 0, 0};
    frame(f, 0xFB, q); f[4] = 5; SteerConfigReceiver::parse(f, 5);
    SteerConfigReceiver::tick(true);
    CHECK(SteerConfigReceiver::invertMotor(), "steercfg: zmiana na biezaco nie przeskakuje w czasie jazdy");
    SteerConfigReceiver::tick(false);
    CHECK(!SteerConfigReceiver::invertMotor(), "steercfg: zastosowana po wylaczeniu sterowania");
    SteerConfigReceiver::restoreInvertMotor(true);
    CHECK(SteerConfigReceiver::invertMotor() && !SteerConfigReceiver::invertNeedsSave(), "steercfg: odtworzenie z pamieci modulu");
  }

  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
