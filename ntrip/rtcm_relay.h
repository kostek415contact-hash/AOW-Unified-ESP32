#pragma once
// ======================================================================
// ntrip/rtcm_relay.h
// EN: Forwards RTCM3 that AgOpenWeb broadcasts on UDP port 2233 straight to the GPS receiver.
// PL: Przekazuje RTCM3 rozsyłane przez AgOpenWeb na porcie UDP 2233 prosto do odbiornika GPS.
// Used by: connectivity/network_manager.h, web/web_server.h
// Depends on: config.h, gps_universal.h, log_buffer.h, pgn_messages.h, rtcm_arbiter.h
// ======================================================================
#include <WiFiUdp.h>
#include "../config/config.h"
#include "../sensors/gps_universal.h"
#include "rtcm_arbiter.h"
#include "../connectivity/pgn_messages.h"   // nmActiveNet, NetAddr::sameSubnet
#include "../web/log_buffer.h"

// AgOpenWeb (NOT our module) does all the NTRIP work: it connects to the caster,
// logs in, sends GGA to the VRS network - and passes the ready RTCM3 corrections on
// as a plain UDP broadcast on port 2233 (see NtripClientService.cs in
// the AgOpenWeb sources: "Forwards RTCM3 corrections to GPS module via UDP
// port 2233"). All we need to do is listen on that port and pass the raw
// bytes straight to the GPS receiver - it works identically regardless of whether
// AgOpenWeb pulls DGPS corrections or full RTK from the caster.
class RtcmRelay {
private:
    static AogUdpClass udp;
    static uint32_t totalBytesRelayed;
    static uint32_t totalPackets;
    static bool everReceivedAnything;
    static uint32_t rejectedPackets;   // packets from outside our subnet (dropped)
    static unsigned long lastSummaryMillis;

public:
    static void begin() {
        udp.begin(RTCM_LISTEN_PORT);
        totalBytesRelayed = 0;
        totalPackets = 0;
        rejectedPackets = 0;
        everReceivedAnything = false;
        lastSummaryMillis = 0;
    }

    // Rebuilds the socket after WiFi dropped/recovered - on ESP32 the existing UDP
    // socket is left "orphaned" on the old interface after a reconnect
    // (the same problem as with udp8888/udp5126 - see network_manager.h).
    // Unlike begin(), it does NOT zero the counters - it is the same
    // logical connection, only the socket is rebuilt.
    static void restart() {
        udp.stop();
        udp.begin(RTCM_LISTEN_PORT);
    }

    static uint32_t getTotalBytesRelayed() { return totalBytesRelayed; }
    static uint32_t getTotalPackets() { return totalPackets; }
    static uint32_t getRejectedPackets() { return rejectedPackets; }

    // Call every loop() pass.
    static void update() {
        int packetSize = udp.parsePacket();
        if (packetSize > 0) {
            uint8_t buffer[RTCM_RELAY_BUFFER_SIZE];
            int len = udp.read(buffer, sizeof(buffer));
            if (packetSize > (int)sizeof(buffer)) len = 0;   // longer than any RTCM3 frame: cut mid-frame it would only confuse the receiver
            // Only from our own subnet: any other source (a rogue access point, a scanner) must not write into the GPS receiver.
            // Without a usable address of our own (AP mode, link not up yet) nothing can be judged, so the packet is not dropped.
            IPAddress myIp, mask, gw;
            nmActiveNet(myIp, mask, gw);
            if (myIp != IPAddress(0, 0, 0, 0) && mask != IPAddress(0, 0, 0, 0) &&
                !NetAddr::sameSubnet((uint32_t)udp.remoteIP(), (uint32_t)myIp, (uint32_t)mask)) {
                rejectedPackets++;
                len = 0;
            }
            // The packet has already been read (we always drain the socket); it goes to the receiver only when our own NTRIP client
            // is not streaming - two streams at once interleave and break RTK.
            if (len > 0 && RtcmArbiter::filterExternal(millis(), (uint32_t)len)) {
                GpsHandler::writeRtcm(buffer, (size_t)len);
                totalBytesRelayed += (uint32_t)len;
                totalPackets++;

                if (!everReceivedAnything) {
                    everReceivedAnything = true;
                    LogBuffer::printlnIf(DEBUG_RTCM, "[RTCM] First corrections from AgOpenWeb - forwarding to GPS.");
                }
            }
        }

        // Summary every 5 s, so as not to flood the log with 1 Hz+ corrections
        if (everReceivedAnything && millis() - lastSummaryMillis >= 5000) {
            lastSummaryMillis = millis();
            LogBuffer::printfIf(DEBUG_RTCM, "[RTCM] Forwarded so far: %lu packets, %lu bytes.\n",
                                (unsigned long)totalPackets, (unsigned long)totalBytesRelayed);
        }
    }
};

AogUdpClass RtcmRelay::udp;
uint32_t RtcmRelay::totalBytesRelayed = 0;
uint32_t RtcmRelay::totalPackets = 0;
uint32_t RtcmRelay::rejectedPackets = 0;
bool RtcmRelay::everReceivedAnything = false;
unsigned long RtcmRelay::lastSummaryMillis = 0;
