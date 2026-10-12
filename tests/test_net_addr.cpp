// Testy wyboru adresow sieciowych (NetAddr): broadcast, ta sama podsiec, adres docelowy ramek do AgOpenWeb.
// Czysty C++, z makrami Arduino (wylapywanie kolizji nazw):
//     g++ -std=c++17 -Wall -Wextra tests/test_net_addr.cpp -o /tmp/tna && /tmp/tna
#define EXTERNAL 0
#define INTERNAL 3
#define DEFAULT 1
#define INPUT 1
#define OUTPUT 3
#define HIGH 1
#define LOW 0
#include <cstdio>
#include <cstdint>
#include "../connectivity/net_addr.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
// adres w kolejnosci sieciowej (a.b.c.d -> 32 bity, a najstarszy): to samo, co po rzutowaniu IPAddress na uint32_t w obu endiannessach spojnych
static uint32_t A(int a, int b, int c, int d) { return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d; }
static const uint32_t M24 = 0xFFFFFF00u, M28 = 0xFFFFFFF0u, M16 = 0xFFFF0000u, M20 = 0xFFFFF000u;

int main() {
    using namespace NetAddr;
    // ---- broadcast ----
    CHECK(broadcast(A(192,168,1,120), M24) == A(192,168,1,255), "broadcast /24 domowy: 192.168.1.120 -> 192.168.1.255");
    CHECK(broadcast(A(10,20,255,186), M24) == A(10,20,255,255), "broadcast hotspotu telefonu (10.20.255.186 /24) -> 10.20.255.255");
    CHECK(broadcast(A(172,20,10,5), M28) == A(172,20,10,15), "broadcast iPhone /28: 172.20.10.5 -> 172.20.10.15 (a nie .255)");
    CHECK(broadcast(A(10,20,200,9), M20) == A(10,20,207,255), "broadcast /20: 10.20.200.9 -> 10.20.207.255");
    CHECK(broadcast(A(192,168,43,77), M16) == A(192,168,255,255), "broadcast /16");
    // ---- ta sama podsiec ----
    CHECK(sameSubnet(A(10,20,255,186), A(10,20,255,117), M24), "ESP i brama telefonu w jednej podsieci /24");
    CHECK(!sameSubnet(A(192,168,1,120), A(10,20,255,117), M24), "domowy adres modulu NIE jest w podsieci hotspotu");
    CHECK(!sameSubnet(A(172,20,10,5), A(172,20,10,20), M28), "/28: .5 i .20 to rozne podsieci");
    CHECK(sameSubnet(A(172,20,10,5), A(172,20,10,14), M28), "/28: .5 i .14 w jednej podsieci");
    CHECK(!sameSubnet(A(10,0,0,1), A(10,0,0,2), 0), "maska 0 = brak informacji: false");

    // ---- adres docelowy ramek do AgOpenWeb ----
    const uint32_t HOME_AGIO = A(192,168,1,255);
    // 1) siec domowa, wszystko jak dotychczas
    CHECK(pickDest(false, 0, HOME_AGIO, A(192,168,1,120), M24) == HOME_AGIO, "dom, AgOpenWeb nieznany: skonfigurowany broadcast 192.168.1.255");
    CHECK(pickDest(true, A(192,168,1,25), HOME_AGIO, A(192,168,1,120), M24) == A(192,168,1,25), "dom, AgOpenWeb poznany: unicast");
    // 2) hotspot telefonu: skonfigurowany adres domowy NIE pasuje
    CHECK(pickDest(false, 0, HOME_AGIO, A(10,20,255,186), M24) == A(10,20,255,255), "hotspot, AgOpenWeb nieznany: broadcast AKTUALNEJ podsieci (nie domowy 192.168.1.255)");
    CHECK(pickDest(true, A(10,20,255,40), HOME_AGIO, A(10,20,255,186), M24) == A(10,20,255,40), "hotspot, AgOpenWeb poznany w tej podsieci: unicast");
    // 3) kluczowy blad: adres nauczony w domu, a modul przeszedl na hotspot
    CHECK(pickDest(true, A(192,168,1,25), HOME_AGIO, A(10,20,255,186), M24) == A(10,20,255,255), "adres poznany w DOMU jest nieaktualny po przejsciu na hotspot: broadcast hotspotu");
    CHECK(pickDest(true, A(192,168,1,25), A(10,20,255,255), A(10,20,255,186), M24) == A(10,20,255,255), "j.w., gdy skonfigurowany adres akurat pasuje do hotspotu: uzywamy go");
    // 4) iPhone /28
    CHECK(pickDest(false, 0, HOME_AGIO, A(172,20,10,5), M28) == A(172,20,10,15), "iPhone /28: broadcast .15");
    // 5) brak informacji o sieci (tryb AP, jeszcze bez polaczenia): jak dotychczas
    CHECK(pickDest(false, 0, HOME_AGIO, 0, 0) == HOME_AGIO, "bez adresu/maski: skonfigurowany");
    CHECK(pickDest(true, A(192,168,1,25), HOME_AGIO, 0, 0) == A(192,168,1,25), "bez adresu/maski, poznany: poznany (zachowanie sprzed zmiany)");
    CHECK(pickDest(false, 0, HOME_AGIO, A(10,20,255,186), 0) == HOME_AGIO, "adres jest, maski brak: nie zgadujemy, skonfigurowany");
    // 6) skonfigurowany adres z dobrej podsieci, nieznany AgOpenWeb
    CHECK(pickDest(false, 0, A(10,20,255,255), A(10,20,255,186), M24) == A(10,20,255,255), "skonfigurowany broadcast zgodny z hotspotem: uzyty");
    CHECK(pickDest(false, 0, A(10,20,255,40), A(10,20,255,186), M24) == A(10,20,255,40), "skonfigurowany UNICAST (np. stały adres tabletu) w tej podsieci: uzyty");

    printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
    return fails ? 1 : 0;
}
