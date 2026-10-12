// Fuzz + property tests of the text parsers (NMEA sentences, NTRIP replies) and of the motor math (PwmMath, RunawayGuard).
// Exactly-sized buffers: with SANITIZE=1 bash tools/run_tests.sh any over-read or UB is an error.
//     g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined tests/test_fuzz_parsers.cpp -o /tmp/tfp && /tmp/tfp
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include "../sensors/nmea.h"
#include "../ntrip/ntrip_logic.h"
#include "../actuators/pwm_math.h"
#include "../actuators/runaway_guard.h"
static int g_ok = 0, g_all = 0;
#define CHECK(c) do { g_all++; if (c) g_ok++; else std::printf("BLAD (linia %d): %s\n", __LINE__, #c); } while (0)

static uint32_t rs = 0x9E3779B9u;
static uint32_t rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static float rndf(float lo, float hi) { return lo + (hi - lo) * (float)(rnd() % 100001) / 100000.0f; }

// A plausible sentence with random fields; then possibly damaged.
static std::string sentence() {
    static const char* HEAD[] = {"GNGGA", "GPGGA", "GNVTG", "GNRMC", "GNGST", "PANDA", "XXXXX", ""};
    static const char ALPHA[] = "0123456789.,-+NSEW MAx*$\r\n\x01\xff";
    std::string body = HEAD[rnd() % 8];
    int nf = (int)(rnd() % 20);
    for (int i = 0; i < nf; i++) {
        body += ',';
        int fl = (int)(rnd() % 14);
        for (int j = 0; j < fl; j++) body += ALPHA[rnd() % (sizeof(ALPHA) - 1)];
    }
    uint8_t cs = Nmea::checksum(body.c_str(), (uint8_t)(body.size() > 255 ? 255 : body.size()));
    char tail[8]; snprintf(tail, sizeof(tail), "*%02X", cs);
    std::string s = "$" + body + tail;
    switch (rnd() % 5) {
        case 1: if (!s.empty()) s[rnd() % s.size()] ^= (char)(1u << (rnd() % 8)); break;
        case 2: s.resize(rnd() % (s.size() + 1)); break;
        case 3: s += std::string(rnd() % 300, 'A'); break;
        default: break;
    }
    return s;
}

int main() {
    // ---- NMEA: any text must be handled without an over-read, however it is cut ----
    int accepted = 0;
    for (int it = 0; it < 100000; it++) {
        std::string s = sentence();
        char* line = new char[s.size() + 1]; memcpy(line, s.c_str(), s.size() + 1);   // exactly sized, NUL-terminated
        if (Nmea::validateLine(line)) {
            accepted++;
            char* f[24]; uint8_t n = Nmea::splitFields(line, f, 24);
            for (uint8_t i = n; i < 24; i++) f[i] = line + s.size();   // unused slots point at an empty string, as the firmware's callers guard with count
            Nmea::Gga g{}; Nmea::Vtg v{}; Nmea::Gst gs{}; Nmea::Rmc r{};
            if (f[0][0] == 'G' || f[0][0] == 'P') {
                Nmea::parseGga(f, n, g); Nmea::parseVtg(f, n, v); Nmea::parseGst(f, n, gs); Nmea::parseRmc(f, n, r);
            }
        }
        delete[] line;
    }
    CHECK(accepted > 5000);   // the generator really produced many valid checksums, not only garbage

    // ---- NTRIP: random replies, cut at every length ----
    static const char* REPLIES[] = {
        "ICY 200 OK\r\n\r\n", "HTTP/1.1 200 OK\r\nContent-Type: gnss/data\r\n\r\n", "HTTP/1.1 401 Unauthorized\r\n\r\n",
        "HTTP/1.1 404 Not Found\r\n\r\n", "SOURCETABLE 200 OK\r\nServer: x\r\n", "HTTP/1.1 302 Found\r\nLocation: http://host.example:2101/MOUNT\r\n\r\n",
        "HTTP/1.1 301 Moved\r\nLocation: http://", "HTTP/1.0 307 T\r\nLocation: https://secure.example/x\r\n\r\n"};
    bool hostOk = true;
    for (int it = 0; it < 60000; it++) {
        std::string s = REPLIES[rnd() % 8];
        if (rnd() % 3 == 0 && !s.empty()) s[rnd() % s.size()] = (char)rnd();
        if (rnd() % 4 == 0) s += std::string(rnd() % 400, (char)('a' + rnd() % 26));            // a very long Location / header
        if (rnd() % 5 == 0) s.insert(s.find("Location: ") == std::string::npos ? 0 : s.find("Location: ") + 10, std::string(rnd() % 300, 'h'));
        size_t cut = rnd() % (s.size() + 1);
        char* buf = new char[cut ? cut : 1]; if (cut) memcpy(buf, s.data(), cut);
        NtripLogic::Reply rp = NtripLogic::parse(buf, cut);
        if (strnlen(rp.host, sizeof(rp.host)) >= sizeof(rp.host)) hostOk = false;   // never an unterminated host
        delete[] buf;
    }
    CHECK(hostOk);

    // ---- PwmMath: whatever the inputs, |pwm| <= highPwm <= 255, finite error/gain only ----
    bool pwmOk = true;
    for (int it = 0; it < 200000; it++) {
        float e = (rnd() % 20 == 0) ? NAN : (rnd() % 20 == 0 ? INFINITY : rndf(-400, 400));
        float gp = (rnd() % 30 == 0) ? NAN : rndf(-5, 300);
        int hi = (int)(rnd() % 600) - 100, mn = (int)(rnd() % 600) - 100;
        int p = PwmMath::compute(e, gp, hi, mn);
        int cap = hi < 0 ? 0 : (hi > 255 ? 255 : hi);
        if (std::abs(p) > cap) pwmOk = false;
        if ((std::isnan(e) || std::isinf(e) || std::isnan(gp)) && p != 0) pwmOk = false;
        if (PwmMath::displayByte(p) != (uint8_t)std::abs(p)) pwmOk = false;
    }
    CHECK(pwmOk);

    // ---- RunawayGuard: random drive histories never crash, and a tripped latch never un-trips by itself while "engaged" ----
    bool latchOk = true;
    for (int run = 0; run < 2000; run++) {
        RunawayGuard g; g.configure({true, 1000, 1.0f, 3.0f});
        uint32_t t = rnd(); float angle = rndf(-40, 40); bool wasTripped = false;
        for (int i = 0; i < 300; i++) {
            t += 10; int pwm = (rnd() % 5 == 0) ? 0 : (int)(rnd() % 241) - 120;
            float err = (rnd() % 50 == 0) ? NAN : rndf(-30, 30);
            angle += (rnd() % 3 == 0) ? rndf(-2, 2) : 0.0f;
            g.update(t, rnd() % 8 != 0, pwm, err, angle, (rnd() % 4) ? (uint32_t)(i + 1) : 0, (int16_t)(rnd() % 3));
            g.tick(t, true);                                   // steering stays "engaged": the latch must hold
            if (wasTripped && !g.isTripped()) latchOk = false;
            wasTripped = g.isTripped();
        }
    }
    CHECK(latchOk);

    std::printf("WYNIK: %d/%d kontroli OK\n", g_ok, g_all);
    return g_ok == g_all ? 0 : 1;
}
