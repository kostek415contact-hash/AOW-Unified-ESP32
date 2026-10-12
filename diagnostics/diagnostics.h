#pragma once
// ======================================================================
// diagnostics/diagnostics.h
// EN: Per-step timing with stall logging, the CPU load profile and the hardware watchdog helpers (begin /
//     feed).
// PL: Pomiar czasu kroków z logowaniem przestojów, profil obciążenia CPU oraz pomocniki sprzętowego watchdoga
//     (begin / feed).
// Used by: connectivity/network_manager.h, web/web_server.h
// Depends on: config.h, log_buffer.h
// ======================================================================
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "../config/config.h"
#include "../web/log_buffer.h"

// Two independent diagnostic tools, both small - combined in
// one file.

// Measures the execution time of a single step and logs if it exceeds a threshold -
// a diagnostic to pin down WHICH exact function is responsible for
// [LOOP STALL], instead of guessing from the total time of the whole loop().
// Active only under DEBUG_STEP_TIMING - it has served its purpose (we found the
// blocking Serial.print on USB-CDC), leave it off day to day, turn it
// back on if [LOOP STALL] ever returns.
//
// The threshold was lowered from 50 to 10 - at 50 ms we never caught a single [STEP]
// line during the observed Ethernet failures, even though the loop evidently
// loses time somewhere (a near watchdog timeout in one case) - either
// no SINGLE function of ours is to blame (and the time loss happens
// BETWEEN passes, e.g. an internal W5500 driver task stealing the
// CPU - which this instrumentation by design will not catch), or the threshold was simply
// too high.
inline void nmTimeStep(const char* name, unsigned long startMillis) {
    if (!DEBUG_STEP_TIMING) return;
    unsigned long elapsed = millis() - startMillis;
    if (elapsed > 10) {
        LogBuffer::printf("[STEP] %s took %lu ms\n", name, elapsed);
    }
}

// Per-module CPU profile: what % of core 1's time the loop spends in each step.
// Measured in MICROseconds (a loop pass takes ~0.3 ms, milliseconds were too
// coarse), summed in 1 s windows. NOTE: this is the % of WALL-CLOCK time of core 1
// spent inside the module, INCLUDING waiting (e.g. a blocking ADS1115 read
// over I2C takes a lot of time, but the CPU is free during it) -
// it is not pure CPU usage. Active only under
// DEBUG_STEP_TIMING (zero cost day to day - see the NM_STEP macro).
class CpuProfile {
public:
    static const uint8_t MAX_STEPS = 24;
private:
    static const char* names[MAX_STEPS];
    static uint32_t acc[MAX_STEPS];      // sum of us in the current window
    static uint32_t mx[MAX_STEPS];       // max of a single call (us) in the current window
    static float shownPct[MAX_STEPS];    // result of the last full window
    static uint32_t shownMaxUs[MAX_STEPS];
    static volatile uint8_t used;
    static uint32_t winStartUs;
public:
    static void add(int8_t& slot, const char* name, uint32_t us) {
        if (slot < 0) {
            if (used >= MAX_STEPS) return;
            names[used] = name;
            slot = (int8_t)used;
            used = used + 1;
        }
        acc[slot] += us;
        if (us > mx[slot]) mx[slot] = us;
        if (us > 10000) { // like the old nmTimeStep: log single steps > 10 ms
            LogBuffer::printf("[STEP] %s took %lu ms\n", name, (unsigned long)(us / 1000));
        }
    }
    // Call once per loop pass - every 1 s it closes the window and computes the percentages.
    static void tick() {
        uint32_t now = micros();
        uint32_t elapsed = now - winStartUs;
        if (elapsed < 1000000UL) return;
        for (uint8_t i = 0; i < used; i++) {
            shownPct[i] = 100.0f * (float)acc[i] / (float)elapsed;
            shownMaxUs[i] = mx[i];
            acc[i] = 0;
            mx[i] = 0;
        }
        winStartUs = now;
    }
    static uint8_t count() { return used; }
    static const char* nameAt(uint8_t i) { return names[i]; }
    static float pctAt(uint8_t i) { return shownPct[i]; }
    static uint32_t maxUsAt(uint8_t i) { return shownMaxUs[i]; }
};
const char* CpuProfile::names[CpuProfile::MAX_STEPS] = {nullptr};
uint32_t CpuProfile::acc[CpuProfile::MAX_STEPS] = {0};
uint32_t CpuProfile::mx[CpuProfile::MAX_STEPS] = {0};
float CpuProfile::shownPct[CpuProfile::MAX_STEPS] = {0};
uint32_t CpuProfile::shownMaxUs[CpuProfile::MAX_STEPS] = {0};
volatile uint8_t CpuProfile::used = 0;
uint32_t CpuProfile::winStartUs = 0;

// Measures one loop step. When DEBUG_STEP_TIMING is off - an ordinary call,
// no overhead apart from one flag check.
#define NM_STEP(NAME, CALL) do { \
if (DEBUG_STEP_TIMING) { \
    static int8_t _nmSlot = -1; \
    uint32_t _nmT0 = micros(); \
    CALL; \
    CpuProfile::add(_nmSlot, NAME, micros() - _nmT0); \
} else { \
    CALL; \
} \
} while (0)

// Hardware watchdog - if loop() hangs for longer than
// WATCHDOG_TIMEOUT_S (see config.h), the ESP32 restarts itself instead of
// hanging forever without sending heartbeats to AgOpenWeb. This is a
// "just in case" safeguard - it works regardless of WHAT
// exactly hangs the loop in the future (not only the already found and
// fixed blocking Serial problem - see web/log_buffer.h).
//
// NOTE: we call watchdogBegin() only AFTER setup() finishes (not during
// it) - the WiFi connection (wifiHandlerBegin, blocking until done) and other
// steps in setup() can take longer than WATCHDOG_TIMEOUT_S, so attaching the
// watchdog too early would restart the device over and over before loop()
// even starts.
//
// NOTE 2: the esp_task_wdt API differs between ESP32 Arduino core versions.
// The code below fits newer cores (3.x, with the esp_task_wdt_config_t struct).
// If you have an older core (2.x) and this does not compile, replace begin() with:
//   esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
//   esp_task_wdt_add(NULL);
inline void watchdogBegin() {
    esp_task_wdt_config_t config = {};
    config.timeout_ms = WATCHDOG_TIMEOUT_S * 1000;
    config.idle_core_mask = 0;
    config.trigger_panic = true;

    // The framework (Arduino/ESP-IDF) sometimes already starts a default watchdog
    // (for the IDLE tasks) before our code even runs - esp_task_wdt_init()
    // then returns ESP_ERR_INVALID_STATE instead of a critical error.
    // In that case we use esp_task_wdt_reconfigure() (adapting the
    // already running watchdog to our timeout), instead of trying
    // to initialize it anew.
    esp_err_t err = esp_task_wdt_init(&config);
    if (err == ESP_ERR_INVALID_STATE) {
        esp_task_wdt_reconfigure(&config);
    }
    // ESP_ERR_INVALID_ARG here = the task is already added by the framework -
    // also harmless, we are simply already protected.
    esp_task_wdt_add(NULL);

    LogBuffer::printfIf(DEBUG_SYS, "[WATCHDOG] Active, timeout=%lus\n", (unsigned long)WATCHDOG_TIMEOUT_S);
}

// Call once on EVERY loop() pass - it "feeds" the watchdog so it knows
// the loop is working normally. No call for WATCHDOG_TIMEOUT_S = restart.
inline void watchdogFeed() {
    esp_task_wdt_reset();
}
