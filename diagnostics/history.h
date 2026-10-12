#pragma once
// ======================================================================
// diagnostics/history.h
// EN: PSRAM-backed chart history (Arduino/FreeRTOS glue around HistoryStore); 24 h with fallbacks, disabled
//     without PSRAM.
// PL: Historia wykresów w PSRAM (klej Arduino/FreeRTOS wokół HistoryStore); 24 h z awaryjnymi krótszymi
//     zakresami, bez PSRAM wyłączona.
// Tests: tests/test_history.cpp (host, plain C++).
// Used by: diagnostics/history_sampler.h, web/web_server.h
// Depends on: history_store.h, log_buffer.h
// ======================================================================
// Chart history in PSRAM - the glue between the pure HistoryStore and Arduino/FreeRTOS.
// Write: once per second from the main loop (nmHistoryTick() in history_sampler.h) - it works even
// when no browser is connected. Read: /history in web_server.h.
// Memory: we try 24 h, and if there is not enough room 12/6/3/1 h; without PSRAM the history is disabled
// (60 s * 17 series * 2 B = 2 KB per minute - internal RAM would not have the room).
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include "history_store.h"
#include "../web/log_buffer.h"

class History {
private:
    static HistoryStore store;
    static bool ready_;
    static unsigned long lastPushMillis;   // 0 = nothing written yet
    static portMUX_TYPE mux;
    static void lockFn() { portENTER_CRITICAL(&mux); }
    static void unlockFn() { portEXIT_CRITICAL(&mux); }

public:
    static void begin() {
        static const uint32_t TRY_SECONDS[5] = { 86400UL, 43200UL, 21600UL, 10800UL, 3600UL };
        for (int i = 0; i < 5; i++) {
            size_t bytes = (size_t)TRY_SECONDS[i] * HistoryFormat::NS * sizeof(int16_t);
            int16_t* mem = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
            if (mem && store.init(mem, TRY_SECONDS[i])) {
                store.setLock(lockFn, unlockFn);
                ready_ = true;
                LogBuffer::printf("[HISTORY] Chart recording in the module: %lu h (%u KB in PSRAM).\n",
                                  (unsigned long)(TRY_SECONDS[i] / 3600), (unsigned)(bytes / 1024));
                return;
            }
            if (mem) free(mem);
        }
        LogBuffer::println("[HISTORY] No PSRAM or no space - chart history in the module is off (charts only in the browser).");
    }

    static bool isReady() { return ready_; }
    static bool due(unsigned long now) { return ready_ && (lastPushMillis == 0 || now - lastPushMillis >= 1000UL); }

    // values[HistoryFormat::NS]; NAN = no data. Missing seconds (loop stalled) are filled with
    // "gaps" so the time axis stays true; we keep the phase (no drift).
    static void push(unsigned long now, const float* values) {
        if (!ready_) return;
        int16_t row[HistoryFormat::NS];
        for (int s = 0; s < HistoryFormat::NS; s++) row[s] = HistoryFormat::encode(s, values[s]);
        if (lastPushMillis == 0) {
            store.pushRow(row);
            lastPushMillis = now;
            return;
        }
        uint32_t secs = (uint32_t)((now - lastPushMillis) / 1000UL);
        if (secs < 1) return;
        if (secs > 1) store.pushGap(secs - 1 > 600 ? 600 : secs - 1);
        store.pushRow(row);
        lastPushMillis += (unsigned long)secs * 1000UL;
    }

    static uint32_t capacitySec() { return ready_ ? store.capacity() : 0; }
    static uint32_t storedSec() { return ready_ ? store.count() : 0; }
    static bool plan(uint32_t rangeSec, uint32_t maxPoints, HistoryStore::Plan* p) { return store.plan(rangeSec, maxPoints, p); }
    static void series(const HistoryStore::Plan& p, int s, int16_t* out) { store.series(p, s, out); }
    static size_t writeHeader(uint8_t* out, const HistoryStore::Plan& p) {
        unsigned long age = (lastPushMillis == 0) ? 0UL : (unsigned long)(millis() - lastPushMillis);
        return store.writeHeader(out, p, (uint32_t)age);
    }
};

HistoryStore History::store;
bool History::ready_ = false;
unsigned long History::lastPushMillis = 0;
portMUX_TYPE History::mux = portMUX_INITIALIZER_UNLOCKED;
