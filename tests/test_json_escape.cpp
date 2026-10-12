// Host test of web/json_escape.h (plain C++).
//     g++ -std=c++17 -Wall -Wextra tests/test_json_escape.cpp -o /tmp/tje && /tmp/tje
#include <cstdio>
#include <string>
#include "../web/json_escape.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
static std::string esc(const std::string& in) {
  std::string out; char tmp[8];
  for (unsigned char c : in) { size_t n = JsonEscape::escapeChar(c, tmp); out.append(tmp, n); }
  return out;
}
// Minimal check that a string literal body is valid JSON: no raw quote, backslash only as a valid escape, no raw control bytes.
static bool validBody(const std::string& s) {
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '"' || c < 0x20) return false;
    if (c == '\\') {
      if (++i >= s.size()) return false;
      char e = s[i];
      if (e == 'u') { if (i + 4 >= s.size()) return false; i += 4; }
      else if (!(e == '"' || e == '\\' || e == 'n' || e == 'r' || e == 't')) return false;
    }
  }
  return true;
}
int main() {
  CHECK(esc("plain") == "plain", "zwykly tekst bez zmian");
  CHECK(esc("Jan's \"Net\"") == "Jan's \\\"Net\\\"", "cudzyslow jest escapowany, apostrof nie");
  CHECK(esc("a\\b") == "a\\\\b", "backslash jest podwajany");
  CHECK(esc("a\nb\r\tc") == "a\\nb\\r\\tc", "znaki sterujace \\n \\r \\t");
  CHECK(esc(std::string("a\x01" "b", 3)) == "a\\u0001b", "inny znak sterujacy jako \\u00XX");
  CHECK(esc("\x7f") == "\\u007f", "DEL jako \\u007f");
  CHECK(esc("zażółć") == "zażółć", "UTF-8 przechodzi bez zmian");
  std::string all; for (int c = 0; c < 256; c++) all += (char)c;
  CHECK(validBody(esc(all)), "wszystkie 256 bajtow daje poprawne cialo napisu JSON");
  CHECK(validBody(esc("\"\"\"\\\\\\")), "same cudzyslowy i backslashe: poprawny JSON");
  char t[8]; CHECK(JsonEscape::escapeChar('"', t) == 2 && JsonEscape::escapeChar(0x1f, t) == 6 && JsonEscape::escapeChar('x', t) == 1, "dlugosci wyniku 2 / 6 / 1");
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
