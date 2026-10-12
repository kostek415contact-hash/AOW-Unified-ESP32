// Test blokady ustawien (SettingsLock / SettingsWriteGuard / SettingsReadGuard): dwa watki, jeden zapisuje
// pary tekstow (jak panel WWW), drugi czyta migawke (jak petla glowna) - pary musza byc zawsze spojne.
// Pelny dowod wyscigu (ThreadSanitizer, stary vs nowy kod) - patrz opis zmiany; ten test pilnuje kontraktu.
//     g++ -std=c++17 -Wall -Wextra -pthread tests/test_settings_lock.cpp -o /tmp/tsl && /tmp/tsl
#include <cstdio>
#include <string>
#include <thread>
#include <atomic>
#include "../web/settings_lock.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
int main() {
  uint32_t v0 = SettingsLock::version();
  { SettingsWriteGuard g; }
  CHECK(SettingsLock::version() == v0 + 1, "kazdy zapis podbija wersje o 1");
  { SettingsReadGuard g; }
  CHECK(SettingsLock::version() == v0 + 1, "odczyt nie zmienia wersji");
  { SettingsWriteGuard a; }
  { SettingsWriteGuard b; }
  CHECK(SettingsLock::version() == v0 + 3, "trzy zapisy = +3");

  std::string ssid = "krotka", pass = "haslo-k";
  const std::string longSsid(200, 'L'), longPass(63, 'P');
  std::atomic<bool> stop(false);
  std::thread writer([&]() {
    for (int i = 0; i < 20000 && !stop; i++) {
      SettingsWriteGuard g;
      if (i % 2) { ssid = longSsid; pass = longPass; } else { ssid = "krotka"; pass = "haslo-k"; }
    }
  });
  long torn = 0; uint32_t lastVer = SettingsLock::version(); bool monotonic = true;
  for (int i = 0; i < 20000; i++) {
    std::string s, p;
    { SettingsReadGuard g; s = ssid; p = pass; }
    if (!((s == "krotka" && p == "haslo-k") || (s == longSsid && p == longPass))) torn++;
    uint32_t v = SettingsLock::version(); if (v < lastVer) monotonic = false; lastVer = v;
  }
  stop = true; writer.join();
  CHECK(torn == 0, "20000 odczytow pod blokada: zadna para (SSID, haslo) nie jest rozerwana");
  CHECK(monotonic, "wersja nigdy nie maleje");
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
