#pragma once
// ======================================================================
// web/settings_lock.h
// EN: Read/write lock for text settings shared between the panel (core 0) and the main loop (core 1).
// PL: Blokada odczytu/zapisu ustawień tekstowych współdzielonych między panelem (rdzeń 0) a pętlą główną (rdzeń
//     1).
// Tests: tests/test_settings_lock.cpp (host, plain C++).
// Used by: connectivity/wifi_handler.h, ntrip/ntrip_client.h, web/runtime_settings.h
// ======================================================================
// Lock for text settings (WIFI_SSID/PASS, AP_*, NTRIP_*). They are written by the web panel (core 0) and read by
// the main loop (core 1). An Arduino `String` resizes its buffer on assignment and frees the old one - if
// the other core happens to be reading it, it gets garbage or crashes with LoadProhibited. Rule:
//   - writer (panel):  SettingsWriteGuard g;   - locks and bumps the version when done
//   - reader:          SettingsReadGuard g;    - copies the needed texts into LOCAL variables
//   - frequent reader (WiFi): SettingsLock::version() - copies a snapshot only when the version changed.
// Do not hold the lock longer than needed to copy the texts (no network operations inside).
#include <atomic>
#include <cstdint>
#if defined(ARDUINO_ARCH_ESP32)
  #include <freertos/FreeRTOS.h>
  #include <freertos/semphr.h>
#else
  #include <mutex>
#endif

class SettingsLock {
public:
    // Idempotent; call as early as possible (before the web server starts). Before the first call
    // the lock is empty - that is safe, because only one thread runs until the web server starts.
    static void begin() {
#if defined(ARDUINO_ARCH_ESP32)
        if (!handle) handle = xSemaphoreCreateMutex();
#endif
    }
    static void lock() {
#if defined(ARDUINO_ARCH_ESP32)
        if (handle) xSemaphoreTake(handle, portMAX_DELAY);
#else
        hostMutex.lock();
#endif
    }
    static void unlock() {
#if defined(ARDUINO_ARCH_ESP32)
        if (handle) xSemaphoreGive(handle);
#else
        hostMutex.unlock();
#endif
    }
    static uint32_t version() { return ver.load(std::memory_order_acquire); }
    static void bump() { ver.fetch_add(1, std::memory_order_release); }
private:
    static std::atomic<uint32_t> ver;
#if defined(ARDUINO_ARCH_ESP32)
    static SemaphoreHandle_t handle;
#else
    static std::mutex hostMutex;
#endif
};
std::atomic<uint32_t> SettingsLock::ver(1);
#if defined(ARDUINO_ARCH_ESP32)
SemaphoreHandle_t SettingsLock::handle = nullptr;
#else
std::mutex SettingsLock::hostMutex;
#endif

struct SettingsReadGuard {
    SettingsReadGuard() { SettingsLock::lock(); }
    ~SettingsReadGuard() { SettingsLock::unlock(); }
    SettingsReadGuard(const SettingsReadGuard&) = delete;
    SettingsReadGuard& operator=(const SettingsReadGuard&) = delete;
};
struct SettingsWriteGuard {
    SettingsWriteGuard() { SettingsLock::lock(); }
    ~SettingsWriteGuard() { SettingsLock::bump(); SettingsLock::unlock(); }   // the version bumps BEFORE others get in
    SettingsWriteGuard(const SettingsWriteGuard&) = delete;
    SettingsWriteGuard& operator=(const SettingsWriteGuard&) = delete;
};
