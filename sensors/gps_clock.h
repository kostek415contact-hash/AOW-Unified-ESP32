#pragma once
// ======================================================================
// sensors/gps_clock.h
// EN: UTC clock from the GPS RMC sentence (the module has no RTC/NTP); 0 = unknown.
// PL: Zegar UTC z zdania RMC odbiornika GPS (moduł nie ma RTC ani NTP); 0 = nieznany.
// Tests: tests/test_gps_clock.cpp (host, plain C++).
// Used by: diagnostics/event_sampler.h, sensors/gps_universal.h
// ======================================================================
// UTC clock from the GPS receiver: time (hhmmss[.ss]) and date (ddmmyy) from the RMC sentence -> seconds since 1970 (UTC). The module has no RTC or NTP, and a restart
// loses the uptime, so GPS is the only source of the time of day "from real life" (e.g. exactly when something happened before a restart). Pure C++ - host-tested
// (tests/test_gps_clock.cpp). Needs only stdint/cstring. A value of 0 = time unknown.
#include <cstdint>
#include <cstring>

class GpsClock {
public:
    static const uint32_t MAX_AGE_MS = 6UL * 60UL * 60UL * 1000UL;   // after this many ms without a fresh RMC we stop guessing (millis() drift is small, but we do not trust it forever)
    static const int MIN_YEAR = 2024, MAX_YEAR = 2099;               // receivers without a date return 1980/2000 - that is not a time

    // time: "hhmmss" or "hhmmss.ss"; date: "ddmmyy". Returns true if the field is valid and the clock was set.
    static bool noteRmc(const char* time, const char* date, uint32_t nowMs) {
        int64_t e = toEpoch(time, date);
        if (e <= 0) return false;
        baseEpoch_ = (uint32_t)e; baseMs_ = nowMs; valid_ = true;
        return true;
    }
    // UTC seconds since 1970 "now" (extrapolated from the last RMC) or 0 when unknown / too old.
    static uint32_t epochNow(uint32_t nowMs) {
        if (!valid_) return 0;
        uint32_t age = nowMs - baseMs_;
        if (age > MAX_AGE_MS) return 0;
        return baseEpoch_ + age / 1000UL;
    }
    static void reset() { valid_ = false; baseEpoch_ = 0; baseMs_ = 0; }

    // Conversion "hhmmss[.ss]" + "ddmmyy" -> UTC epoch; -1 on bad data. Public for tests.
    static int64_t toEpoch(const char* time, const char* date) {
        if (!time || !date || std::strlen(time) < 6 || std::strlen(date) != 6) return -1;
        for (int i = 0; i < 6; i++) if (time[i] < '0' || time[i] > '9' || date[i] < '0' || date[i] > '9') return -1;
        if (time[6] != 0 && time[6] != '.') return -1;
        auto d2 = [](const char* p) { return (p[0] - '0') * 10 + (p[1] - '0'); };
        const int hh = d2(time), mm = d2(time + 2), ss = d2(time + 4);
        const int dd = d2(date), mo = d2(date + 2), yy = 2000 + d2(date + 4);
        if (hh > 23 || mm > 59 || ss > 60) return -1;
        if (yy < MIN_YEAR || yy > MAX_YEAR || mo < 1 || mo > 12 || dd < 1 || dd > daysIn(yy, mo)) return -1;
        return (int64_t)daysFromCivil(yy, mo, dd) * 86400 + hh * 3600 + mm * 60 + (ss > 59 ? 59 : ss);
    }

private:
    static int daysIn(int y, int m) {
        static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        return (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) ? 29 : d[m - 1];
    }
    // days since 1970-01-01 (Howard Hinnant's algorithm)
    static int64_t daysFromCivil(int y, int m, int d) {
        y -= m <= 2;
        const int64_t era = (y >= 0 ? y : y - 399) / 400;
        const int64_t yoe = y - era * 400;
        const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }
    static bool valid_;
    static uint32_t baseEpoch_, baseMs_;
};
bool GpsClock::valid_ = false;
uint32_t GpsClock::baseEpoch_ = 0;
uint32_t GpsClock::baseMs_ = 0;
