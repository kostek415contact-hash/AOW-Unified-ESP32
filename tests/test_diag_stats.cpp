// Testy liczników sieci (NetStats) i stanu OTA (OtaStatus) - czysty C++, bez Arduino.
// Uruchomienie z katalogu glownego projektu:
//     g++ -std=c++17 -Wall -Wextra tests/test_diag_stats.cpp -o /tmp/test_diag_stats && /tmp/test_diag_stats
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
#include <string>
#include "../diagnostics/net_stats.h"
#include "../ota/ota_status.h"
static int fails = 0, total = 0;
#define CHECK(cond, name) do { total++; if (!(cond)) { fails++; printf("  BLAD: %s\n", name); } } while (0)

int main() {
    // ---- NetStats: wysylanie ----
    CHECK(NetStats::msSinceLastTxFail(5000) == -1, "brak bledu wysylania => -1");
    NetStats::noteTx(true, 1000); NetStats::noteTx(true, 1010); NetStats::noteTx(false, 2000); NetStats::noteTx(true, 2010);
    CHECK(NetStats::txOk == 3 && NetStats::txFail == 1, "3 udane, 1 nieudane");
    CHECK(NetStats::msSinceLastTxFail(2500) == 500, "ile ms temu byl ostatni blad");
    NetStats::noteTx(false, 0);
    CHECK(NetStats::lastTxFailMs == 1 && NetStats::msSinceLastTxFail(1001) == 1000, "blad w chwili millis()=0 nie jest mylony z 'nigdy'");
    NetStats::noteUdpRebuild(); NetStats::noteUdpRebuild();
    CHECK(NetStats::udpRebuilds == 2, "odbudowy gniazd UDP");
    NetStats::noteBadFrame();
    CHECK(NetStats::badFrames == 1, "odrzucone ramki");
    NetStats::noteCrc(1); NetStats::noteCrc(1); NetStats::noteCrc(0); NetStats::noteCrc(-1);
    CHECK(NetStats::crcOk == 2 && NetStats::crcBad == 1, "CRC: 2 poprawne, 1 bledne, -1 (nie do oceny) pomijane");

    // ---- NetStats: PGN ----
    for (int i = 0; i < 5; i++) NetStats::noteRx(254);
    NetStats::noteRx(239); NetStats::noteRx(252); NetStats::noteRx(254);
    CHECK(NetStats::used == 3, "3 rozne PGN");
    CHECK(NetStats::table[0].pgn == 254 && NetStats::table[0].count == 6, "PGN 254 policzone 6 razy");
    CHECK(NetStats::table[1].pgn == 239 && NetStats::table[1].count == 1 && NetStats::table[2].pgn == 252, "kolejnosc wg pierwszego wystapienia");
    for (uint16_t p = 1000; p < 1030; p++) NetStats::noteRx(p);
    CHECK(NetStats::used == NetStats::MAX_PGN, "tabela zapelnia sie do MAX_PGN i nie wychodzi poza");
    CHECK(NetStats::otherPgn == (30 - (NetStats::MAX_PGN - 3)), "nadmiarowe PGN liczone w 'inne'");
    NetStats::noteRx(254);
    CHECK(NetStats::table[0].count == 7, "znany PGN nadal liczony po zapelnieniu tabeli");

    // ---- OtaStatus ----
    CHECK(OtaStatus::getState() == OtaStatus::IDLE && OtaStatus::msSinceEnd(1000) == -1, "stan poczatkowy: bezczynny");
    otaStatusBootInit(true);
    CHECK(!bootAfterOta, "zwykly start (bez znacznika) nie jest 'po OTA'");
    OtaStatus::start(OtaStatus::WEB, 0, 10000);
    CHECK(OtaStatus::getState() == OtaStatus::RUNNING && OtaStatus::getSource() == OtaStatus::WEB, "trwa, zrodlo: strona WWW");
    OtaStatus::progress(123456);
    CHECK(OtaStatus::getBytes() == 123456, "postep w bajtach");
    OtaStatus::fail("Blad zapisu: \"flash\"\n\\", 15000);
    CHECK(OtaStatus::getState() == OtaStatus::FAILED, "stan: blad");
    { std::string e = OtaStatus::getError(); CHECK(e.find('"') == std::string::npos && e.find('\n') == std::string::npos && e.find('\\') == std::string::npos, "tekst bledu bezpieczny do wstawienia w JSON"); }
    CHECK(OtaStatus::msSinceEnd(16000) == 1000, "ile ms temu zakonczona proba");
    OtaStatus::progress(999);
    CHECK(OtaStatus::getBytes() == 123456, "po zakonczeniu postep sie nie zmienia");
    CHECK(rtcOtaMagic == 0, "porazka NIE zostawia znacznika 'po OTA'");
    OtaStatus::start(OtaStatus::IDE, 800000, 20000);
    CHECK(OtaStatus::getState() == OtaStatus::RUNNING && OtaStatus::getBytes() == 0 && OtaStatus::getError()[0] == '\0' && OtaStatus::getTotal() == 800000, "nowa proba zeruje bajty i blad, zna rozmiar");
    OtaStatus::success(800000, 30000);
    CHECK(OtaStatus::getState() == OtaStatus::OK && OtaStatus::getBytes() == 800000, "sukces");
    CHECK(rtcOtaMagic == OTA_RTC_MAGIC, "sukces zostawia znacznik dla nowego firmware");
    otaStatusBootInit(true);
    CHECK(bootAfterOta && rtcOtaMagic == 0, "nowy start po restarcie programowym: 'po aktualizacji OTA', znacznik zuzyty");
    rtcOtaMagic = OTA_RTC_MAGIC; otaStatusBootInit(false);
    CHECK(!bootAfterOta, "start po panice/zasilaniu (nie restart programowy) nie dziedziczy znacznika");
    OtaStatus::fail(nullptr, 40000);
    CHECK(std::string(OtaStatus::getError()) == "unknown error", "brak tekstu bledu => 'nieznany blad'");
    printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
    return fails ? 1 : 0;
}
