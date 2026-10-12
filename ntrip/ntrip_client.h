#pragma once
// ======================================================================
// ntrip/ntrip_client.h
// EN: Own non-blocking NTRIP client: connects to the caster, sends GGA, receives RTCM and feeds the GPS (works
//     around AgOpenWeb's VRS limitations).
// PL: Własny nieblokujący klient NTRIP: łączy się z casterem, wysyła GGA, odbiera RTCM i podaje do GPS (omija
//     ograniczenia AgOpenWeb przy VRS).
// Used by: connectivity/network_manager.h, web/web_server.h
// Depends on: config.h, gps_universal.h, log_buffer.h, ntrip_logic.h, rtcm_arbiter.h, rtcm_stats.h,
//     settings_lock.h
// ======================================================================
#include "ntrip_logic.h"
#include "rtcm_arbiter.h"
#include "rtcm_stats.h"
#include <Arduino.h>
#include <WiFiClient.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../config/config.h"
#include "../web/log_buffer.h"
#include "../web/settings_lock.h"
#include "../sensors/gps_universal.h"

// Our own, independent NTRIP client - the ESP32 connects to the caster ITSELF
// (91.198.76.2 etc.), sends GGA, receives RTCM and passes it to the GPS.
// AgOpenWeb takes no part in this process at all - this bypasses its
// documented problem with VRS/multi mountpoints (see the rationale
// at ENABLE_NTRIP_CLIENT in config.h).
//
// NTRIP PROTOCOL: a plain HTTP GET with a Basic Auth header, to which the
// caster replies "ICY 200 OK" or "HTTP/1.1 200 OK", and then streams
// raw RTCM3 data (binary) on the same TCP connection until the client
// sends GGA (VRS) - see our own earlier analysis of the
// AgOpenGPS PR #1225 bug.
//
// NON-BLOCKING: all logic is based on a state machine checked every
// loop() pass - no step uses delay() or waits in place,
// in line with the rest of the project (see e.g. WifiHandler).

// RTCM3 frame parser for diagnostics: ntrip/rtcm_stats.h (with CRC24Q verification, host tests).

class NtripClient {
private:
    enum class State { DISCONNECTED, CONNECTING, WAITING_RESPONSE, STREAMING };

    static WiFiClient client;
    static State state;
    static unsigned long stateChangedMillis;
    static unsigned long lastGgaSentMillis;
    static unsigned long lastReconnectAttemptMillis;
    static String responseBuffer;
    // Pauses between attempts (ntrip_logic.h) and caster redirects (301/302/...): up to 3 hops in a row,
    // the redirect address is valid until the first failure, then we return to the configured host.
    static NtripLogic::Backoff backoff;
    static NtripLogic::StallWatch stall;      // a stream that looks open but delivers no RTCM (cut data link)
    static std::atomic<bool> settingsDirty;   // the panel saved new NTRIP data (written on core 0, read in the loop)
    static String redirHost;
    static uint16_t redirPort;
    static uint8_t redirHops;
    static bool redirNow;   // true only until the nearest attempt after a redirect (without waiting for the pause)
    static const uint8_t MAX_REDIRECTS = 3;
    static uint32_t lastRetryDelayMs;

    // Connection/reply failure: remember the kind (the pause until the next attempt depends on it).
    static void failWith(NtripLogic::Kind k) {
        backoff.onFailure(k);
        redirHost = ""; redirHops = 0; redirNow = false;
        lastRetryDelayMs = backoff.delayMs();
        setState(State::DISCONNECTED);
    }

    // TCP connecting to the caster (connect + sending the request) happens in a SHORT TASK on core 0,
    // not in the main loop: WiFiClient::connect() without its own limit waited 3000 ms by default when
    // the caster was unreachable (e.g. a network without internet), which stopped the WHOLE main loop (heartbeats,
    // IMU, steering) for 3007 ms every 10 s - visible in the log as "[LOOP STALL] 3007 ms".
    // In the CONNECTING state the `client` object belongs EXCLUSIVELY to the task; the loop does not touch it.
    struct ConnectJob {
        char host[96];
        uint16_t port;
        char request[512];   // the full HTTP request (GET + headers + Authorization)
    };
    static std::atomic<uint8_t> jobResult;   // 0 = no result, 1 = connected and request sent, 2 = error
    static std::atomic<bool> jobRunning;     // true while the connecting task is running
    static const int32_t CONNECT_TIMEOUT_MS = 3000;
    static const unsigned long RESPONSE_TIMEOUT_MS = 15000;   // the caster accepted TCP but does not respond

    static void connectTask(void* arg) {
        ConnectJob* job = (ConnectJob*)arg;
        bool ok = client.connect(job->host, job->port, CONNECT_TIMEOUT_MS);
        if (ok) {
            size_t n = strlen(job->request);
            ok = (client.write((const uint8_t*)job->request, n) == n);
            if (!ok) client.stop();
        }
        free(job);
        jobResult.store(ok ? 1 : 2, std::memory_order_release);
        jobRunning.store(false, std::memory_order_release);
        vTaskDelete(nullptr);
    }

    static uint32_t totalBytesRelayed;
    static uint32_t totalPackets;

    // Extended diagnostics - see the getters below for an explanation of each.
    static uint32_t reconnectCount;
    static uint32_t realGgaSentCount;
    static uint32_t fallbackGgaSentCount;
    static unsigned long lastStreamingStartMillis;
    static unsigned long msToFirstFloatOrFix; // 0 = not reached yet in this connection
    static unsigned long msToFirstFix;         // 0 = not reached yet (RTK FIX specifically)

    static String base64Encode(const String& input) {
        static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        String out;
        size_t len = input.length();
        out.reserve(((len + 2) / 3) * 4);
        for (size_t i = 0; i < len; i += 3) {
            uint32_t chunk = (uint8_t)input[i] << 16;
            if (i + 1 < len) chunk |= (uint8_t)input[i + 1] << 8;
            if (i + 2 < len) chunk |= (uint8_t)input[i + 2];

            out += alphabet[(chunk >> 18) & 0x3F];
            out += alphabet[(chunk >> 12) & 0x3F];
            out += (i + 1 < len) ? alphabet[(chunk >> 6) & 0x3F] : '=';
            out += (i + 2 < len) ? alphabet[chunk & 0x3F] : '=';
        }
        return out;
    }

    static bool buildFallbackGga(char* out, size_t outSize) {
        if (GpsHandler::getLatitude()[0] == '\0') return false;

        char body[100];
        snprintf(body, sizeof(body),
                 "GPGGA,%s,%s,%c,%s,%c,1,%02d,%.1f,%.1f,M,0.0,M,,",
                 GpsHandler::getTime(), GpsHandler::getLatitude(), GpsHandler::getLatDir(),
                 GpsHandler::getLongitude(), GpsHandler::getLonDir(),
                 GpsHandler::getSatelliteCount(), GpsHandler::getHdop(), GpsHandler::getAltitude());

        uint8_t cs = 0;
        for (size_t i = 0; i < strlen(body); i++) cs ^= (uint8_t)body[i];
        snprintf(out, outSize, "$%s*%02X", body, cs);
        return true;
    }

    static void setState(State s) {
        if (s == State::DISCONNECTED && state == State::STREAMING) {
            reconnectCount++;
        }
        if (s == State::DISCONNECTED && state == State::STREAMING) {
            backoff.onStreamEnded(millis() - lastStreamingStartMillis);   // a short stream does not reset the failure series
            lastRetryDelayMs = backoff.delayMs();
        }
        if (s == State::STREAMING) {
            lastStreamingStartMillis = millis();
            stall.start((uint32_t)millis());
            msToFirstFloatOrFix = 0;
            msToFirstFix = 0;
            RtcmMessageStats::reset();
        }
        state = s;
        stateChangedMillis = millis();
    }

    static void startConnection() {
        if (jobRunning.load(std::memory_order_acquire)) return;   // the previous connecting is still running

        // Copies of the settings UNDER THE LOCK - the web panel (core 0) may be writing them at the same moment, and a String
        // frees its buffer on assignment. From here on we work only on local copies.
        String host, mount, user, pass;
        uint16_t port;
        {
            SettingsReadGuard guard;
            host = NTRIP_HOST; mount = NTRIP_MOUNTPOINT; user = NTRIP_USER; pass = NTRIP_PASS; port = NTRIP_PORT;
        }
        redirNow = false;
        if (redirHost.length() > 0) { host = redirHost; port = redirPort; }   // address from the caster redirect

        LogBuffer::printfIf(DEBUG_RTCM, "[NTRIP-CLIENT] Connecting to %s:%u/%s...\n",
                            host.c_str(), port, mount.c_str());

        ConnectJob* job = (ConnectJob*)malloc(sizeof(ConnectJob));
        if (!job) {
            LogBuffer::println("[NTRIP-CLIENT] Out of memory for the connect request.");
            failWith(NtripLogic::NR_PENDING);
            return;
        }
        // Copies of the settings - the web panel (core 0) may change them during connecting.
        strlcpy(job->host, host.c_str(), sizeof(job->host));
        job->port = port;
        String auth = base64Encode(user + ":" + pass);
        int n = snprintf(job->request, sizeof(job->request),
                         "GET /%s HTTP/1.0\r\nUser-Agent: NTRIP ESP32AutosteerModule/2.0\r\nAccept: */*\r\nAuthorization: Basic %s\r\n\r\n",
                         mount.c_str(), auth.c_str());
        if (n <= 0 || (size_t)n >= sizeof(job->request)) {
            LogBuffer::println("[NTRIP-CLIENT] Caster request too long (mountpoint / login data) - not connecting.");
            free(job);
            failWith(NtripLogic::NR_OTHER);
            return;
        }

        jobResult.store(0, std::memory_order_release);
        jobRunning.store(true, std::memory_order_release);
        if (xTaskCreatePinnedToCore(connectTask, "ntripConn", 6144, job, 1, nullptr, 0) != pdPASS) {
            jobRunning.store(false, std::memory_order_release);
            free(job);
            LogBuffer::println("[NTRIP-CLIENT] Could not create the connect task.");
            failWith(NtripLogic::NR_PENDING);
            return;
        }
        setState(State::CONNECTING);
    }

    // Tracks the time from the start of streaming to the first
    // FLOAT/FIX (and separately to FIX alone) - once per connection
    // (reset in setState() at every new STREAMING).
    static void trackFixTiming() {
        if (lastStreamingStartMillis == 0) return;
        int q = GpsHandler::getFixQuality();
        if (msToFirstFloatOrFix == 0 && (q == 4 || q == 5)) {
            msToFirstFloatOrFix = millis() - lastStreamingStartMillis;
        }
        if (msToFirstFix == 0 && q == 4) {
            msToFirstFix = millis() - lastStreamingStartMillis;
        }
    }

public:
    static void begin() {
        state = State::DISCONNECTED;
        stateChangedMillis = 0;
        lastGgaSentMillis = 0;
        lastReconnectAttemptMillis = 0;
        totalBytesRelayed = 0;
        totalPackets = 0;
        reconnectCount = 0;
        backoff.reset(); redirHost = ""; redirHops = 0; redirNow = false; lastRetryDelayMs = 0;
        realGgaSentCount = 0;
        fallbackGgaSentCount = 0;
        lastStreamingStartMillis = 0;
        msToFirstFloatOrFix = 0;
        msToFirstFix = 0;
    }

    static bool isStreaming() { return state == State::STREAMING; }
    // Called by the web panel after NTRIP settings were saved: the next pass drops the old connection (if any), forgets the
    // failure series and the caster redirect, and connects with the new data at once instead of after the old pause.
    static void settingsChanged() { settingsDirty.store(true, std::memory_order_release); }
    static uint32_t getTotalBytesRelayed() { return totalBytesRelayed; }
    static uint32_t getTotalPackets() { return totalPackets; }

    // --- Extended diagnostics ---
    static uint32_t getReconnectCount() { return reconnectCount; }
    static uint8_t getFailStreak() { return backoff.fails(); }
    static uint8_t getLastFailKind() { return (uint8_t)backoff.lastKind(); }   // 0 network, 3 bad password, 4 no mountpoint, 5 source table, 6 other
    static uint32_t getRetryDelayMs() { return lastRetryDelayMs; }
    static bool isRedirected() { return redirHost.length() > 0; }
    static uint32_t getRealGgaSentCount() { return realGgaSentCount; }
    static uint32_t getFallbackGgaSentCount() { return fallbackGgaSentCount; }
    // Returns 0 if not yet reached in the CURRENT connection (or
    // we are not connected at all) - distinguish this from a real 0 ms.
    static unsigned long getMsToFirstFloatOrFix() { return msToFirstFloatOrFix; }
    static unsigned long getMsToFirstFix() { return msToFirstFix; }

    // Call every loop() pass.
    static void update() {
        if (ENABLE_NTRIP_CLIENT) trackFixTiming();

        // Result of the connecting task (if it has just finished).
        if (!jobRunning.load(std::memory_order_acquire)) {
            uint8_t r = jobResult.exchange(0, std::memory_order_acq_rel);
            if (r != 0) {
                if (state == State::CONNECTING) {
                    if (r == 1) {
                        responseBuffer = "";
                        setState(State::WAITING_RESPONSE);
                    } else {
                        LogBuffer::printlnIf(DEBUG_RTCM, "[NTRIP-CLIENT] Could not connect (TCP).");
                        failWith(NtripLogic::NR_PENDING);
                    }
                } else if (r == 1) {
                    client.stop();   // orphaned result (state changed in the meantime, e.g. begin()) - close the connection
                }
            }
        }

        // New settings: do not touch `client` while the connecting task owns it (CONNECTING) - handled right after it ends.
        if (state != State::CONNECTING && settingsDirty.exchange(false, std::memory_order_acq_rel)) {
            if (state == State::WAITING_RESPONSE || state == State::STREAMING) client.stop();
            setState(State::DISCONNECTED);
            backoff.reset(); redirHost = ""; redirHops = 0; redirNow = false;
            lastRetryDelayMs = 0; lastReconnectAttemptMillis = 0;
            LogBuffer::printlnIf(DEBUG_RTCM, "[NTRIP-CLIENT] Settings changed - reconnecting with the new data.");
        }

        // Switched off in the panel: close an open connection (before, it stayed open and kept streaming into the GPS).
        if (!ENABLE_NTRIP_CLIENT) {
            if (state == State::WAITING_RESPONSE || state == State::STREAMING) {
                client.stop();
                setState(State::DISCONNECTED);
            }
            return;
        }

        switch (state) {
            case State::CONNECTING:
                break;   // the task is running - the loop does not wait and does not touch `client`

            case State::DISCONNECTED:
                if (GpsHandler::getFixQuality() <= 0) {
                    break;
                }
                if (millis() - lastReconnectAttemptMillis >= (redirNow ? 0UL : (unsigned long)backoff.delayMs())) {
                    lastReconnectAttemptMillis = millis();
                    startConnection();
                }
                break;

            case State::WAITING_RESPONSE: {
                while (client.available()) {
                    char c = client.read();
                    responseBuffer += c;
                    NtripLogic::Reply rp = NtripLogic::parse(responseBuffer.c_str(), responseBuffer.length());
                    if (rp.kind == NtripLogic::NR_PENDING) {
                        if (responseBuffer.length() > 2048) responseBuffer = "";   // long redirect or cookie headers fit; a runaway reply is dropped
                        continue;
                    }
                    if (rp.kind == NtripLogic::NR_OK) {
                        LogBuffer::println("[NTRIP-CLIENT] Connected and authorised - waiting for RTCM data.");
                        redirHops = 0;
                        setState(State::STREAMING);
                        lastGgaSentMillis = 0;
                    } else if (rp.kind == NtripLogic::NR_REDIRECT && redirHops < MAX_REDIRECTS) {
                        redirHops++;
                        redirHost = rp.host; redirPort = rp.port;
                        LogBuffer::printf("[NTRIP-CLIENT] Redirect (%d) to %s:%u - reconnecting.\n", rp.code, rp.host, (unsigned)rp.port);
                        client.stop();
                        setState(State::DISCONNECTED);
                        redirNow = true;   // next attempt immediately (host from the redirect)
                    } else {
                        const char* why = rp.kind == NtripLogic::NR_AUTH ? "wrong login data (next attempt after a longer pause)"
                                        : rp.kind == NtripLogic::NR_NOT_FOUND ? "no such mountpoint"
                                        : rp.kind == NtripLogic::NR_SOURCETABLE ? "the caster returned the source list - check the mountpoint"
                                        : rp.kind == NtripLogic::NR_REDIRECT ? "too many redirects"
                                        : "unsupported response";
                        LogBuffer::printf("[NTRIP-CLIENT] Rejected: %s. Response: %.80s\n", why, responseBuffer.c_str());
                        client.stop();
                        failWith(rp.kind);
                    }
                    responseBuffer = "";
                    break;
                }
                if (!client.connected() && state == State::WAITING_RESPONSE) {
                    LogBuffer::printlnIf(DEBUG_RTCM, "[NTRIP-CLIENT] Connection dropped while waiting for the response.");
                    failWith(NtripLogic::NR_PENDING);
                }
                // The caster accepted the TCP connection but does not respond: without this the client would hang here forever.
                if (state == State::WAITING_RESPONSE && millis() - stateChangedMillis >= RESPONSE_TIMEOUT_MS) {
                    LogBuffer::println("[NTRIP-CLIENT] The caster did not respond within 15 s - closing and retrying.");
                    client.stop();
                    failWith(NtripLogic::NR_PENDING);
                }
                break;
            }

            case State::STREAMING:
                if (!client.connected()) {
                    LogBuffer::printlnIf(DEBUG_RTCM, "[NTRIP-CLIENT] Connection dropped - retrying.");
                    client.stop();
                    setState(State::DISCONNECTED);
                    break;
                }

                if (stall.stalled((uint32_t)millis())) {
                    LogBuffer::printf("[NTRIP-CLIENT] No RTCM data for %lu s although the connection looks open - closing and reconnecting.\n",
                                      (unsigned long)(NtripLogic::StallWatch::STALL_MS / 1000UL));
                    client.stop();
                    setState(State::DISCONNECTED);
                    break;
                }

                if (millis() - lastGgaSentMillis >= NTRIP_GGA_INTERVAL_MS) {
                    lastGgaSentMillis = millis();
                    if (GpsHandler::getFixQuality() > 0) {
                        const char* gga = GpsHandler::getLastRawGga(5000);
                        if (gga[0] != '\0') {
                            client.print(gga);
                            client.print("\r\n");
                            realGgaSentCount++;
                            LogBuffer::printfIf(DEBUG_RTCM, "[NTRIP-CLIENT] GGA sent: %s\n", gga);
                        } else {
                            LogBuffer::printlnIf(DEBUG_RTCM, "[NTRIP-CLIENT] No fresh GGA from the GPS - skipping this cycle.");
                        }
                    } else {
                        char fallback[100];
                        if (buildFallbackGga(fallback, sizeof(fallback))) {
                            client.print(fallback);
                            client.print("\r\n");
                            fallbackGgaSentCount++;
                            LogBuffer::printfIf(DEBUG_RTCM, "[NTRIP-CLIENT] No fix - sent a fallback GGA from the last known position: %s\n", fallback);
                        } else {
                            LogBuffer::printlnIf(DEBUG_RTCM, "[NTRIP-CLIENT] No fix and no earlier position at all - skipping this cycle.");
                        }
                    }
                }

                if (client.available()) {
                    uint8_t buffer[512];
                    int len = client.read(buffer, sizeof(buffer));
                    if (len > 0) {
                        stall.onData((uint32_t)millis());
                        GpsHandler::writeRtcm(buffer, (size_t)len);
                        RtcmArbiter::noteOwn(millis());   // our own stream is flowing - AgOpenWeb sources are then rejected
                        RtcmMessageStats::feed(buffer, (size_t)len);
                        totalBytesRelayed += (uint32_t)len;
                        totalPackets++;
                    }
                }
                break;
        }
    }
};

WiFiClient NtripClient::client;
std::atomic<uint8_t> NtripClient::jobResult(0);
std::atomic<bool> NtripClient::jobRunning(false);
NtripClient::State NtripClient::state = NtripClient::State::DISCONNECTED;
unsigned long NtripClient::stateChangedMillis = 0;
unsigned long NtripClient::lastGgaSentMillis = 0;
unsigned long NtripClient::lastReconnectAttemptMillis = 0;
String NtripClient::responseBuffer = "";
uint32_t NtripClient::totalBytesRelayed = 0;
uint32_t NtripClient::totalPackets = 0;
uint32_t NtripClient::reconnectCount = 0;
NtripLogic::Backoff NtripClient::backoff;
NtripLogic::StallWatch NtripClient::stall;
std::atomic<bool> NtripClient::settingsDirty(false);
String NtripClient::redirHost = "";
uint16_t NtripClient::redirPort = 0;
uint8_t NtripClient::redirHops = 0;
bool NtripClient::redirNow = false;
uint32_t NtripClient::lastRetryDelayMs = 0;
uint32_t NtripClient::realGgaSentCount = 0;
uint32_t NtripClient::fallbackGgaSentCount = 0;
unsigned long NtripClient::lastStreamingStartMillis = 0;
unsigned long NtripClient::msToFirstFloatOrFix = 0;
unsigned long NtripClient::msToFirstFix = 0;
