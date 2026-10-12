// Testy wyboru nadawcy AgOpenWeb (connectivity/agio_senders.h). Czysty C++, bez Arduino.
// Kompilacja: g++ -std=c++17 -Wall -Wextra tests/test_agio_senders.cpp -o /tmp/test_agio_senders && /tmp/test_agio_senders
#include <cstdio>
#include <cstdint>
#include "../connectivity/agio_senders.h"

static int g_ok = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_ok++; } else { g_fail++; std::printf("BLAD: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)

static const uint32_t PC = 0x1101A8C0;       // 192.168.1.17 (dowolna, spojna reprezentacja)
static const uint32_t PHONE = 0x0801A8C0;    // 192.168.1.8
static const uint32_t TABLET = 0x0901A8C0;   // 192.168.1.9

static const AgioSenders::Entry* entryOf(uint32_t ip) {
    for (uint8_t i = 0; i < AgioSenders::count(); i++) { const AgioSenders::Entry* e = AgioSenders::nth(i); if (e && e->ip == ip) return e; }
    return nullptr;
}

int main() {
    // 1. Pierwszy nadawca zostaje wybrany i jego ramki sa wykonywane.
    AgioSenders::reset();
    CHECK(AgioSenders::selected() == 0, "na starcie nikt nie jest wybrany");
    CHECK(AgioSenders::note(PC, 1000) == true, "pierwszy nadawca: ramka przyjeta");
    CHECK(AgioSenders::selected() == PC, "pierwszy nadawca zostaje wybrany");
    CHECK(!AgioSenders::isManual(), "domyslnie tryb automatyczny");
    CHECK(AgioSenders::switches == 0, "pierwszy wybor nie liczy sie jako przelaczenie");

    // 2. Drugi nadawca, gdy pierwszy sie odzywa: ignorowany, wybor sie nie zmienia.
    CHECK(AgioSenders::note(PHONE, 1010) == false, "drugi nadawca: ramka zignorowana");
    CHECK(AgioSenders::selected() == PC, "drugi nadawca nie przejmuje wyboru");
    CHECK(AgioSenders::count() == 2, "obaj nadawcy sa na liscie");
    CHECK(entryOf(PHONE) && entryOf(PHONE)->ignored == 1 && entryOf(PHONE)->frames == 1, "licznik ignorowanych ramek drugiego");
    CHECK(AgioSenders::ignoredTotal == 1, "licznik ignorowanych lacznie");

    // 3. Scenariusz z pomiaru: komputer i telefon pisza na przemian, 100 Hz kazdy. Moduł zostaje przy komputerze: 100% jego ramek przyjetych.
    AgioSenders::reset();
    int accepted = 0, rejected = 0;
    uint32_t t = 0;
    AgioSenders::note(PC, t);
    for (int i = 0; i < 1000; i++) {
        t += 5;
        if (AgioSenders::note((i % 2) ? PHONE : PC, t)) accepted++; else rejected++;
    }
    CHECK(accepted == 500 && rejected == 500, "naprzemienne ramki: tylko komputer przyjety, telefon w calosci zignorowany");
    CHECK(AgioSenders::selected() == PC && AgioSenders::switches == 0, "naprzemienne ramki: wybor sie nie przelacza");

    // 4. Wybrany milczy dluzej niz ACTIVE_MS: nastepny nadawca, ktory sie odezwie, zostaje wybrany (jedno przelaczenie).
    AgioSenders::reset();
    AgioSenders::note(PC, 0);
    CHECK(AgioSenders::note(PHONE, 4999) == false, "tuz przed limitem: komputer nadal wazny (telefon ignorowany)");
    CHECK(AgioSenders::note(PHONE, 5001) == true, "komputer milczy > 5 s: telefon przejmuje");
    CHECK(AgioSenders::selected() == PHONE, "telefon wybrany");
    CHECK(AgioSenders::switches == 1, "jedno przelaczenie");
    CHECK(AgioSenders::note(PC, 5002) == false, "po powrocie komputera: ignorowany, telefon jest aktywny");

    // 5. Zamkniecie programu i powrot z nowym adresem (DHCP): po ciszy nowy adres zostaje wybrany.
    AgioSenders::reset();
    AgioSenders::note(PC, 100);
    CHECK(AgioSenders::note(TABLET, 6000) == true, "komputer zamilkl, nowy adres przejmuje");
    CHECK(AgioSenders::selectedActive(6000), "nowy wybrany jest aktywny");
    CHECK(!AgioSenders::isActive(PC, 6000), "stary adres nieaktywny");

    // 6. Reczny wybor: tylko przypiety, nawet gdy jeszcze nic od niego nie przyszlo.
    AgioSenders::reset();
    AgioSenders::note(PHONE, 10);
    AgioSenders::setPinned(PC);
    CHECK(AgioSenders::isManual() && AgioSenders::selected() == PC, "przypiecie ustawia tryb reczny i wybor");
    CHECK(entryOf(PC) && !entryOf(PC)->seen && entryOf(PC)->frames == 0, "przypiety adres jest na liscie, choc nic od niego nie przyszlo");
    CHECK(AgioSenders::ageMs(*entryOf(PC), 20) == -1, "wiek przypietego bez ramek = -1");
    CHECK(AgioSenders::note(PHONE, 20) == false, "reczny wybor: dotychczasowy nadawca zignorowany");
    CHECK(AgioSenders::note(PC, 30) == true, "reczny wybor: przypiety przyjety");
    CHECK(AgioSenders::switches == 1, "przypiecie innego niz biezacy = jedno przelaczenie");

    // 7. Reczny wybor NIE przechodzi na innego nadawce, gdy przypiety milczy (nikt nie przejmie sterowania po cichu).
    CHECK(AgioSenders::note(PHONE, 60000) == false, "przypiety milczy od 60 s: telefon nadal ignorowany");
    CHECK(AgioSenders::selected() == PC, "przypiety zostaje wybrany mimo ciszy");
    CHECK(!AgioSenders::selectedActive(60000), "wybrany jest nieaktywny (panel pokaze 'milczy')");

    // 8. Powrot do automatycznego: zostajemy przy biezacym, dopoki sie odzywa; gdy zamilknie, wybiera sie nowy.
    AgioSenders::reset();
    AgioSenders::setPinned(PC);
    AgioSenders::note(PC, 0);
    AgioSenders::clearPin();
    CHECK(!AgioSenders::isManual(), "clearPin: tryb automatyczny");
    CHECK(AgioSenders::note(PHONE, 1000) == false, "po clearPin: komputer aktywny, telefon ignorowany");
    CHECK(AgioSenders::note(PHONE, 9000) == true, "po clearPin: komputer zamilkl, telefon przejmuje");

    // 9. Zmiana przypiecia z jednego na drugi.
    AgioSenders::reset();
    AgioSenders::setPinned(PC);
    AgioSenders::setPinned(PHONE);
    CHECK(AgioSenders::selected() == PHONE && AgioSenders::switches == 1, "przepiecie na inny adres");
    CHECK(AgioSenders::note(PC, 5) == false && AgioSenders::note(PHONE, 6) == true, "po przepieciu liczy sie tylko nowy");
    AgioSenders::setPinned(0);
    CHECK(!AgioSenders::isManual(), "setPinned(0) = tryb automatyczny");

    // 10. Adres 0 jest odrzucany i nie trafia na liste.
    AgioSenders::reset();
    CHECK(AgioSenders::note(0, 5) == false && AgioSenders::count() == 0, "adres 0 ignorowany");

    // 11. Pelna tabela: najdawniej slyszany nadawca jest wypierany, wybrany nigdy.
    AgioSenders::reset();
    AgioSenders::note(PC, 0);                                   // wybrany
    for (uint32_t k = 1; k < AgioSenders::MAX; k++) AgioSenders::note(0x0A000000u + k, k * 10);   // 7 innych: tabela pelna
    CHECK(AgioSenders::count() == AgioSenders::MAX, "tabela pelna");
    AgioSenders::note(0x0B000001u, 1000);                       // nowy nadawca wypiera najdawniejszego (nie PC)
    CHECK(AgioSenders::count() == AgioSenders::MAX, "po wyparciu nadal MAX wpisow");
    CHECK(entryOf(PC) != nullptr, "wybrany nigdy nie jest wypierany, choc slyszany najdawniej");
    CHECK(entryOf(0x0A000001u) == nullptr, "wyparty najdawniejszy z pozostalych");
    CHECK(entryOf(0x0B000001u) != nullptr, "nowy nadawca jest na liscie");
    CHECK(AgioSenders::selected() == PC, "wybor bez zmian po wyparciu");

    // 12. Zawiniecie millis() (po ok. 49 dniach): wiek liczony poprawnie.
    AgioSenders::reset();
    AgioSenders::note(PC, 0xFFFFFF00u);
    CHECK(AgioSenders::isActive(PC, 0x00000100u), "po zawinieciu: 0x200 ms od ramki = aktywny");
    CHECK(AgioSenders::ageMs(*entryOf(PC), 0x00000100u) == 0x200, "po zawinieciu: wiek 512 ms");
    CHECK(!AgioSenders::isActive(PC, 0x00002000u), "po zawinieciu: > 5 s = nieaktywny");
    CHECK(AgioSenders::note(PHONE, 0x00002000u) == true, "po zawinieciu: cisza > 5 s, telefon przejmuje");

    // 13. Liczniki ramek.
    AgioSenders::reset();
    for (int i = 0; i < 10; i++) AgioSenders::note(PC, i);
    for (int i = 0; i < 4; i++) AgioSenders::note(PHONE, 20 + i);
    CHECK(entryOf(PC)->frames == 10 && entryOf(PC)->ignored == 0, "komputer: 10 ramek, 0 zignorowanych");
    CHECK(entryOf(PHONE)->frames == 4 && entryOf(PHONE)->ignored == 4, "telefon: 4 ramki, 4 zignorowane");
    CHECK(AgioSenders::ignoredTotal == 4, "razem zignorowane: 4");

    // 14. Kolejnosc listy = kolejnosc dodania (stabilna - panel nie przeskakuje).
    AgioSenders::reset();
    AgioSenders::note(PHONE, 1); AgioSenders::note(PC, 2); AgioSenders::note(TABLET, 3);
    CHECK(AgioSenders::nth(0)->ip == PHONE && AgioSenders::nth(1)->ip == PC && AgioSenders::nth(2)->ip == TABLET, "kolejnosc dodania");
    CHECK(AgioSenders::nth(3) == nullptr, "poza lista = nullptr");

    // 15. Wyscig panel/petla: ramka z PÓZNIEJSZYM znacznikiem niz `now` panelu nie moze dac wieku ujemnego ani "milczy".
    AgioSenders::reset();
    AgioSenders::note(PC, 10000);
    CHECK(AgioSenders::ageMs(*AgioSenders::nth(0), 9990) == 0, "wiek nie jest ujemny, gdy ramka jest nowsza niz now panelu");
    CHECK(AgioSenders::isActive(PC, 9990), "nadawca nadal aktywny przy now sprzed ramki");
    CHECK(AgioSenders::ageMs(*AgioSenders::nth(0), 10250) == 250, "zwykly wiek bez zmian");
    CHECK(AgioSenders::selectedActive(9999), "wybrany aktywny (now tuz przed ramka)");
    AgioSenders::reset(); AgioSenders::note(PC, 0xFFFFFFF0u);
    CHECK(AgioSenders::ageMs(*AgioSenders::nth(0), 0x10u) == 32, "wiek przez zawiniecie millis()");

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_ok + g_fail);
    return g_fail == 0 ? 0 : 1;
}
