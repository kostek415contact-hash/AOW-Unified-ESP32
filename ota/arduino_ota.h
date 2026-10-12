#pragma once
// ======================================================================
// ota/arduino_ota.h
// EN: Native Arduino IDE OTA (mDNS, network port) running alongside the panel's /update page; refused while
//     steering is engaged.
// PL: Natywne OTA z Arduino IDE (mDNS, port sieciowy) działające obok strony /update; odrzucane przy włączonym
//     prowadzeniu.
// Used by: connectivity/network_manager.h
// Depends on: config.h, log_buffer.h, ota_keeper.h, pgn_receivers.h, status_led.h
// ======================================================================
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "../config/config.h"
#include "../web/log_buffer.h"
#include "../io/status_led.h"
#include "../connectivity/pgn_receivers.h"
#include "ota_keeper.h"

// Support for native OTA from Arduino IDE 2: after flashing this version once
// over USB, the device announces itself on the network (mDNS) under the name
// OTA_HOSTNAME and appears in the Arduino IDE as a "network port" - subsequent
// uploads are done with a plain Sketch -> Upload, without a cable.
//
// It works IN PARALLEL with /update on the web page (web/web_server.h) - these are two
// independent ways to flash firmware, use whichever is more convenient
// (the IDE - while developing; the web page - e.g. from a phone, without the IDE).
class ArduinoOtaModule {
private:
    static OtaKeeper::State keeper;
    static uint32_t reinitCount;
    static bool inProgress;   // true from onStart to onEnd/onError - then we do NOT block handling, so as not to cut off an upload in progress
    // (Re)starts the OTA listener and the mDNS announcement (network port in the IDE + <name>.local).
    static void startAnnounce() {
        ArduinoOTA.begin();
        MDNS.addService("http", "tcp", 80);
    }
public:
    static uint32_t getReinitCount() { return reinitCount; }
    static void begin() {
        ArduinoOTA.setHostname(OTA_HOSTNAME);
        if (strlen(OTA_PASSWORD) > 0) {
            ArduinoOTA.setPassword(OTA_PASSWORD);
        }

        ArduinoOTA.onStart([]() {
            const char* type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "system (SPIFFS)";
            inProgress = true;
            StatusLed::setOtaActive(true);
            LogBuffer::printfIf(DEBUG_WEB, "[OTA-IDE] Update started: %s\n", type);
        });
        ArduinoOTA.onEnd([]() {
            inProgress = false;
            StatusLed::setOtaActive(false);
            LogBuffer::printlnIf(DEBUG_WEB, "[OTA-IDE] Finished, restarting...");
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            static int8_t lastPct = -1;
            int8_t pct = (total > 0) ? (int8_t)((progress * 100) / total) : 0;
            if (pct != lastPct && pct % 10 == 0) {
                lastPct = pct;
                LogBuffer::printfIf(DEBUG_WEB, "[OTA-IDE] progress: %d%%\n", pct);
            }
        });
        ArduinoOTA.onError([](ota_error_t error) {
            inProgress = false;
            StatusLed::setOtaActive(false);
            LogBuffer::printfIf(DEBUG_WEB, "[OTA-IDE] Error #%u\n", (unsigned)error);
        });

        startAnnounce();
        // ArduinoOTA starts mDNS under the name OTA_HOSTNAME; we add an announcement of the web service so the panel is at
        // http://<OTA_HOSTNAME>.local (no need to look for the IP address, e.g. on a phone hotspot with a random subnet).
        LogBuffer::printfIf(DEBUG_WEB,
                            "[OTA-IDE] Ready - in Arduino IDE 2 look for the network port '%s'%s\n",
                            OTA_HOSTNAME, strlen(OTA_PASSWORD) > 0 ? " (with password)" : "");
    }

    // Call every loop() pass.
    // While steering is ENABLED we do not serve invitations from the Arduino IDE (the IDE will report no response): flashing
    // firmware while driving would stop the module. An update already in progress is finished.
    // ip: the module's current address (0 = no network). On an address change and every OtaKeeper::REFRESH_MS the OTA listener and mDNS
    // are started anew (the mDNS announcement then goes out again) - without this the port in the IDE vanished about 2 min after startup.
    static void update(uint32_t ip) {
        const bool engaged = SteerCommandReceiver::isAutoSteerActive();
        if (!inProgress && engaged) return;
        const uint32_t now = millis();
        const OtaKeeper::Reason why = OtaKeeper::decide(keeper, ip, now, inProgress || engaged);
        if (why != OtaKeeper::NONE && why != OtaKeeper::FIRST_IP) {
            ArduinoOTA.end();
            startAnnounce();
            reinitCount++;
            LogBuffer::printfIf(DEBUG_WEB, "[OTA-IDE] OTA/mDNS announced again (%s)\n",
                                why == OtaKeeper::PERIODIC ? "refresh" : "IP address change");
        }
        if (why != OtaKeeper::NONE) OtaKeeper::markInit(keeper, ip, now);
        ArduinoOTA.handle();
    }
};

bool ArduinoOtaModule::inProgress = false;
OtaKeeper::State ArduinoOtaModule::keeper;
uint32_t ArduinoOtaModule::reinitCount = 0;
