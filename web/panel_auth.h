#pragma once
// ======================================================================
// web/panel_auth.h
// EN: Panel protection by PIN or password: login tokens, brute-force lockout, every mutating action requires
//     authorisation.
// PL: Zabezpieczenie panelu PIN-em lub hasłem: tokeny logowania, blokada przy zgadywaniu, każda zmiana wymaga
//     autoryzacji.
// Tests: tests/test_panel_auth.cpp (host, plain C++).
// Used by: web/runtime_settings.h, web/web_server.h
// Depends on: msg.h
// ======================================================================
#include "msg.h"
// Protecting the web panel with a PIN or a password. Pure C++ (no Arduino) - host-tested (tests/test_panel_auth.cpp).
//
// MODEL:
//   - on first run (NOT_SET) the panel offers a choice: PIN (4-8 digits), password (8-63 characters) or no protection;
//   - in PIN / PASSWORD mode every CHANGING operation (saving settings, restart, motor test, calibration etc.) requires login;
//     viewing (status, charts, log) stays open;
//   - after a correct code the module issues a random token (32 hex characters) valid for TOKEN_TTL_MS; the panel sends it in the X-Auth header;
//   - guess protection: after 5 wrong codes in a row a 30 s lockout, further errors double it (max 5 min);
//   - the code is kept in the module's memory (NVS) in plain text, like the WiFi password - the protection is about the network, not physical access to the board.
// Recovering access after forgetting the code: PANEL_AUTH_RESET_ON_BOOT in config.h (clears the code at startup), or reflashing the firmware.
#include <cstdint>
#include <cstring>

class PanelAuth {
public:
    enum Mode : uint8_t { NOT_SET = 0, PIN = 1, PASSWORD = 2, DECLINED = 3 };
    enum LoginResult : uint8_t { LOGIN_OK = 0, LOGIN_WRONG = 1, LOGIN_LOCKED = 2 };

    static const uint8_t MAX_TOKENS = 4;
    static const uint32_t TOKEN_TTL_MS = 12UL * 3600UL * 1000UL;
    static const uint8_t FAILS_BEFORE_LOCK = 5;
    static const uint32_t LOCK_BASE_MS = 30000;
    static const uint32_t LOCK_MAX_MS = 300000;
    static const size_t SECRET_MAX = 63;

    // Validation of the code when setting it. nullptr = valid, otherwise an error text for the panel.
    static const char* validateSecret(Mode mode, const char* s) {
        if (mode == DECLINED) return nullptr;
        if (!s) return MSG("No code");
        size_t n = std::strlen(s);
        if (mode == PIN) {
            if (n < 4 || n > 8) return MSG("PIN: 4-8 digits");
            for (size_t i = 0; i < n; i++) if (s[i] < '0' || s[i] > '9') return MSG("PIN: digits only");
            return nullptr;
        }
        if (mode == PASSWORD) {
            if (n < 8 || n > SECRET_MAX) return MSG("Password: 8-63 characters");
            for (size_t i = 0; i < n; i++) if ((unsigned char)s[i] < 0x20 || s[i] == 0x7F) return MSG("Password: no control characters");
            return nullptr;
        }
        return MSG("Unknown mode");
    }

    static Mode mode() { return curMode; }
    static bool isProtected() { return curMode == PIN || curMode == PASSWORD; }
    static bool isOpenForSetup() { return curMode == NOT_SET; }

    // Load from NVS (RuntimeSettings::begin). Invalid data = NOT_SET (the panel will ask for setup).
    static void load(uint8_t m, const char* secret) {
        revokeAll();
        curMode = NOT_SET; secret_[0] = 0;
        if (m == DECLINED) { curMode = DECLINED; return; }
        if ((m == PIN || m == PASSWORD) && !validateSecret((Mode)m, secret)) {
            curMode = (Mode)m;
            std::strncpy(secret_, secret, SECRET_MAX); secret_[SECRET_MAX] = 0;
        }
    }
    static const char* secret() { return secret_; }

    // Setting the mode (after validation and authorization in the handler). Invalidates all tokens - a new token is issued separately.
    static bool set(Mode m, const char* secret) {
        if (validateSecret(m, secret)) return false;
        revokeAll();
        curMode = m;
        secret_[0] = 0;
        if (m == PIN || m == PASSWORD) { std::strncpy(secret_, secret, SECRET_MAX); secret_[SECRET_MAX] = 0; }
        fails = 0; lockedUntil = 0; lockUntilValid = false;
        return true;
    }

    // Checking the code (login AND confirmation when changing/disabling the protection). Counts errors and locks:
    // after FAILS_BEFORE_LOCK errors in a row a lockout, each further error doubles the time (max LOCK_MAX_MS). lockLeftMs: how much lockout remains.
    static LoginResult check(const char* code, uint32_t now, uint32_t* lockLeftMs = nullptr) {
        if (lockUntilValid && (int32_t)(lockedUntil - now) > 0) {
            if (lockLeftMs) *lockLeftMs = lockedUntil - now;
            return LOGIN_LOCKED;
        }
        lockUntilValid = false;
        if (!isProtected() || !code || !eq(code, secret_)) {
            if (++fails >= FAILS_BEFORE_LOCK) {
                uint32_t step = fails - FAILS_BEFORE_LOCK;
                uint32_t ms = LOCK_BASE_MS;
                for (uint32_t i = 0; i < step && ms < LOCK_MAX_MS; i++) ms *= 2;
                if (ms > LOCK_MAX_MS) ms = LOCK_MAX_MS;
                lockedUntil = now + ms; lockUntilValid = true;
            }
            return LOGIN_WRONG;
        }
        fails = 0;
        return LOGIN_OK;
    }

    // Login. rnd = 4 random words (from esp_random()); out >= 33 bytes.
    static LoginResult login(const char* code, uint32_t now, const uint32_t rnd[4], char* out, uint32_t* lockLeftMs = nullptr) {
        LoginResult r = check(code, now, lockLeftMs);
        if (r == LOGIN_OK) issue(now, rnd, out);
        return r;
    }

    // Whether a request with this token may be executed. When the panel is not protected - always yes.
    static bool authorized(const char* token, uint32_t now) {
        if (!isProtected()) return true;
        if (!token || std::strlen(token) != 32) return false;
        for (uint8_t i = 0; i < MAX_TOKENS; i++) {
            if (!toks[i].used) continue;
            if ((uint32_t)(now - toks[i].issuedMs) > TOKEN_TTL_MS) { toks[i].used = false; continue; }
            if (eq(token, toks[i].val)) return true;
        }
        return false;
    }

    // Issuing a token without checking the code (after the code was set/changed by an authorized person).
    static void issue(uint32_t now, const uint32_t rnd[4], char* out) {
        static const char* DIGITS = "0123456789abcdef";
        int slot = -1; uint32_t oldest = 0;
        for (uint8_t i = 0; i < MAX_TOKENS; i++) {
            if (!toks[i].used) { slot = i; break; }
            uint32_t age = now - toks[i].issuedMs;
            if (slot < 0 || age > oldest) { slot = i; oldest = age; }
        }
        for (int w = 0; w < 4; w++) for (int k = 0; k < 8; k++) out[w * 8 + k] = DIGITS[(rnd[w] >> (28 - 4 * k)) & 0xF];
        out[32] = 0;
        toks[slot].used = true; toks[slot].issuedMs = now;
        std::memcpy(toks[slot].val, out, 33);
    }

    static void revokeAll() { for (uint8_t i = 0; i < MAX_TOKENS; i++) toks[i].used = false; }

    // Comparison without early exit (time independent of where the first difference is).
    static bool eq(const char* a, const char* b) {
        size_t la = std::strlen(a), lb = std::strlen(b);
        uint8_t d = (uint8_t)(la != lb);
        size_t n = la < lb ? la : lb;
        for (size_t i = 0; i < n; i++) d |= (uint8_t)(a[i] ^ b[i]);
        return d == 0;
    }

    static void resetForTest() { revokeAll(); curMode = NOT_SET; secret_[0] = 0; fails = 0; lockedUntil = 0; lockUntilValid = false; }

private:
    struct Tok { bool used; uint32_t issuedMs; char val[33]; };
    static Mode curMode;
    static char secret_[SECRET_MAX + 1];
    static Tok toks[MAX_TOKENS];
    static uint32_t fails;
    static uint32_t lockedUntil;
    static bool lockUntilValid;
};
PanelAuth::Mode PanelAuth::curMode = PanelAuth::NOT_SET;
char PanelAuth::secret_[PanelAuth::SECRET_MAX + 1] = {0};
PanelAuth::Tok PanelAuth::toks[PanelAuth::MAX_TOKENS] = {};
uint32_t PanelAuth::fails = 0;
uint32_t PanelAuth::lockedUntil = 0;
bool PanelAuth::lockUntilValid = false;
