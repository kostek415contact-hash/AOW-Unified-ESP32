#pragma once
// ======================================================================
// io/status_led.h
// EN: WS2812 status LED: one colour/blink pattern shows the whole system state with priorities (OTA, AP, no
//     AgOpenWeb, no fix, ...).
// PL: Dioda statusowa WS2812: jeden kolor/miganie pokazuje stan całego systemu wg priorytetów (OTA, AP, brak
//     AgOpenWeb, brak fixa, ...).
// Used by: connectivity/network_manager.h, ota/arduino_ota.h, web/web_server.h
// Depends on: config.h, gps_universal.h, pgn_messages.h, pgn_receivers.h
// ======================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include "../config/config.h"
#include "../connectivity/pgn_receivers.h"
#include "../connectivity/pgn_messages.h"
#include "../sensors/gps_universal.h"

// Built-in RGB LED (WS2812, GPIO38 on ESP32-S3-N16R8, see config.h) - the full status
// at a glance from the cab, without a phone. Priorities from the most important:
//
//   WHITE, fast blinking    - an OTA update is in progress (DO NOT POWER OFF)
//   PURPLE, slow blinking   - AP mode (no normal WiFi connection)
//   RED, steady             - no connection to AgOpenWeb
//   RED, fast blinking      - connected, but the GPS has no fix yet (quality=0)
//   YELLOW, steady          - connected, GPS/DGPS (accuracy too low for driving)
//   BLUE, steady            - connected, RTK FLOAT
//   GREEN, steady           - connected, RTK FIX - ready to drive
//   GREEN, pulsing          - RTK FIX AND you are actively steering (AgOpenWeb sends
//                             steering commands - SteerCommandReceiver::isFresh())
//
// OTA SIGNAL: set from outside by setOtaActive() - hook it into the
// existing ArduinoOTA callbacks (onStart -> setOtaActive(true),
// onEnd/onError -> setOtaActive(false)). We do not guess the names of your
// own OTA functions here - you know where you have them.
//
// AP SIGNAL: WiFi.getMode() == WIFI_AP - a standard Arduino API, always
// available, needs no extra variable from your network code.
//
// AGOPENWEB CONNECTION SIGNAL: nmIsAgioFresh() (pgn_messages.h) - true
// if we received ANY valid packet from AgOpenWeb in the last 2 s.
//
// FIX QUALITY SIGNAL: GpsHandler::getFixQuality() - the standard
// NMEA GGA values (0=none, 1=GPS, 2=DGPS, 4=RTK FIX, 5=RTK FLOAT).
//
// ACTIVE STEERING SIGNAL: SteerCommandReceiver::isFresh() (PGN254) -
// earlier rejected as a signal for "is AgOpenWeb connected" (because
// AgOpenWeb sends this PGN only when AutoSteer is armed/enabled in the
// interface) - but that is EXACTLY what makes it a good signal for THIS
// specific purpose: "is autosteer actively armed".
//
// COLOR ORDER: NEO_RGB, not the standard NEO_GRB - confirmed live
// that this particular LED/board requires it.
class StatusLed {
private:
    static Adafruit_NeoPixel pixel;
    static uint8_t lastState;
    static uint8_t lastBrightness;
    static bool otaActive;

    enum State : uint8_t {
        OTA_ACTIVE = 0,
        AP_MODE,
        NO_AGIO,
        GPS_SEARCHING,
        GPS_LOW_QUALITY,
        RTK_FLOAT,
        RTK_FIX_STEERING,
        RTK_FIX_IDLE
    };

    static State computeState() {
        if (otaActive) return OTA_ACTIVE;
        if (WiFi.getMode() == WIFI_AP) return AP_MODE;
        if (!nmIsAgioFresh()) return NO_AGIO;

        int q = GpsHandler::getFixQuality();
        if (q == 0) return GPS_SEARCHING;
        if (q == 4) return SteerCommandReceiver::isFresh() ? RTK_FIX_STEERING : RTK_FIX_IDLE;
        if (q == 5) return RTK_FLOAT;
        return GPS_LOW_QUALITY;
    }

    static uint32_t baseColor(State s) {
        switch (s) {
            case OTA_ACTIVE:       return pixel.Color(255, 255, 255);
            case AP_MODE:          return pixel.Color(180, 0, 255);
            case GPS_LOW_QUALITY:  return pixel.Color(255, 180, 0);
            case RTK_FLOAT:        return pixel.Color(0, 0, 255);
            case RTK_FIX_STEERING:
            case RTK_FIX_IDLE:     return pixel.Color(0, 255, 0);
            case GPS_SEARCHING:
            case NO_AGIO:
            default:               return pixel.Color(255, 0, 0);
        }
    }

public:
    static void begin() {
        if (!ENABLE_STATUS_LED) {
            LogBuffer::println("[LED] ENABLE_STATUS_LED is off - the LED is inactive.");
            return;
        }
        pixel.begin();
        pixel.setBrightness(STATUS_LED_BRIGHTNESS);
        lastBrightness = STATUS_LED_BRIGHTNESS;
        lastState = 255;
        otaActive = false;
        pixel.setPixelColor(0, pixel.Color(255, 0, 0));
        pixel.show();
        LogBuffer::printf("[LED] Initialised on GPIO%u, brightness=%u (start: red = no connection to AgOpenWeb).\n",
                          STATUS_LED_PIN, STATUS_LED_BRIGHTNESS);
    }

    // Call from the existing ArduinoOTA callbacks:
    //   ArduinoOTA.onStart([]() { StatusLed::setOtaActive(true); });
    //   ArduinoOTA.onEnd([]()   { StatusLed::setOtaActive(false); });
    //   (and the same in onError, so the LED does not stay white forever
    //   after a failed update)
    static void setOtaActive(bool active) {
        otaActive = active;
    }

    static void update() {
        if (!ENABLE_STATUS_LED) return;

        State state = computeState();
        bool stateChanged = (state != lastState) || (STATUS_LED_BRIGHTNESS != lastBrightness);

        bool needsAnimation = (state == OTA_ACTIVE) || (state == AP_MODE) ||
        (state == GPS_SEARCHING) || (state == RTK_FIX_STEERING);

        if (!stateChanged && !needsAnimation) return;

        lastState = state;
        lastBrightness = STATUS_LED_BRIGHTNESS;
        pixel.setBrightness(STATUS_LED_BRIGHTNESS);

        uint32_t color = baseColor(state);
        unsigned long t = millis();

        switch (state) {
            case OTA_ACTIVE:
                pixel.setPixelColor(0, (t % 250 < 125) ? color : pixel.Color(0, 0, 0));
                break;
            case AP_MODE:
                pixel.setPixelColor(0, (t % 1000 < 500) ? color : pixel.Color(0, 0, 0));
                break;
            case GPS_SEARCHING:
                pixel.setPixelColor(0, (t % 300 < 150) ? color : pixel.Color(0, 0, 0));
                break;
            case RTK_FIX_STEERING: {
                float phase = (t % 2000) / 2000.0f * 2.0f * PI;
                uint8_t b = (uint8_t)(64 + 191 * ((sinf(phase) + 1.0f) / 2.0f));
                pixel.setPixelColor(0, pixel.Color(0, b, 0));
                break;
            }
            default:
                pixel.setPixelColor(0, color);
                break;
        }
        pixel.show();
    }
};

Adafruit_NeoPixel StatusLed::pixel(1, STATUS_LED_PIN, NEO_RGB + NEO_KHZ800);
uint8_t StatusLed::lastState = 255;
uint8_t StatusLed::lastBrightness = 0;
bool StatusLed::otaActive = false;
