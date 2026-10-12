// Testy logiki wyjść maszyny (io/machine_outputs.h): mapa pinów z PGN 236, hydraulika, tory, geo-stop, Invert relay.
// Uruchomienie: g++ -std=c++17 -Wall -Wextra tests/test_machine_outputs.cpp -o /tmp/test_machine_outputs && /tmp/test_machine_outputs
#include <cstdio>
#include "../io/machine_outputs.h"

static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

int main() {
    // 1. Układ przed konfiguracją z AgOpenWeb: Sec1..Sec16 na pinach 1..16, nic więcej (żaden pin nie jest wyjątkowy)
    {   MachinePinMap m;
        CHECK(m.fn[0] == 1 && m.fn[14] == 15 && m.fn[15] == 16 && m.fn[16] == 0 && m.fn[23] == 0);
        CHECK(!m.has(MachineFn::HYD_UP) && !m.has(MachineFn::HYD_DOWN));
        MachineInputs in; in.fresh = true; in.sections = 0x0005;   // sekcje 1 i 3
        CHECK(machineLogicalMask(m, in) == 0x0005);
        in.sections = 0x8000;                                        // sekcja 16 -> pin 16
        CHECK(machineLogicalMask(m, in) == 0x8000);
        in.sections = 0; in.hydUp = true;
        CHECK(machineLogicalMask(m, in) == 0);                       // bez HydUp w mapie podnośnik nic nie włącza
    }
    // 2. Konfiguracja z AgOpenWeb: dowolny pin dowolną funkcją
    {   uint8_t raw[24] = {0}; raw[0] = 3; raw[1] = 1; raw[2] = MachineFn::HYD_DOWN; raw[3] = MachineFn::HYD_UP;
        raw[4] = MachineFn::TRAM_LEFT; raw[5] = MachineFn::TRAM_RIGHT; raw[6] = MachineFn::GEO_STOP; raw[7] = 16;
        MachinePinMap m; m.load(raw);
        MachineInputs in; in.fresh = true;
        in.sections = 1u << 2;                                        // sekcja 3 -> pin 1
        CHECK(machineLogicalMask(m, in) == 0x0001);
        in.sections = 1u << 15;                                       // sekcja 16 -> pin 8
        CHECK(machineLogicalMask(m, in) == 0x0080);
        in.sections = 0; in.hydDown = true;
        CHECK(machineLogicalMask(m, in) == 0x0004);                   // HydDown na pinie 3
        in.hydDown = false; in.hydUp = true;
        CHECK(machineLogicalMask(m, in) == 0x0008);                   // HydUp na pinie 4
        in.hydUp = false; in.tram = 0x02;                             // bit 1 = lewe koło -> TramLeft (pin 5)
        CHECK(machineLogicalMask(m, in) == 0x0010);
        in.tram = 0x01;                                               // bit 0 = prawe koło -> TramRight (pin 6)
        CHECK(machineLogicalMask(m, in) == 0x0020);
        in.tram = 0; in.geoStop = true;
        CHECK(machineLogicalMask(m, in) == 0x0040);                   // GeoStop pin 7
        CHECK(m.has(MachineFn::GEO_STOP) && !m.has(MachineFn::SEC_FIRST + 3));
    }
    // 3. Kody spoza listy i piny 17..24
    {   uint8_t raw[24]; for (int i = 0; i < 24; i++) raw[i] = 200;
        raw[16] = 1;                                                  // pin 17 nie ma sprzętu
        MachinePinMap m; m.load(raw);
        CHECK(m.fn[0] == 0 && m.fn[23] == 0 && m.fn[16] == 1);
        MachineInputs in; in.fresh = true; in.sections = 0xFFFF; in.hydUp = in.hydDown = true; in.tram = 3; in.geoStop = true;
        CHECK(machineLogicalMask(m, in) == 0);                        // nic nie jest przypisane do istniejących pinów
        CHECK(!m.has(1));                                             // pin 17 nie liczy się jako istniejący
        raw[0] = MachineFn::MAX; MachinePinMap m2; m2.load(raw); CHECK(m2.fn[0] == MachineFn::MAX);
        raw[0] = MachineFn::MAX + 1; m2.load(raw); CHECK(m2.fn[0] == 0);
    }
    // 4. Nieświeże polecenie wyłącza sekcje, tory i geo-stop (hydraulika zależy od własnego wejścia)
    {   MachinePinMap m; MachineInputs in; in.fresh = false; in.sections = 0xFFFF; in.tram = 3; in.geoStop = true;
        CHECK(machineLogicalMask(m, in) == 0);
    }
    // 5. Invert relay: wszystkie 16 wyjść jest odwrócone, także nieużywane; stan jałowy = wszystko HIGH
    {   CHECK(machineIdleMask(false) == 0x0000 && machineIdleMask(true) == 0xFFFF);
        CHECK(machinePhysicalMask(0x0005, false) == 0x0005 && machinePhysicalMask(0x0005, true) == 0xFFFA);
    }
    // 6. Równość map (do zapisu tylko przy zmianie)
    {   MachinePinMap a, b; CHECK(a == b);
        b.fn[3] = 0; CHECK(a != b);
    }
    // 7. Hydraulika bez czasów (0 / jeszcze nieodebrane) = poziom: 1 = opuść, 2 = podnieś, 0 = nic; nieświeże = nic
    {   HydLogic h;
        h.update(HydState::NONE, true, 1000, 0, 0);  CHECK(!h.up() && !h.down());
        h.update(HydState::RAISE, true, 1100, 0, 0); CHECK(h.up() && !h.down());
        h.update(HydState::LOWER, true, 1200, 0, 0); CHECK(!h.up() && h.down());   // w polu: podnośnik NIE jest podnoszony
        h.update(HydState::RAISE, false, 1300, 0, 0); CHECK(!h.up() && !h.down()); // utrata łączności
        h.update(HydState::LOWER, true, 1400, 0, 0); CHECK(!h.up() && h.down());
        h.update(7, true, 1500, 0, 0);               CHECK(!h.up() && !h.down());  // nieznany stan
    }
    // 8. Impulsy czasowe: podnoszenie raiseTime, opuszczanie lowerTime, natychmiastowe zwolnienie po zniknięciu polecenia
    {   HydLogic h;
        h.update(HydState::RAISE, true, 0, 4, 2);     CHECK(h.up() && !h.down());
        h.update(HydState::RAISE, true, 3900, 4, 2);  CHECK(h.up());
        h.update(HydState::RAISE, true, 4000, 4, 2);  CHECK(!h.up());               // po 4 s puszczone
        h.update(HydState::RAISE, true, 9000, 4, 2);  CHECK(!h.up());               // bez nowego zbocza nie startuje
        h.update(HydState::NONE, true, 9100, 4, 2);   CHECK(!h.up() && !h.down());
        h.update(HydState::LOWER, true, 10000, 4, 2); CHECK(!h.up() && h.down());
        h.update(HydState::LOWER, true, 11900, 4, 2); CHECK(h.down());
        h.update(HydState::LOWER, true, 12000, 4, 2); CHECK(!h.down());             // po 2 s puszczone
        h.update(HydState::RAISE, true, 13000, 4, 2); CHECK(h.up());
        h.update(HydState::NONE, true, 13100, 4, 2);  CHECK(!h.up());               // polecenie zniknęło
        h.update(HydState::RAISE, true, 14000, 0, 0); CHECK(h.up());                // czas nieznany: poziom
        h.reset(); CHECK(!h.up() && !h.down());
    }
    // 9. Reczne sterowanie podnosnikiem (HydControl): start zawsze automatyczny, reczne wygrywa z AgOpenWeb, powrot do automatu
    {   HydControl c;
        CHECK(!c.manual() && !c.up() && !c.down());
        c.update(HydState::RAISE, true, 0, 0, 0);          CHECK(c.up() && !c.down());          // automat
        c.setManual(false); c.update(HydState::RAISE, true, 100, 0, 0);
        CHECK(c.manual() && !c.up() && c.down() && !c.manualRaise());                           // reczne OPUSC wygrywa z RAISE
        c.update(HydState::NONE, false, 200, 0, 0);        CHECK(c.down());                     // reczne przetrwa brak PGN 239
        c.setManual(true); c.update(HydState::LOWER, true, 300, 0, 0);
        CHECK(c.up() && !c.down() && c.manualRaise());
        c.setAuto(); c.update(HydState::NONE, true, 400, 0, 0);
        CHECK(!c.manual() && !c.up() && !c.down());                                              // automat: polecenia brak
        c.update(HydState::LOWER, true, 500, 0, 0);        CHECK(c.down());
        c.setManual(true); c.idle();                       CHECK(!c.up() && !c.down());         // wylaczony ekspander: nic nie jest sterowane
        // impuls czasowy przerwany przejsciem na reczne i z powrotem nie startuje sam
        HydControl d; d.update(HydState::RAISE, true, 0, 4, 2); CHECK(d.up());
        d.setManual(false); d.update(HydState::RAISE, true, 1000, 4, 2); CHECK(!d.up() && d.down());
        d.setAuto(); d.update(HydState::RAISE, true, 2000, 4, 2);        CHECK(d.up());          // polecenie nadal trwa: nowy impuls
    }
    // 10. Utrata lacznosci (PGN 239 nieswiezy): sekcje, tramline, geo-stop i podnosnik automatyczny gasna; reczny podnosnik zostaje
    {   MachinePinMap m; m.setDefault();
        m.fn[13] = MachineFn::TRAM_LEFT; m.fn[14] = MachineFn::GEO_STOP; m.fn[15] = MachineFn::HYD_UP;
        HydControl c;
        MachineInputs in; in.fresh = true; in.sections = 0x1FFF; in.tram = 3; in.geoStop = true;
        c.update(HydState::RAISE, true, 0, 0, 0); in.hydUp = c.up(); in.hydDown = c.down();
        CHECK(machineLogicalMask(m, in) != 0);
        in.fresh = false; c.update(HydState::RAISE, false, 100, 0, 0); in.hydUp = c.up(); in.hydDown = c.down();
        CHECK(machineLogicalMask(m, in) == 0);
        CHECK(machinePhysicalMask(machineLogicalMask(m, in), true) == 0xFFFF);                   // przekazniki aktywne niskim: bezpieczny stan = wszystko HIGH
        c.setManual(true); c.update(HydState::NONE, false, 200, 0, 0); in.hydUp = c.up();
        CHECK(machineLogicalMask(m, in) == (1u << 15));                                          // tylko reczny HydUp
    }
    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
