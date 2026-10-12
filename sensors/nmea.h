#pragma once
// ======================================================================
// sensors/nmea.h
// EN: Pure NMEA-0183 helpers: checksum, line validation, field splitting and the GGA/VTG/RMC/GST field parsers.
// PL: Czyste funkcje NMEA-0183: suma kontrolna, sprawdzanie linii, dzielenie na pola i parsery pól GGA/VTG/RMC/GST.
// Tests: tests/test_nmea.cpp (host, plain C++).
// Used by: sensors/gps_universal.h
// Depends on: gps_age.h
// ======================================================================
// NMEA parsing without Arduino, so it can be tested on a computer. GpsHandler keeps the state (last position, timestamps, counters)
// and only copies what these functions return.
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include "gps_age.h"

namespace Nmea {

inline uint8_t checksum(const char* s, uint8_t len) {
    uint8_t c = 0;
    for (uint8_t i = 0; i < len; i++) c ^= (uint8_t)s[i];
    return c;
}

inline int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// "$....*HH" with EXACTLY two hex digits after the asterisk (a CR/LF or end of string may follow). Before, strtol() accepted garbage
// such as "*ZZ" or "*G1" as 0 / 1 and a line whose real checksum is 0 passed with any non-hex text after the asterisk.
inline bool validateLine(const char* line) {
    if (!line || line[0] != '$') return false;
    const char* star = strchr(line, '*');
    if (!star) return false;
    const int hi = hexVal(star[1]);
    if (hi < 0) return false;
    const int lo = hexVal(star[2]);
    if (lo < 0) return false;
    if (star[3] != '\0' && star[3] != '\r' && star[3] != '\n') return false;
    const size_t bodyLen = (size_t)(star - line - 1);
    if (bodyLen > 255) return false;
    return checksum(line + 1, (uint8_t)bodyLen) == (uint8_t)(hi * 16 + lo);
}

// Splits on commas and the asterisk, modifying the buffer in place. Returns the number of fields (at most maxFields).
inline uint8_t splitFields(char* line, char* fields[], uint8_t maxFields) {
    uint8_t count = 0;
    if (maxFields == 0) return 0;
    char* p = line;
    fields[count++] = p;
    while (*p && count < maxFields) {
        if (*p == ',' || *p == '*') {
            *p = '\0';
            fields[count++] = p + 1;
        }
        p++;
    }
    return count;
}

// Sentence type from the header (e.g. "GPGGA" -> "GGA"), ignoring the 2-letter talker/constellation id.
inline bool sentenceEndsWith(const char* header, const char* suffix) {
    size_t len = strlen(header);
    if (len < 3) return false;
    return strcmp(header + (len - 3), suffix) == 0;
}

inline void copyField(char* dst, size_t dstSize, const char* src) {
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

// ---- GGA: $xxGGA,time,lat,N/S,lon,E/W,fix,numSat,hdop,alt,M,geoid,M,age,station ----
// has* = the field was not empty (an empty field leaves the previous value untouched in the caller).
struct Gga {
    char time[12]; bool hasTime;
    char lat[13];  bool hasLat;  char latDir; bool hasLatDir;
    char lon[14];  bool hasLon;  char lonDir; bool hasLonDir;
    int fix; int sats; float hdop; float alt;
    bool ageValid; float age;
};
inline bool parseGga(char* const fields[], uint8_t count, Gga& g) {
    if (count < 10) return false;
    g.hasTime = fields[1][0] != 0; g.time[0] = 0; if (g.hasTime) copyField(g.time, sizeof(g.time), fields[1]);
    g.hasLat = fields[2][0] != 0;  g.lat[0] = 0;  if (g.hasLat) copyField(g.lat, sizeof(g.lat), fields[2]);
    g.hasLatDir = fields[3][0] != 0; g.latDir = fields[3][0];
    g.hasLon = fields[4][0] != 0;  g.lon[0] = 0;  if (g.hasLon) copyField(g.lon, sizeof(g.lon), fields[4]);
    g.hasLonDir = fields[5][0] != 0; g.lonDir = fields[5][0];
    g.fix = atoi(fields[6]);
    g.sats = atoi(fields[7]);
    g.hdop = fields[8][0] ? (float)atof(fields[8]) : 0.0f;
    g.alt = fields[9][0] ? (float)atof(fields[9]) : 0.0f;
    float age = 0.0f;
    g.ageValid = (count > 13) && GpsAge::parse(fields[13], &age);
    g.age = g.ageValid ? age : 0.0f;
    return true;
}

// ---- VTG: $xxVTG,trueTrack,T,magTrack,M,speedKnots,N,speedKmh,K,mode ----
struct Vtg { bool hasHeading; float heading; bool hasSpeed; float speedKnots; };
inline bool parseVtg(char* const fields[], uint8_t count, Vtg& v) {
    if (count < 6) return false;
    v.hasHeading = fields[1][0] != 0; v.heading = v.hasHeading ? (float)atof(fields[1]) : 0.0f;
    v.hasSpeed = fields[5][0] != 0;   v.speedKnots = v.hasSpeed ? (float)atof(fields[5]) : 0.0f;
    return true;
}

// ---- GST: $xxGST,time,rms,semiMajor,semiMinor,orientation,errLat,errLon,errAlt ----
// hAcc = sqrt(errLat^2 + errLon^2), vAcc = errAlt or -1.
struct Gst { float hAcc; float vAcc; };
inline bool parseGst(char* const fields[], uint8_t count, Gst& g) {
    if (count < 9) return false;
    if (!fields[6][0] || !fields[7][0]) return false;
    const float latErr = (float)atof(fields[6]);
    const float lonErr = (float)atof(fields[7]);
    g.hAcc = sqrtf(latErr * latErr + lonErr * lonErr);
    g.vAcc = fields[8][0] ? (float)atof(fields[8]) : -1.0f;
    return true;
}

// ---- RMC: $xxRMC,time,status,lat,N/S,lon,E/W,speedKnots,course,date,... ----
// Returns false when too short or the status is not 'A' (V = unreliable). hasDate: the date field (index 9) exists.
struct Rmc {
    char time[12]; bool hasTime;
    char lat[13];  bool hasLat;  char latDir; bool hasLatDir;
    char lon[14];  bool hasLon;  char lonDir; bool hasLonDir;
    bool hasSpeed; float speedKnots;
    bool hasCourse; float course;
    bool hasDate;
};
inline bool parseRmc(char* const fields[], uint8_t count, Rmc& r) {
    if (count < 9) return false;
    if (fields[2][0] != 'A') return false;
    r.hasTime = fields[1][0] != 0; r.time[0] = 0; if (r.hasTime) copyField(r.time, sizeof(r.time), fields[1]);
    r.hasLat = fields[3][0] != 0;  r.lat[0] = 0;  if (r.hasLat) copyField(r.lat, sizeof(r.lat), fields[3]);
    r.hasLatDir = fields[4][0] != 0; r.latDir = fields[4][0];
    r.hasLon = fields[5][0] != 0;  r.lon[0] = 0;  if (r.hasLon) copyField(r.lon, sizeof(r.lon), fields[5]);
    r.hasLonDir = fields[6][0] != 0; r.lonDir = fields[6][0];
    r.hasSpeed = fields[7][0] != 0;  r.speedKnots = r.hasSpeed ? (float)atof(fields[7]) : 0.0f;
    r.hasCourse = fields[8][0] != 0; r.course = r.hasCourse ? (float)atof(fields[8]) : 0.0f;
    r.hasDate = count >= 10;
    return true;
}

} // namespace Nmea
