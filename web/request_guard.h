#pragma once
// ======================================================================
// web/request_guard.h
// EN: Pure decisions for the web server: cross-origin (CSRF) refusal and who may flash firmware over /update.
// PL: Czyste decyzje serwera WWW: odrzucanie żądań z obcej strony (CSRF) i kto może wgrywać firmware przez /update.
// Tests: tests/test_request_guard.cpp (host, plain C++).
// Used by: web/web_server.h
// ======================================================================
#include <cstring>
#include <cctype>
#include <cstddef>

namespace RequestGuard {

    // Cross-site request forgery: a page opened from another site can make the user's browser POST to the module
    // (e.g. /restart) while the panel is NOT protected by a PIN. Browsers always add an `Origin` header to such a POST,
    // and it names the OTHER site. So: no Origin (a script, curl, an old browser) = allowed; Origin whose host:port
    // equals the Host header the request was sent to = allowed (the panel itself); anything else (including "null") = refused.
    inline bool sameOrigin(const char* origin, const char* host) {
        if (!origin || !origin[0]) return true;
        const char* p = origin;
        if (strncasecmp(p, "http://", 7) == 0) p += 7;
        else if (strncasecmp(p, "https://", 8) == 0) p += 8;
        else return false;                       // "null", "file://", anything unknown
        if (!host || !host[0]) return false;
        size_t n = strlen(p);
        while (n > 0 && p[n - 1] == '/') n--;    // a stray trailing slash
        return n == strlen(host) && strncasecmp(p, host, n) == 0;
    }

    // DNS-rebinding guard: a page from an attacker's domain whose name was re-pointed at the module's IP sends a request
    // whose Origin and Host are BOTH that domain, so sameOrigin() passes. The panel is only ever reached by an IP address,
    // "<hostname>.local" (mDNS), the bare hostname or localhost - any other Host name is refused. No Host header at all
    // (a script, HTTP/1.0) is allowed: a browser always sends one.
    inline bool hostAllowed(const char* host, const char* hostname) {
        if (!host || !host[0]) return true;
        char h[80]; size_t n = 0;
        if (host[0] == '[') return true;                        // an IPv6 literal: also an address, not a name
        while (host[n] && host[n] != ':' && n < sizeof(h) - 1) { h[n] = host[n]; n++; }
        if (host[n] && host[n] != ':') return false;            // longer than any legitimate name
        h[n] = 0;
        if (n == 0) return false;
        bool ip = true;
        for (size_t i = 0; i < n; i++) if (!((h[i] >= '0' && h[i] <= '9') || h[i] == '.')) { ip = false; break; }
        if (ip) return true;
        if (strcasecmp(h, "localhost") == 0) return true;
        if (hostname && hostname[0] && strcasecmp(h, hostname) == 0) return true;
        if (n > 6 && strcasecmp(h + n - 6, ".local") == 0) {    // <name>.local, a single label
            for (size_t i = 0; i < n - 6; i++) if (h[i] == '.') return false;
            return true;
        }
        return false;
    }

    // Who may upload firmware to /update:
    //   - an OTA password is set (secrets.h)  -> only a correct login (HTTP Basic, user "admin");
    //   - no OTA password, panel PIN/password -> only a valid panel token (the same login as for settings);
    //   - neither (the owner left the panel open on purpose) -> open, as for every other setting.
    inline bool otaAllowed(bool otaPasswordSet, bool basicOk, bool panelProtected, bool tokenOk) {
        if (otaPasswordSet) return basicOk;
        if (panelProtected) return tokenOk;
        return true;
    }
}
