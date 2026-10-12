// Test generatora hasla AP (ApPassword) - czysty C++.
//     g++ -std=c++17 -Wall -Wextra tests/test_ap_password.cpp -o /tmp/tap && /tmp/tap
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include "../connectivity/ap_password.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
int main() {
  uint32_t s = 12345;
  auto lcg = [&s]() { s = s * 1664525u + 1013904223u; return s; };
  char a[ApPassword::LENGTH + 1];
  ApPassword::generate(lcg, a);
  CHECK(strlen(a) == (size_t)ApPassword::LENGTH, "dlugosc hasla = LENGTH");
  CHECK(ApPassword::usable(a), "wygenerowane haslo spelnia wymog WPA2 (8-63 znakow)");
  bool alphabetOk = true;
  for (int i = 0; i < ApPassword::LENGTH; i++) if (!strchr(ApPassword::ALPHABET, a[i])) alphabetOk = false;
  CHECK(alphabetOk, "tylko znaki z alfabetu");
  CHECK(!strpbrk(a, "01OlIi"), "bez znakow mylacych (0 O 1 l I i)");
  // wielokrotnie: zawsze poprawne i rozne
  std::set<std::string> seen; bool allOk = true;
  for (int k = 0; k < 500; k++) { char b[ApPassword::LENGTH + 1]; ApPassword::generate(lcg, b); seen.insert(b); if (!ApPassword::usable(b) || strpbrk(b, "01OlIi")) allOk = false; }
  CHECK(allOk, "500 hasel: wszystkie poprawne");
  CHECK(seen.size() == 500, "500 hasel: wszystkie rozne");
  // odrzucanie wartosci poza zakresem (bez przesuniecia modulo): najpierw skrajna wartosc, potem 0
  { int calls = 0; auto r = [&calls]() -> uint32_t { return (calls++ % 2 == 0) ? 0xFFFFFFFFu : 0u; };
    char b[ApPassword::LENGTH + 1]; ApPassword::generate(r, b);
    CHECK(b[0] == ApPassword::ALPHABET[0] && calls == 2 * ApPassword::LENGTH, "wartosc poza zakresem jest losowana ponownie"); }
  // deterministyczne dla tego samego zrodla
  { uint32_t s1 = 7, s2 = 7; auto l1 = [&s1]() { s1 = s1 * 1664525u + 1013904223u; return s1; }; auto l2 = [&s2]() { s2 = s2 * 1664525u + 1013904223u; return s2; };
    char x[ApPassword::LENGTH + 1], y[ApPassword::LENGTH + 1]; ApPassword::generate(l1, x); ApPassword::generate(l2, y);
    CHECK(strcmp(x, y) == 0, "to samo zrodlo losowosci daje to samo haslo"); }
  CHECK(!ApPassword::usable(nullptr) && !ApPassword::usable("") && !ApPassword::usable("1234567") && ApPassword::usable("12345678"), "usable: granice 8..63");
  CHECK(!ApPassword::usable(std::string(64, 'a').c_str()) && ApPassword::usable(std::string(63, 'a').c_str()), "usable: 63 tak, 64 nie");
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total); return fails ? 1 : 0;
}
