// Testy wieku poprawek (GpsAge): parsowanie pola 13 GGA (brak != 0) i poziom ostrzeżenia dla panelu.
// Czysty C++, z makrami Arduino (wylapywanie kolizji nazw):
//     g++ -std=c++17 -Wall -Wextra tests/test_gps_age.cpp -o /tmp/tga && /tmp/tga
#define EXTERNAL 0
#define INTERNAL 3
#define DEFAULT 1
#define INPUT 1
#define OUTPUT 3
#define HIGH 1
#define LOW 0
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "../sensors/gps_age.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)

// Dokladnie jak GpsHandler::splitFields(): tnie po ',' ORAZ po '*', maks. 24 pola.
static uint8_t splitFields(char* line, char* fields[], uint8_t maxFields) {
    uint8_t count = 0; char* p = line; fields[count++] = p;
    while (*p && count < maxFields) { if (*p == ',' || *p == '*') { *p = '\0'; fields[count++] = p + 1; } p++; }
    return count;
}

static bool ageFromGga(const char* sentence, float* out) {
    char b[128]; strcpy(b, sentence);
    char* f[24]; uint8_t n = splitFields(b, f, 24);
    return n > 13 && GpsAge::parse(f[13], out);
}

int main() {
    float v = -7.0f;

    // --- parsowanie pola ---
    CHECK(GpsAge::parse("1.0", &v) && v == 1.0f, "1.0 -> 1.0");
    CHECK(GpsAge::parse("0.0", &v) && v == 0.0f, "0.0 jest wazne (swieze poprawki)");
    CHECK(GpsAge::parse("12.5", &v) && v == 12.5f, "12.5");
    CHECK(GpsAge::parse("3", &v) && v == 3.0f, "liczba calkowita");
    CHECK(GpsAge::parse("9999", &v) && v == 9999.0f, "gorna granica jeszcze wazna");
    v = -7.0f;
    CHECK(!GpsAge::parse("", &v) && v == -7.0f, "puste pole = brak wieku, wynik nietkniety");
    CHECK(!GpsAge::parse(nullptr, &v), "nullptr");
    CHECK(!GpsAge::parse("abc", &v), "tekst");
    CHECK(!GpsAge::parse("12x", &v), "smiecie po liczbie");
    CHECK(!GpsAge::parse("-1", &v), "ujemny wiek odrzucony");
    CHECK(!GpsAge::parse("nan", &v), "NaN odrzucony");
    CHECK(!GpsAge::parse("inf", &v), "inf odrzucony");
    CHECK(!GpsAge::parse("1e9", &v), "absurdalnie duzy odrzucony");
    CHECK(!GpsAge::parse("10000", &v), "tuz powyzej granicy odrzucony");
    CHECK(v == -7.0f, "odrzucone pola nie zmieniaja wyniku");

    // --- prawdziwe zdania GGA (jak splitFields w GpsHandler) ---
    CHECK(ageFromGga("$GNGGA,123519.00,5206.12345,N,02101.54321,E,5,24,0.6,101.2,M,34.1,M,1.0,0000*5B", &v) && v == 1.0f,
          "RTK FLOAT z wiekiem 1.0 i stacja 0000");
    CHECK(ageFromGga("$GNGGA,123519.00,5206.12345,N,02101.54321,E,4,24,0.6,101.2,M,34.1,M,2.0,0000", &v) && v == 2.0f,
          "bez sumy kontrolnej tez dziala");
    CHECK(!ageFromGga("$GNGGA,123519.00,5206.12345,N,02101.54321,E,1,24,0.6,101.2,M,34.1,M,,*5B", &v),
          "zwykly GPS: puste pole 13 = brak wieku");
    CHECK(!ageFromGga("$GNGGA,123519.00,5206.12345,N,02101.54321,E,1,24,0.6,101.2,M,34.1,M", &v),
          "zdanie bez pola 13 = brak wieku");

    // --- poziom dla panelu ---
    using namespace GpsAge;
    CHECK(level(true, 1.0f, 4) == LVL_OK, "FIX, 1 s: ok");
    CHECK(level(true, 0.0f, 5) == LVL_OK, "FLOAT, 0 s: ok");
    CHECK(level(true, 10.0f, 4) == LVL_OK, "dokladnie 10 s: jeszcze ok");
    CHECK(level(true, 10.1f, 4) == LVL_WARN, "10,1 s: ostrzezenie");
    CHECK(level(true, 120.0f, 5) == LVL_WARN, "dokladnie 120 s: jeszcze ostrzezenie");
    CHECK(level(true, 120.1f, 5) == LVL_BAD, "120,1 s: zle");
    CHECK(level(true, 600.0f, 2) == LVL_BAD, "DGPS, 600 s: zle");
    CHECK(level(false, 0.0f, 4) == LVL_NONE, "brak wieku przy FIX: brak danych, nie ok i nie awaria");
    CHECK(level(false, 999.0f, 5) == LVL_NONE, "brak wieku ignoruje wartosc");
    CHECK(level(true, 500.0f, 1) == LVL_NONE, "zwykly GPS (bez poprawek): wiek nie ma sensu");
    CHECK(level(true, 500.0f, 0) == LVL_NONE, "brak fixa: wiek nie ma sensu");
    CHECK(level(true, 500.0f, 3) == LVL_NONE, "nieznany typ fixa: brak danych");
    CHECK(level(true, 500.0f, -1) == LVL_NONE, "fix -1: brak danych");

    printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
    return fails ? 1 : 0;
}
