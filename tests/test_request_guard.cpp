// Tests: web/request_guard.h (CSRF origin check, OTA permission)
#include <cstdio>
#include "../web/request_guard.h"
static int ok = 0, total = 0;
#define CHECK(c, m) do { total++; if (c) ok++; else printf("BLAD: %s\n", m); } while (0)
int main() {
    using namespace RequestGuard;
    CHECK(sameOrigin("", "192.168.1.120"), "brak Origin = ok");
    CHECK(sameOrigin(nullptr, "192.168.1.120"), "nullptr Origin = ok");
    CHECK(sameOrigin("http://192.168.1.120", "192.168.1.120"), "panel z IP");
    CHECK(sameOrigin("http://aog.local", "aog.local"), "panel z nazwy mDNS");
    CHECK(sameOrigin("HTTP://AOG.LOCAL", "aog.local"), "wielkosc liter");
    CHECK(sameOrigin("http://192.168.4.1:80", "192.168.4.1:80"), "z portem");
    CHECK(sameOrigin("http://192.168.1.120/", "192.168.1.120"), "ukosnik na koncu");
    CHECK(!sameOrigin("http://evil.example", "192.168.1.120"), "obca strona");
    CHECK(!sameOrigin("http://192.168.1.120.evil.example", "192.168.1.120"), "prefiks hosta nie wystarcza");
    CHECK(!sameOrigin("http://evil.example", ""), "pusty Host");
    CHECK(!sameOrigin("null", "192.168.1.120"), "Origin null");
    CHECK(!sameOrigin("file://", "192.168.1.120"), "file://");
    CHECK(!sameOrigin("http://192.168.1.120:8080", "192.168.1.120"), "inny port");
    CHECK(otaAllowed(true, true, false, false), "haslo OTA + dobry login");
    CHECK(!otaAllowed(true, false, true, true), "haslo OTA: sam token panelu nie wystarcza");
    CHECK(!otaAllowed(true, false, false, false), "haslo OTA + zly login");
    CHECK(otaAllowed(false, false, true, true), "panel chroniony + token");
    CHECK(!otaAllowed(false, false, true, false), "panel chroniony bez tokenu");
    CHECK(otaAllowed(false, false, false, false), "nic nie ustawione = otwarte");
    // DNS rebinding: tylko IP, nazwa modulu, <nazwa>.local i localhost
    CHECK(hostAllowed("192.168.1.120", "AOG-ESP32") && hostAllowed("192.168.4.1:80", "AOG-ESP32"), "host: adres IP (z portem tez)");
    CHECK(hostAllowed("AOG-ESP32.local", "AOG-ESP32") && hostAllowed("aog-esp32.LOCAL:80", "AOG-ESP32"), "host: nazwa.local");
    CHECK(hostAllowed("AOG-ESP32", "AOG-ESP32") && hostAllowed("localhost", "AOG-ESP32"), "host: sama nazwa modulu, localhost");
    CHECK(hostAllowed("", "AOG-ESP32") && hostAllowed(nullptr, "AOG-ESP32"), "host: brak naglowka = skrypt, ok");
    CHECK(!hostAllowed("evil.example.com", "AOG-ESP32") && !hostAllowed("evil.example.com:80", "AOG-ESP32"), "host: obca domena (rebinding) odrzucona");
    CHECK(!hostAllowed("a.b.local", "AOG-ESP32") && !hostAllowed("192.168.1.1.evil.com", "AOG-ESP32"), "host: wieloczlonowe .local i IP-jako-poddomena odrzucone");
    CHECK(!hostAllowed(":80", "AOG-ESP32"), "host: sam port odrzucony");
    printf("WYNIK: %d/%d kontroli OK\n", ok, total);
    return ok == total ? 0 : 1;
}
