#pragma once
// ======================================================================
// connectivity/ethernet_handler.h
// EN: Optional W5500 Ethernet transport (ETH.h, same lwIP stack as WiFi). Off by default; switched on in the web panel
//     (Settings -> Ethernet, after a restart); falls back to WiFi.
// PL: Opcjonalny transport Ethernet W5500 (ETH.h, ten sam stos lwIP co WiFi). Domyślnie wyłączony; włączany w panelu
//     (Ustawienia -> Ethernet, po restarcie); awaryjnie WiFi.
// Used by: connectivity/network_manager.h, web/web_server.h
// Depends on: config.h, log_buffer.h
// ======================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <ETH.h>
#include "../config/config.h"
#include "../web/log_buffer.h"

// Handling of the W5500 module through the built-in ETH.h library (not the separate
// "Ethernet") - on ESP32 3.x cores this is the recommended way, because they share the same
// network stack (lwIP) as WiFi. Thanks to that AogUdpClass = WiFiUDP (see
// config.h) works unchanged - no separate EthernetUDP class is needed.
//
// DEBUGGING HISTORY (so as not to repeat the same attempts):
// - An isolated test (without SectionExpander/WebServer/RtcmRelay/watchdog, without
//   real traffic to AgOpenWeb) worked FLAWLESSLY, for any length of time.
// - The full project catches the link correctly and works stably AT REST (no
//   traffic) - the problem appears ONLY when real, continuous UDP traffic starts to flow
//   (commands from AgOpenWeb + our heartbeats).
// - Symptom: "emac_w5500_transmit: free size (0) < send length" (the chip's
//   transmit buffer full), then a real (confirmed by the RJ45 LED!) loss and
//   return of the physical link in a perfectly regular ~2 s cycle, over and over, with no
//   spontaneous return to a normal state.
// - RULED OUT as the cause: WiFi enabled in parallel (a test with a 10 s grace
//   period, WiFi did not start at all, the cycle continued identically);
//   16x individual I2C writes to the MCP23017 on every pass (batched into
//   a single writeGPIOAB(), no change in the symptom); our own TX FAIL logging
//   (limited to once per 200 ms, no change in the symptom).
// - SUSPECTED, NOT RULED OUT: the web page polled /log every 1 s, and
//   LogBuffer::snapshot() did a 64 KB memcpy INSIDE A CRITICAL SECTION (interrupts
//   disabled) plus a loop of 65536 single characters into a String - a real
//   CPU load on the same core as the WebServer, exactly at the
//   moments when someone was watching the page (i.e. every time this
//   problem was reported). Fixed in log_buffer.h (8 KB buffer, a bulk
//   concat instead of a loop) - not yet confirmed by a test whether this was the
//   actual cause.
// - A "live" repair after an Ethernet failure (a repeated ETH.begin(),
//   WiFi.disconnect()) ALWAYS ended in a hard crash (Guru Meditation,
//   InstrFetchProhibited) - REMOVED. The only confirmed repair is a FULL
//   physical/software restart of the whole ESP32.
//
// A dedicated SPI object (not the global SPI) and a limited frequency (1 MHz,
// lowered gradually from 20 MHz) - the variant most robust to longer wires/
// breadboards than the default (high) frequency that we
// found, but BY ITSELF it did not remove the cyclic failures under load.
static SPIClass ethSPI(HSPI);

class EthernetHandler {
private:
    static volatile bool gotIp;

    static void onEvent(WiFiEvent_t event) {
        switch (event) {
            case ARDUINO_EVENT_ETH_START:
                ETH.setHostname(OTA_HOSTNAME);
                break;
            case ARDUINO_EVENT_ETH_CONNECTED:
                LogBuffer::printlnIf(DEBUG_ETHERNET, "[ETHERNET] Link UP.");
                break;
            case ARDUINO_EVENT_ETH_GOT_IP:
                gotIp = true;
                LogBuffer::printfIf(DEBUG_ETHERNET, "[ETHERNET] IP address: %s\n", ETH.localIP().toString().c_str());
                break;
            case ARDUINO_EVENT_ETH_DISCONNECTED:
                gotIp = false;
                LogBuffer::printlnIf(DEBUG_ETHERNET, "[ETHERNET] Link DOWN.");
                break;
            case ARDUINO_EVENT_ETH_STOP:
                gotIp = false;
                break;
            default:
                break;
        }
    }

public:
    // Returns true if a connection (an IP address) was obtained in time
    // - false if not (the caller may then switch to WiFi).
    static bool begin() {
        WiFi.onEvent(onEvent); // ETH events also go through the same dispatcher

        ethSPI.begin(ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);

        // NOTE: we do not do our own reset of ETH_RST_PIN here - we pass it
        // as a parameter to ETH.begin() below, which manages it itself.
        // Doing the reset MANUALLY and THROUGH THE LIBRARY at the same time can
        // give time-inconsistent behaviour (sometimes it works, sometimes not).
        //
        // BREAKTHROUGH DISCOVERY (isolated test, TEST_W5500_LOAD): physically
        // DISCONNECTING the RST wire from the ESP32 gave a PERFECT result (60
        // packets/s, ZERO errors, ZERO disconnects, for the whole length of
        // the test under full load) - whereas with RST CONNECTED the same
        // test failed almost every packet from the first second. All
        // the rest of the debugging (SPI frequency, send rate, I2C, log
        // buffer, watchdog) was in effect looking in the wrong place -
        // it was this one pin all along. So we pass -1 (no reset pin)
        // instead of ETH_RST_PIN - the driver gets NO control over this
        // pin at all. Most W5500 modules (including this one) have their own
        // power-on-reset circuit on the board (RC), so they do not need a reset
        // driven by the MCU for a proper start.
        if (!ETH.begin(ETH_PHY_W5500, 1, ETH_CS_PIN, -1, -1, ethSPI, 1)) {
            LogBuffer::println("[ETHERNET] ETH.begin() error - check the SPI wiring / W5500 power.");
            return false;
        }

        ETH.config(LOCAL_IP, GATEWAY, SUBNET_MASK); // the same address as for WiFi

        // Extended from 5 s to 15 s - observed in the logs: the link sometimes
        // appears only after more than 20 s from startup (a possible delay of
        // PHY negotiation or Spanning Tree on the switch). The fallback to WiFi
        // at startup is PERMANENT for the whole session (loop() no longer checks
        // Ethernet once wifiActive has been set here) - so it is
        // better to wait longer here than to stay on WiFi for the whole session without
        // need.
        unsigned long start = millis();
        while (!gotIp && millis() - start < 15000) {
            delay(100);
        }

        if (!gotIp) {
            LogBuffer::println("[ETHERNET] No link after 15 s - check the cable / switch.");
        }
        return gotIp;
    }

    static bool isConnected() {
        return gotIp;
    }
};

volatile bool EthernetHandler::gotIp = false;
