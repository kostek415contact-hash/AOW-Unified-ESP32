#pragma once
// ======================================================================
// connectivity/wifi_handler.h
// EN: WiFi station transport: up to 4 known networks, strongest-network choice, non-blocking reconnect,
//     failover scan, outage timing, AP fallback.
// PL: Transport WiFi (stacja): do 4 znanych sieci, wybór najsilniejszej, nieblokujące wznawianie, skan
//     failover, pomiar zaników, awaryjny AP.
// Used by: connectivity/network_manager.h
// Depends on: config.h, log_buffer.h, settings_lock.h, wifi_outage.h
// ======================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include "../config/config.h"
#include "../web/log_buffer.h"
#include "../web/settings_lock.h"
#include "../diagnostics/wifi_outage.h"

// WiFi transport layer. Counterpart of EthernetHandler for the second
// transport option (see ethernet_handler.h) - both have the same shape
// (begin/isConnected), so network_manager.h can switch between them with
// the Ethernet setting (ENABLE_ETHERNET) without changes anywhere else.
class WifiHandler {
private:
    struct Net { const String* ssid; const String* pass; };
    // 4 slots - order = order in the web panel. Empty ssid = slot skipped.
    // Panel writes (core 0) change the global Strings while the loop (core 1) is running. So the loop
    // works on its OWN snapshot, refreshed under a lock only when the panel saved something (version change).
    static String snapSsid[4];
    static String snapPass[4];
    static bool snapStatic[4];     // forced static address for the slot (default false = DHCP): snapshot like the network names
    static uint32_t snapVersion;
    static const Net* networks() {
        static const Net N[4] = {{&snapSsid[0], &snapPass[0]}, {&snapSsid[1], &snapPass[1]},
        {&snapSsid[2], &snapPass[2]}, {&snapSsid[3], &snapPass[3]}};
        if (snapVersion != SettingsLock::version()) {
            SettingsReadGuard guard;
            snapSsid[0] = WIFI_SSID;  snapPass[0] = WIFI_PASS;
            snapSsid[1] = WIFI_SSID2; snapPass[1] = WIFI_PASS2;
            snapSsid[2] = WIFI_SSID3; snapPass[2] = WIFI_PASS3;
            snapSsid[3] = WIFI_SSID4; snapPass[3] = WIFI_PASS4;
            for (int i = 0; i < 4; i++) snapStatic[i] = WIFI_STATIC[i];
            snapVersion = SettingsLock::version();   // read under the lock: not lower than the version of the write we just waited for
        }
        return N;
    }
    static int currentSlot; // which network from networks() is CURRENTLY being tried/connected (-1 = none)
    static bool eventRegistered; // WiFi.onEvent() only ONCE - every call adds another, duplicate handler

    // --- Diagnostics for the panel (WiFi card). WiFi events arrive from a different task than the
    // main loop, so the counters are atomic. ---
    static std::atomic<uint32_t> lostCount;      // how many times the CONNECTION vanished (was connected -> disconnected)
    static std::atomic<int32_t> lastReason;      // reason code of the last disconnect (-1 = none yet)
    static std::atomic<uint32_t> lastReasonMs;   // millis() of that event (0 = none)
    static std::atomic<bool> linkUp;             // whether the last event was 'connected'
    static uint32_t failoverScans;               // how many scans failover started
    static uint32_t failoverSwitches;            // how many times failover switched to a DIFFERENT network
    static WifiOutage outage;                    // outage durations (main loop writes, web server reads 32-bit numbers)

    // ===== Failover between known networks WHILE RUNNING =====
    // After losing the network (e.g. hotspot/router switched off) the controller keeps retrying the same network. Instead of
    // blindly rotating, after FAILOVER_DELAY_MS we scan (asynchronously - the loop does not wait) and switch
    // to the STRONGEST visible known network. maintain() drives the state machine over successive loop passes.
    enum class Failover : uint8_t { IDLE, SCANNING };
    static Failover failover;
    static unsigned long failoverScanStart;    // when the current scan started
    static unsigned long failoverLastScanEnd;  // when the previous scan ended (0 = none yet)
    static unsigned long lossStart;            // since when the connection has been lost (0 = connected)
    static unsigned long connectStartMillis;   // when we last started connecting (connectToSlot)
    static uint8_t noneVisibleCount;           // how many scans in a row found NO known network
    static bool switchedFlag;                  // set when failover switched to a DIFFERENT network (see consumeSwitched)
    static const unsigned long FAILOVER_DELAY_MS = 4000;       // time the driver gets to recover on its own (e.g. router restart)
    static const unsigned long FAILOVER_SPACING_MS = 8000;     // minimum spacing between scans
    static const unsigned long FAILOVER_SCAN_TIMEOUT_MS = 10000;

    // Logs the REAL disconnect reason reported by the WiFi driver (e.g.
    // beacon timeout, authentication expiry, deauth from the AP) - without it we see
    // only the EFFECT (disconnect), not the CAUSE.
    static void onEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            lastReason.store((int32_t)info.wifi_sta_disconnected.reason);
            lastReasonMs.store(millis() ? millis() : 1);
            if (linkUp.exchange(false)) lostCount++;     // we count connection LOSSES, not every failed attempt
            LogBuffer::printfIf(DEBUG_WIFI, "[WIFI EVENT] Disconnected, reason=%d t=%lu ms\n",
                                info.wifi_sta_disconnected.reason, millis());
        } else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            linkUp.store(true);
            LogBuffer::printfIf(DEBUG_WIFI, "[WIFI EVENT] Connected to AP t=%lu ms\n", millis());
        }
    }

public:
    // Starts connecting to WiFi WITHOUT waiting for the result - maintain() finishes
    // the connection in the background over successive loop() passes. Also used for the fallback
    // WHILE RUNNING (e.g. Ethernet dropped because of the cable) - the blocking begin()
    // below would freeze the whole loop (and the heartbeats to AgOpenWeb) for the duration
    // of connecting, which we do not want here.
    // Connects to ONE specific known slot (by number), without scanning -
    // used by startConnection() (after picking the best one) and by
    // maintain() when moving on to the next network in the list.
    static void connectToSlot(int slot) {
        currentSlot = slot;
        connectStartMillis = millis();
        const Net& n = networks()[slot];
        if (!eventRegistered) { WiFi.onEvent(onEvent); eventRegistered = true; }
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false); // disables radio power saving - avoids
        // latency spikes of 100-300 ms typical for ESP32
        // NOTE: we deliberately do NOT call esp_wifi_set_ps() here - on ESP32-C6, calling
        // it in addition to WiFi.setSleep(false) turned out to destabilize the
        // connection (see the comment in maintain() about reason=15).
        // Ethernet (if it was used) is always properly released
        // (EthernetHandler::end()) BEFORE this function is called - see
        // network_manager.h - so WiFi can safely use the same
        // address; they are never active at the same time.
        // Abort any "connecting" in progress BEFORE changing the configuration: when the driver is in the
        // "connecting" state, esp_wifi_set_config() is rejected ("sta is connecting, cannot set config" -
        // visible in the log), so rotating to the next network did not change the network at all.
        WiFi.disconnect(false, false);
        delay(50);
        // By DEFAULT the module obtains the address, gateway and mask itself (DHCP) - a phone hotspot has a random, changing subnet, so we force nothing.
        // A static address only when the user forced it for this slot. Address 0.0.0.0 in config() RESTORES DHCP - important when moving
        // from a network with a static address to DHCP without a restart.
        const bool useStatic = snapStatic[slot];
        if (useStatic) WiFi.config(LOCAL_IP, GATEWAY, SUBNET_MASK);
        else           WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
        // Fast scan: connecting ends the channel scan at the FIRST network with the given name (instead of going through all
        // channels), which shortens recovery after an outage by about 1-2 s. Sorting by signal picks the strongest access point among
        // those found. NOT verified on hardware (see ROADMAP) - disabled with the WIFI_FAST_CONNECT constant in config.h.
        if (WIFI_FAST_CONNECT) {
            WiFi.setScanMethod(WIFI_FAST_SCAN);
            WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
        } else {
            WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
        }
        WiFi.begin(n.ssid->c_str(), n.pass->c_str());
        LogBuffer::printfIf(DEBUG_WIFI, "[WIFI] Connecting to slot %d: '%s' (%s)\n", slot, n.ssid->c_str(), useStatic ? "static address" : "DHCP: address and gateway from the network");
    }

    // From the scan results (found networks, available via WiFi.SSID(i)/RSSI(i)) picks the slot of the KNOWN network
    // visible with the STRONGEST signal - like a phone between remembered networks. -1 = none.
    static int bestKnownFromResults(int found, int* bestRssiOut = nullptr) {
        int bestSlot = -1, bestRssi = -1000;
        for (int i = 0; i < found; i++) {
            String seen = WiFi.SSID(i);
            for (int s = 0; s < 4; s++) {
                if (networks()[s].ssid->length() == 0) continue;
                if (seen != *networks()[s].ssid) continue;
                int rssi = WiFi.RSSI(i);
                if (rssi > bestRssi) { bestRssi = rssi; bestSlot = s; }
            }
        }
        if (bestRssiOut) *bestRssiOut = bestRssi;
        return bestSlot;
    }

    static int configuredCount() {
        int c = 0;
        for (int s = 0; s < 4; s++) if (networks()[s].ssid->length() > 0) c++;
        return c;
    }
    static int nextConfiguredSlot() {
        for (int i = 1; i <= 4; i++) {
            int cand = (currentSlot + i) % 4;
            if (networks()[cand].ssid->length() > 0) return cand;
        }
        return currentSlot;
    }
    static void switchTo(int slot) {
        if (slot != currentSlot) { switchedFlag = true; failoverSwitches++; }
        connectToSlot(slot);
    }

    // Scans visible networks (BLOCKING - ~2-4 s, acceptable only at boot, BEFORE arming the
    // watchdog - see startConnection()). While running we use the asynchronous scan (maintain()).
    // Returns -1 if NO known network is in range.
    static int scanBestKnownSlot() {
        WiFi.mode(WIFI_STA); // required BEFORE scanNetworks() for a stable scan on ESP32
        int found = WiFi.scanNetworks();
        int bestRssi = -1000;
        int bestSlot = (found > 0) ? bestKnownFromResults(found, &bestRssi) : -1;
        WiFi.scanDelete();
        if (bestSlot >= 0) {
            LogBuffer::printfIf(DEBUG_WIFI, "[WIFI] Scan: chose slot %d ('%s', %d dBm) out of %d visible networks.\n",
                                bestSlot, networks()[bestSlot].ssid->c_str(), bestRssi, found);
        } else {
            LogBuffer::printfIf(DEBUG_WIFI, "[WIFI] Scan: no known network in range (visible: %d).\n", found);
        }
        return bestSlot;
    }

    // Starts an asynchronous scan after the network is lost; maintain() collects the result.
    static void startFailoverScan(unsigned long now) {
        // First we abort any "connecting" in progress: during it a scan may be rejected, and simultaneous
        // connection attempts disturb the channel scan.
        WiFi.disconnect(false, false);
        delay(50);
        int r = WiFi.scanNetworks(true);
        if (r == WIFI_SCAN_FAILED) {
            failoverLastScanEnd = now;
            connectToSlot(currentSlot);            // the scan failed - go back to trying the current network
            return;
        }
        failover = Failover::SCANNING;
        failoverScanStart = now;
        failoverScans++;
        LogBuffer::printfIf(DEBUG_WIFI, "[WIFI] No connection for %lu ms - scanning for another known network.\n", now - lossStart);
    }

    // Starts connecting to the best visible known network WITHOUT waiting for the
    // result - maintain() finishes the connection in the background over successive
    // loop() passes. If no known network is visible, it tries network no. 1
    // anyway (the behaviour from before this change, in case the scan failed/was late).
    static void startConnection() {
        int slot = scanBestKnownSlot();
        connectToSlot(slot >= 0 ? slot : 0);
    }

    // Connects to a WiFi network according to the settings from config.h. Waits at most
    // timeoutMs, then STOPS waiting and returns the result - used only at
    // startup (setup()). Returns true if connected within that time.
    //
    // IMPORTANT: this function used to wait FOREVER (a while loop without
    // a time limit) - without that, in the field with no known network in range,
    // the device would hang here forever and NEVER get to
    // starting its own access point (see startAP() below) -
    // i.e. it would be completely unreachable instead of being configurable.
    static bool begin(unsigned long timeoutMs) {
        startConnection();
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
            delay(200);
        }
        return WiFi.status() == WL_CONNECTED;
    }

    // Last-resort fallback - starts WiFi as its OWN access point (AP),
    // when neither Ethernet nor a connection to an existing WiFi network (station) succeeded
    // (see networkManagerBegin() in network_manager.h).
    // The device always stays reachable - connect with a phone/laptop to
    // its own SSID and open the web panel at the default ESP32 address
    // for AP mode: http://192.168.4.1
    static void startAP() {
        // Clean up after any failed station-mode attempts before
        // switching to AP - without it (switching WiFi.mode() right
        // after many failed STA attempts) the ESP32 WiFi radio can
        // start the AP in an unstable state: visible in a network scan, but
        // rejecting client connections. WiFi.disconnect(true) plus a delay
        // gives the radio a chance to return to a clean state before starting the AP.
        WiFi.disconnect(true, true);
        delay(100);
        WiFi.mode(WIFI_AP);
        delay(100);

        String apSsid, apPass;
        {
            SettingsReadGuard guard;   // copy under the lock - the panel may be saving right now
            apSsid = AP_SSID;
            apPass = AP_PASSWORD;
        }
        bool ok = WiFi.softAP(apSsid.c_str(), apPass.c_str());
        if (ok) {
            LogBuffer::printf("[WIFI] AP mode started: SSID='%s' address=%s\n",
                              apSsid.c_str(), WiFi.softAPIP().toString().c_str());
            // The generated password goes to Serial ONLY (never into the log buffer, which the panel shows to anyone).
            // Written only when it fits without blocking (see the USB-CDC note in web/log_buffer.h).
            if (AP_PASSWORD_AUTO && Serial.availableForWrite() >= 96) {
                Serial.printf("[WIFI] AP password of this device: %s  (change it in the panel: Settings -> Network)\n", apPass.c_str());
            }
        } else {
            LogBuffer::println("[WIFI] ERROR: WiFi.softAP() returned false - the AP did NOT start correctly.");
        }
    }

    // While the module is its own access point with the GENERATED password, repeat the password on the USB serial once a minute.
    // It is printed once at AP start only, and a person who opens the serial monitor later would otherwise never see it (and
    // could not join the AP to reach the panel). Never goes to the log buffer (the panel shows it to anyone).
    static void apPasswordReminder() {
        static unsigned long last = 0;
        if (!AP_PASSWORD_AUTO || millis() - last < 60000UL) return;
        last = millis();
        if (Serial.availableForWrite() < 96) return;   // do not block when nobody reads the USB port
        String apSsid, apPass;
        { SettingsReadGuard guard; apSsid = AP_SSID; apPass = AP_PASSWORD; }
        Serial.printf("[WIFI] AP '%s' password: %s\n", apSsid.c_str(), apPass.c_str());
    }

    static bool isAPMode() {
        return WiFi.getMode() == WIFI_AP;
    }

    static bool isConnected() {
        return WiFi.status() == WL_CONNECTED;
    }

    // Number of the network currently connected/being tried (1-4, as in the web panel) -
    // to show in diagnostics which of several known networks is actually working.
    static int getCurrentSlot() { return currentSlot + 1; }
    // Whether the network we are connecting to NOW uses DHCP (panel: Diagnostics -> WiFi).
    // Called from the web server (core 0): reads only the bool snapshot and does NOT refresh it (the refresh belongs to the loop on core 1;
    // two refreshing threads could corrupt the snapshot Strings).
    static bool currentSlotDhcp() { int s = currentSlot; return s >= 0 && s < 4 && !snapStatic[s]; }

    // true ONCE after failover switched to a DIFFERENT network - network_manager.h uses it
    // so that the restart monitor (wifi_fallback.h) does not restart during such a switch.
    static bool consumeSwitched() { bool s = switchedFlag; switchedFlag = false; return s; }

    // --- getters for the panel ---
    static uint32_t getLostCount() { return lostCount.load(); }
    static int getLastDisconnectReason() { return lastReason.load(); }     // -1 = no disconnect yet
    static long getMsSinceLastDisconnect() {
        uint32_t t = lastReasonMs.load();
        return t == 0 ? -1L : (long)(millis() - t);
    }
    static uint32_t getFailoverScans() { return failoverScans; }
    static uint32_t getFailoverSwitches() { return failoverSwitches; }
    // Duration of connection outages (ms): last finished, longest, average, and current (0 = connected)
    static uint32_t getOutageCount() { return outage.count(); }
    static uint32_t getOutageLastMs() { return outage.lastMs(); }
    static uint32_t getOutageMaxMs() { return outage.maxMs(); }
    static uint32_t getOutageAvgMs() { return outage.avgMs(); }
    static uint32_t getOutageNowMs() { return outage.outageSoFarMs(millis()); }

    // Call on every loop() pass. When WiFi drops, it tries to resume the connection every 3 s.
    // Returns true EXACTLY at the moment the connection is regained
    // (the disconnected -> connected transition) - this is the signal that the UDP
    // sockets must be rebuilt (see the comment in network_manager.h).
    //
    // NOTE: the interval is deliberately NOT aggressive (it used to be 300 ms) - calling
    // WiFi.reconnect() too often right after a disconnect collides with the still
    // ongoing key exchange (4-way handshake) and showed up as a
    // regular "reason=15" about every 5 s, despite a good signal. The simpler, older
    // version of this code (without this loop at all) did not have this problem.
    static bool maintain() {
        static bool wasConnected = true; // we assume begin() already connected
        static unsigned long lastAttempt = 0;
        bool nowConnected = isConnected();
        unsigned long now = millis();

        if (nowConnected) {
            if (failover == Failover::SCANNING) { WiFi.scanDelete(); failover = Failover::IDLE; }
            lossStart = 0;
            noneVisibleCount = 0;
        } else {
            if (lossStart == 0) {
                lossStart = now;
                LogBuffer::printfIf(DEBUG_WIFI, "[WIFI] Connection lost (slot %d) t=%lu ms\n", currentSlot + 1, now);
            }
            if (failover == Failover::SCANNING) {
                int n = WiFi.scanComplete();
                if (n == WIFI_SCAN_RUNNING) {
                    if (now - failoverScanStart > FAILOVER_SCAN_TIMEOUT_MS) {      // the scan got stuck
                        WiFi.scanDelete();
                        failover = Failover::IDLE;
                        failoverLastScanEnd = now;
                        connectToSlot(currentSlot);
                    }
                } else {
                    failover = Failover::IDLE;
                    failoverLastScanEnd = now;
                    int bestRssi = -1000;
                    int best = (n > 0) ? bestKnownFromResults(n, &bestRssi) : -1;
                    WiFi.scanDelete();
                    if (best >= 0) {
                        noneVisibleCount = 0;
                        LogBuffer::printfIf(DEBUG_WIFI, "[WIFI] Scan: best known network is slot %d ('%s', %d dBm)%s.\n", best + 1,
                                            networks()[best].ssid->c_str(), bestRssi, best == currentSlot ? " - the same, retrying" : " - switching to it");
                        switchTo(best);
                    } else {
                        noneVisibleCount++;
                        // Two scans in a row without ANY known network (e.g. a hidden SSID) - we try the next one "blindly".
                        int target = currentSlot;
                        if (noneVisibleCount >= 2) { target = nextConfiguredSlot(); noneVisibleCount = 0; }
                        LogBuffer::printfIf(DEBUG_WIFI, "[WIFI] Scan: no known network is visible - %s (slot %d).\n",
                                            target == currentSlot ? "retrying the current one" : "blindly trying the next one", target + 1);
                        switchTo(target);
                    }
                }
            } else if (configuredCount() > 1 && now - lossStart >= FAILOVER_DELAY_MS &&
                (failoverLastScanEnd == 0 || now - failoverLastScanEnd >= FAILOVER_SPACING_MS)) {
                startFailoverScan(now);
                } else if (now - lastAttempt >= 3000 && now - connectStartMillis >= 6000) {
                    // Ordinary retry (brief outages, e.g. router restart). Not during a freshly
                    // started connection - reconnect() then reports "sta is connecting, return error".
                    lastAttempt = now;
                    WiFi.reconnect();
                }
        }

        bool justReconnected = (!wasConnected && nowConnected);
        const bool recovered = outage.update(nowConnected, now);
        if (recovered) {
            LogBuffer::printfIf(DEBUG_WIFI, "[WIFI] Reconnected after %lu ms (outage no. %lu, longest %lu ms) RSSI=%d\n",
                                (unsigned long)outage.lastMs(), (unsigned long)outage.count(), (unsigned long)outage.maxMs(), WiFi.RSSI());
        } else if (justReconnected) {
            LogBuffer::printfIf(DEBUG_WIFI, "[WIFI] Reconnected t=%lu ms RSSI=%d\n", millis(), WiFi.RSSI());
        }
        wasConnected = nowConnected;
        return justReconnected;
    }
};

int WifiHandler::currentSlot = 0;
bool WifiHandler::eventRegistered = false;
String WifiHandler::snapSsid[4];
String WifiHandler::snapPass[4];
bool WifiHandler::snapStatic[4] = {false, false, false, false};
uint32_t WifiHandler::snapVersion = 0;
std::atomic<uint32_t> WifiHandler::lostCount(0);
std::atomic<int32_t> WifiHandler::lastReason(-1);
std::atomic<uint32_t> WifiHandler::lastReasonMs(0);
std::atomic<bool> WifiHandler::linkUp(false);
uint32_t WifiHandler::failoverScans = 0;
uint32_t WifiHandler::failoverSwitches = 0;
WifiOutage WifiHandler::outage;
WifiHandler::Failover WifiHandler::failover = WifiHandler::Failover::IDLE;
unsigned long WifiHandler::failoverScanStart = 0;
unsigned long WifiHandler::failoverLastScanEnd = 0;
unsigned long WifiHandler::lossStart = 0;
unsigned long WifiHandler::connectStartMillis = 0;
uint8_t WifiHandler::noneVisibleCount = 0;
bool WifiHandler::switchedFlag = false;
