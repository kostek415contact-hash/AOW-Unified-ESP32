#pragma once
// ======================================================================
// diagnostics/history_store.h
// EN: Ring buffer of chart history (1 sample/s, int16 scaled series) with aggregation for long ranges.
// PL: Bufor kołowy historii wykresów (1 próbka/s, serie int16 ze skalą) z agregacją dla długich zakresów.
// Used by: diagnostics/history.h
// ======================================================================
// Chart history recorded BY THE MODULE (not by the browser): charts keep accumulating even with
// the web page closed, and when it is opened the browser fetches the ready history from /history.
// Pure C++ with no Arduino dependency - so it is host-tested (tests/).
//
// Ring buffer: one sample per second, NS series of int16 (value * SCALE, INVALID = no data).
// A read (/history) may be aggregated to fewer points - for long ranges
// we keep the WORST value in the bucket (AGG_MIN/AGG_MAX), not the average, so short events
// (IMU dropout, fix drop, CPU spike) do not vanish when averaging.
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>

namespace HistoryFormat {

    static const int NS = 19;
    // Order = the order of the series in the response. The browser reads the names from the response header, so
    // adding a series here does not require a decoder change (it only has to exist in SER in web/ui/js_charts.h so that it
    // gets drawn).
    static const char NAMES[] = "hAccCm,fixLvl,sats,speed,roll,pitch,angle,target,rssi,ntripKBs,gpsKBs,cpu1,cpu0,heap,minheap,imuPps,imuBps,curA,ageDgps";
    static const uint16_t SCALE[NS] = { 10, 1, 1, 100, 100, 100, 100, 100, 1, 100, 100, 100, 100, 10, 10, 1, 1, 100, 10 };
    enum Agg : uint8_t { AGG_MEAN = 0, AGG_MIN = 1, AGG_MAX = 2 };
    static const uint8_t AGG[NS] = { AGG_MAX, AGG_MIN, AGG_MIN, AGG_MEAN, AGG_MEAN, AGG_MEAN, AGG_MEAN, AGG_MEAN, AGG_MIN,
        AGG_MEAN, AGG_MEAN, AGG_MAX, AGG_MAX, AGG_MIN, AGG_MIN, AGG_MIN, AGG_MIN, AGG_MAX, AGG_MAX };
        static const int16_t INVALID = INT16_MIN;
        static const size_t FIXED_HEADER = 26;

        // Value -> int16 (scaling, clipping, none/NaN -> INVALID).
        inline int16_t encode(int s, float v) {
            if (!std::isfinite(v)) return INVALID;
            float x = v * (float)SCALE[s];
            if (x > 32767.0f) x = 32767.0f;
            if (x < -32767.0f) x = -32767.0f;
            return (int16_t)std::lround(x);
        }

} // namespace HistoryFormat

class HistoryStore {
public:
    typedef void (*LockFn)(void);
    // Snapshot for reading: buckets are counted from the NEWEST sample backwards (the newest is always full).
    struct Plan { uint32_t cnt; uint32_t head; uint32_t first; uint32_t step; uint32_t nPoints; };

    bool init(int16_t* memory, uint32_t capacitySamples) {
        if (!memory || capacitySamples < 8) return false;
        buf = memory; cap = capacitySamples; head = 0; cnt = 0;
        return true;
    }
    // The lock only for the moment of changing the indices (write: core 1, HTTP read: core 0).
    void setLock(LockFn l, LockFn u) { lock_ = l; unlock_ = u; }
    bool ready() const { return buf != nullptr; }
    uint32_t capacity() const { return cap; }
    uint32_t count() const { return cnt; }

    void pushRow(const int16_t* row) {
        if (!buf) return;
        memcpy(buf + (size_t)head * HistoryFormat::NS, row, HistoryFormat::NS * sizeof(int16_t));
        publish();
    }
    // Missing seconds (main loop stalled) - the time axis stays correct, and the chart shows a gap.
    void pushGap(uint32_t seconds) {
        if (!buf) return;
        if (seconds > cap) seconds = cap;
        for (uint32_t i = 0; i < seconds; i++) {
            int16_t* dst = buf + (size_t)head * HistoryFormat::NS;
            for (int s = 0; s < HistoryFormat::NS; s++) dst[s] = HistoryFormat::INVALID;
            publish();
        }
    }

    bool plan(uint32_t rangeSec, uint32_t maxPoints, Plan* p) const {
        if (!buf || !p || rangeSec == 0 || maxPoints == 0) return false;
        uint32_t c, h;
        if (lock_) lock_();
        c = cnt; h = head;
        if (unlock_) unlock_();
        // When the buffer is full, the oldest samples may just be getting overwritten by a write
        // from the other core - we skip a few of the oldest.
        uint32_t skip = (c >= cap) ? 4 : 0;
        uint32_t usable = c > skip ? c - skip : 0;
        uint32_t want = rangeSec < usable ? rangeSec : usable;
        p->cnt = c; p->head = h; p->first = 0; p->step = 1; p->nPoints = 0;
        if (want == 0) return true;
        uint32_t step = (want + maxPoints - 1) / maxPoints;
        if (step < 1) step = 1;
        uint32_t n = want / step;
        if (n == 0) n = 1;
        p->step = step; p->nPoints = n; p->first = c - n * step;
        return true;
    }

    // Fills out[0..nPoints-1] (oldest to newest) for series s.
    void series(const Plan& p, int s, int16_t* out) const {
        using namespace HistoryFormat;
        for (uint32_t i = 0; i < p.nPoints; i++) {
            int32_t sum = 0, mn = 32767, mx = -32767;
            uint32_t valid = 0;
            for (uint32_t j = 0; j < p.step; j++) {
                uint32_t k = p.first + i * p.step + j;
                uint32_t idx = (p.head + cap - p.cnt + k) % cap;
                int16_t v = buf[(size_t)idx * NS + s];
                if (v == INVALID) continue;
                sum += v; valid++;
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
            if (!valid) { out[i] = INVALID; continue; }
            switch (AGG[s]) {
                case AGG_MIN: out[i] = (int16_t)mn; break;
                case AGG_MAX: out[i] = (int16_t)mx; break;
                default: {
                    int32_t half = (int32_t)(valid / 2);
                    out[i] = (int16_t)((sum >= 0 ? sum + half : sum - half) / (int32_t)valid);   // rounding away from zero
                }
            }
        }
    }

    // Header of the /history response (little-endian):
    //   'A''O''G''H', version(1), NS, nPoints(u16), stepSec(u16), reserved(u16),
    //   lastAgeMs(u32) - how many ms ago the last sample was made,
    //   capacitySec(u32), storedSec(u32), namesLen(u16, including the zero and padding to even),
    //   series names separated by commas, NS x scale(u16). Then NS series of nPoints x int16 each.
    static size_t namesPadded() { return (strlen(HistoryFormat::NAMES) + 2) & ~(size_t)1; }
    static size_t headerSize() { return HistoryFormat::FIXED_HEADER + namesPadded() + (size_t)HistoryFormat::NS * 2; }
    size_t writeHeader(uint8_t* out, const Plan& p, uint32_t lastAgeMs) const {
        using namespace HistoryFormat;
        size_t o = 0;
        out[o++] = 'A'; out[o++] = 'O'; out[o++] = 'G'; out[o++] = 'H';
        out[o++] = 1; out[o++] = (uint8_t)NS;
        put16(out, o, (uint16_t)p.nPoints);
        put16(out, o, (uint16_t)p.step);
        put16(out, o, 0);
        put32(out, o, lastAgeMs);
        put32(out, o, cap);
        put32(out, o, p.cnt);
        put16(out, o, (uint16_t)namesPadded());
        size_t nl = strlen(NAMES);
        memcpy(out + o, NAMES, nl);
        for (size_t i = nl; i < namesPadded(); i++) out[o + i] = 0;
        o += namesPadded();
        for (int s = 0; s < NS; s++) put16(out, o, SCALE[s]);
        return o;
    }

private:
    int16_t* buf = nullptr;
    uint32_t cap = 0, head = 0, cnt = 0;
    LockFn lock_ = nullptr, unlock_ = nullptr;

    void publish() {
        if (lock_) lock_();
        head = (head + 1) % cap;
        if (cnt < cap) cnt++;
        if (unlock_) unlock_();
    }
    static void put16(uint8_t* b, size_t& o, uint16_t v) { b[o++] = (uint8_t)(v & 0xFF); b[o++] = (uint8_t)(v >> 8); }
    static void put32(uint8_t* b, size_t& o, uint32_t v) { for (int i = 0; i < 4; i++) b[o++] = (uint8_t)((v >> (8 * i)) & 0xFF); }
};
