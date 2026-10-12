// Test katalogow jezykow panelu (web/lang/languages.h): wybor katalogu po kodzie jezyka oraz podstawowa
// poprawnosc skryptu, ktory modul wysyla przegladarce jako /lang.js (pelna kontrola kluczy: tools/i18n_check.py).
//     g++ -std=c++17 -Wall -Wextra tests/test_languages.cpp -o /tmp/tlang && /tmp/tlang
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <cctype>
#define PROGMEM   // na komputerze nie ma pamieci flash
#include "../web/lang/languages.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
int main() {
  CHECK(webLangCatalog("en") != nullptr, "angielski ma katalog");
  CHECK(webLangCatalog("pl") != nullptr, "polski ma katalog");
  CHECK(webLangCatalog("pl") != webLangCatalog("en"), "pl i en to rozne katalogi");
  CHECK(webLangCatalog("logsrc") != nullptr, "szablony dziennika (logsrc) maja swoj \"katalog\"");
  CHECK(std::string(webLangCatalog("logsrc")).find("window.AOG_LOGSRC={") != std::string::npos, "logsrc rejestruje window.AOG_LOGSRC");
  CHECK(webLangCatalog("") == nullptr, "pusty kod: brak katalogu");
  CHECK(webLangCatalog(nullptr) == nullptr, "nullptr: brak katalogu");
  CHECK(webLangCatalog("xx") == nullptr, "nieznany jezyk: brak katalogu");
  CHECK(webLangCatalog("EN") == nullptr, "kod rozrozniany wielkoscia liter (panel zawsze wysyla male)");
  CHECK(webLangCatalog("en;alert(1)") == nullptr, "kod z dopiskiem nie pasuje");

  for (const char* code : {"pl", "en"}) {
  const std::string js(webLangCatalog(code));
  CHECK(js.rfind("window.AOG_CAT=window.AOG_CAT||{};window.AOG_CAT.", 0) == 0, "katalog zaczyna sie od window.AOG_CAT=...");
  CHECK(js.find(std::string("window.AOG_CAT.") + code + "={") != std::string::npos, "katalog rejestruje sie pod wlasnym kodem jezyka");
  CHECK(js.size() > 2 && js.compare(js.size() - 2, 2, "};") == 0, "katalog konczy sie na };");
  std::string low = js; std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return std::tolower(c); });
  CHECK(low.find("</script") == std::string::npos, "w katalogu nie ma </script (nie rozbije strony)");
  CHECK(js.size() < 200000, "katalog miesci sie w rozsadnym rozmiarze (< 200 KB)");
  }
  CHECK(std::string(webLangCatalog("en")).find("\"yes\": \"yes\"") != std::string::npos, "przykladowy wpis angielski: yes");
  CHECK(std::string(webLangCatalog("pl")).find("\"yes\": \"tak\"") != std::string::npos, "przykladowy wpis polski: yes -> tak");
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
