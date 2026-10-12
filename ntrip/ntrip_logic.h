#pragma once
// ======================================================================
// ntrip/ntrip_logic.h
// EN: Pure NTRIP logic: caster reply classification (OK, redirect, bad credentials, source table) and retry
//     back-off.
// PL: Czysta logika NTRIP: rozpoznawanie odpowiedzi castera (OK, przekierowanie, złe hasło, tablica źródeł) i
//     narastające przerwy między próbami.
// Tests: tests/test_ntrip_logic.cpp (host, plain C++).
// Used by: ntrip/ntrip_client.h
// ======================================================================
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>

// Pure NTRIP client logic (no Arduino, tests: tests/test_ntrip_logic.cpp):
//  1) NtripReply::parse - recognizing the caster's reply (OK, redirect with Location, wrong password, wrong mountpoint,
//     a source table instead of a stream),
//  2) NtripBackoff - growing pauses between attempts, so the caster is not flooded and the account is not locked by a wrong password.
namespace NtripLogic {

enum Kind : uint8_t {
    NR_PENDING = 0,    // reply not complete yet
    NR_OK,             // 200 / ICY 200 OK - RTCM stream
    NR_REDIRECT,       // 301/302/303/307/308 with Location http://host[:port]/...
    NR_AUTH,           // 401 / 403 - wrong credentials (do not retry quickly!)
    NR_NOT_FOUND,      // 404 - no such mountpoint
    NR_SOURCETABLE,    // "SOURCETABLE 200 OK" - the caster returns a list instead of a stream = wrong mountpoint
    NR_OTHER           // anything else (including a redirect to https, which we do not support)
};

struct Reply {
    Kind kind = NR_PENDING;
    int code = 0;
    char host[96] = {0};      // NR_REDIRECT only
    uint16_t port = 0;        // NR_REDIRECT only
};

// buf = bytes received so far (need not end with a zero). Decision:
//  - SOURCETABLE / 401 / 403 / 404 are decided right after the first line,
//  - OK and redirect require complete headers (an empty line), as before.
inline Reply parse(const char* buf, size_t len) {
    Reply r;
    size_t eol = len;
    for (size_t i = 0; i + 1 < len; i++) if (buf[i] == '\r' && buf[i + 1] == '\n') { eol = i; break; }
    if (eol == len) return r;                                   // first line incomplete
    auto startsWith = [&](const char* p) { size_t n = strlen(p); return eol >= n && memcmp(buf, p, n) == 0; };

    if (startsWith("SOURCETABLE 200")) { r.kind = NR_SOURCETABLE; r.code = 200; return r; }

    int code = 0;
    if (startsWith("ICY ") || startsWith("HTTP/1.0 ") || startsWith("HTTP/1.1 ")) {
        size_t p = (buf[0] == 'I') ? 4 : 9;
        while (p < eol && buf[p] >= '0' && buf[p] <= '9' && code < 1000) { code = code * 10 + (buf[p] - '0'); p++; }
    }
    r.code = code;
    if (code == 401 || code == 403) { r.kind = NR_AUTH; return r; }
    if (code == 404) { r.kind = NR_NOT_FOUND; return r; }

    bool headersDone = false;
    for (size_t i = 0; i + 3 < len; i++)
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') { headersDone = true; break; }
    if (!headersDone) return r;                                 // NR_PENDING

    if (code == 200) { r.kind = NR_OK; return r; }
    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
        // look for the Location header (case-insensitive)
        for (size_t i = eol + 2; i + 9 < len; i++) {
            if ((i == eol + 2 || buf[i - 1] == '\n') && strncasecmp(buf + i, "Location:", 9) == 0) {
                size_t j = i + 9;
                while (j < len && buf[j] == ' ') j++;
                size_t e = j;
                while (e < len && buf[e] != '\r' && buf[e] != '\n') e++;
                if (e - j > 7 && strncasecmp(buf + j, "http://", 7) == 0) {
                    size_t h = j + 7, he = h;
                    while (he < e && buf[he] != ':' && buf[he] != '/') he++;
                    if (he > h && he - h < sizeof(r.host)) {
                        memcpy(r.host, buf + h, he - h); r.host[he - h] = 0;
                        uint32_t port = 80;
                        if (he < e && buf[he] == ':') {
                            port = 0; size_t q = he + 1;
                            while (q < e && buf[q] >= '0' && buf[q] <= '9' && port < 100000) { port = port * 10 + (buf[q] - '0'); q++; }
                            if (q == he + 1) port = 0;
                        }
                        if (port > 0 && port <= 65535) { r.port = (uint16_t)port; r.kind = NR_REDIRECT; return r; }
                    }
                }
                break;      // https, brak hosta, zly port
            }
        }
    }
    r.kind = NR_OTHER;
    return r;
}

// Growing pauses between attempts. Connection error/drop: 5, 10, 20, 40, 80, 120 s (limit). Wrong credentials:
// 120 s, and from the second time 300 s (RTK services can lock the account after a series of bad logins). A wrong mountpoint
// and other replies: 60 s. A series counts as successful when the stream flowed for at least STABLE_MS.
class Backoff {
public:
    static constexpr uint32_t STABLE_MS = 30000;
    static constexpr uint32_t CAP_MS = 120000;

    void onFailure(Kind k) { kind_ = k; if (fails_ < 250) fails_++; }          // k: NR_PENDING = an ordinary network error
    void onStreamEnded(uint32_t streamedMs) {
        if (streamedMs >= STABLE_MS) fails_ = 0; else if (fails_ < 250) fails_++;
        kind_ = NR_PENDING;
    }
    void reset() { fails_ = 0; kind_ = NR_PENDING; }

    uint32_t delayMs() const {
        uint8_t f = fails_ ? fails_ - 1 : 0;                                    // 0 failures = 0 earlier ones
        switch (kind_) {
            case NR_AUTH: return f == 0 ? 120000UL : 300000UL;
            case NR_NOT_FOUND: case NR_SOURCETABLE: case NR_OTHER: return 60000UL;
            default: { uint32_t d = 5000UL << (f > 5 ? 5 : f); return d > CAP_MS ? CAP_MS : d; }
        }
    }
    uint8_t fails() const { return fails_; }
    Kind lastKind() const { return kind_; }
private:
    uint8_t fails_ = 0;
    Kind kind_ = NR_PENDING;
};


// Detects a "dead" stream: the TCP connection looks open (no FIN/RST - e.g. a cut mobile data link), but no RTCM has
// arrived for STALL_MS. Casters send at least one message per second, so 20 s of silence means the link is gone.
// The caller then closes the socket and reconnects.
class StallWatch {
public:
    static constexpr uint32_t STALL_MS = 20000;
    void start(uint32_t now) { lastData_ = now; }                 // the stream has just begun
    void onData(uint32_t now) { lastData_ = now; }
    bool stalled(uint32_t now) const { return (uint32_t)(now - lastData_) >= STALL_MS; }
private:
    uint32_t lastData_ = 0;
};

}  // namespace NtripLogic
