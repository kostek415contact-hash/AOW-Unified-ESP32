#pragma once
// ======================================================================
// web/json_escape.h
// EN: Escaping of one character for a JSON string, so user-typed texts (SSIDs, NTRIP host/user...) cannot break /status.
// PL: Escapowanie jednego znaku do tekstu JSON, żeby teksty wpisane przez użytkownika (SSID, host/login NTRIP...) nie psuły /status.
// Tests: tests/test_json_escape.cpp (host, plain C++).
// Used by: web/web_server.h
// ======================================================================
#include <cstddef>
#include <cstdint>

// A quote or a backslash inside an SSID (e.g. Jan's "Net") used to make the whole /status response invalid JSON: the panel then
// failed to parse it, showed "no connection" for good and could not even open Settings to fix the text. Rule: every string that a
// user can type goes through escapeChar() when it is written into the JSON.
namespace JsonEscape {
    // Writes the JSON form of byte c into out (at least 7 bytes) and returns how many bytes were written (1-6). Not NUL-terminated.
    // Bytes >= 0x80 pass through unchanged (UTF-8 sequences stay intact).
    inline size_t escapeChar(uint8_t c, char* out) {
        static const char HEX_DIGITS[] = "0123456789abcdef";
        switch (c) {
            case '"':  out[0] = '\\'; out[1] = '"';  return 2;
            case '\\': out[0] = '\\'; out[1] = '\\'; return 2;
            case '\n': out[0] = '\\'; out[1] = 'n';  return 2;
            case '\r': out[0] = '\\'; out[1] = 'r';  return 2;
            case '\t': out[0] = '\\'; out[1] = 't';  return 2;
            default: break;
        }
        if (c < 0x20 || c == 0x7F) {          // other control characters: \u00XX
            out[0] = '\\'; out[1] = 'u'; out[2] = '0'; out[3] = '0';
            out[4] = HEX_DIGITS[c >> 4]; out[5] = HEX_DIGITS[c & 0x0F];
            return 6;
        }
        out[0] = (char)c;
        return 1;
    }
}
