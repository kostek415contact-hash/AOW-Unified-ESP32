#pragma once
// ======================================================================
// io/inputs.h
// EN: Two debounced digital inputs: the steering button and the work switch. update() (main loop only) writes,
//     readers are read-only and thread-safe.
// PL: Dwa wejścia cyfrowe z debouncingiem: przycisk prowadzenia i przełącznik pracy. update() (tylko pętla
//     główna) zapisuje, odczyty są tylko do odczytu.
// Used by: connectivity/pgn_messages.h, web/web_server.h
// Depends on: config.h, log_buffer.h
// ======================================================================
#include <Arduino.h>
#include "../config/config.h"
#include "../web/log_buffer.h"

// Two physical input switches, both simple, debounced digital
// inputs - combined in one file because they are almost identical in structure.
//
// update()/isPressed() (or isActive()) are deliberately SEPARATED (not one
// function doing everything): update() - called ONLY from the main
// loop (core 1) - computes the debounce and is the only one that writes the state. A read is a
// pure read, with no writes - safe to call also from the web
// page (which runs on a separate FreeRTOS task, usually core 0) without a race on the
// same state.

// A physical button for manually enabling/disabling steering (autosteer).
// Connected between STEER_BUTTON_PIN and GND, with the ESP32's internal pull-up -
// pressed = low state (LOW), released = high state (HIGH).
class SteerButton {
private:
    static bool debouncedState;
    static bool lastRawState;
    static unsigned long lastChangeTime;

public:
    static void begin() {
        pinMode(STEER_BUTTON_PIN, INPUT_PULLUP);
    }

    // Call every loop() pass - the only place that writes the state.
    static void update() {
        bool rawPressed = (digitalRead(STEER_BUTTON_PIN) == LOW);

        if (rawPressed != lastRawState) {
            lastChangeTime = millis();
            lastRawState = rawPressed;
        }

        if (millis() - lastChangeTime >= STEER_BUTTON_DEBOUNCE_MS) {
            if (debouncedState != rawPressed) {
                debouncedState = rawPressed;
                LogBuffer::printfIf(DEBUG_STEER_BUTTON, "[BUTTON] Steering: %s\n", debouncedState ? "PRESSED" : "released");
            }
        }
    }

    // A pure read - safe from any thread/core.
    static bool isPressed() { return debouncedState; }
};

// A physical work switch - signals whether the implement is
// lowered/working. Connected between WORK_SWITCH_PIN and GND, with the ESP32's internal
// pull-up - pressed/closed = low state = "working".
class WorkSwitch {
private:
    static bool debouncedState;
    static bool lastRawState;
    static unsigned long lastChangeTime;

public:
    static void begin() {
        pinMode(WORK_SWITCH_PIN, INPUT_PULLUP);
    }

    // Call every loop() pass - the only place that writes the state.
    static void update() {
        bool rawActive = (digitalRead(WORK_SWITCH_PIN) == LOW);

        if (rawActive != lastRawState) {
            lastChangeTime = millis();
            lastRawState = rawActive;
        }

        if (millis() - lastChangeTime >= WORK_SWITCH_DEBOUNCE_MS) {
            if (debouncedState != rawActive) {
                debouncedState = rawActive;
                LogBuffer::printfIf(DEBUG_WORK_SWITCH, "[WORK] Switch: %s\n", debouncedState ? "WORKING" : "transport");
            }
        }
    }

    // A pure read - safe from any thread/core.
    static bool isActive() { return debouncedState; }
};

bool SteerButton::debouncedState = false; // idle: pull-up -> HIGH -> not pressed
bool SteerButton::lastRawState = false;
unsigned long SteerButton::lastChangeTime = 0;

bool WorkSwitch::debouncedState = false;
bool WorkSwitch::lastRawState = false;
unsigned long WorkSwitch::lastChangeTime = 0;
