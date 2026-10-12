// Testy tabeli satelitow (GpsSats): GSV (C/N0 per satelita i sygnal), GSA (uzywane), wygasanie, przepelnienie, odczyt bez blokad.
// Czysty C++, z makrami Arduino (wylapywanie kolizji nazw):
//     g++ -std=c++17 -Wall -Wextra -pthread tests/test_gps_sats.cpp -o /tmp/tgs && /tmp/tgs
#define EXTERNAL 0
#define INTERNAL 3
#define DEFAULT 1
#define INPUT 1
#define OUTPUT 3
#define HIGH 1
#define LOW 0
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include "../sensors/gps_sats.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)

// Dokladnie jak GpsHandler::splitFields(): tnie po ',' ORAZ po '*' (suma kontrolna to ostatnie pole), maks. 24 pola.
static uint8_t splitFields(char* line, char* fields[], uint8_t maxFields) {
    uint8_t count = 0; char* p = line; fields[count++] = p;
    while (*p && count < maxFields) { if (*p == ',' || *p == '*') { *p = '\0'; fields[count++] = p + 1; } p++; }
    return count;
}
static void gsv(const char* s, unsigned long now) { char b[128]; strcpy(b, s); char* f[24]; uint8_t n = splitFields(b, f, 24); GpsSats::feedGsv(f, n, now); }
static void gsa(const char* s, unsigned long now) { char b[128]; strcpy(b, s); char* f[24]; uint8_t n = splitFields(b, f, 24); GpsSats::feedGsa(f, n, now); }
static int snap(GpsSats::Sat* o, unsigned long now, bool* uk = nullptr) { bool k; int n = GpsSats::snapshot(o, GpsSats::MAX, now, uk ? uk : &k); return n; }
static const GpsSats::Sat* at(const GpsSats::Sat* a, int n, int sys, int prn, int sig);
static const GpsSats::Sat* find(const GpsSats::Sat* a, int n, int sys, int prn, int sig) { for (int i = 0; i < n; i++) if (a[i].sys == sys && a[i].prn == prn && a[i].sig == sig) return &a[i]; return nullptr; }

static const GpsSats::Sat* at(const GpsSats::Sat* a, int n, int sys, int prn, int sig) {      // gdy brak wpisu: wpis zerowy (czysty blad testu zamiast awarii)
    static const GpsSats::Sat none = {255, 0, 0, 0, 0, 0, false};
    const GpsSats::Sat* r = find(a, n, sys, prn, sig); return r ? r : &none;
}
static void hookWrite() { char b[128]; strcpy(b, "$GPGSV,1,1,01,02,45,120,77,1*7A"); char* f[24]; uint8_t n = splitFields(b, f, 24); GpsSats::feedGsv(f, n, 1000); }   // "pisarz" zmienia dane w trakcie kopiowania

int main() {
    GpsSats::Sat s[GpsSats::MAX]; bool uk;
    // ---------- GSV: podstawy ----------
    GpsSats::reset();
    gsv("$GPGSV,3,1,11,02,45,120,42,05,30,250,38,12,60,045,45,13,10,300,,1*7A", 1000);
    gsv("$GPGSV,3,2,11,15,20,080,35,18,75,180,47,20,05,015,28,24,40,330,41,1*7B", 1000);
    gsv("$GPGSV,3,3,11,29,12,200,30,31,55,100,44,32,25,270,36,1*4D", 1000);
    int n = snap(s, 1100);
    CHECK(n == 11, "trzy zdania GSV (4+4+3 satelity) daja 11 wpisow");
    const GpsSats::Sat* a = find(s, n, GpsSats::SYS_GPS, 12, 1);
    CHECK(a && a->snr == 45 && a->elev == 60 && a->az == 45, "GPS 12: C/N0 45, elewacja 60, azymut 45");
    a = find(s, n, GpsSats::SYS_GPS, 13, 1);
    CHECK(a && a->snr == GpsSats::SNR_NONE, "puste pole SNR (GPS 13): satelita widoczny, bez sygnalu (255)");
    CHECK(GpsSats::rxGsv == 3, "licznik odebranych GSV = 3");
    // ---------- wiele sygnalow (id po ostatniej grupie) ----------
    gsv("$GPGSV,2,1,07,02,45,120,35,05,30,250,30,12,60,045,38,15,20,080,33,6*55", 1000);
    gsv("$GPGSV,2,2,07,18,75,180,40,20,05,015,22,24,40,330,36,6*55", 1000);
    n = snap(s, 1100);
    CHECK(find(s, n, GpsSats::SYS_GPS, 12, 1) && find(s, n, GpsSats::SYS_GPS, 12, 6), "ten sam satelita na dwoch sygnalach (1 i 6) to dwa osobne wpisy");
    CHECK(at(s, n, GpsSats::SYS_GPS, 12, 1)->snr == 45 && at(s, n, GpsSats::SYS_GPS, 12, 6)->snr == 38, "kazdy sygnal ma swoje C/N0 (45 i 38)");
    CHECK(n == 11 + 7, "razem 11 + 7 wpisow");
    // ---------- systemy ----------
    GpsSats::reset();
    gsv("$GLGSV,1,1,04,65,30,100,40,66,45,200,42,79,10,300,,80,50,050,38,1*50", 1000);
    gsv("$GAGSV,1,1,03,05,40,100,44,11,20,200,39,24,60,300,47,7*50", 1000);
    gsv("$GBGSV,1,1,02,06,55,100,41,19,25,200,36,1*50", 1000);
    gsv("$GQGSV,1,1,01,01,70,100,45,1*50", 1000);
    n = snap(s, 1000);
    CHECK(find(s, n, GpsSats::SYS_GLO, 65, 1) && find(s, n, GpsSats::SYS_GLO, 80, 1), "GLONASS (GL), numeracja NMEA 65-96");
    CHECK(find(s, n, GpsSats::SYS_GAL, 24, 7) && at(s, n, GpsSats::SYS_GAL, 24, 7)->snr == 47, "Galileo (GA), sygnal 7");
    CHECK(find(s, n, GpsSats::SYS_BDS, 19, 1), "BeiDou (GB)");
    CHECK(find(s, n, GpsSats::SYS_QZS, 1, 1), "QZSS (GQ)");
    CHECK(at(s, n, GpsSats::SYS_GLO, 79, 1)->snr == GpsSats::SNR_NONE, "GLONASS 79: puste SNR = 255");
    // ---------- zdanie bez id sygnalu (starszy NMEA) ----------
    GpsSats::reset();
    gsv("$GPGSV,1,1,03,02,45,120,42,05,30,250,38,12,60,045,45*7A", 1000);
    n = snap(s, 1000);
    CHECK(n == 3 && find(s, n, GpsSats::SYS_GPS, 5, 0), "bez id sygnalu: 3 wpisy, sygnal 0");
    // ---------- aktualizacja w miejscu, wygasanie ----------
    GpsSats::reset();
    gsv("$GPGSV,1,1,02,02,45,120,30,05,30,250,38,1*7A", 1000);
    gsv("$GPGSV,1,1,02,02,46,121,41,05,30,250,39,1*7A", 2000);
    n = snap(s, 2100);
    CHECK(n == 2, "kolejny cykl aktualizuje wpisy w miejscu (bez duplikatow)");
    CHECK(at(s, n, GpsSats::SYS_GPS, 2, 1)->snr == 41 && at(s, n, GpsSats::SYS_GPS, 2, 1)->elev == 46, "wartosci z nowszego cyklu");
    CHECK(snap(s, 2000 + 3900) == 2, "3,9 s po ostatnim odswiezeniu wpisy jeszcze sa");
    CHECK(snap(s, 2000 + 4100) == 0, "po 4,1 s bez odswiezenia wygasaja");
    gsv("$GPGSV,1,1,01,02,45,120,33,1*7A", 10000);
    CHECK(snap(s, 10100) == 1 && find(s, 1, GpsSats::SYS_GPS, 2, 1), "po wygasnieciu wpis wraca z nowym zdaniem (stare, nieodswiezone nie wracaja)");
    // ---------- przepelnienie ----------
    GpsSats::reset();
    for (int i = 0; i < 130; i++) { char b[128]; snprintf(b, sizeof b, "$GPGSV,1,1,01,%02d,45,120,%02d,%d*7A", 1 + (i % 99), 20 + (i % 30), 1 + i / 99); gsv(b, 1000 + i); }
    n = snap(s, 1200);
    CHECK(n <= GpsSats::MAX && n >= 120, "130 roznych wpisow: tablica ograniczona do MAX, bez bledu");
    CHECK(find(s, n, GpsSats::SYS_GPS, 1 + (129 % 99), 1 + 129 / 99) != nullptr, "najnowszy wpis jest w tablicy (nadpisany najstarszy)");
    // przy pelnej tablicy nadpisywany jest NAJSTARSZY wpis, a nie pierwszy slot
    GpsSats::reset();
    for (int i = 0; i < GpsSats::MAX; i++) { char b[128]; snprintf(b, sizeof b, "$GPGSV,1,1,01,%02d,45,120,40,%d*7A", 1 + (i % 99), 1 + i / 99); gsv(b, 1000 + i); }   // 128 wpisow, indeks 0 = najstarszy
    gsv("$GPGSV,1,1,01,01,45,120,41,1*7A", 1500);        // odswiezenie wpisu z indeksu 0 (PRN 1, sygnal 1): teraz jest NAJNOWSZY
    gsv("$GPGSV,1,1,01,99,45,120,42,7*7A", 1600);        // nowy, 129. wpis - tablica pelna
    n = snap(s, 1700);
    CHECK(at(s, n, GpsSats::SYS_GPS, 99, 7)->snr == 42, "nowy wpis trafil do tablicy");
    CHECK(at(s, n, GpsSats::SYS_GPS, 1, 1)->snr == 41, "odswiezony (najnowszy) wpis z indeksu 0 zostal, mimo ze jest pierwszy w tablicy");
    CHECK(find(s, n, GpsSats::SYS_GPS, 2, 1) == nullptr, "nadpisany zostal NAJSTARSZY (PRN 2, sygnal 1), nie pierwszy slot");
    // ---------- GSA: ktore satelity sa uzywane ----------
    GpsSats::reset();
    gsv("$GPGSV,1,1,04,02,45,120,42,05,30,250,38,12,60,045,45,13,10,300,30,1*7A", 1000);
    gsv("$GLGSV,1,1,02,65,30,100,40,66,45,200,42,1*50", 1000);
    gsa("$GNGSA,A,3,02,05,12,,,,,,,,,,1.2,0.7,1.0,1*34", 1000);
    gsa("$GNGSA,A,3,65,,,,,,,,,,,,1.2,0.7,1.0,2*34", 1000);
    n = snap(s, 1100, &uk);
    CHECK(uk, "po GSA informacja o uzywanych jest znana");
    CHECK(at(s, n, GpsSats::SYS_GPS, 12, 1)->used && at(s, n, GpsSats::SYS_GPS, 2, 1)->used, "GPS 2 i 12 uzywane (id systemu 1)");
    CHECK(!at(s, n, GpsSats::SYS_GPS, 13, 1)->used, "GPS 13 widoczny, ale NIE uzywany");
    CHECK(at(s, n, GpsSats::SYS_GLO, 65, 1)->used && !at(s, n, GpsSats::SYS_GLO, 66, 1)->used, "GLONASS 65 uzywany, 66 nie (id systemu 2, niezalezny od GPS)");
    // GSA bez id systemu: z talkera; GN bez id: ignorowane
    GpsSats::reset();
    gsv("$GPGSV,1,1,02,02,45,120,42,05,30,250,38,1*7A", 1000);
    gsa("$GPGSA,A,3,02,,,,,,,,,,,,1.2,0.7,1.0*34", 1000);
    n = snap(s, 1100, &uk);
    CHECK(uk && at(s, n, GpsSats::SYS_GPS, 2, 1)->used && !at(s, n, GpsSats::SYS_GPS, 5, 1)->used, "GPGSA bez id systemu: systemu szukamy po talkerze");
    GpsSats::reset();
    gsv("$GPGSV,1,1,01,02,45,120,42,1*7A", 1000);
    gsa("$GNGSA,A,3,02,,,,,,,,,,,,1.2,0.7,1.0*34", 1000);
    n = snap(s, 1100, &uk);
    CHECK(!uk && !at(s, n, GpsSats::SYS_GPS, 2, 1)->used, "GNGSA bez id systemu: nie zgadujemy, uzywane nieznane");
    // wiele GSA w jednym cyklu sumuje sie, nowy cykl zastepuje
    GpsSats::reset();
    gsv("$GPGSV,1,1,04,02,45,120,42,05,30,250,38,12,60,045,45,13,10,300,30,1*7A", 1000);
    gsa("$GNGSA,A,3,02,05,,,,,,,,,,,1.2,0.7,1.0,1*34", 1000);
    gsa("$GNGSA,A,3,12,13,,,,,,,,,,,1.2,0.7,1.0,1*34", 1100);
    n = snap(s, 1200);
    CHECK(at(s, n, GpsSats::SYS_GPS, 2, 1)->used && at(s, n, GpsSats::SYS_GPS, 13, 1)->used, "dwa GSA tego samego systemu w jednym cyklu: sumuja sie");
    gsa("$GNGSA,A,3,02,,,,,,,,,,,,1.2,0.7,1.0,1*34", 2000);      // nowy cykl (1 s pozniej)
    gsv("$GPGSV,1,1,04,02,45,120,42,05,30,250,38,12,60,045,45,13,10,300,30,1*7A", 2000);
    n = snap(s, 2100);
    CHECK(at(s, n, GpsSats::SYS_GPS, 2, 1)->used && !at(s, n, GpsSats::SYS_GPS, 13, 1)->used && !at(s, n, GpsSats::SYS_GPS, 5, 1)->used, "nowy cykl GSA zastepuje poprzedni zestaw uzywanych");
    n = snap(s, 2000 + 4100, &uk);
    CHECK(!uk, "po 4 s bez GSA informacja o uzywanych jest nieznana");
    // ---------- odpornosc na smieci ----------
    GpsSats::reset();
    gsv("$GPGSV,1,1*7A", 1000); gsv("$GPGSV*7A", 1000); gsv("$XXGSV,1,1,01,02,45,120,42,1*7A", 1000);
    gsv("$GPGSV,1,1,02,,,,,05,30,250,38,1*7A", 1000); gsv("$GPGSV,1,1,02,xyz,45,120,42,300,10,10,10,1*7A", 1000);
    gsa("$GPGSA,A,3,02*7A", 1000); gsa("$GPGSA*7A", 1000);
    n = snap(s, 1000);
    CHECK(n == 1 && find(s, n, GpsSats::SYS_GPS, 5, 1), "smieci (za krotkie, nieznany talker, puste grupy, PRN nienumeryczny lub > 255): tylko poprawny wpis (GPS 5)");
    gsv("$GPGSV,1,1,01,02,95,400,150,1*7A", 1000);
    n = snap(s, 1000);
    CHECK(at(s, n, GpsSats::SYS_GPS, 2, 1)->elev == 90 && at(s, n, GpsSats::SYS_GPS, 2, 1)->az == 359 && at(s, n, GpsSats::SYS_GPS, 2, 1)->snr == 99, "wartosci poza zakresem sa przycinane (elew. 90, azym. 359, C/N0 99)");
    // ---------- odczyt bez blokad ----------
    GpsSats::reset();
    gsv("$GPGSV,1,1,02,02,45,120,42,05,30,250,38,1*7A", 1000);
    GpsSats::seq.fetch_add(1);          // symulacja: pisarz w trakcie zapisu (licznik nieparzysty)
    n = snap(s, 1100);
    GpsSats::seq.fetch_add(1);
    CHECK(n == 2, "licznik nieparzysty (zapis w toku): czytelnik NIE wisi, po wyczerpaniu prob zwraca ostatnia kopie");
    // deterministycznie: zapis w trakcie kopiowania => kopia niespojna => czytelnik MUSI powtorzyc i zwrocic nowe, spojne dane
    GpsSats::reset();
    gsv("$GPGSV,1,1,01,02,45,120,10,1*7A", 1000);
    GpsSats::testHook = hookWrite;
    n = snap(s, 1100);
    CHECK(n == 1 && at(s, n, GpsSats::SYS_GPS, 2, 1)->snr == 77, "zapis w trakcie kopiowania: czytelnik powtarza i zwraca NOWE dane (77), nie polowiczna kopie (10)");
    CHECK(GpsSats::testHook == nullptr, "zaczep testowy jednorazowy (nie zostaje w produkcji)");
    // test wspolbieznosci: pisarz zapisuje spojne wartosci (snr = prn), czytelnik sprawdza, czy nie widzi polowicznych wpisow
    GpsSats::reset();
    std::atomic<bool> stop(false); std::atomic<long> bad(0), reads(0);
    std::thread w([&] { unsigned long t = 1000; while (!stop.load()) { char b[128]; for (int p = 1; p <= 12; p++) { snprintf(b, sizeof b, "$GPGSV,1,1,01,%02d,45,120,%02d,1*7A", p, p); gsv(b, t); } t++; std::this_thread::sleep_for(std::chrono::microseconds(400)); } });
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (std::chrono::steady_clock::now() < end) {
        GpsSats::Sat r[GpsSats::MAX]; bool k; int m = GpsSats::snapshot(r, GpsSats::MAX, 1000000, &k); (void)m;
        for (int i = 0; i < m; i++) if (r[i].snr != r[i].prn) bad++;
        reads++;
    }
    stop = true; w.join();
    CHECK(reads > 100 && bad == 0, "300 ms jednoczesnego zapisu i odczytu: zadnego niespojnego wpisu (snr != prn)");
    printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
    return fails ? 1 : 0;
}
