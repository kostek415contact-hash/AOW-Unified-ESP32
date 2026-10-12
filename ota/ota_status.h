#pragma once
// ======================================================================
// ota/ota_status.h
// EN: Result of the last firmware update (source, bytes, error) kept for the panel.
// PL: Wynik ostatniej aktualizacji firmware (źródło, bajty, błąd) przechowywany dla panelu.
// Used by: connectivity/network_manager.h, web/web_server.h
// Depends on: msg.h
// ======================================================================
#include "../web/msg.h"
// State of the last firmware update (OTA) for the panel: the result of the last attempt, the source (Arduino IDE or
// the /update page), how many bytes and any error. Pure C++ (apart from the RTC memory attribute) - host-tested.
//
// LIMITATION that the panel shows to the user: the web server and ArduinoOTA.handle() handle
// the upload in the SAME task, so DURING an upload the panel does not respond - live
// progress cannot be shown. What is visible is the RESULT of the last attempt (especially an error, after which the module
// does not restart) and the fact that the last boot followed a successful update.
#include <cstdint>
#include <cstring>

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR   // host test build: a plain variable
#endif

static const uint32_t OTA_RTC_MAGIC = 0xA0C0074AUL;   // "this restart followed a successful OTA update"
RTC_NOINIT_ATTR static uint32_t rtcOtaMagic;           // survives a software restart (not a power loss)
static bool bootAfterOta = false;                      // this boot followed a successful update

// Call once at startup (like WifiFallback::countAtBoot).
inline void otaStatusBootInit(bool softwareReset) {
    bootAfterOta = softwareReset && (rtcOtaMagic == OTA_RTC_MAGIC);
    rtcOtaMagic = 0;                                    // one-shot marker
}

class OtaStatus {
public:
    enum State : uint8_t { IDLE = 0, RUNNING = 1, OK = 2, FAILED = 3 };
    enum Source : uint8_t { NONE = 0, IDE = 1, WEB = 2 };

    static void start(Source src, uint32_t totalBytes, unsigned long nowMs) {
        source = src; total = totalBytes; bytes = 0; error[0] = '\0';
        startedMs = (nowMs == 0) ? 1 : nowMs; endedMs = 0;
        state = RUNNING;
    }
    static void progress(uint32_t doneBytes) { if (state == RUNNING) bytes = doneBytes; }
    // After a successful write the module restarts ITSELF - we leave a marker so the new firmware knows this was an update.
    static void success(uint32_t finalBytes, unsigned long nowMs) {
        if (finalBytes) bytes = finalBytes;
        endedMs = (nowMs == 0) ? 1 : nowMs;
        state = OK;
        rtcOtaMagic = OTA_RTC_MAGIC;
    }
    static void fail(const char* msg, unsigned long nowMs) {
        strncpy(error, msg ? msg : MSG("unknown error"), sizeof(error) - 1);
        error[sizeof(error) - 1] = '\0';
        for (char* p = error; *p; ++p) if ((unsigned char)*p < 0x20 || *p == '"' || *p == '\\' || (unsigned char)*p > 0x7E) *p = '?';   // JSON-safe
        endedMs = (nowMs == 0) ? 1 : nowMs;
        state = FAILED;
    }

    static uint8_t getState() { return state; }
    static uint8_t getSource() { return source; }
    static uint32_t getBytes() { return bytes; }
    static uint32_t getTotal() { return total; }
    static const char* getError() { return error; }
    // How many ms ago the last attempt ended (-1: none yet, or still running)
    static long msSinceEnd(unsigned long nowMs) { return (endedMs == 0) ? -1L : (long)(nowMs - endedMs); }

    static uint8_t state;
    static uint8_t source;
    static uint32_t bytes, total;
    static char error[64];
    static unsigned long startedMs, endedMs;
};

uint8_t OtaStatus::state = OtaStatus::IDLE;
uint8_t OtaStatus::source = OtaStatus::NONE;
uint32_t OtaStatus::bytes = 0;
uint32_t OtaStatus::total = 0;
char OtaStatus::error[64] = "";
unsigned long OtaStatus::startedMs = 0;
unsigned long OtaStatus::endedMs = 0;
