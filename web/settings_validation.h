#pragma once
// ======================================================================
// web/settings_validation.h
// EN: Validation of settings coming from the panel or NVS; returns nullptr when valid, otherwise a short MSG()
//     error.
// PL: Walidacja ustawień z panelu i z pamięci NVS; zwraca nullptr, gdy poprawne, albo krótki błąd MSG().
// Used by: web/runtime_settings.h, web/web_server.h
// Depends on: msg.h
// ======================================================================
#include "msg.h"
// Validation of settings from the web panel and values loaded from memory (NVS).
// Pure C++ with no Arduino dependency - so it is host-tested.
// All functions take const char* (for an Arduino String: .c_str()).
// The validate...() functions return nullptr when the data is valid, or a short error
// message (ASCII, no diacritics - like the rest of the firmware messages) which the panel shows to the user.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cerrno>

namespace SettingsValidation {

    inline bool hasControlChars(const char* s) {
        for (; *s; ++s) if ((unsigned char)*s < 0x20 || (unsigned char)*s == 0x7F) return true;
        return false;
    }

    // A whole integer (surrounding spaces allowed, trailing garbage not), in the range [lo, hi].
    inline bool parseLong(const char* s, long lo, long hi, long* out) {
        if (!s) return false;
        while (*s == ' ') ++s;
        if (!*s) return false;
        char* end = nullptr;
        errno = 0;
        long v = strtol(s, &end, 10);
        if (end == s || errno == ERANGE) return false;
        while (*end == ' ') ++end;
        if (*end) return false;
        if (v < lo || v > hi) return false;
        *out = v;
        return true;
    }

    // A finite floating-point number (decimal point) in the range [lo, hi].
    // Rejects "nan", "inf", hexadecimal notation and anything that is not a plain number.
    inline bool parseFloatRange(const char* s, float lo, float hi, float* out) {
        if (!s) return false;
        while (*s == ' ') ++s;
        if (!*s) return false;
        for (const char* p = s; *p; ++p) {
            char c = *p;
            if (!((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E' || c == ' ')) return false;
        }
        char* end = nullptr;
        errno = 0;
        double v = strtod(s, &end);
        if (end == s || errno == ERANGE) return false;
        while (*end == ' ') ++end;
        if (*end) return false;
        // Compared in float (like the bounds): an exact boundary value, e.g. "0.2" with lo = 0.2f, would be rejected in double
        // (0.2 < 0.2f widened to double), although the user entered exactly the allowed minimum.
        if (!std::isfinite(v) || (float)v < lo || (float)v > hi) return false;
        *out = (float)v;
        return true;
    }

    inline bool validSsid(const char* s) {
        size_t n = strlen(s);
        return n >= 1 && n <= 32 && !hasControlChars(s);
    }
    // WiFi network password: empty (open network) or 8-63 characters - anything shorter is never valid for WPA/WPA2.
    inline bool validWifiPass(const char* s) {
        size_t n = strlen(s);
        return (n == 0 || (n >= 8 && n <= 63)) && !hasControlChars(s);
    }
    inline bool validApPass(const char* s) {
        size_t n = strlen(s);
        return n >= 8 && n <= 63 && !hasControlChars(s);
    }

    // Strict a.b.c.d notation (4 numbers 0-255 separated by dots, no spaces or other characters).
    inline bool parseIpv4(const char* s, uint32_t* out) {
        if (!s) return false;
        uint32_t value = 0;
        int octets = 0;
        const char* p = s;
        while (true) {
            if (*p < '0' || *p > '9') return false;
            int digits = 0;
            unsigned v = 0;
            while (*p >= '0' && *p <= '9') { v = v * 10 + (unsigned)(*p - '0'); ++p; if (++digits > 3) return false; }
            if (v > 255) return false;
            value = (value << 8) | v;
            ++octets;
            if (*p == '.') { if (octets == 4) return false; ++p; continue; }
            if (*p == '\0') break;
            return false;
        }
        if (octets != 4) return false;
        *out = value;
        return true;
    }
    // Subnet mask: a run of ones, then zeros (e.g. 255.255.255.0), not 0.0.0.0.
    inline bool validNetmask(const char* s) {
        uint32_t m;
        if (!parseIpv4(s, &m) || m == 0) return false;
        uint32_t inv = ~m;
        return (inv & (inv + 1)) == 0;
    }

    // Sanity of the static address plan. Each value alone may be a well-formed IPv4 and still make the module unreachable:
    //  - the module address must be a normal unicast host address: not 0.x, 127.x, 224+ (multicast/reserved), not the network or
    //    broadcast address of its own subnet;
    //  - the gateway must lie in the same subnet and differ from the module address (else nothing is routed);
    //  - the AgOpenWeb address is either that subnet's broadcast, 255.255.255.255, or a unicast address (not 0.x, 127.x, 224+ ).
    // Returns nullptr when fine.
    inline bool isUnicastHost(uint32_t ip) {
        const uint32_t a = ip >> 24;
        return a != 0 && a != 127 && a < 224;
    }
    inline const char* validateIpPlan(const char* localIp, const char* gateway, const char* subnet, const char* agioIp) {
        uint32_t ip = 0, gw = 0, mask = 0, agio = 0;
        if (!parseIpv4(localIp, &ip) || !parseIpv4(gateway, &gw) || !validNetmask(subnet) || !parseIpv4(subnet, &mask) || !parseIpv4(agioIp, &agio)) return nullptr;   // the format errors are reported elsewhere
        if (!isUnicastHost(ip)) return MSG("Module IP address: must be an ordinary host address (not 0.x, 127.x, or 224 and above)");
        if (mask != 0xFFFFFFFFu && ((ip & ~mask) == 0 || (ip | mask) == 0xFFFFFFFFu))
            return MSG("Module IP address: cannot be the network or broadcast address of its subnet");
        if ((gw & mask) != (ip & mask)) return MSG("Gateway: must be in the same subnet as the module address");
        if (gw == ip) return MSG("Gateway: cannot be the same as the module address");
        if (agio != 0xFFFFFFFFu && !isUnicastHost(agio)) return MSG("AgOpenWeb address: use a broadcast (e.g. 192.168.1.255) or the PC address");
        return nullptr;
    }

    // ---------- individual forms ----------
    inline const char* validateNetwork(const char* ssid1, const char* pass1,
                                       const char* ssid2, const char* pass2,
                                       const char* ssid3, const char* pass3,
                                       const char* ssid4, const char* pass4,
                                       const char* apSsid, const char* apPass,
                                       const char* localIp, const char* gateway,
                                       const char* subnet, const char* agioIp) {
        static const char* const BAD_SSID[4]  = {MSG("WiFi network 1 SSID: 1-32 characters required"), MSG("WiFi network 2 SSID: max. 32 characters"),
            MSG("WiFi network 3 SSID: max. 32 characters"), MSG("WiFi network 4 SSID: max. 32 characters")};
            static const char* const BAD_PASS[4]  = {MSG("WiFi network 1 password: empty (open network) or 8-63 characters"), MSG("WiFi network 2 password: empty or 8-63 characters"),
                MSG("WiFi network 3 password: empty or 8-63 characters"), MSG("WiFi network 4 password: empty or 8-63 characters")};
                const char* ssid[4] = {ssid1, ssid2, ssid3, ssid4};
                const char* pass[4] = {pass1, pass2, pass3, pass4};
                if (!validSsid(ssid[0])) return BAD_SSID[0];
                if (!validWifiPass(pass[0])) return BAD_PASS[0];
                for (int i = 1; i < 4; i++) {
                    if (ssid[i][0] == '\0') continue;                 // empty slot = unused
                    if (!validSsid(ssid[i])) return BAD_SSID[i];
                    if (!validWifiPass(pass[i])) return BAD_PASS[i];
                }
                if (!validSsid(apSsid)) return MSG("AP SSID: 1-32 characters required");
                if (!validApPass(apPass)) return MSG("AP password: 8-63 characters (WPA2 requirement) - a shorter password prevents the access point from starting");
                uint32_t tmp;
        if (!parseIpv4(localIp, &tmp)) return MSG("Module IP address: invalid (e.g. 192.168.1.120)");
        if (!parseIpv4(gateway, &tmp)) return MSG("Gateway: invalid IP address");
        if (!validNetmask(subnet)) return MSG("Subnet mask: invalid (e.g. 255.255.255.0)");
        if (!parseIpv4(agioIp, &tmp)) return MSG("AgOpenWeb broadcast address: invalid IP address");
        return validateIpPlan(localIp, gateway, subnet, agioIp);
                                       }

                                       inline const char* validateNtrip(const char* host, const char* port, const char* mount,
                                                                        const char* user, const char* pass, uint16_t* portOut) {
                                           size_t hl = strlen(host);
                                           if (hl < 1 || hl > 80) return MSG("NTRIP host: 1-80 characters required");
                                           if (hasControlChars(host) || strchr(host, ' ')) return MSG("NTRIP host: forbidden characters (spaces, control characters)");
                                           long p;
                                           if (!parseLong(port, 1, 65535, &p)) return MSG("NTRIP port: a number from 1 to 65535");
                                           if (strlen(mount) > 60 || hasControlChars(mount) || strchr(mount, ' ')) return MSG("Mountpoint: max. 60 characters, no spaces or control characters");
                                           if (strlen(user) > 60 || hasControlChars(user) || strchr(user, ':')) return MSG("NTRIP user: max. 60 characters, no colon or control characters");
                                           if (strlen(pass) > 60 || hasControlChars(pass)) return MSG("NTRIP password: max. 60 characters, no control characters");
                                           *portOut = (uint16_t)p;
                                           return nullptr;
                                                                        }

                                                                        struct Thresholds { unsigned long ethGraceMs; unsigned long wifiTimeoutMs; uint32_t watchdogS; uint8_t ledBrightness; };
                                                                        inline const char* validateThresholds(const char* ethGraceMs, const char* wifiTimeoutMs,
                                                                                                              const char* watchdogS, const char* ledBrightness, Thresholds* out) {
                                                                            long v;
                                                                            if (!parseLong(ethGraceMs, 1000, 60000, &v)) return MSG("Ethernet wait time: a number 1000-60000 ms");
                                                                            out->ethGraceMs = (unsigned long)v;
                                                                            if (!parseLong(wifiTimeoutMs, 3000, 120000, &v)) return MSG("WiFi timeout: a number 3000-120000 ms");
                                                                            out->wifiTimeoutMs = (unsigned long)v;
                                                                            if (!parseLong(watchdogS, 3, 300, &v)) return MSG("Watchdog: a number 3-300 s (0 or too small causes a restart loop)");
                                                                            out->watchdogS = (uint32_t)v;
                                                                            if (!parseLong(ledBrightness, 0, 255, &v)) return MSG("LED brightness: a number 0-255");
                                                                            out->ledBrightness = (uint8_t)v;
                                                                            return nullptr;
                                                                                                              }

                                                                                                              inline const char* validateSteeringRange(const char* leftDeg, const char* rightDeg, float* l, float* r) {
                                                                                                                  if (!parseFloatRange(leftDeg, -90.0f, 90.0f, l)) return MSG("LEFT angle: a number in the range -90...90 degrees");
                                                                                                                  if (!parseFloatRange(rightDeg, -90.0f, 90.0f, r)) return MSG("RIGHT angle: a number in the range -90...90 degrees");
                                                                                                                  if (std::fabs(*r - *l) < 5.0f) return MSG("LEFT and RIGHT angles must differ by at least 5 degrees");
                                                                                                                  return nullptr;
                                                                                                              }

                                                                                                              // Address of the manually selected AgOpenWeb sender. Empty = automatic mode (valid). We reject 0.x.x.x, 127.x.x.x and 224.x.x.x and above
                                                                                                              // (multicast, broadcast) - that is never the address of an AgOpenWeb device.
                                                                                                              inline const char* validateAgioSender(const char* ip) {
                                                                                                                  if (!ip) return MSG("AgOpenWeb sender address: no value");
                                                                                                                  if (ip[0] == '\0') return nullptr;
                                                                                                                  uint32_t v;
                                                                                                                  if (!parseIpv4(ip, &v)) return MSG("AgOpenWeb sender address: invalid IP address (e.g. 192.168.1.17)");
                                                                                                                  uint32_t first = v >> 24;
                                                                                                                  if (first == 0 || first == 127 || first >= 224) return MSG("AgOpenWeb sender address: not a device address (0.x, 127.x and 224 and above are not allowed)");
                                                                                                                  return nullptr;
                                                                                                              }

                                                                                                              // Mask of the "steering enabled" bits in the PGN 254 status byte: 1 (older AgOpenWeb), 4 (newer), 5 (both).
                                                                                                              inline const char* validateAgioEngageMask(const char* s, uint8_t* out) {
                                                                                                                  if (!s) return MSG("Steering engage reading: no value");
                                                                                                                  if (strcmp(s, "1") == 0) { if (out) *out = 1; return nullptr; }
                                                                                                                  if (strcmp(s, "4") == 0) { if (out) *out = 4; return nullptr; }
                                                                                                                  if (strcmp(s, "5") == 0) { if (out) *out = 5; return nullptr; }
                                                                                                                  return MSG("Steering engage reading: allowed 1 (older AgOpenWeb), 4 (newer) or 5 (both)");
                                                                                                              }

                                                                                                              inline const char* validateImuRotation(const char* rotDeg, float* out) {
                                                                                                                  if (!parseFloatRange(rotDeg, -180.0f, 180.0f, out)) return MSG("IMU board rotation: a number in the range -180...180 degrees");
                                                                                                                  return nullptr;
                                                                                                              }

                                                                                                              // ---------- defence when loading from NVS (values once saved without validation) ----------
                                                                                                              inline uint32_t clampOrDefault(uint32_t v, uint32_t lo, uint32_t hi, uint32_t def) { return (v < lo || v > hi) ? def : v; }
                                                                                                              inline float finiteOrDefault(float v, float lo, float hi, float def) { return (std::isfinite(v) && v >= lo && v <= hi) ? v : def; }
                                                                                                              inline bool steeringRangeOk(float l, float r) {
                                                                                                                  return std::isfinite(l) && std::isfinite(r) && l >= -90.0f && l <= 90.0f && r >= -90.0f && r <= 90.0f && std::fabs(r - l) >= 5.0f;
                                                                                                              }

    // ---------- motor current protection thresholds (Calibration -> Current sensor) ----------
    // The ACS71240-030B3 measures +-30 A, so the hard threshold cannot be higher; the hard threshold must be clearly above the soft one.
    inline bool currentLimitsOk(float kickA, long kickMs, float hardA) {
        return std::isfinite(kickA) && std::isfinite(hardA) && kickA >= 0.5f && kickA <= 28.0f
            && kickMs >= 20 && kickMs <= 5000 && hardA >= kickA + 0.5f && hardA <= 30.0f;
    }
    inline const char* validateCurrent(const char* kickA, const char* kickMs, const char* hardA, float* a, long* ms, float* hard) {
        if (!parseFloatRange(kickA, 0.5f, 28.0f, a)) return MSG("Current threshold (kickout): a number 0.5-28 A");
        if (!parseLong(kickMs, 20, 5000, ms)) return MSG("Kickout time: an integer 20-5000 ms");
        if (!parseFloatRange(hardA, 1.0f, 30.0f, hard)) return MSG("Hard threshold: a number 1-30 A");
        if (*hard < *a + 0.5f) return MSG("The hard threshold must be at least 0.5 A above the kickout threshold");
        return nullptr;
    }

    // ---------- motor: runaway detector and direction reversal (Calibration -> Motor) ----------
    inline bool runawayParamsOk(long windowMs, float moveDeg, float errDeg) {
        return windowMs >= 300 && windowMs <= 5000 && std::isfinite(moveDeg) && moveDeg >= 0.2f && moveDeg <= 10.0f
            && std::isfinite(errDeg) && errDeg >= 0.5f && errDeg <= 20.0f;
    }
    inline bool parseBool01(const char* s, bool* out) {
        if (!s) return false;
        if (s[0] == '1' && s[1] == '\0') { *out = true; return true; }
        if (s[0] == '0' && s[1] == '\0') { *out = false; return true; }
        return false;
    }
    struct MotorSettings { bool runEn; long windowMs; float moveDeg; float errDeg; bool invert; };
    inline const char* validateMotor(const char* runEn, const char* windowMs, const char* moveDeg, const char* errDeg, const char* invert, MotorSettings* o) {
        if (!parseBool01(runEn, &o->runEn)) return MSG("Runaway detector: value 0 or 1");
        if (!parseLong(windowMs, 300, 5000, &o->windowMs)) return MSG("Detector window: an integer 300-5000 ms");
        if (!parseFloatRange(moveDeg, 0.2f, 10.0f, &o->moveDeg)) return MSG("Minimum movement in the window: a number 0.2-10 degrees");
        if (!parseFloatRange(errDeg, 0.5f, 20.0f, &o->errDeg)) return MSG("Minimum error for evaluation: a number 0.5-20 degrees");
        if (!parseBool01(invert, &o->invert)) return MSG("Reverse direction: value 0 or 1");
        return nullptr;
    }

    // ---------- manual motor test (Calibration -> Motor: Left / Right) ----------
    // dir: "L" (left) or "R" (right); pwm 10-150 out of 255; time 100-1000 ms. The same limits are enforced by MotorDriver::requestTestPulse().
    inline const char* validateMotorTest(const char* dir, const char* pwm, const char* ms, int* dirOut, long* pwmOut, long* msOut) {
        if (!dir || !((dir[0] == 'L' || dir[0] == 'R') && dir[1] == '\0')) return MSG("Test direction: L (left) or R (right)");
        *dirOut = (dir[0] == 'R') ? 1 : -1;
        if (!parseLong(pwm, 10, 150, pwmOut)) return MSG("Test PWM: an integer 10-150");
        if (!parseLong(ms, 100, 1000, msOut)) return MSG("Test time: an integer 100-1000 ms");
        return nullptr;
    }

} // namespace SettingsValidation
