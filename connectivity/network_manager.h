#pragma once
// ======================================================================
// connectivity/network_manager.h
// EN: Orchestration: starts transports, GPS, NTRIP, web server, OTA; runs the main loop steps, heartbeats,
//     watchdog and diagnostics sampling.
// PL: Orkiestracja: uruchamia transport, GPS, NTRIP, serwer WWW i OTA; prowadzi kroki głównej pętli,
//     heartbeaty, watchdog i próbkowanie diagnostyki.
// Used by: AOW-Unified-ESP32.ino
// Depends on: actuators.h, arduino_ota.h, config.h, current_sensor.h, diagnostics.h, ethernet_handler.h,
//     event_sampler.h, gps_universal.h, heap_trend.h, history_sampler.h ...
// ======================================================================
#include <Arduino.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include "wifi_fallback.h"
#include "../diagnostics/net_stats.h"
#include "../diagnostics/persist_log.h"
#include "../diagnostics/heap_trend.h"
#include "pgn_frame.h"
#include "../ntrip/rtcm_arbiter.h"
#include "../ota/ota_status.h"
#include "../config/config.h"
#include "../web/runtime_settings.h"
#include "wifi_handler.h"
#include "ethernet_handler.h"
#include "pgn_messages.h"
#include "hw_message.h"
#include "pgn_receivers.h"
#include "../sensors/gps_universal.h"
#include "../sensors/steering_angle_sensor.h"
#include "../sensors/imu_sensor.h"
#include "../ntrip/rtcm_relay.h"
#include "../ntrip/ntrip_client.h"
#include "../web/log_buffer.h"

// These two variables MUST be declared HERE (before the #include of web_server.h
// below) - web_server.h (handleStatus()) reads them, and in C++ something has to be
// declared BEFORE use in the same file after preprocessing (the whole
// project is one merged translation unit - see #pragma once).
// We already hit the same problem with apModeActive - there we avoided it
// by using WifiHandler::isAPMode() instead of our own flag; here there is no
// ready-made getter, so we move the declarations instead.
static volatile float coreZeroLoadPercent = 0.0f; // core 0 - real % (idle counters, see idleCounterTask0/1 below)
static volatile float lastAvgLoopMs = 0.0f;       // core 1 - raw ms/pass, for the [LOOP AVG] log (extra diagnostics)
static volatile float lastLoopLoadPercent = 0.0f; // core 1 - real % (idle counters, see idleCounterTask0/1 below)
// Counter of restarts caused by WiFi loss - kept in RTC memory, so it survives a software restart (it does not
// survive a power cycle). See wifi_fallback.h. The declarations MUST come before web_server.h
// (bootAfterWifiLoss reads resetReasonText()).
RTC_NOINIT_ATTR static uint32_t rtcWifiFbMagic;
RTC_NOINIT_ATTR static uint32_t rtcWifiFbCount;
// Latest events in RTC memory (they survive a restart/crash, not a power loss) - see diagnostics/persist_log.h.
RTC_NOINIT_ATTR static PersistLog::Blob rtcEventBlob;
static uint32_t wifiFbCount = 0;          // how many consecutive restarts preceded this boot
static WifiFallback::Monitor wifiFbMonitor;   // decides on a restart when WiFi is lost while running
static bool bootAfterWifiLoss = false;    // this boot followed a restart caused by WiFi loss
#include "../web/web_server.h"
#include "../ota/arduino_ota.h"
#include "../actuators/actuators.h"
#include "../io/section_expander.h"
#include "../io/status_led.h"
#include "../sensors/current_sensor.h"
#include "../diagnostics/diagnostics.h"
#include "../diagnostics/history_sampler.h"
#include "../diagnostics/event_sampler.h"

// Text on the AgOpenWeb screen (PGN 221): the operator in the cab sees what the module has just detected.
inline void nmSendHwMessage(const char* text, uint8_t seconds) {
    uint8_t f[80];
    const size_t n = HwMessage::build(f, sizeof(f), SRC_AUTOSTEER, text, seconds, true);
    if (n) nmSendPacket(f, (uint16_t)n);
}

// Hardware that worked and then stopped (angle sensor, section expander, IMU) is announced on the AgOpenWeb screen: once when it
// happens, then again every minute while it lasts. Hardware that never worked (a bench setup) stays silent. Only while an AgOpenWeb is talking.
inline void nmHwAlertTick() {
    static HwMessage::LostAfterOk angleLost, expanderLost, imuLost;
    static HwMessage::AlertGate angleGate, expanderGate, imuGate, armGate;
    const uint32_t now = (uint32_t)millis();
    const bool angleBad = angleLost.update(SteeringAngleSensor::isConnected() && SteeringAngleSensor::isRawInRange());
    const bool expanderBad = expanderLost.update(!SectionExpander::isEnabled() || SectionExpander::isFound());
    const bool imuBad = imuLost.update(!ENABLE_IMU || ImuSensor::isConnected());
    const bool listening = AgioSenders::selectedActive(now);
    if (angleGate.update(angleBad && listening, now, 60000)) nmSendHwMessage("Angle sensor fault - check the wiring", 8);
    if (expanderGate.update(expanderBad && listening, now, 60000)) nmSendHwMessage("Section expander lost", 8);
    if (imuGate.update(imuBad && listening, now, 60000)) nmSendHwMessage("IMU lost", 8);
    // An abnormal restart (crash, watchdog, power dip) is announced ONCE, as soon as an AgOpenWeb is listening.
    static bool bootAlertSent = false;
    if (!bootAlertSent && listening) {
        bootAlertSent = true;
        HwMessage::BootKind bk = HwMessage::BK_NORMAL;
        switch (esp_reset_reason()) {
            case ESP_RST_PANIC:    bk = HwMessage::BK_PANIC; break;
            case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: bk = HwMessage::BK_WATCHDOG; break;
            case ESP_RST_BROWNOUT: bk = HwMessage::BK_BROWNOUT; break;
            default: break;
        }
        const char* bt = HwMessage::bootAlertText(bk);
        if (bt) nmSendHwMessage(bt, 10);
    }
    // After a restart the motor stays still until steering was seen OFF: tell the operator what to do (repeated while he waits).
    if (armGate.update(MotorDriver::isArmBlocking() && listening, now, 15000)) nmSendHwMessage("Motor not armed - switch steering off and on", 8);
}

// ONE line in the log 20 s after start: what works and what does not (OK = works, NO = enabled but not working, off = switched off).
// Whoever opens the log after a bad start sees the whole picture at once instead of hunting through separate lines.
inline const char* nmFeatureState(bool enabled, bool ok) { return !enabled ? "off" : ok ? "OK" : "NO"; }
inline void nmBootSelfCheck() {
    static bool done = false;
    if (done || millis() < 20000) return;
    done = true;
    LogBuffer::printf("[BOOT] Self-check: angle=%s IMU=%s expander=%s current=%s GPS=%s net=%s\n",
                      nmFeatureState(true, SteeringAngleSensor::isConnected() && SteeringAngleSensor::isRawInRange()),
                      nmFeatureState(ENABLE_IMU, ImuSensor::isConnected()),
                      nmFeatureState(SectionExpander::isEnabled(), SectionExpander::isFound()),
                      nmFeatureState(CurrentSensor::isEnabled(), CurrentSensor::stateCode() == 2),
                      nmFeatureState(true, GpsHandler::getFixQuality() > 0),
                      nmFeatureState(true, EthernetHandler::isConnected() || WiFi.status() == WL_CONNECTED));
}

// Kickout signal to AgOpenWeb (PGN 250): the motor protection is holding the motor = steering must be switched off in AgOpenWeb.
// Called every loop pass after MotorDriver::update().
inline void nmKickoutTick() {
    unsigned long now = millis();
    // cause: motor motion supervision or the current sensor (missing data right after boot does not count - see kickout_signal.h)
    const uint8_t cause = kickoutCause(MotorDriver::runawayState(), CurrentSensor::stateCode(), (uint32_t)now);
    bool tripped = (cause != KC_NONE);
    float load = (CurrentSensor::isEnabled() && CURRENT_HARD_LIMIT_A > 0.1f) ? CurrentSensor::getRecentPeakA() / CURRENT_HARD_LIMIT_A : 0.0f;
    uint32_t before = kickoutSig.count();
    kickoutSig.update((uint32_t)now, tripped, load);
    if (kickoutSig.count() != before) {
        kickoutSig.noteCause(cause);
        LogBuffer::printf("[KICKOUT] Motor protection tripped (cause: %s) - AgOpenWeb receives PGN 250 = 255 (it will disengage steering if the Current sensor is enabled).\n",
                          kickoutCauseText(cause));
        char msg[HwMessage::MAX_TEXT + 1];
        snprintf(msg, sizeof(msg), "Motor protection: %s", kickoutCauseText(cause));
        nmSendHwMessage(msg, 10);
    }
}

// ORCHESTRATION: who calls what and when (begin/loop, FreeRTOS tasks, WiFi/Ethernet
// handling). The content of the PGN frames themselves (building, CRC, heartbeat
// schedule) is in pgn_messages.h - this file does not know it, it only calls
// the ready-made functions from there in the right order.
//
// When adding a new feature to the project you usually do NOT edit this file:
//   - new frame content / new heartbeat -> pgn_messages.h
//   - new sensor/actuator -> its own file, a static class with begin()/update()
//     (see sensors/, actuators/, io/ - every existing file is a ready
//     pattern to copy) + one begin()/update() line HERE

// Drives the motor to the angle requested by AgOpenWeb - ONLY when steering is
// really enabled and we have a fresh command (SteerCommandReceiver::isFresh()) -
// otherwise it returns to center. See actuators/actuators.h (for now a
// placeholder, waiting for the Cytron MD13S).
inline void nmUpdateMotorFromCommand() {
    SteerConfigReceiver::tick(SteerCommandReceiver::isAutoSteerActive());   // "Invert motor" from AgOpenWeb: only while disengaged
    if (!MotorDriver::isEnabled()) return;
    if (SteerCommandReceiver::isAutoSteerActive()) {
        MotorDriver::setTargetAngleDeg(SteerCommandReceiver::getTargetAngleDeg());
    } else {
        MotorDriver::setTargetAngleDeg(0.0f);
    }
}

// 1 KB buffer: RTCM3 frames in PGN 215 are up to ~1 KB (a smaller one would truncate them). Static buffer (not on the stack); called from a single task.
static const int NM_UDP_BUFFER_SIZE = 1024;
// Max datagrams handled per socket per loop() pass: a backlog (e.g. after a pause) is drained at once,
// but a flood cannot starve the rest of the loop.
static const int NM_UDP_MAX_PER_PASS = 16;

// Handles ONE datagram; returns false when nothing was waiting.
inline bool nmCheckOneUdp(AogUdpClass &udp) {
    int packetSize = udp.parsePacket();
    if (packetSize <= 0) return false;
    {
        static uint8_t buffer[NM_UDP_BUFFER_SIZE];
        int len = udp.read(buffer, sizeof(buffer));

        if (len >= 5 && buffer[0] == 0x80 && buffer[1] == 0x81) {
            // Foreign sender (e.g. a second AgOpenWeb on the network): listed in the panel, but its frames are NOT executed - neither steering commands,
            // nor RTCM, nor Hello/Scan queries (connectivity/agio_senders.h).
            if (!nmNoteAgioSender(udp.remoteIP())) return true;
            uint8_t incomingPGN = buffer[3];
            NetStats::noteRx(incomingPGN);   // per-PGN frame counter (panel: Diagnostics -> PGN)
            if (incomingPGN != PGN_RTCM) {
                int crcRes = PgnFrame::crcCheck(buffer, len);
                NetStats::noteCrc(crcRes);
                // Steering commands with a bad checksum are rejected (before 2026-10-08 they were only counted; after 88,451 frames without a single error
                // it is safe to reject them). Hello/Scan and RTCM unchanged.
                if (PgnFrame::shouldRejectOnCrc(incomingPGN, crcRes)) { NetStats::crcRejected++; return true; }
            }
            if (incomingPGN == PGN_HELLO_REQUEST || incomingPGN == PGN_SCAN_REQUEST) {
                nmHandleDiscoveryRequest(incomingPGN);
            } else if (incomingPGN == PGN_AUTOSTEER_COMMAND) {
                uint8_t declaredLen = buffer[4];
                if (len >= 5 + declaredLen) {
                    SteerCommandReceiver::parse(buffer, declaredLen, udp.remoteIP());
                } else {
                    NetStats::noteBadFrame();
                }
            } else if (incomingPGN == PGN_MACHINE_COMMAND) {
                uint8_t declaredLen = buffer[4];
                if (len >= 5 + declaredLen) {
                    MachineCommandReceiver::parse(buffer, declaredLen);
                } else {
                    NetStats::noteBadFrame();
                }
            } else if (incomingPGN == PGN_MACHINE_CONFIG) {
                uint8_t declaredLen = buffer[4];
                if (len >= 5 + declaredLen) {
                    MachineConfigReceiver::parse(buffer, declaredLen);
                } else {
                    NetStats::noteBadFrame();
                }
            } else if (incomingPGN == PGN_MACHINE_PINS) {
                uint8_t declaredLen = buffer[4];
                if (len >= 5 + declaredLen) {
                    MachinePinConfigReceiver::parse(buffer, declaredLen);
                } else {
                    NetStats::noteBadFrame();
                }
            } else if (incomingPGN == PGN_STEER_CONFIG) {
                uint8_t declaredLen = buffer[4];
                if (len >= 5 + declaredLen) {
                    SteerConfigReceiver::parse(buffer, declaredLen);
                } else {
                    NetStats::noteBadFrame();
                }
            } else if (incomingPGN == PGN_STEER_SETTINGS) {
                uint8_t declaredLen = buffer[4];
                if (len >= 5 + declaredLen) {
                    SteerSettingsReceiver::parse(buffer, declaredLen);
                } else {
                    NetStats::noteBadFrame();
                }
            } else if (incomingPGN == PGN_RTCM) {
                // RTCM wrapped in a PGN on the MAIN port (an alternative method
                // next to RtcmRelay on the separate port 2233 - see config.h at
                // PGN_RTCM). declaredLen may be 0 if AgOpenWeb encodes the
                // length differently for this particular PGN (the RTCM3 data itself
                // carries its own length in the header) - in that case
                // we take EVERYTHING from byte 5 to the end of the received packet.
                // The length is computed more robustly than the header byte alone (which wraps for frames > 255 B) - see pgn_frame.h.
                int rtcmLen = PgnFrame::pgn215PayloadLen(len, buffer[4]);
                if (rtcmLen > 0) {
                    // External source: let it through only when our own NTRIP client is not streaming (ntrip/rtcm_arbiter.h).
                    if (RtcmArbiter::filterExternal(millis(), (uint32_t)rtcmLen)) {
                        GpsHandler::writeRtcm(buffer + 5, (size_t)rtcmLen);
                        LogBuffer::printfIf(DEBUG_RTCM, "[RTCM] Received %u bytes via PGN215 (main port) - forwarded to GPS.\n",
                                            (unsigned)rtcmLen);
                    }
                }
            }
        } else {
            NetStats::noteBadFrame();   // no 0x80 0x81 header, or shorter than 5 bytes
        }
    }
    return true;
}

inline void nmCheckUDP(AogUdpClass &udp) {
    for (int i = 0; i < NM_UDP_MAX_PER_PASS; i++) {
        if (!nmCheckOneUdp(udp)) break;
    }
}

// The WebServer and ArduinoOTA live in a SEPARATE FreeRTOS task. Both can
// block for a while (e.g. when someone connects over OTA or holds
// a web connection open) - keeping them in the main loop() caused
// multi-second stalls of GPS reading and PGN heartbeats at the same time.
// In a separate task they block ONLY themselves, not the rest of the system - it works
// the same on the single-core ESP32-C6 as on the dual-core S3.
static void webOtaTaskFunction(void* pvParameters) {
    (void)pvParameters;
    for (;;) {
        WebServerModule::update();
        if (ENABLE_ARDUINO_OTA) {
            {
                IPAddress ip = WiFi.localIP();
                if (ENABLE_ETHERNET && ip == IPAddress(0, 0, 0, 0)) ip = ETH.localIP();
                ArduinoOtaModule::update((uint32_t)ip);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ===== REAL CPU LOAD MEASUREMENT, SEPARATELY FOR EACH CORE =====
// Technique: two extra tasks, PINNED (xTaskCreatePinnedToCore) to
// core 0 and 1, with a priority IDENTICAL to the system's internal
// idle task (tskIDLE_PRIORITY=0) - so they share CPU time
// EVENLY with the real idle task, without starving it (and it
// handles, among other things, the ESP-IDF internal idle watchdog - starving it
// completely would risk a restart entirely independent of our own
// watchdog in diagnostics/diagnostics.h). Each of them in a loop simply
// counts how many increments it managed in a given time window - the more
// "free" CPU time, the more it manages to count.
//
// CALIBRATION: to turn the raw count into %, we need a reference
// point "this many counts = 0% load (core fully idle)". We measure
// it once, briefly, right after startup (see calibrateCpuLoad() called from
// networkManagerBegin()) - it is an approximation (the system is not
// perfectly "empty" at that moment, because other tasks are starting), but good enough
// for the indicators to make relative sense (rising/falling = the load
// really rises/falls), which was the purpose of this function.
static volatile uint32_t idleTicks0 = 0;
static volatile uint32_t idleTicks1 = 0;
static uint32_t idleBaseline0PerSec = 1; // never 0 - divisor below
static uint32_t idleBaseline1PerSec = 1;

static void idleCounterTask0(void* pvParameters) {
    (void)pvParameters;
    for (;;) { idleTicks0++; }
}
static void idleCounterTask1(void* pvParameters) {
    (void)pvParameters;
    for (;;) { idleTicks1++; }
}

// Call ONCE, shortly after creating the idleCounterTask0/1 tasks - it sets the
// "0% load" baseline for both cores from a window of about 300 ms.
inline void calibrateCpuLoad() {
    idleTicks0 = 0;
    idleTicks1 = 0;
    unsigned long start = millis();
    delay(300);
    // We measure the ACTUAL window time (delay() is not perfectly accurate) and
    // scale PRECISELY (float) to the equivalent of 1 second - an earlier
    // version did this with integer division (1000/300 = 3 instead of
    // 3.33...), lowering the baseline by ~10% and thereby artificially inflating the
    // computed idle % (the load always came out close to 0%).
    unsigned long actualMs = millis() - start;
    if (actualMs == 0) actualMs = 1; // guard against division by zero
    float scale = 1000.0f / (float)actualMs;
    idleBaseline0PerSec = (uint32_t)((idleTicks0 > 0 ? idleTicks0 : 1) * scale);
    idleBaseline1PerSec = (uint32_t)((idleTicks1 > 0 ? idleTicks1 : 1) * scale);
    LogBuffer::printf("[CPU] Calibration: core0=%lu/%lums core1=%lu/%lums -> baseline0=%lu/s baseline1=%lu/s\n",
                      (unsigned long)idleTicks0, actualMs, (unsigned long)idleTicks1, actualMs,
                      (unsigned long)idleBaseline0PerSec, (unsigned long)idleBaseline1PerSec);
}

// Call every main loop pass - once per second it converts the raw counts to a %
// load (100% - % of time spent in our idle-counting
// tasks). The result goes to coreZeroLoadPercent/coreOneLoadPercent.
inline void updateCpuLoad() {
    static unsigned long lastCalc = 0;
    static uint32_t lastTicks0 = 0;
    static uint32_t lastTicks1 = 0;

    if (millis() - lastCalc >= 1000) {
        uint32_t delta0 = idleTicks0 - lastTicks0;
        uint32_t delta1 = idleTicks1 - lastTicks1;
        lastTicks0 = idleTicks0;
        lastTicks1 = idleTicks1;
        lastCalc = millis();

        float idlePct0 = (float)delta0 / (float)idleBaseline0PerSec * 100.0f;
        float idlePct1 = (float)delta1 / (float)idleBaseline1PerSec * 100.0f;
        if (idlePct0 > 100.0f) idlePct0 = 100.0f;
        if (idlePct1 > 100.0f) idlePct1 = 100.0f;

        coreZeroLoadPercent = 100.0f - idlePct0;
        lastLoopLoadPercent = 100.0f - idlePct1;

        LogBuffer::printfIf(DEBUG_SYS, "[CPU] core0: delta=%lu baseline=%lu -> %.2f%% load | core1: delta=%lu baseline=%lu -> %.2f%% load\n",
                            (unsigned long)delta0, (unsigned long)idleBaseline0PerSec, coreZeroLoadPercent,
                            (unsigned long)delta1, (unsigned long)idleBaseline1PerSec, lastLoopLoadPercent); // core 1 - now the REAL %, not a slot budget (the variable name stays the same, to change no more than needed in web_server.h)
    }
}

// true if WiFi was actually started in this session (either because
// Ethernet off in Settings, or because Ethernet did not connect and the fallback kicked in) -
// decides whether networkManagerLoop() should watch over/rebuild WiFi.
static bool wifiActive = false;
// true if we run as our OWN access point (AP) - the last-resort
// fallback when neither Ethernet nor a connection to an existing WiFi network
// succeeded. In this mode we do NOT try to connect anything more in the background -
// the device simply stays reachable as its own SSID until you
// restart it (e.g. after fixing WIFI_SSID/WIFI_PASS and flashing again).
static bool apModeActive = false;

// Initializes the transport layer and opens the UDP sockets. Call once from setup().
inline void networkManagerBegin() {
    LogBuffer::begin();
    History::begin(); // chart history in PSRAM - see diagnostics/history.h
    wifiFbCount = WifiFallback::countAtBoot(esp_reset_reason() == ESP_RST_SW, rtcWifiFbMagic, rtcWifiFbCount);
    rtcWifiFbMagic = 0;                    // one-shot marker - a normal restart does not inherit the counter
    otaStatusBootInit(esp_reset_reason() == ESP_RST_SW);   // whether this boot followed a successful OTA update
    rtcWifiFbCount = wifiFbCount;
    wifiFbMonitor.setBootCount(wifiFbCount);
    bootAfterWifiLoss = (wifiFbCount > 0);
    if (bootAfterWifiLoss) {
        LogBuffer::printf("[NET] Started after a restart caused by WiFi loss (no. %u in a row).\n", (unsigned)wifiFbCount);
    }
    RuntimeSettings::begin(); // MUST come before EthernetHandler/WifiHandler::begin() - see the note in web/runtime_settings.h
    {   // lifetime counters: this start is counted (abnormal = crash, watchdog or power dip)
        const esp_reset_reason_t rr = esp_reset_reason();
        RuntimeSettings::lifeBegin(rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT || rr == ESP_RST_BROWNOUT);
    }

    bool connected = false;
    if (ENABLE_ETHERNET) {
        if (EthernetHandler::begin()) {
            connected = true;
        } else {
            LogBuffer::println("[NET] Ethernet did not connect - switching to WiFi.");
            connected = WifiHandler::begin(WIFI_CONNECT_TIMEOUT_MS);
            wifiActive = true;
        }
    } else {
        connected = WifiHandler::begin(WIFI_CONNECT_TIMEOUT_MS);
        wifiActive = true;
    }

    // LAST-RESORT FALLBACK: neither Ethernet nor WiFi (station) connected -
    // instead of staying completely unreachable, we start our own
    // access point. Connect to it and open the web panel (192.168.4.1) -
    // the "Settings" section now lets you ACTUALLY fix WIFI_SSID/
    // WIFI_PASS on the spot (see web/runtime_settings.h), without reflashing
    // the firmware - only a restart is needed for it to take effect.
    if (!connected) {
        LogBuffer::println("[NET] Neither Ethernet nor WiFi (station) connected - starting the own access point (AP).");
        WifiHandler::startAP();
        apModeActive = true;
        wifiActive = false; // AP is not a "station" - the loop below has nothing to watch/rebuild
    }

    SteerButton::begin();
    WorkSwitch::begin();
    GpsHandler::begin(GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    GpsHandler::autoConfigure(GPS_TARGET_RATE_HZ);
    SteeringAngleSensor::begin();
    ImuSensor::begin(); // will eventually replace the compass - see the comment in sensors/imu_sensor.h
    MotorDriver::begin();
    SectionExpander::begin();
    StatusLed::begin();
    CurrentSensor::begin();
    RtcmRelay::begin();
    NtripClient::begin();
    WebServerModule::begin();
    if (ENABLE_ARDUINO_OTA) {
        ArduinoOtaModule::begin();
    }

    udp8888.begin(MODULE_LISTEN_PORT);
    udp5126.begin(AUX_PORT);

    // Core 0: on ESP32-C6 it is the only available core (it still works, because
    // the separation is decided by the separate task, not a physical core); on ESP32-S3
    // it separates this from the main loop(), which by default runs on core 1.
    xTaskCreatePinnedToCore(webOtaTaskFunction, "WebOTA", 8192, nullptr, 1, nullptr, 0);

    // The idle-counting tasks (see "REAL CPU LOAD MEASUREMENT"
    // above) - unlike webOtaTaskFunction above, HERE we assume
    // two cores (ESP32-S3, the target hardware of this project) - per-core
    // measurement makes no sense on the single-core ESP32-C6, so we do not try
    // to make it portable as with webOtaTaskFunction.
    xTaskCreatePinnedToCore(idleCounterTask0, "IdleCnt0", 1024, nullptr, 0, nullptr, 0);
    xTaskCreatePinnedToCore(idleCounterTask1, "IdleCnt1", 1024, nullptr, 0, nullptr, 1);
    calibrateCpuLoad(); // fine that it blocks for 300 ms - one-time, during boot

    // We arm the watchdog at the VERY END - only now is the whole boot (WiFi,
    // GPS, I2C...) behind us, so no legitimate delay from there can
    // trigger it (see the note in diagnostics/diagnostics.h).
    watchdogBegin();
}

// Call once per loop() pass. Handles incoming PGNs (Hello/Scan)
// and sends heartbeats/PANDA according to the TSS schedule (see pgn_messages.h).
inline void networkManagerLoop() {
    watchdogFeed();
    updateCpuLoad();

    // Ethernet went down WHILE RUNNING (e.g. cable pulled) - but we do NOT
    // switch to WiFi right after the first failed check.
    // We give ETHERNET_DOWN_GRACE_MS to recover on its own BEFORE we start
    // the WiFi radio at all - suspicion (based on logs): turning on
    // WiFi right next to the still-active SPI/Ethernet may ITSELF cause
    // interference (shared power/clock/RF noise) that keeps
    // Ethernet in a bad state - whereas without it this could have been just
    // a brief, self-healing fault. startConnection() is
    // NON-BLOCKING - WifiHandler::maintain() below finishes connecting
    // over the following passes, without freezing the loop.
    //
    // The opposite direction (WiFi -> back to Ethernet) WITHOUT re-initializing
    // the W5500 driver ON THE FLY: every version tried (a full
    // EthernetHandler::begin(), and just WiFi.disconnect() + rebuilding the sockets
    // without a new begin()) ended in a hard crash - Guru Meditation
    // (InstrFetchProhibited, PC=0x0) right after switching, confirmed
    // repeatedly in tests. Returning to Ethernet requires a full, manual
    // restart of the device.
    if (ENABLE_ETHERNET && !wifiActive && !apModeActive) {
        static unsigned long lastEthCheck = 0;
        static unsigned long ethDownSince = 0;
        if (millis() - lastEthCheck >= 500) {
            lastEthCheck = millis();
            if (EthernetHandler::isConnected()) {
                ethDownSince = 0; // link is back - reset the counter so a brief blip does not add up with the next one
            } else {
                if (ethDownSince == 0) ethDownSince = millis();
                if (millis() - ethDownSince >= ETHERNET_DOWN_GRACE_MS) {
                    LogBuffer::println("[NET] Ethernet has been down for a while - starting WiFi as a backup.");
                    WifiHandler::startConnection();
                    wifiActive = true;
                }
            }
        }
    }

    if (wifiActive) {
        uint32_t tWifi = micros();

        // LAST-RESORT FALLBACK WHILE RUNNING: if WiFi (station) does not
        // come back within WIFI_CONNECT_TIMEOUT_MS, we STOP retrying in a loop
        // (which led to endless "wifi:sta is connecting,
        // return error" / reason=201 seen in the logs) and switch
        // to our own access point - the same as at startup (see
        // networkManagerBegin() above), only triggered here while running,
        // when Ethernet goes down AFTER startup and the WiFi station fails too.
        //
        // CHANGE: instead of switching the radio to AP "on the fly" (which ended in a core 0 panic
        // with the web/OTA server running and sockets open) we RESTART the device -
        // startup follows the proven path from networkManagerBegin(): a station attempt for
        // WIFI_CONNECT_TIMEOUT_MS, and if there is still no network - our own AP. See wifi_fallback.h
        // (including the protection against a restart loop).
        // Failover switched to another known network - let the monitor give it the full time before it declares WiFi lost.
        if (WifiHandler::consumeSwitched()) wifiFbMonitor.noteProgress(millis());
        switch (wifiFbMonitor.update(WifiHandler::isConnected(), millis(), WIFI_CONNECT_TIMEOUT_MS)) {
            case WifiFallback::Monitor::RESTART:
                LogBuffer::printf("[NET] WiFi (station) has not been back for %lu s - restarting the device (%u of %u); if there is still no network, the own access point (AP) will start after boot.\n",
                                  wifiFbMonitor.downForMs(millis()) / 1000UL, (unsigned)(wifiFbMonitor.count() + 1), (unsigned)WifiFallback::MAX_RESTARTS);
                rtcWifiFbCount = wifiFbMonitor.count() + 1;
                rtcWifiFbMagic = WifiFallback::MAGIC;
                delay(300);            // finish current tasks (e.g. a journal write)
                ESP.restart();
                break;
            case WifiFallback::Monitor::GIVE_UP:
                LogBuffer::println("[NET] WiFi (station) still not back and earlier restarts did not help - not looping restarts. It keeps trying to connect; the own AP will start after a manual restart if there is no network.");
                break;
            default:
                break;
        }

        if (wifiActive && WifiHandler::maintain()) {
            // WiFi came back after a drop - on ESP32 the existing WiFiUDP sockets are left
            // "orphaned" on the old interface and endPacket() will keep returning fail
            // forever until they are closed and opened again.
            udp8888.stop();
            udp5126.stop();
            udp8888.begin(MODULE_LISTEN_PORT);
            udp5126.begin(AUX_PORT);
            RtcmRelay::restart();
            NetStats::noteUdpRebuild();
            LogBuffer::printlnIf(DEBUG_NET, "[NET] UDP sockets rebuilt after the WiFi reconnect (including RTCM).");
        }
        { static int8_t wifiSlot = -1; if (DEBUG_STEP_TIMING) CpuProfile::add(wifiSlot, "WifiHandler::maintain", micros() - tWifi); }
    }

    NM_STEP("nmCheckUDP(8888)", nmCheckUDP(udp8888));
    NM_STEP("nmCheckUDP(5126)", nmCheckUDP(udp5126));
    NM_STEP("GpsHandler::update", GpsHandler::update());
    NM_STEP("GpsHandler::retryIfNeeded", GpsHandler::retryIfNeeded());
    NM_STEP("SteeringAngleSensor::update", SteeringAngleSensor::update());
    NM_STEP("SteerButton::update", SteerButton::update());
    NM_STEP("WorkSwitch::update", WorkSwitch::update());
    NM_STEP("ImuSensor::update", ImuSensor::update());
    NM_STEP("SectionExpander::update", SectionExpander::update());
    NM_STEP("StatusLed::update", StatusLed::update());
    NM_STEP("CurrentSensor::update", CurrentSensor::update());
    NM_STEP("RtcmRelay::update", RtcmRelay::update());
    NM_STEP("NtripClient::update", NtripClient::update());
    NM_STEP("nmUpdateMotorFromCommand", nmUpdateMotorFromCommand());
    NM_STEP("MotorDriver::update", MotorDriver::update());
    nmPwmDisplay = PwmMath::displayByte(MotorDriver::getLastPwm());
    NM_STEP("nmKickoutTick", nmKickoutTick());
    NM_STEP("nmHwAlertTick", nmHwAlertTick());
    NM_STEP("nmBootSelfCheck", nmBootSelfCheck());
    NM_STEP("steerSettingsTick", RuntimeSettings::steerSettingsTick());
    NM_STEP("machineCfgTick", RuntimeSettings::machineCfgTick());
    NM_STEP("lifeTick", RuntimeSettings::lifeTick(MotorDriver::getLastPwm() != 0, MotorDriver::runawayTrips(), kickoutSig.count(), MotorDriver::deadmanTrips()));
    // In AP mode (see apModeActive above) we do NOT send heartbeats -
    // the broadcast address (AGIO_IP) belongs to your home network
    // (192.168.1.x), and in AP mode we are on a completely different subnet
    // (192.168.4.x) - those sends would never succeed and would only generate
    // useless log noise ([TX FAIL] over and over).
    if (!apModeActive) {
        NM_STEP("nmProcessHeartbeats", nmProcessHeartbeats());
    } else {
        WifiHandler::apPasswordReminder();
    }

    // Store a chart history point (once per second) - regardless of whether any
    // browser is connected.
    NM_STEP("nmHistoryTick", nmHistoryTick());
    NM_STEP("nmEventTick", nmEventTick());

    CpuProfile::tick();

    static unsigned long lastRssiLog = 0;
    static unsigned long loopIterCount = 0;
    static unsigned long loopWindowStart = 0;
    loopIterCount++;
    if (millis() - lastRssiLog >= 1000) {
        lastRssiLog = millis();
        unsigned long windowMs = millis() - loopWindowStart;
        float avgLoopMs = (loopIterCount > 0) ? (float)windowMs / (float)loopIterCount : 0.0f;
        lastAvgLoopMs = avgLoopMs;
        HeapMonitor::trend.sample(millis(), ESP.getFreeHeap());   // free-RAM trend (leak?), see diagnostics/heap_trend.h
        // lastLoopLoadPercent (the real load % of core 1) is now computed
        // in updateCpuLoad() - see "REAL CPU LOAD MEASUREMENT"
        // at webOtaTaskFunction. avgLoopMs remains only for the log below.
        LogBuffer::printfIf(DEBUG_SYS, "[LOOP AVG] %.2fms/loop (%lu loops in %lums, gap=%lums)\n",
                            avgLoopMs, loopIterCount, windowMs, SLOT_DURATION_MS);
        loopIterCount = 0;
        loopWindowStart = millis();

        if (!apModeActive) {
            LogBuffer::printfIf(DEBUG_WIFI, "[RSSI] %d dBm t=%lu ms\n", WiFi.RSSI(), millis());
        }
        LogBuffer::printfIf(DEBUG_GPS, "[GPS] fresh_position=%d fix_quality=%d sat=%d rate=%.1fHz\n",
                            GpsHandler::hasFreshFix(), GpsHandler::getFixQuality(), GpsHandler::getSatelliteCount(),
                            GpsHandler::getMeasuredRateHz());
        LogBuffer::printfIf(DEBUG_SYS, "[SYS] uptime=%lus freeRAM=%uB\n",
                            millis() / 1000, ESP.getFreeHeap());
    }
}
