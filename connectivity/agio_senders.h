#pragma once
// ======================================================================
// connectivity/agio_senders.h
// EN: Tracks AgOpenWeb senders and picks the one the module talks to (first sender wins, optional manual pin).
// PL: Śledzi nadawców AgOpenWeb i wybiera tego, z którym moduł rozmawia (pierwszy wygrywa, opcjonalne ręczne
//     przypięcie).
// Tests: tests/test_agio_senders.cpp (host, plain C++).
// Used by: connectivity/pgn_messages.h, web/runtime_settings.h
// ======================================================================
// AgOpenWeb frame senders and the choice of the one the module talks to. Pure C++ (no Arduino) - host-tested
// (tests/test_agio_senders.cpp). IP addresses as uint32_t in any, but consistent, byte order (IPAddress cast to
// uint32_t); the module compares them with nothing other than themselves.
//
// WHY: two AgOpenWeb programs can write to the module at once (e.g. a computer and a phone with the program left in the background). Previously the module
// followed the LAST sender - it answered one, then the other, so each got about half of the frames ($PANDA, heartbeats),
// and steering commands from both programs got mixed together.
//
// RULES:
//   - the module talks to ONE sender: it answers only it and executes only its frames (including steering commands and RTCM);
//     frames from other senders are ignored, but the senders stay on the list (panel: Settings -> AgOpenWeb senders);
//   - AUTOMATIC mode (default): the first sender from which a frame arrived is selected. It stays selected as long as it
//     talks; when it is silent for longer than ACTIVE_MS (e.g. AgOpenWeb closed or the computer got a new address from DHCP), the module switches to
//     the sender from which the next frame arrives;
//   - MANUAL mode: the address pinned from the panel is selected (stored in the module's memory). The module listens only to it - also when it is
//     briefly silent (no other device quietly takes over steering). Return to automatic mode: clearPin().
#include <cstdint>

class AgioSenders {
public:
    static const uint8_t MAX = 8;               // this many distinct senders are remembered
    static const uint32_t ACTIVE_MS = 5000;     // a sender is "active" if its frame arrived within this time (like the old 5 s in pgn_messages.h)

    struct Entry {
        uint32_t ip;          // 0 = free entry
        uint32_t frames;      // frames from this sender (accepted and ignored)
        uint32_t ignored;     // of these ignored (the sender was not selected)
        uint32_t lastMs;      // time of the last frame (millis), valid only when seen
        bool seen;            // false = an entry added from the panel (pinned address) from which nothing has arrived yet
    };

    static Entry table[MAX];
    static volatile uint32_t sel;               // who we talk to; 0 = nobody yet
    static volatile uint32_t pin;               // manually pinned address; 0 = automatic mode
    static uint32_t switches;                   // how many times the selected sender changed (excluding the first selection)
    static uint32_t ignoredTotal;               // how many frames were ignored in total (foreign senders)

    // Called for EVERY valid frame with an AgOpenWeb header. Returns true when the frame is from the sender we talk to
    // (it is to be executed); false = a foreign sender, skip the frame.
    static bool note(uint32_t ip, uint32_t now) {
        if (ip == 0) return false;
        int i = find(ip);
        if (i < 0) i = alloc(ip);
        if (i < 0) return false;                // impossible in practice (only 2 entries of MAX are protected), but without memory risk

        uint32_t p = pin;
        if (p != 0) {
            if (sel != p) switchTo(p);
        } else if (sel != ip && (sel == 0 || !isActive(sel, now))) {
            switchTo(ip);                       // nobody was selected or the selected one went silent - we take whoever is talking
        }

        bool mine = (ip == sel);
        Entry& e = table[i];
        e.frames++;
        e.lastMs = now;
        e.seen = true;
        if (!mine) { e.ignored++; ignoredTotal++; }
        return mine;
    }

    // Manual selection from the panel: from now on we talk exclusively to this address (also when it is briefly silent).
    static void setPinned(uint32_t ip) {
        if (ip == 0) { pin = 0; return; }
        if (find(ip) < 0) alloc(ip);            // the entry is visible on the list even before anything arrives
        pin = ip;
        if (sel != ip) switchTo(ip);
    }
    // Return to automatic mode: we stay with the current sender as long as it talks.
    static void clearPin() { pin = 0; }

    static bool isManual() { return pin != 0; }
    static uint32_t selected() { return sel; }

    // Time since the last frame, never negative. The panel reads `now` on the web task (core 0) while the main loop (core 1) may
    // store a frame with a LATER timestamp a moment afterwards; the plain unsigned difference then wraps to ~4.29e9, which as a
    // signed value is -1 - the panel showed "last frame has not arrived yet" / "selected but silent" for one refresh.
    static uint32_t sinceMs(uint32_t now, uint32_t last) {
        int32_t d = (int32_t)(now - last);
        return d < 0 ? 0u : (uint32_t)d;
    }

    // Whether the sender is still talking (a frame within ACTIVE_MS).
    static bool isActive(uint32_t ip, uint32_t now) {
        int i = find(ip);
        return i >= 0 && table[i].seen && sinceMs(now, table[i].lastMs) <= ACTIVE_MS;
    }
    static bool selectedActive(uint32_t now) { return sel != 0 && isActive(sel, now); }

    // For the panel: the number of entries (order = order of addition to the table) and the age of the last frame (-1 = never).
    static uint8_t count() { uint8_t n = 0; for (uint8_t i = 0; i < MAX; i++) if (table[i].ip != 0) n++; return n; }
    static const Entry* nth(uint8_t n) {
        for (uint8_t i = 0; i < MAX; i++) if (table[i].ip != 0 && n-- == 0) return &table[i];
        return nullptr;
    }
    static long ageMs(const Entry& e, uint32_t now) { return e.seen ? (long)sinceMs(now, e.lastMs) : -1L; }

    // Tests and factory reset only.
    static void reset() {
        for (uint8_t i = 0; i < MAX; i++) table[i] = Entry{0, 0, 0, 0, false};
        sel = 0; pin = 0; switches = 0; ignoredTotal = 0;
    }

private:
    static int find(uint32_t ip) {
        for (uint8_t i = 0; i < MAX; i++) if (table[i].ip == ip) return i;
        return -1;
    }
    // A free entry; when none, we evict the longest-silent sender, but never the selected or pinned one.
    static int alloc(uint32_t ip) {
        int slot = -1;
        for (uint8_t i = 0; i < MAX; i++) if (table[i].ip == 0) { slot = i; break; }
        if (slot < 0) {
            // The longest-silent = the largest "age" relative to the freshest entry (no clock: a relative comparison, immune to millis() wraparound).
            uint32_t newest = 0; bool have = false;
            for (uint8_t i = 0; i < MAX; i++) if (table[i].seen) { if (!have || (int32_t)(table[i].lastMs - newest) > 0) newest = table[i].lastMs; have = true; }
            uint32_t bestAge = 0; bool found = false;
            for (uint8_t i = 0; i < MAX; i++) {
                if (table[i].ip == sel || table[i].ip == pin) continue;
                uint32_t age = table[i].seen ? (uint32_t)(newest - table[i].lastMs) : 0xFFFFFFFFu;   // never heard = the longest-silent
                if (!found || age > bestAge) { bestAge = age; slot = i; found = true; }
            }
            if (slot < 0) return -1;
        }
        table[slot] = Entry{ip, 0, 0, 0, false};
        return slot;
    }
    static void switchTo(uint32_t ip) {
        if (sel != 0 && sel != ip) switches++;
        sel = ip;
    }
};

AgioSenders::Entry AgioSenders::table[AgioSenders::MAX];
volatile uint32_t AgioSenders::sel = 0;
volatile uint32_t AgioSenders::pin = 0;
uint32_t AgioSenders::switches = 0;
uint32_t AgioSenders::ignoredTotal = 0;
