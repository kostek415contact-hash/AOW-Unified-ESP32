// Tests: sensors/nmea.h (checksum, line validation, field splitting, GGA/VTG/GST/RMC parsers)
#include <cstdio>
#include <cstring>
#include <string>
#include "../sensors/nmea.h"
static int ok = 0, total = 0;
#define CHECK(c, m) do { total++; if (c) ok++; else printf("BLAD: %s\n", m); } while (0)

// Builds "$body*HH" with the right checksum.
static std::string frame(const std::string& body) {
    char cs[8]; snprintf(cs, sizeof cs, "*%02X", Nmea::checksum(body.c_str(), (uint8_t)body.size()));
    return "$" + body + cs;
}
struct Parsed { char buf[160]; char* f[24]; uint8_t n; };
static void split(Parsed& p, const std::string& line) { strncpy(p.buf, line.c_str(), sizeof p.buf - 1); p.buf[sizeof p.buf - 1] = 0; p.n = Nmea::splitFields(p.buf, p.f, 24); }

int main() {
    using namespace Nmea;
    // --- suma kontrolna ---
    CHECK(checksum("", 0) == 0, "pusta suma");
    CHECK(checksum("GPGGA", 5) == ('G' ^ 'P' ^ 'G' ^ 'G' ^ 'A'), "xor znakow");
    // znany przyklad z dokumentacji NMEA
    CHECK(validateLine("$GPGLL,4916.45,N,12311.12,W,225444,A,*1D"), "znana poprawna linia GLL");
    CHECK(!validateLine("$GPGLL,4916.45,N,12311.12,W,225444,A,*1E"), "zla suma");
    CHECK(validateLine("$GPGLL,4916.45,N,12311.12,W,225444,A,*1d"), "male litery hex");
    CHECK(validateLine("$GPGLL,4916.45,N,12311.12,W,225444,A,*1D\r\n"), "CR LF po sumie");
    CHECK(!validateLine("$GPGLL,4916.45,N,12311.12,W,225444,A,*1D junk"), "smieci po sumie");
    CHECK(!validateLine("$GPGLL,4916.45,N,12311.12,W,225444,A,*1"), "tylko jedna cyfra sumy");
    CHECK(!validateLine("$GPGLL,4916.45,N,12311.12,W,225444,A,*"), "brak cyfr sumy");
    CHECK(!validateLine("$GPGLL,4916.45,N,12311.12,W,225444,A,"), "brak gwiazdki");
    CHECK(!validateLine("GPGLL,4916.45,N,12311.12,W,225444,A,*1D"), "brak $ na poczatku");
    CHECK(!validateLine(""), "pusta linia");
    CHECK(!validateLine(nullptr), "nullptr");
    // suma 00 + zle znaki: dawniej strtol dawal 0 i linia przechodzila
    { std::string body; for (int i = 0; i < 256; i++) { body = "AB"; body += (char)('A' ^ 'B'); /* xor = 0 */ break; }
      std::string good = frame(body); CHECK(Nmea::checksum(body.c_str(), (uint8_t)body.size()) == 0, "przyklad z suma 0");
      CHECK(validateLine(good.c_str()), "suma 00 poprawna");
      CHECK(!validateLine(("$" + body + "*ZZ").c_str()), "zle znaki hex nie przechodza jako 00"); }
    // --- dzielenie ---
    { Parsed p; split(p, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47");
      CHECK(p.n >= 15, "GGA ma 15+ pol"); CHECK(strcmp(p.f[0], "$GPGGA") == 0, "pole 0"); CHECK(strcmp(p.f[6], "1") == 0, "pole fix");
      CHECK(p.f[13][0] == 0, "puste pole wieku"); }
    { Parsed p; split(p, "a,b,c"); CHECK(p.n == 3 && strcmp(p.f[2], "c") == 0, "trzy pola"); }
    { char b[] = "x,y,z,w"; char* f[2]; CHECK(Nmea::splitFields(b, f, 2) == 2, "limit pol"); }
    { char b[] = "x"; char* f[4]; CHECK(Nmea::splitFields(b, f, 0) == 0 && Nmea::splitFields(b, f, 4) == 1, "maxFields 0 i jedno pole"); }
    CHECK(sentenceEndsWith("GPGGA", "GGA") && sentenceEndsWith("GNGGA", "GGA") && sentenceEndsWith("GBVTG", "VTG"), "rozpoznanie typu");
    CHECK(!sentenceEndsWith("GP", "GGA") && !sentenceEndsWith("GPGGA", "RMC") && !sentenceEndsWith("", "GGA"), "krotkie i inne typy");
    // --- GGA ---
    { Parsed p; split(p, "$GNGGA,123519.00,5201.12345,N,02101.54321,E,4,18,0.6,101.5,M,35.2,M,1.5,0000*5A");
      Gga g; CHECK(parseGga(p.f, p.n, g), "GGA ok");
      CHECK(g.hasTime && strcmp(g.time, "123519.00") == 0, "czas"); CHECK(strcmp(g.lat, "5201.12345") == 0 && g.latDir == 'N', "szerokosc");
      CHECK(strcmp(g.lon, "02101.54321") == 0 && g.lonDir == 'E', "dlugosc"); CHECK(g.fix == 4 && g.sats == 18, "fix i satelity");
      CHECK(fabsf(g.hdop - 0.6f) < 1e-4f && fabsf(g.alt - 101.5f) < 1e-3f, "hdop i wysokosc");
      CHECK(g.ageValid && fabsf(g.age - 1.5f) < 1e-4f, "wiek poprawek"); }
    { Parsed p; split(p, "$GNGGA,,,,,,0,00,99.99,,M,,M,,*00");
      Gga g; CHECK(parseGga(p.f, p.n, g), "GGA bez fixa");
      CHECK(!g.hasTime && !g.hasLat && !g.hasLon && !g.hasLatDir, "puste pola oznaczone"); CHECK(g.fix == 0 && g.sats == 0, "fix 0");
      CHECK(g.alt == 0.0f && !g.ageValid && g.age == 0.0f, "puste wysokosc i wiek"); }
    { Parsed p; split(p, "$GPGGA,1,2,N"); Gga g; CHECK(!parseGga(p.f, p.n, g), "za krotkie GGA odrzucone"); }
    { // przepelnienie pol: bardzo dlugi czas jest obciety, nie wychodzi poza bufor
      Parsed p; split(p, "$GPGGA,123456789012345678901234567890,5201.1,N,02101.5,E,1,08,0.9,10,M,1,M,,*00");
      Gga g; CHECK(parseGga(p.f, p.n, g) && strlen(g.time) == sizeof(g.time) - 1, "dlugi czas obciety do bufora"); }
    // --- VTG ---
    { Parsed p; split(p, "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K,A*00");
      Vtg v; CHECK(parseVtg(p.f, p.n, v) && v.hasHeading && v.hasSpeed, "VTG ok");
      CHECK(fabsf(v.heading - 54.7f) < 1e-3f && fabsf(v.speedKnots - 5.5f) < 1e-3f, "kurs i predkosc"); }
    { Parsed p; split(p, "$GPVTG,,T,,M,,N,,K,N*00"); Vtg v; CHECK(parseVtg(p.f, p.n, v) && !v.hasHeading && !v.hasSpeed, "VTG puste pola"); }
    { Parsed p; split(p, "$GPVTG,1,T"); Vtg v; CHECK(!parseVtg(p.f, p.n, v), "VTG za krotkie"); }
    // --- GST ---
    { Parsed p; split(p, "$GNGST,123519.00,1.2,0.8,0.6,45.0,0.03,0.04,0.09*00");
      Gst g; CHECK(parseGst(p.f, p.n, g), "GST ok"); CHECK(fabsf(g.hAcc - 0.05f) < 1e-4f, "hAcc = sqrt(0.03^2+0.04^2)"); CHECK(fabsf(g.vAcc - 0.09f) < 1e-4f, "vAcc"); }
    { Parsed p; split(p, "$GNGST,1,1,1,1,1,0.03,0.04,*00"); Gst g; CHECK(parseGst(p.f, p.n, g) && g.vAcc == -1.0f, "GST bez vAcc = -1"); }
    { Parsed p; split(p, "$GNGST,1,1,1,1,1,,0.04,0.09*00"); Gst g; CHECK(!parseGst(p.f, p.n, g), "GST bez bledu szerokosci odrzucone"); }
    { Parsed p; split(p, "$GNGST,1,1"); Gst g; CHECK(!parseGst(p.f, p.n, g), "GST za krotkie"); }
    // --- RMC ---
    { Parsed p; split(p, "$GNRMC,123519.00,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W,A*00");
      Rmc r; CHECK(parseRmc(p.f, p.n, r), "RMC ok"); CHECK(r.hasDate, "jest data");
      CHECK(fabsf(r.speedKnots - 22.4f) < 1e-3f && fabsf(r.course - 84.4f) < 1e-3f, "predkosc i kurs");
      CHECK(strcmp(r.lat, "4807.038") == 0 && r.latDir == 'N' && r.lonDir == 'E', "pozycja"); }
    { Parsed p; split(p, "$GNRMC,123519.00,V,,,,,,,230394,,*00"); Rmc r; CHECK(!parseRmc(p.f, p.n, r), "status V odrzucony"); }
    { Parsed p; split(p, "$GNRMC,1,A,2,N,3,E,4,5"); Rmc r; CHECK(parseRmc(p.f, p.n, r) && !r.hasDate, "RMC 9 pol bez daty"); }
    { Parsed p; split(p, "$GNRMC,1,A"); Rmc r; CHECK(!parseRmc(p.f, p.n, r), "RMC za krotkie"); }
    printf("WYNIK: %d/%d kontroli OK\n", ok, total);
    return ok == total ? 0 : 1;
}
