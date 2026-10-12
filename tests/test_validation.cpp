// Testy jednostkowe czystej logiki (bez Arduino): walidacja ustawien z panelu i przeliczenie kata.
// Uruchomienie na komputerze, z katalogu glownego projektu:
//     g++ -std=c++17 -Wall -Wextra tests/test_validation.cpp -o /tmp/test_validation && /tmp/test_validation
// Folder tests/ nie jest kompilowany przez Arduino IDE (kompiluje tylko katalog glowny i src/).
// Pokrywa m.in. dokladnie te przypadki, ktore wczesniej psuly urzadzenie: watchdog = 0 (petla
// restartow), port 70000 (cicho 4464), "nan" w katach, CRLF w mountpoincie NTRIP, LEWO = PRAWO.
#include <cstdio>
#include <cstring>
#include <string>
#include "../web/settings_validation.h"
#include "../sensors/steering_math.h"
using namespace SettingsValidation;
static int fails = 0, total = 0;
#define CHECK(cond, name) do { total++; if (!(cond)) { fails++; printf("  BLAD: %s\n", name); } } while (0)
static std::string rep(char c, int n) { return std::string(n, c); }

int main() {
    // ---- parseLong ----
    long v = 0;
    CHECK(parseLong("25", 3, 300, &v) && v == 25, "25 w 3..300");
    CHECK(parseLong("  25  ", 3, 300, &v) && v == 25, "spacje dookola");
    CHECK(!parseLong("0", 3, 300, &v), "0 poza zakresem (watchdog=0 to petla restartow)");
    CHECK(!parseLong("", 3, 300, &v), "pusty tekst (toInt dawal 0!)");
    CHECK(!parseLong("abc", 3, 300, &v), "abc (toInt dawal 0!)");
    CHECK(!parseLong("12abc", 3, 300, &v), "12abc - smieci na koncu");
    CHECK(!parseLong("1e3", 3, 5000, &v), "1e3 - zapis wykladniczy nie jest calkowity");
    CHECK(!parseLong("301", 3, 300, &v), "301 powyzej zakresu");
    CHECK(!parseLong("-5", 3, 300, &v), "-5 ponizej zakresu");
    CHECK(!parseLong("99999999999999999999", 0, 100, &v), "przepelnienie long");
    CHECK(!parseLong(nullptr, 0, 10, &v), "nullptr");
    CHECK(!parseLong("0x10", 0, 100, &v), "zapis szesnastkowy odrzucony");
    CHECK(parseLong("+7", 3, 300, &v) && v == 7, "+7");
    CHECK(parseLong("300", 3, 300, &v) && v == 300 && parseLong("3", 3, 300, &v) && v == 3, "granice wlacznie");

    // ---- parseFloatRange ----
    float f = 0;
    CHECK(parseFloatRange("-45.5", -90, 90, &f) && f == -45.5f, "-45.5");
    CHECK(parseFloatRange("45", -90, 90, &f) && f == 45.0f, "45");
    CHECK(!parseFloatRange("nan", -90, 90, &f), "nan (toFloat dawal NaN!)");
    CHECK(!parseFloatRange("NaN", -90, 90, &f), "NaN");
    CHECK(!parseFloatRange("inf", -90, 90, &f), "inf");
    CHECK(!parseFloatRange("-inf", -90, 90, &f), "-inf");
    CHECK(!parseFloatRange("1e999", -90, 90, &f), "1e999 (przepelnienie)");
    CHECK(!parseFloatRange("0x1p3", -90, 90, &f), "hex-float");
    CHECK(!parseFloatRange("45,5", -90, 90, &f), "przecinek dziesietny odrzucony");
    CHECK(!parseFloatRange("", -90, 90, &f), "pusty");
    CHECK(!parseFloatRange("91", -90, 90, &f), "91 poza zakresem");
    CHECK(!parseFloatRange("12abc", -90, 90, &f), "smieci");
    CHECK(parseFloatRange("1e1", -90, 90, &f) && f == 10.0f, "1e1 = 10 (poprawny zapis wykladniczy)");

    // ---- SSID / hasla ----
    CHECK(validSsid("IOT") && validSsid(rep('a', 32).c_str()), "SSID 3 i 32 znaki");
    CHECK(!validSsid("") && !validSsid(rep('a', 33).c_str()), "SSID pusty / 33 znaki");
    CHECK(!validSsid("a\nb"), "SSID ze znakiem nowej linii");
    CHECK(validWifiPass("") && validWifiPass("12345678") && validWifiPass(rep('x', 63).c_str()), "haslo WiFi: puste, 8, 63");
    CHECK(!validWifiPass("1234567") && !validWifiPass(rep('x', 64).c_str()), "haslo WiFi: 7 i 64 znaki");
    CHECK(!validApPass("") && !validApPass("1234567") && validApPass("aog12345"), "haslo AP: min 8 (puste niedozwolone)");

    // ---- IP / maska ----
    uint32_t ip;
    CHECK(parseIpv4("192.168.1.120", &ip) && ip == 0xC0A80178u, "192.168.1.120");
    CHECK(parseIpv4("0.0.0.0", &ip) && parseIpv4("255.255.255.255", &ip), "skrajne adresy");
    CHECK(!parseIpv4("256.1.1.1", &ip) && !parseIpv4("1.2.3", &ip) && !parseIpv4("1.2.3.4.5", &ip), "256 / 3 oktety / 5 oktetow");
    CHECK(!parseIpv4("1.2.3.4 ", &ip) && !parseIpv4(" 1.2.3.4", &ip) && !parseIpv4("1..3.4", &ip) && !parseIpv4("a.b.c.d", &ip), "spacje, puste oktety, litery");
    CHECK(!parseIpv4("1.2.3.4.", &ip) && !parseIpv4("1234.1.1.1", &ip) && !parseIpv4("", &ip), "kropka na koncu, 4 cyfry, pusty");
    CHECK(validNetmask("255.255.255.0") && validNetmask("255.255.0.0") && validNetmask("255.255.255.252") && validNetmask("128.0.0.0") && validNetmask("255.255.255.255"), "poprawne maski");
    CHECK(!validNetmask("0.0.0.0") && !validNetmask("255.0.255.0") && !validNetmask("255.255.255.1") && !validNetmask("192.168.1.1"), "niepoprawne maski");

    // ---- formularz sieci ----
    const char* e;
    e = validateNetwork("IOT", "HomeNet-5G", "Hotspot", "hotspot123", "", "", "", "", "AOG-Setup", "aog12345", "192.168.1.120", "192.168.1.1", "255.255.255.0", "192.168.1.255");
    CHECK(e == nullptr, "siec: poprawny komplet (2 sieci + puste sloty)");
    e = validateNetwork("", "x", "", "", "", "", "", "", "AP", "aog12345", "1.1.1.2", "1.1.1.1", "255.0.0.0", "1.1.1.255");
    CHECK(e && strstr(e, "network 1"), "siec: pusta siec nr 1 odrzucona");
    e = validateNetwork("IOT", "abc", "", "", "", "", "", "", "AP", "aog12345", "1.1.1.2", "1.1.1.1", "255.0.0.0", "1.1.1.255");
    CHECK(e && strstr(e, "network 1"), "siec: haslo nr 1 = 3 znaki odrzucone");
    e = validateNetwork("IOT", "", "", "", "", "", "", "", "AP", "aog12345", "1.1.1.2", "1.1.1.1", "255.0.0.0", "1.1.1.255");
    CHECK(e == nullptr, "siec: siec otwarta (puste haslo) dozwolona");
    e = validateNetwork("IOT", "", "Dom", "krotkie", "", "", "", "", "AP", "aog12345", "1.1.1.2", "1.1.1.1", "255.0.0.0", "1.1.1.255");
    CHECK(e && strstr(e, "network 2"), "siec: slot 2 z zlym haslem wskazuje numer 2");
    e = validateNetwork("IOT", "", "", "krotkie", "", "", "", "", "AP", "aog12345", "1.1.1.2", "1.1.1.1", "255.0.0.0", "1.1.1.255");
    CHECK(e == nullptr, "siec: haslo w PUSTYM slocie 2 jest ignorowane");
    e = validateNetwork("IOT", "", "", "", "", "", "", "", "AP", "1234567", "1.1.1.2", "1.1.1.1", "255.0.0.0", "1.1.1.255");
    CHECK(e && strstr(e, "AP password"), "siec: haslo AP 7 znakow odrzucone (AP by sie nie uruchomil)");
    e = validateNetwork("IOT", "", "", "", "", "", "", "", "AP", "aog12345", "1.1.1", "1.1.1.1", "255.0.0.0", "1.1.1.255");
    CHECK(e && strstr(e, "IP address"), "siec: zly adres IP");
    e = validateNetwork("IOT", "", "", "", "", "", "", "", "AP", "aog12345", "1.1.1.1", "1.1.1.1", "255.0.255.0", "1.1.1.255");
    CHECK(e && strstr(e, "Subnet mask"), "siec: niepoprawna maska");

    // ---- plan adresow (B8) ----
    auto plan = [](const char* ip, const char* gw, const char* mask, const char* agio) {
        return validateNetwork("IOT", "", "", "", "", "", "", "", "AP", "aog12345", ip, gw, mask, agio); };
    CHECK(plan("192.168.1.120", "192.168.1.1", "255.255.255.0", "192.168.1.255") == nullptr, "plan: typowy");
    CHECK(plan("192.168.1.120", "192.168.1.1", "255.255.255.0", "255.255.255.255") == nullptr, "plan: broadcast globalny");
    CHECK(plan("192.168.1.120", "192.168.1.1", "255.255.255.0", "192.168.1.50") == nullptr, "plan: AgOpenWeb unicast");
    CHECK(plan("0.0.0.0", "192.168.1.1", "255.255.255.0", "192.168.1.255") != nullptr, "plan: adres 0.0.0.0");
    CHECK(plan("127.0.0.1", "127.0.0.2", "255.0.0.0", "127.255.255.255") != nullptr, "plan: loopback");
    CHECK(plan("224.0.0.5", "224.0.0.1", "255.255.255.0", "224.0.0.255") != nullptr, "plan: multicast");
    CHECK(plan("192.168.1.0", "192.168.1.1", "255.255.255.0", "192.168.1.255") != nullptr, "plan: adres sieci");
    CHECK(plan("192.168.1.255", "192.168.1.1", "255.255.255.0", "192.168.1.255") != nullptr, "plan: adres rozgloszeniowy");
    CHECK(plan("192.168.1.120", "192.168.2.1", "255.255.255.0", "192.168.1.255") != nullptr, "plan: brama poza podsiecia");
    CHECK(plan("192.168.1.120", "192.168.1.120", "255.255.255.0", "192.168.1.255") != nullptr, "plan: brama = modul");
    CHECK(plan("192.168.1.120", "192.168.1.1", "255.255.255.0", "0.0.0.0") != nullptr, "plan: AgOpenWeb 0.0.0.0");
    CHECK(plan("10.0.5.20", "10.0.0.1", "255.255.0.0", "10.0.255.255") == nullptr, "plan: podsiec /16");

    // ---- NTRIP ----
    uint16_t port = 0;
    CHECK(validateNtrip("91.198.76.2", "8086", "RTK4G_MULTI_RTCM32", "user", "pass", &port) == nullptr && port == 8086, "ntrip: poprawne dane");
    CHECK(validateNtrip("host", "70000", "m", "u", "p", &port) != nullptr, "ntrip: port 70000 odrzucony (bylo cicho 4464)");
    CHECK(validateNtrip("host", "0", "m", "u", "p", &port) != nullptr, "ntrip: port 0 odrzucony");
    CHECK(validateNtrip("host", "abc", "m", "u", "p", &port) != nullptr, "ntrip: port 'abc' odrzucony");
    CHECK(validateNtrip("", "8086", "m", "u", "p", &port) != nullptr, "ntrip: pusty host");
    CHECK(validateNtrip("ho st", "8086", "m", "u", "p", &port) != nullptr, "ntrip: spacja w hoscie");
    CHECK(validateNtrip("host", "8086", "m\r\nX-Evil: 1", "u", "p", &port) != nullptr, "ntrip: wstrzykniecie naglowka przez mountpoint (CRLF)");
    CHECK(validateNtrip("host", "8086", "m", "u\r\nX: 1", "p", &port) != nullptr, "ntrip: CRLF w uzytkowniku");
    CHECK(validateNtrip("host", "8086", "m", "us:er", "p", &port) != nullptr, "ntrip: dwukropek w uzytkowniku (psuje Basic Auth)");
    CHECK(validateNtrip("host", "8086", "m", "u", "p\n", &port) != nullptr, "ntrip: nowa linia w hasle");
    CHECK(validateNtrip("host", "8086", "", "", "", &port) == nullptr, "ntrip: pusty mountpoint/uzytkownik/haslo dozwolone");
    CHECK(validateNtrip(rep('h', 81).c_str(), "8086", "m", "u", "p", &port) != nullptr, "ntrip: host 81 znakow");

    // ---- nadawca AgOpenWeb ----
    CHECK(validateAgioSender("") == nullptr, "nadawca: pusty = tryb automatyczny");
    CHECK(validateAgioSender("192.168.1.17") == nullptr, "nadawca: poprawny adres");
    CHECK(validateAgioSender(nullptr) != nullptr, "nadawca: brak wartosci odrzucony");
    { uint8_t m = 0;
      CHECK(validateAgioEngageMask("1", &m) == nullptr && m == 1, "maska wlaczenia: 1 (starszy AgOpenWeb)");
      CHECK(validateAgioEngageMask("4", &m) == nullptr && m == 4, "maska wlaczenia: 4 (nowszy)");
      CHECK(validateAgioEngageMask("5", &m) == nullptr && m == 5, "maska wlaczenia: 5 (oba)");
      m = 9;
      CHECK(validateAgioEngageMask("0", &m) != nullptr && m == 9, "maska wlaczenia: 0 odrzucone, wynik bez zmian");
      CHECK(validateAgioEngageMask("2", &m) != nullptr, "maska wlaczenia: 2 odrzucone");
      CHECK(validateAgioEngageMask("", &m) != nullptr, "maska wlaczenia: pusty odrzucony");
      CHECK(validateAgioEngageMask("04", &m) != nullptr, "maska wlaczenia: 04 odrzucone (tylko dokladne wartosci)");
      CHECK(validateAgioEngageMask("0x04", &m) != nullptr, "maska wlaczenia: 0x04 odrzucone");
      CHECK(validateAgioEngageMask(nullptr, &m) != nullptr, "maska wlaczenia: brak wartosci odrzucony");
      CHECK(validateAgioEngageMask("1", nullptr) == nullptr, "maska wlaczenia: bez wskaznika wyniku nie wywala");
    }
    CHECK(validateAgioSender("abc") != nullptr, "nadawca: tekst odrzucony");
    CHECK(validateAgioSender("192.168.1") != nullptr, "nadawca: niepelny adres odrzucony");
    CHECK(validateAgioSender("192.168.1.300") != nullptr, "nadawca: oktet 300 odrzucony");
    CHECK(validateAgioSender("0.0.0.0") != nullptr, "nadawca: 0.0.0.0 odrzucony (tryb auto = puste pole)");
    CHECK(validateAgioSender("127.0.0.1") != nullptr, "nadawca: 127.x odrzucony");
    CHECK(validateAgioSender("224.0.0.1") != nullptr, "nadawca: multicast odrzucony");
    CHECK(validateAgioSender("255.255.255.255") != nullptr, "nadawca: broadcast odrzucony");

    // ---- progi ----
    Thresholds t;
    CHECK(validateThresholds("10000", "15000", "25", "40", &t) == nullptr && t.watchdogS == 25 && t.ledBrightness == 40 && t.ethGraceMs == 10000 && t.wifiTimeoutMs == 15000, "progi: wartosci domyslne");
    CHECK(validateThresholds("10000", "15000", "0", "40", &t) != nullptr, "progi: watchdog = 0 odrzucony (petla restartow)");
    CHECK(validateThresholds("10000", "15000", "abc", "40", &t) != nullptr, "progi: watchdog 'abc' odrzucony (toInt dawal 0)");
    CHECK(validateThresholds("10000", "15000", "2", "40", &t) != nullptr, "progi: watchdog = 2 odrzucony");
    CHECK(validateThresholds("10000", "15000", "301", "40", &t) != nullptr, "progi: watchdog = 301 odrzucony");
    CHECK(validateThresholds("10000", "15000", "25", "300", &t) != nullptr, "progi: jasnosc 300 odrzucona (bylo cicho 44)");
    CHECK(validateThresholds("10000", "15000", "25", "-1", &t) != nullptr, "progi: jasnosc -1 odrzucona");
    CHECK(validateThresholds("999", "15000", "25", "40", &t) != nullptr, "progi: czas Ethernet 999 ms odrzucony");
    CHECK(validateThresholds("10000", "2999", "25", "40", &t) != nullptr, "progi: timeout WiFi 2999 ms odrzucony");
    CHECK(validateThresholds("1000", "3000", "3", "0", &t) == nullptr && t.ledBrightness == 0, "progi: wartosci graniczne dozwolone");

    // ---- zakres kata ----
    float l, r;
    CHECK(validateSteeringRange("-45", "45", &l, &r) == nullptr && l == -45 && r == 45, "kat: -45..45");
    CHECK(validateSteeringRange("-42", "41.5", &l, &r) == nullptr && r == 41.5f, "kat: -42..41.5");
    CHECK(validateSteeringRange("10", "10", &l, &r) != nullptr, "kat: LEWO = PRAWO odrzucone");
    CHECK(validateSteeringRange("10", "12", &l, &r) != nullptr, "kat: rozpietosc 2 stopnie odrzucona");
    CHECK(validateSteeringRange("nan", "45", &l, &r) != nullptr, "kat: NaN odrzucony");
    CHECK(validateSteeringRange("-45", "inf", &l, &r) != nullptr, "kat: inf odrzucony");
    CHECK(validateSteeringRange("-91", "45", &l, &r) != nullptr, "kat: -91 odrzucony");
    CHECK(validateSteeringRange("45", "-45", &l, &r) == nullptr, "kat: odwrocony kierunek (LEWO > PRAWO) dozwolony");

    // ---- obrot IMU ----
    CHECK(validateImuRotation("-19.8", &f) == nullptr && f < -19.7f, "imu: -19.8");
    CHECK(validateImuRotation("nan", &f) != nullptr && validateImuRotation("181", &f) != nullptr && validateImuRotation("", &f) != nullptr, "imu: nan / 181 / pusty odrzucone");

    // ---- obrona przy wczytywaniu ----
    CHECK(clampOrDefault(0, 3, 300, 25) == 25 && clampOrDefault(25, 3, 300, 25) == 25 && clampOrDefault(4000000000u, 3, 300, 25) == 25 && clampOrDefault(2, 3, 300, 25) == 25 && clampOrDefault(300, 3, 300, 25) == 300, "load: watchdog 0/2/ogromny -> 25, 300 zostaje");
    CHECK(finiteOrDefault(NAN, -180, 180, 0) == 0 && finiteOrDefault(INFINITY, -180, 180, 0) == 0 && finiteOrDefault(500, -180, 180, 0) == 0 && finiteOrDefault(12.5f, -180, 180, 0) == 12.5f, "load: NaN/inf/poza zakresem -> domyslna");
    CHECK(steeringRangeOk(-45, 45) && !steeringRangeOk(5, 5) && !steeringRangeOk(NAN, 45) && !steeringRangeOk(-200, 45), "load: zakres kata");

    // ---- steering_math ----
    bool ok;
    float a = SteeringMath::rawToAngleDeg(16000, 5000, 27000, -45, 45, &ok);
    CHECK(ok && std::fabs(a - 0.0f) < 0.6f, "kat: srodek zakresu ~0 stopni");
    a = SteeringMath::rawToAngleDeg(5000, 5000, 27000, -45, 45, &ok);   CHECK(ok && a == -45.0f, "kat: LEWY koniec = -45");
    a = SteeringMath::rawToAngleDeg(27000, 5000, 27000, -45, 45, &ok);  CHECK(ok && a == 45.0f, "kat: PRAWY koniec = 45");
    a = SteeringMath::rawToAngleDeg(1234, 20000, 20000, -45, 45, &ok);  CHECK(!ok && a == 0.0f && std::isfinite(a), "kat: LEWO == PRAWO -> niewazny, 0 (nie NaN)");
    a = SteeringMath::rawToAngleDeg(1234, 20000, 20100, -45, 45, &ok);  CHECK(!ok && a == 0.0f, "kat: rozpietosc 100 < 200 -> niewazny");
    a = SteeringMath::rawToAngleDeg(1234, 20000, 20200, -45, 45, &ok);  CHECK(ok && std::isfinite(a), "kat: rozpietosc dokladnie 200 -> wazny");
    a = SteeringMath::rawToAngleDeg(32767, 20000, 20200, -45, 45, &ok); CHECK(ok && a == 180.0f, "kat: ekstremalny odczyt obciety do +180");
    // wiarygodnosc surowego odczytu (zerwany/luzny przewod czujnika)
    CHECK(SteeringMath::rawPlausible(16000, 5000, 27000, 0.15f), "raw: srodek zakresu wiarygodny");
    CHECK(SteeringMath::rawPlausible(5000, 5000, 27000, 0.15f) && SteeringMath::rawPlausible(27000, 5000, 27000, 0.15f), "raw: oba konce wiarygodne");
    CHECK(SteeringMath::rawPlausible(3000, 5000, 27000, 0.15f) && SteeringMath::rawPlausible(29000, 5000, 27000, 0.15f), "raw: luz mechaniczny za koncami (w marginesie 15%) wiarygodny");
    CHECK(!SteeringMath::rawPlausible(1000, 5000, 27000, 0.15f) && !SteeringMath::rawPlausible(31000, 5000, 27000, 0.15f), "raw: daleko poza zakresem niewiarygodny");
    CHECK(!SteeringMath::rawPlausible(0, 5000, 27000, 0.15f) && !SteeringMath::rawPlausible(32767, 5000, 27000, 0.15f), "raw: szyna 0 / pelna skala niewiarygodna");
    CHECK(SteeringMath::rawPlausible(16000, 27000, 5000, 0.15f) && !SteeringMath::rawPlausible(1000, 27000, 5000, 0.15f), "raw: odwrocona kolejnosc LEWO/PRAWO dziala tak samo");
    CHECK(SteeringMath::rawPlausible(20100, 20000, 20200, 0.15f) && SteeringMath::rawPlausible(19750, 20000, 20200, 0.15f) && !SteeringMath::rawPlausible(19600, 20000, 20200, 0.15f), "raw: waska rozpietosc - margines co najmniej 300");
    CHECK(SteeringMath::rawPlausible(0, 0, 32767, 0.15f) && SteeringMath::rawPlausible(32767, 0, 32767, 0.15f), "raw: kalibracja domyslna (0..32767) przyjmuje caly zakres");
    CHECK(!SteeringMath::rawPlausible(1000, 5000, 27000, NAN) && SteeringMath::rawPlausible(5000, 5000, 27000, NAN), "raw: margines NaN -> minimum 300, bez UB");
    a = SteeringMath::rawToAngleDeg(-32768, 20000, 20200, -45, 45, &ok); CHECK(ok && a == -180.0f, "kat: ekstremalny odczyt obciety do -180");
    a = SteeringMath::rawToAngleDeg(16000, 27000, 5000, -45, 45, &ok); CHECK(ok && std::fabs(a) < 0.6f, "kat: odwrocona kalibracja (LEWO ADC > PRAWO ADC) dziala");
    a = SteeringMath::rawToAngleDeg(16000, 5000, 27000, NAN, 45, &ok); CHECK(!ok && a == 0.0f, "kat: NaN w kacie skrajnym -> niewazny");
    a = SteeringMath::rawToAngleDeg(0, -32768, 32767, -45, 45, &ok);   CHECK(ok && std::isfinite(a), "kat: pelny zakres int16 bez przepelnienia");
    CHECK(SteeringMath::spanOk(0, 32767) && !SteeringMath::spanOk(100, 100) && SteeringMath::spanOk(32767, 0), "spanOk");

    // ---- progi ochrony pradowej (Kalibracja -> Czujnik pradu) ----
    {
        float a = 0, h = 0; long ms = 0;
        CHECK(validateCurrent("8", "400", "15", &a, &ms, &h) == nullptr && a == 8.0f && ms == 400 && h == 15.0f, "prad: domyslne 8 A / 400 ms / 15 A przechodza");
        CHECK(validateCurrent("8.5", "250", "20.5", &a, &ms, &h) == nullptr && a == 8.5f && h == 20.5f, "prad: wartosci z czescia dziesietna");
        CHECK(validateCurrent("0.5", "20", "1.0", &a, &ms, &h) == nullptr, "prad: dolne granice (0,5 A, 20 ms, 1,0 A) przechodza");
        CHECK(validateCurrent("28", "5000", "30", &a, &ms, &h) == nullptr, "prad: gorne granice (28 A, 5000 ms, 30 A) przechodza");
        CHECK(validateCurrent("0.4", "400", "15", &a, &ms, &h) != nullptr, "prad: kickout 0,4 A za niski (szum czujnika ~0,1 A)");
        CHECK(validateCurrent("28.5", "400", "30", &a, &ms, &h) != nullptr, "prad: kickout 28,5 A za wysoki (czujnik saturuje przy 30 A)");
        CHECK(validateCurrent("8", "19", "15", &a, &ms, &h) != nullptr && validateCurrent("8", "5001", "15", &a, &ms, &h) != nullptr, "prad: czas 19 ms i 5001 ms poza zakresem");
        CHECK(validateCurrent("8", "400.5", "15", &a, &ms, &h) != nullptr, "prad: czas nie moze byc ulamkiem");
        CHECK(validateCurrent("8", "400", "30.5", &a, &ms, &h) != nullptr, "prad: prog twardy > 30 A (poza zakresem czujnika) odrzucony");
        CHECK(validateCurrent("10", "400", "10.4", &a, &ms, &h) != nullptr, "prad: prog twardy mniej niz 0,5 A nad miekkim odrzucony");
        CHECK(validateCurrent("10", "400", "10.5", &a, &ms, &h) == nullptr, "prad: prog twardy dokladnie 0,5 A nad miekkim przechodzi");
        CHECK(validateCurrent("15", "400", "8", &a, &ms, &h) != nullptr, "prad: twardy nizszy od miekkiego (pomylone pola) odrzucony");
        CHECK(validateCurrent("", "400", "15", &a, &ms, &h) != nullptr && validateCurrent("8", "", "15", &a, &ms, &h) != nullptr && validateCurrent("8", "400", "", &a, &ms, &h) != nullptr, "prad: puste pola odrzucone");
        CHECK(validateCurrent("abc", "400", "15", &a, &ms, &h) != nullptr && validateCurrent("nan", "400", "15", &a, &ms, &h) != nullptr && validateCurrent("inf", "400", "15", &a, &ms, &h) != nullptr, "prad: abc / nan / inf odrzucone");
        CHECK(currentLimitsOk(8.0f, 400, 15.0f) && !currentLimitsOk(NAN, 400, 15.0f) && !currentLimitsOk(8.0f, 400, NAN) && !currentLimitsOk(8.0f, 0, 15.0f) && !currentLimitsOk(0.0f, 400, 15.0f) && !currentLimitsOk(8.0f, 400, 8.2f), "currentLimitsOk: poprawne tak, NaN / zero / zly prog nie (obrona przy wczytaniu z NVS)");
    }

    // ---- silnik: detektor ucieczki i odwrocenie kierunku ----
    {
        MotorSettings m;
        CHECK(validateMotor("1", "1000", "1.0", "3.0", "0", &m) == nullptr && m.runEn && m.windowMs == 1000 && m.moveDeg == 1.0f && m.errDeg == 3.0f && !m.invert, "silnik: domyslne (1 / 1000 ms / 1,0 / 3,0 / kierunek normalny) przechodza");
        CHECK(validateMotor("0", "300", "0.2", "0.5", "1", &m) == nullptr && !m.runEn && m.invert, "silnik: dolne granice (300 ms, 0,2 deg, 0,5 deg), detektor wylaczony, kierunek odwrocony");
        CHECK(validateMotor("1", "5000", "10", "20", "1", &m) == nullptr, "silnik: gorne granice (5000 ms, 10 deg, 20 deg)");
        CHECK(validateMotor("1", "299", "1", "3", "0", &m) != nullptr && validateMotor("1", "5001", "1", "3", "0", &m) != nullptr, "silnik: okno 299 i 5001 ms poza zakresem");
        CHECK(validateMotor("1", "1000.5", "1", "3", "0", &m) != nullptr, "silnik: okno nie moze byc ulamkiem");
        CHECK(validateMotor("1", "1000", "0.1", "3", "0", &m) != nullptr && validateMotor("1", "1000", "10.1", "3", "0", &m) != nullptr, "silnik: ruch 0,1 i 10,1 deg poza zakresem");
        CHECK(validateMotor("1", "1000", "1", "0.4", "0", &m) != nullptr && validateMotor("1", "1000", "1", "20.1", "0", &m) != nullptr, "silnik: prog bledu 0,4 i 20,1 deg poza zakresem");
        CHECK(validateMotor("2", "1000", "1", "3", "0", &m) != nullptr && validateMotor("", "1000", "1", "3", "0", &m) != nullptr && validateMotor("true", "1000", "1", "3", "0", &m) != nullptr, "silnik: runEn tylko '0'/'1' (nie 2, puste, 'true')");
        CHECK(validateMotor("1", "1000", "1", "3", "yes", &m) != nullptr && validateMotor("1", "1000", "1", "3", "", &m) != nullptr, "silnik: motorInvert tylko '0'/'1'");
        CHECK(validateMotor("1", "abc", "1", "3", "0", &m) != nullptr && validateMotor("1", "1000", "nan", "3", "0", &m) != nullptr && validateMotor("1", "1000", "1", "inf", "0", &m) != nullptr, "silnik: abc / nan / inf odrzucone");
        CHECK(runawayParamsOk(1000, 1.0f, 3.0f) && !runawayParamsOk(100, 1.0f, 3.0f) && !runawayParamsOk(1000, NAN, 3.0f) && !runawayParamsOk(1000, 1.0f, NAN) && !runawayParamsOk(1000, 0.0f, 3.0f), "runawayParamsOk: poprawne tak, zle okno / NaN / zero nie (obrona przy wczytaniu z NVS)");
    }

    // ---- wartosci graniczne dziesietne (float vs double): dokladnie dozwolony minimum/maksimum musi przechodzic ----
    {
        float o = 0;
        CHECK(parseFloatRange("0.2", 0.2f, 10.0f, &o) && o == 0.2f, "graniczna 0.2 przy lo = 0.2f przechodzi (wczesniej odrzucana: double 0.2 < float 0.2)");
        CHECK(parseFloatRange("0.1", 0.1f, 5.0f, &o) && parseFloatRange("0.3", 0.3f, 5.0f, &o) && parseFloatRange("0.7", 0.7f, 5.0f, &o), "graniczne 0.1 / 0.3 / 0.7 przechodza");
        CHECK(parseFloatRange("10.1", 0.5f, 10.1f, &o) && parseFloatRange("2.3", 0.5f, 2.3f, &o), "gorne graniczne 10.1 / 2.3 przechodza");
        CHECK(!parseFloatRange("0.19", 0.2f, 10.0f, &o) && !parseFloatRange("10.2", 0.2f, 10.1f, &o), "tuz poza zakresem (0.19, 10.2) nadal odrzucane");
    }

    // ---- reczny test silnika (Lewo / Prawo) ----
    {
        int d = 0; long pwm = 0, ms = 0;
        CHECK(validateMotorTest("R", "60", "300", &d, &pwm, &ms) == nullptr && d == 1 && pwm == 60 && ms == 300, "test: prawo, 60, 300 ms przechodzi (kierunek +1)");
        CHECK(validateMotorTest("L", "10", "100", &d, &pwm, &ms) == nullptr && d == -1 && pwm == 10 && ms == 100, "test: lewo, dolne granice 10 / 100 ms (kierunek -1)");
        CHECK(validateMotorTest("R", "150", "1000", &d, &pwm, &ms) == nullptr, "test: gorne granice 150 / 1000 ms");
        CHECK(validateMotorTest("X", "60", "300", &d, &pwm, &ms) != nullptr && validateMotorTest("", "60", "300", &d, &pwm, &ms) != nullptr && validateMotorTest("RL", "60", "300", &d, &pwm, &ms) != nullptr && validateMotorTest("r", "60", "300", &d, &pwm, &ms) != nullptr, "test: kierunek tylko 'L' albo 'R' (nie X, puste, RL, male r)");
        CHECK(validateMotorTest(nullptr, "60", "300", &d, &pwm, &ms) != nullptr, "test: brak kierunku (nullptr) odrzucony");
        CHECK(validateMotorTest("R", "9", "300", &d, &pwm, &ms) != nullptr && validateMotorTest("R", "151", "300", &d, &pwm, &ms) != nullptr, "test: PWM 9 i 151 poza zakresem");
        CHECK(validateMotorTest("R", "60", "99", &d, &pwm, &ms) != nullptr && validateMotorTest("R", "60", "1001", &d, &pwm, &ms) != nullptr, "test: czas 99 i 1001 ms poza zakresem");
        CHECK(validateMotorTest("R", "60.5", "300", &d, &pwm, &ms) != nullptr && validateMotorTest("R", "60", "300.5", &d, &pwm, &ms) != nullptr, "test: ulamki w PWM i czasie odrzucone");
        CHECK(validateMotorTest("R", "abc", "300", &d, &pwm, &ms) != nullptr && validateMotorTest("R", "60", "", &d, &pwm, &ms) != nullptr && validateMotorTest("R", "-60", "300", &d, &pwm, &ms) != nullptr, "test: tekst, puste i ujemne odrzucone");
    }

    printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
    return fails ? 1 : 0;
}
