#pragma once
// ======================================================================
// diagnostics/heap_trend.h
// EN: Free-RAM trend (5-minute minima, linear regression over up to 6 h) to detect slow memory leaks.
// PL: Trend wolnej pamięci RAM (minima 5-minutowe, regresja liniowa z do 6 h) do wykrywania powolnych wycieków.
// Tests: tests/test_heap_trend.cpp (host, plain C++).
// Used by: connectivity/network_manager.h, web/web_server.h
// ======================================================================
// Free RAM trend: is memory gradually shrinking (a leak)? Pure C++ (no Arduino), host-tested
// (tests/test_heap_trend.cpp). Samples every loop pass (called from network_manager.h once per second) -> the MINIMUM in each
// 5-minute "bucket" (minimum, because momentary allocations, e.g. a /status response, lower memory only briefly and
// would add noise to an average) -> a simple linear regression over the latest buckets (up to 6 h).
//
// Leak = slope <= -2 KB/h with a good line fit (R^2 >= 0.7), computed only after >= 2 h of data.
// Memory: 72 buckets x 4 B. Output: /status (heapTrend*), panel: Diagnostics -> System and a warning on Status.
#include <cstdint>
#include <cstddef>

class HeapTrend {
public:
    static constexpr uint32_t BUCKET_MS   = 300000;   // 5 minutes
    static constexpr size_t   MAX_BUCKETS = 72;       // 6 h
    static constexpr size_t   MIN_BUCKETS = 24;       // 2 h - we do not judge earlier
    static constexpr float    LEAK_BPH    = -2048.0f; // slope indicating a leak [B/h]
    static constexpr float    LEAK_R2     = 0.7f;     // how "straight" the decline must be

    struct Result { bool valid; float slopeBph; float r2; uint16_t buckets; };

    // Call regularly (e.g. every 1 s) with the current free RAM value [B].
    void sample(uint32_t nowMs, uint32_t freeHeap) {
        if (!started) { started = true; bucketStart = nowMs; curMin = freeHeap; return; }
        if (freeHeap < curMin) curMin = freeHeap;
        if ((uint32_t)(nowMs - bucketStart) >= BUCKET_MS) {
            ring[head] = curMin;
            head = (head + 1) % MAX_BUCKETS;
            if (count < MAX_BUCKETS) count++;
            bucketStart = nowMs;          // the next bucket starts from now (after a long gap in samples we do not "catch up")
            curMin = freeHeap;
        }
    }

    Result result() const {
        Result r{false, 0.0f, 0.0f, (uint16_t)count};
        if (count < MIN_BUCKETS) return r;
        const double n = (double)count, hPerBucket = (double)BUCKET_MS / 3600000.0;
        double sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0;
        for (size_t i = 0; i < count; i++) {
            // the oldest bucket has x = 0
            size_t idx = (count < MAX_BUCKETS) ? i : (head + i) % MAX_BUCKETS;
            double x = (double)i * hPerBucket, y = (double)ring[idx];
            sx += x; sy += y; sxx += x * x; sxy += x * y; syy += y * y;
        }
        double dx = n * sxx - sx * sx;
        if (dx <= 0) return r;
        double slope = (n * sxy - sx * sy) / dx;
        double dy = n * syy - sy * sy;
        double r2 = (dy <= 0) ? 0.0 : (n * sxy - sx * sy) * (n * sxy - sx * sy) / (dx * dy);   // constant memory: R^2 = 0 (no trend)
        r.valid = true; r.slopeBph = (float)slope; r.r2 = (float)r2;
        return r;
    }

    // true = memory is steadily shrinking (a leak is likely)
    bool leakSuspect() const {
        Result r = result();
        return r.valid && r.slopeBph <= LEAK_BPH && r.r2 >= LEAK_R2;
    }

    void reset() { *this = HeapTrend(); }

private:
    uint32_t ring[MAX_BUCKETS] = {0};
    size_t head = 0, count = 0;
    bool started = false;
    uint32_t bucketStart = 0, curMin = 0;
};

// One instance for the whole module: samples are added by the main loop (network_manager.h), read by the web server (/status).
class HeapMonitor {
public:
    static HeapTrend trend;
};
HeapTrend HeapMonitor::trend;
