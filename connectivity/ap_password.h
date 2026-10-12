#pragma once
// ======================================================================
// connectivity/ap_password.h
// EN: Generates a random access-point password for a device that has none configured, so no two modules share a
//     public default. Pure C++ (the random source is passed in) - host-tested.
// PL: Generuje losowe hasło punktu dostępowego dla urządzenia, które żadnego nie ma ustawionego, żeby dwa moduły nie
//     miały wspólnego publicznego hasła. Czysty C++ (źródło losowości jest parametrem) - testowane na komputerze.
// Tests: tests/test_ap_password.cpp (host, plain C++).
// Used by: web/runtime_settings.h
// ======================================================================
#include <cstdint>
#include <cstring>

namespace ApPassword {
    // Letters and digits without look-alikes (no 0/O, 1/l/i), so the password is easy to copy from a screen.
    static const char ALPHABET[] = "abcdefghjkmnpqrstuvwxyz23456789";
    static const int  LENGTH = 10;            // 31^10 combinations; WPA2 needs 8-63 characters

    // Writes LENGTH characters plus a terminator to out (out must hold LENGTH + 1 bytes).
    // `rnd` returns 32 random bits per call (on the ESP32: esp_random).
    template <typename Rnd>
    inline void generate(Rnd rnd, char* out) {
        const uint32_t n = (uint32_t)(sizeof(ALPHABET) - 1);
        const uint32_t limit = 0xFFFFFFFFu - (0xFFFFFFFFu % n) - 1;   // rejection sampling: no modulo bias
        for (int i = 0; i < LENGTH; i++) {
            uint32_t r = rnd();
            while (r > limit) r = rnd();
            out[i] = ALPHABET[r % n];
        }
        out[LENGTH] = 0;
    }

    // WPA2 passphrase length check (what the panel also enforces).
    inline bool usable(const char* p) {
        if (!p) return false;
        const size_t n = strlen(p);
        return n >= 8 && n <= 63;
    }
}
