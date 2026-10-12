#pragma once
// ======================================================================
// web/log_buffer.h
// EN: Ring log buffer in PSRAM mirrored to Serial and shown in the panel; printf/println with optional DEBUG-
//     flag gating.
// PL: Bufor kołowy dziennika w PSRAM, lustrzany do Serial i pokazywany w panelu; printf/println z opcjonalnym
//     warunkiem flagi DEBUG.
// Used by: actuators/actuators.h, connectivity/ethernet_handler.h, connectivity/network_manager.h,
//     connectivity/pgn_messages.h, connectivity/pgn_receivers.h, connectivity/wifi_handler.h,
//     diagnostics/diagnostics.h, diagnostics/history.h ...
// Depends on: config.h
// ======================================================================
#include <Arduino.h>
#include <stdarg.h>
#include <esp_heap_caps.h>
#include "../config/config.h"

// Everything that used to go only to Serial (WiFi diagnostics, GPS, TX FAIL
// etc.) now lands HERE AS WELL - in a ring buffer, so it can be
// seen through the browser (web/web_server.h), not only over the USB cable.
// Serial.begin() in the .ino stays unchanged - this is just an extra "mirror".
//
// PSRAM: the buffer (and its working copy in snapshot()) are allocated in PSRAM
// (heap_caps_malloc with MALLOC_CAP_SPIRAM), not in internal RAM - the
// N16R8 board has 8 MB of PSRAM, so we can afford a large LOG_BUFFER_SIZE (see config.h)
// without taking the tight internal RAM needed by WiFi/BLE. If PSRAM
// is for some reason not available (e.g. "PSRAM" set wrongly in the Arduino
// IDE - see Tools -> PSRAM -> OPI PSRAM), it falls back to a plain malloc,
// so the code also works without PSRAM, just without the size benefit.
//
// NOTE: since WebServer/ArduinoOTA were moved to separate tasks
// (see network_manager.h), more than one thread/core writes to this buffer
// at once - hence the portMUX_TYPE below, so the writes do not get corrupted.
//
// NOTE 2: Serial.print() on native USB-CDC (ESP32-S3) BLOCKS for
// ~2 seconds on EVERY call if nobody is receiving the data (the USB
// transmit buffer fills up and nobody drains it) - this caused
// multi-second [LOOP STALL]s every time someone looked at the web page
// instead of Serial. "if (Serial)" is NOT enough - it only checks whether
// USB is physically connected/enumerated, not whether anyone is actually reading.
// Instead we check Serial.availableForWrite() (non-blocking) and
// write to Serial ONLY when the data fits without waiting - the buffer
// (and thus the web page) always gets the log anyway, so we lose nothing
// in visibility, only occasionally a single line does not reach Serial.
class LogBuffer {
private:
    static char* buffer;   // a pointer, not an array - allocated in begin(), ideally in PSRAM
    static char* copyBuf;  // working copy for snapshot() - also in PSRAM, allocated once
    static size_t head;    // write position in the ring buffer
    static bool wrapped;   // whether the buffer has already wrapped (we have the full history)
    static portMUX_TYPE mux;

    // Tries PSRAM, falls back to ordinary RAM if unavailable - never returns
    // nullptr without trying (malloc is really the last resort).
    static char* allocBuffer(size_t size) {
        char* p = (char*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (p == nullptr) {
            p = (char*)malloc(size);
        }
        return p;
    }

    static void appendRaw(const char* data, size_t len) {
        portENTER_CRITICAL(&mux);
        for (size_t i = 0; i < len; i++) {
            buffer[head] = data[i];
            head = (head + 1) % LOG_BUFFER_SIZE;
            if (head == 0) wrapped = true;
        }
        portEXIT_CRITICAL(&mux);
    }

public:
    static void begin() {
        if (buffer == nullptr) {
            buffer = allocBuffer(LOG_BUFFER_SIZE);
        }
        if (copyBuf == nullptr) {
            copyBuf = allocBuffer(LOG_BUFFER_SIZE);
        }

        portENTER_CRITICAL(&mux);
        head = 0;
        wrapped = false;
        if (buffer != nullptr) memset(buffer, 0, LOG_BUFFER_SIZE);
        portEXIT_CRITICAL(&mux);
    }

    static void print(const char* msg) {
        if (buffer == nullptr) return; // begin() not called yet / the allocation failed
        size_t len = strlen(msg);
        if (Serial.availableForWrite() >= (int)len) Serial.print(msg);
        appendRaw(msg, len);
    }

    static void println(const char* msg = "") {
        if (buffer == nullptr) return;
        size_t len = strlen(msg);
        if (Serial.availableForWrite() >= (int)len + 2) Serial.println(msg);
        appendRaw(msg, len);
        appendRaw("\n", 1);
    }

    static void printf(const char* fmt, ...) {
        if (buffer == nullptr) return;
        char tmp[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(tmp, sizeof(tmp), fmt, args);
        va_end(args);
        if (Serial.availableForWrite() >= (int)strlen(tmp)) Serial.print(tmp);
        appendRaw(tmp, strlen(tmp));
    }

    // Like println/printf, but only if "enabled" == true - used together
    // with the DEBUG_* switches in config.h, so that individual log
    // categories (e.g. GPS only) can be silenced without changes at the call sites.
    static void printlnIf(bool enabled, const char* msg = "") {
        if (!enabled) return;
        println(msg);
    }

    static void printfIf(bool enabled, const char* fmt, ...) {
        if (!enabled || buffer == nullptr) return;
        char tmp[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(tmp, sizeof(tmp), fmt, args);
        va_end(args);
        if (Serial.availableForWrite() >= (int)strlen(tmp)) Serial.print(tmp);
        appendRaw(tmp, strlen(tmp));
    }

    // Returns the whole buffer as ordered text (from the oldest to the
    // newest entry) - to send in an HTTP response. It copies the buffer under
    // the lock (fast), and builds the String outside it, so as not to hold
    // the critical section longer than needed.
    //
    // IMPORTANT: we build the String with TWO bulk appends (concat), NOT a
    // character-by-character loop - that latter version (65536 calls of out += char with the
    // old 64 KB buffer) was a real CPU LOAD on every
    // poll of the web page (every 1 s), on the same core as the WebServer -
    // suspected of worsening Ethernet stability under load.
    static String snapshot() {
        if (buffer == nullptr || copyBuf == nullptr) return String();

        size_t copyHead;
        bool copyWrapped;

        portENTER_CRITICAL(&mux);
        memcpy(copyBuf, buffer, LOG_BUFFER_SIZE);
        copyHead = head;
        copyWrapped = wrapped;
        portEXIT_CRITICAL(&mux);

        String out;
        out.reserve(LOG_BUFFER_SIZE + 1);
        if (copyWrapped) {
            out.concat(copyBuf + copyHead, LOG_BUFFER_SIZE - copyHead);
        }
        out.concat(copyBuf, copyHead);
        return out;
    }
};

char* LogBuffer::buffer = nullptr;
char* LogBuffer::copyBuf = nullptr;
size_t LogBuffer::head = 0;
bool LogBuffer::wrapped = false;
portMUX_TYPE LogBuffer::mux = portMUX_INITIALIZER_UNLOCKED;
