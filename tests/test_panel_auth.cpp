// Testy zabezpieczenia panelu (web/panel_auth.h). Czysty C++.
// Kompilacja: g++ -std=c++17 -Wall -Wextra tests/test_panel_auth.cpp -o /tmp/test_panel_auth && /tmp/test_panel_auth
#include <cstdio>
#include <string>
#include <cstring>
#include "../web/panel_auth.h"

static int g_ok = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_ok++; } else { g_fail++; std::printf("BLAD: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } } while (0)

static void rnd(uint32_t seed, uint32_t out[4]) { for (int i = 0; i < 4; i++) out[i] = seed * 2654435761u + i * 40503u; }

int main() {
    using P = PanelAuth;
    // walidacja
    CHECK(!P::validateSecret(P::PIN, "1234"), "PIN 4 cyfry OK");
    CHECK(!P::validateSecret(P::PIN, "12345678"), "PIN 8 cyfr OK");
    CHECK(P::validateSecret(P::PIN, "123"), "PIN 3 cyfry odrzucony");
    CHECK(P::validateSecret(P::PIN, "123456789"), "PIN 9 cyfr odrzucony");
    CHECK(P::validateSecret(P::PIN, "12a4"), "PIN z litera odrzucony");
    CHECK(P::validateSecret(P::PIN, nullptr), "PIN nullptr odrzucony");
    CHECK(!P::validateSecret(P::PASSWORD, "abcdefgh"), "haslo 8 znakow OK");
    CHECK(P::validateSecret(P::PASSWORD, "abcdefg"), "haslo 7 znakow odrzucone");
    CHECK(P::validateSecret(P::PASSWORD, "abcdefgh\n"), "haslo ze znakiem sterujacym odrzucone");
    std::string longp(64, 'a');
    CHECK(P::validateSecret(P::PASSWORD, longp.c_str()), "haslo 64 znaki odrzucone");
    CHECK(!P::validateSecret(P::PASSWORD, std::string(63, 'a').c_str()), "haslo 63 znaki OK");
    CHECK(!P::validateSecret(P::PASSWORD, "zażółć gęślą"), "haslo z polskimi znakami (UTF-8) OK");
    CHECK(!P::validateSecret(P::DECLINED, nullptr), "bez zabezpieczenia: kod niepotrzebny");

    // start: nieustawione = otwarte, wszystko wolno
    P::resetForTest();
    CHECK(P::mode() == P::NOT_SET && !P::isProtected() && P::isOpenForSetup(), "start: NOT_SET, otwarte");
    CHECK(P::authorized(nullptr, 0), "NOT_SET: zadanie bez zetonu dozwolone");

    // ustawienie PIN
    CHECK(!P::set(P::PIN, "12"), "set: zly PIN odrzucony");
    CHECK(P::mode() == P::NOT_SET, "set: zly PIN nie zmienia trybu");
    CHECK(P::set(P::PIN, "2468"), "set: PIN przyjety");
    CHECK(P::isProtected() && !P::isOpenForSetup(), "PIN: chroniony");
    CHECK(!P::authorized(nullptr, 100) && !P::authorized("", 100), "chroniony: brak zetonu = odmowa");
    CHECK(!P::authorized("0123456789abcdef0123456789abcdef", 100), "chroniony: zmyslony zeton = odmowa");

    // logowanie
    uint32_t r[4]; char tok[33];
    rnd(1, r);
    uint32_t left = 0;
    CHECK(P::login("0000", 1000, r, tok, &left) == P::LOGIN_WRONG, "zly PIN = LOGIN_WRONG");
    CHECK(P::login("2468", 1100, r, tok, &left) == P::LOGIN_OK, "dobry PIN = LOGIN_OK");
    CHECK(std::strlen(tok) == 32, "zeton ma 32 znaki");
    CHECK(P::authorized(tok, 1200), "zeton dziala");
    CHECK(!P::authorized("zzzz", 1200), "krotki zeton odrzucony");
    CHECK(P::authorized(tok, 1100 + P::TOKEN_TTL_MS), "zeton wazny do konca TTL");
    CHECK(!P::authorized(tok, 1100 + P::TOKEN_TTL_MS + 1), "zeton wygasa po TTL");

    // blokada po 5 bledach
    P::resetForTest(); P::set(P::PIN, "2468");
    uint32_t t = 10000;
    for (int i = 0; i < 4; i++) CHECK(P::login("1111", t += 10, r, tok) == P::LOGIN_WRONG, "bledy 1-4: WRONG");
    CHECK(P::login("1111", t += 10, r, tok) == P::LOGIN_WRONG, "5. blad: WRONG (zaklada blokade)");
    CHECK(P::login("2468", t + 100, r, tok, &left) == P::LOGIN_LOCKED, "w blokadzie nawet dobry kod = LOCKED");
    CHECK(left > 29000 && left <= 30000, "pozostalo ok. 30 s blokady");
    CHECK(P::login("2468", t + 30001, r, tok) == P::LOGIN_OK, "po 30 s dobry kod przechodzi");
    // kolejne bledy po odblokowaniu: blokada rosnie
    P::resetForTest(); P::set(P::PIN, "2468"); t = 0;
    for (int i = 0; i < 5; i++) P::login("1", t += 1, r, tok);           // blokada 30 s
    t += 30001;
    CHECK(P::login("1", t, r, tok) == P::LOGIN_WRONG, "6. blad po odblokowaniu: WRONG");
    CHECK(P::login("2468", t + 31000, r, tok, &left) == P::LOGIN_LOCKED && left > 28000, "6. blad: blokada 60 s");
    // sukces zeruje licznik
    P::resetForTest(); P::set(P::PIN, "2468"); t = 0;
    for (int i = 0; i < 4; i++) P::login("1", t += 1, r, tok);
    CHECK(P::login("2468", t += 1, r, tok) == P::LOGIN_OK, "dobry kod po 4 bledach");
    for (int i = 0; i < 4; i++) CHECK(P::login("1", t += 1, r, tok) == P::LOGIN_WRONG, "po sukcesie licznik od zera (4 bledy nie blokuja)");
    CHECK(P::login("2468", t += 1, r, tok) == P::LOGIN_OK, "nadal brak blokady po 4 bledach");

    // zawiniecie millis podczas blokady
    P::resetForTest(); P::set(P::PIN, "2468"); t = 0xFFFFFF00u;
    for (int i = 0; i < 5; i++) P::login("1", t += 1, r, tok);
    CHECK(P::login("2468", 0x00000100u, r, tok) == P::LOGIN_LOCKED, "blokada trzyma po zawinieciu millis");
    CHECK(P::login("2468", 0x00010000u, r, tok) == P::LOGIN_OK, "blokada schodzi po zawinieciu millis");

    // maksymalna blokada
    P::resetForTest(); P::set(P::PIN, "2468"); t = 0;
    for (int i = 0; i < 20; i++) { t += 400000; P::login("1", t, r, tok); }
    CHECK(P::login("2468", t + 100, r, tok, &left) == P::LOGIN_LOCKED && left <= P::LOCK_MAX_MS, "blokada nie przekracza 5 min");

    // zmiana kodu uniewaznia zetony
    P::resetForTest(); P::set(P::PIN, "2468"); rnd(7, r);
    P::login("2468", 5, r, tok);
    CHECK(P::authorized(tok, 6), "zeton przed zmiana kodu dziala");
    P::set(P::PASSWORD, "dluzszehaslo");
    CHECK(!P::authorized(tok, 7), "po zmianie kodu stary zeton uniewazniony");
    CHECK(P::login("2468", 8, r, tok) == P::LOGIN_WRONG, "stary PIN nie dziala");
    CHECK(P::login("dluzszehaslo", 9, r, tok) == P::LOGIN_OK, "nowe haslo dziala");

    // wiele zetonow, wypieranie najstarszego
    P::resetForTest(); P::set(P::PIN, "2468");
    char tk[6][33]; uint32_t rr[4];
    for (int i = 0; i < 5; i++) { rnd(100 + i, rr); P::login("2468", 1000 * (i + 1), rr, tk[i]); }
    CHECK(!P::authorized(tk[0], 5001), "najstarszy z 5 zetonow wyparty (MAX 4)");
    CHECK(P::authorized(tk[4], 5001) && P::authorized(tk[1], 5001), "pozostale dzialaja");

    // odrzucenie przy DECLINED
    P::resetForTest(); P::set(P::DECLINED, nullptr);
    CHECK(P::mode() == P::DECLINED && !P::isProtected() && P::authorized(nullptr, 0), "DECLINED: otwarte, nie pyta ponownie");
    CHECK(P::login("x", 1, r, tok) == P::LOGIN_WRONG, "DECLINED: login nie wydaje zetonu");

    // wczytanie z NVS
    P::resetForTest(); P::load(P::PIN, "2468");
    CHECK(P::mode() == P::PIN && P::isProtected(), "load: PIN");
    P::load(P::PIN, "ab");
    CHECK(P::mode() == P::NOT_SET, "load: uszkodzony PIN = NOT_SET (panel poprosi o ustawienie)");
    P::load(P::DECLINED, "");
    CHECK(P::mode() == P::DECLINED, "load: DECLINED");
    P::load(9, "x");
    CHECK(P::mode() == P::NOT_SET, "load: nieznany tryb = NOT_SET");

    // potwierdzenie obecnego kodu przy zmianie (check): te same blokady co logowanie, zetonu nie wydaje
    P::resetForTest(); P::set(P::PIN, "2468"); t = 0;
    CHECK(P::check("2468", t, &left) == P::LOGIN_OK, "check: dobry kod");
    CHECK(P::check("0000", t += 1, &left) == P::LOGIN_WRONG, "check: zly kod");
    CHECK(!P::authorized("0123456789abcdef0123456789abcdef", t), "check nie wydaje zetonu");
    for (int i = 0; i < 4; i++) P::check("1", t += 1, &left);
    CHECK(P::check("2468", t += 1, &left) == P::LOGIN_LOCKED && left > 0, "check: po 5 bledach (login i check razem) blokada");
    CHECK(P::check("2468", t + 31000, &left) == P::LOGIN_OK, "check: po blokadzie dobry kod przechodzi");
    P::resetForTest();
    CHECK(P::check("2468", 0, &left) == P::LOGIN_WRONG, "check: panel niezabezpieczony - zawsze WRONG");

    // porownanie
    CHECK(P::eq("abc", "abc") && !P::eq("abc", "abd") && !P::eq("abc", "abcd") && !P::eq("", "a") && P::eq("", ""), "eq: rozne dlugosci i tresci");

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_ok + g_fail);
    return g_fail == 0 ? 0 : 1;
}
