#pragma once
// ======================================================================
// diagnostics/event_sampler.h
// EN: Once-per-second glue that gathers state from the subsystems and feeds the event detector / event log.
// PL: Klej wołany raz na sekundę: zbiera stan podsystemów i podaje go detektorowi zdarzeń i dziennikowi.
// Used by: connectivity/network_manager.h
// Depends on: event_detector.h, gps_clock.h, persist_log.h
// ======================================================================
// Collects state once per second and writes events to EventLog (markers on the charts). Called from the main loop (network_manager.h),
// it also works without PSRAM and without a connected browser. Detection logic: event_detector.h (host-tested).
// Requires (included earlier): gps_universal.h, ntrip_client.h, wifi_handler.h, ethernet_handler.h, pgn_receivers.h,
// current_sensor.h, actuators.h, connectivity/agio_senders.h and bootAfterOta / bootAfterWifiLoss.
#include <freertos/FreeRTOS.h>
#include "event_detector.h"
#include "persist_log.h"
#include "../sensors/gps_clock.h"

static portMUX_TYPE nmEventMux = portMUX_INITIALIZER_UNLOCKED;
static void nmEventLock() { portENTER_CRITICAL(&nmEventMux); }
static void nmEventUnlock() { portEXIT_CRITICAL(&nmEventMux); }

inline void nmEventTick() {
    static bool started = false;
    static unsigned long last = 0;
    static EventDetector detector;
    unsigned long now = millis();
    if (!started) {
        started = true; last = now;
        EventLog::setLock(nmEventLock, nmEventUnlock);
        PersistLog::begin(&rtcEventBlob);   // events of the previous run (for the panel) + start of a new record in RTC
        EventLog::setSink([](uint8_t t, int16_t a, uint32_t m) { PersistLog::add(t, a, m); });
        // restart reason: 100 = after an OTA update, 101 = after WiFi loss, otherwise esp_reset_reason()
        int16_t reason = bootAfterOta ? 100 : (bootAfterWifiLoss ? 101 : (int16_t)esp_reset_reason());
        EventLog::add(EventLog::EV_BOOT, reason, now);
        return;
    }
    if (now - last < 1000UL) return;
    last = now;
    PersistLog::alive((uint32_t)now, GpsClock::epochNow((uint32_t)now));   // when the module was last alive (and the time of day per GPS) - for "before restart" in the panel

    EventInputs in;
    int q = GpsHandler::getFixQuality();                                // like the fixLvl series in history_sampler.h
    in.fixLevel = (q == 0) ? 0 : (q == 1) ? 1 : (q == 2) ? 2 : (q == 5) ? 3 : (q == 4) ? 4 : -1;
    in.ntripEnabled = ENABLE_NTRIP_CLIENT;
    in.ntripStreaming = NtripClient::isStreaming();
    in.netUp = EthernetHandler::isConnected() || (!WifiHandler::isAPMode() && WiFi.status() == WL_CONNECTED);
    in.agioActive = AgioSenders::selectedActive(now);
    in.agioSwitches = AgioSenders::switches;
    in.steerOn = SteerCommandReceiver::isAutoSteerActive();
    in.currentLatched = CurrentSensor::isLatched();
    in.runawayTrips = MotorDriver::runawayTrips();
    detector.update(now, in);
}
