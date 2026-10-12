#pragma once
// ======================================================================
// tests/mocks/Arduino.h
// EN: Minimal host stand-in for the Arduino/ESP32 core, just enough to compile firmware headers that
//     include <Arduino.h> (config.h, log_buffer.h, pgn_receivers.h) in the PC tests. A controllable millis().
// PL: Minimalny zamiennik rdzenia Arduino/ESP32 na komputerze - tyle, by skompilowac naglowki firmware'u
//     z <Arduino.h> (config.h, log_buffer.h, pgn_receivers.h) w testach. Sterowalny millis().
// ======================================================================
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>

inline unsigned long g_mockMillis = 1000;                 // tests set this to move time
inline unsigned long millis() { return g_mockMillis; }
inline void mockAdvance(unsigned long ms) { g_mockMillis += ms; }

// A String that behaves like Arduino's for the few calls the headers use.
class String {
public:
    std::string s;
    String() {}
    String(const char* c) : s(c ? c : "") {}
    const char* c_str() const { return s.c_str(); }
    void reserve(size_t n) { s.reserve(n); }
    void concat(const char* p, size_t n) { s.append(p, n); }
    size_t length() const { return s.size(); }
    String& operator+=(const char* c) { s += c; return *this; }
};

struct MockSerial {
    int availableForWrite() { return 0; }                 // 0 = never write to the console in tests
    void print(const char*) {}
    void println(const char*) {}
};
inline MockSerial Serial;

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
