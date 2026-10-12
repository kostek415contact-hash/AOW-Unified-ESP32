#pragma once
// ======================================================================
// io/section_expander.h
// EN: MCP23017 section expander: 16 outputs driven by the section state from AgOpenWeb (PGN 239).
// PL: Ekspander sekcji MCP23017: 16 wyjść sterowanych stanem sekcji z AgOpenWeb (PGN 239).
// Used by: connectivity/network_manager.h, web/web_server.h
// Depends on: config.h, i2c_retry.h, log_buffer.h, pgn_receivers.h
// ======================================================================
#include <Wire.h>
#include "../sensors/i2c_retry.h"
#include <Adafruit_MCP23X17.h>
#include "../config/config.h"
#include "../web/log_buffer.h"
#include "../connectivity/pgn_receivers.h"
#include "../actuators/actuators.h"   // HydraulicLift (advanced and written together with the sections)

// MCP23017 section expander - 16 digital outputs, one per section,
// driven by the state received from AgOpenWeb (PGN239, see
// connectivity/pgn_receivers.h). It shares the I2C bus
// with the ADS1115/compass (SDA/SCL from config.h) - Wire.begin() is already called
// by SteeringAngleSensor::begin(), so we do not repeat it here.
class SectionExpander {
private:
    static Adafruit_MCP23X17 mcp;
    static bool found;
    static uint16_t lastWrittenMask;
    static uint16_t logicalMask;     // outputs that are on, before the relay polarity (for the panel)
    static I2cRetry retry;            // re-probe timer while the chip is missing
    static uint32_t lastHealthMs;     // last presence check of a working chip
    static uint8_t  healthMisses;     // consecutive failed presence checks
    static const uint32_t HEALTH_MS = 1000;        // presence check period
    static const uint32_t HEALTH_RETRY_MS = 200;   // after a miss, check again this soon
    static const uint8_t  HEALTH_MISSES_LOST = 2;  // misses in a row => the chip is declared lost

    // An empty write to the chip address: acknowledged = the chip is on the bus (~0.2 ms; at most the
    // 10 ms bus timeout when it is gone).
    static bool present() {
        Wire.beginTransmission((uint8_t)MCP23017_I2C_ADDRESS);
        return Wire.endTransmission() == 0;
    }
    // Configures all 16 pins as outputs and switches every section off. Used at boot and after a recovery.
    static bool initChip() {
        if (!mcp.begin_I2C(MCP23017_I2C_ADDRESS, &Wire)) return false;
        const uint16_t idle = machineIdleMask(MachineConfigReceiver::getInvertRelay());
        // The output latch powers up at 0 (= relay ON on an active-low board): load the idle state into it BEFORE the pins become
        // outputs, otherwise every relay clicks for a few ms at each boot and chip recovery.
        mcp.writeGPIOAB(idle);
        for (uint8_t i = 0; i < 16; i++) {
            mcp.pinMode(i, OUTPUT);
        }
        mcp.writeGPIOAB(idle); // safe startup state - everything off (HIGH on an active-low board, see "Invert relay")
        lastWrittenMask = idle;
        return true;
    }

public:
    static void begin() {
        if (!ENABLE_SECTION_EXPANDER) return;

        found = initChip();
        if (!found) {
            LogBuffer::println("[SECTIONS] MCP23017 not found on I2C - check the address / connection.");
            retry.markLost((uint32_t)millis());   // probed again every few seconds
            return;
        }
        lastHealthMs = millis();

        LogBuffer::printf("[SECTIONS] MCP23017 detected at address 0x%02X.\n", MCP23017_I2C_ADDRESS);
    }

    // Call every loop() pass - it sets all 16 pins with one I2C write
    // (writeGPIOAB), instead of 16 separate digitalWrite() calls (each is
    // a separate I2C transaction) - that loaded the I2C bus on every loop
    // pass, which in the full project (unlike isolated tests)
    // could block the processor often enough to disturb the polling
    // of the Ethernet link state by the W5500 driver (false Link UP/DOWN).
    // In addition we skip the write entirely when the mask has not changed.
    //
    // Which pin carries which function (Sec1..16, HydUp, HydDown, TramLeft/Right, GeoStop) is decided by the AgOpenWeb Pin
    // Config (PGN 236, MachinePinConfigReceiver) and the polarity by "Invert relay" (PGN 238); see io/machine_outputs.h.
    // The hydraulic lift (actuators/actuators.h) is advanced here and written in the SAME writeGPIOAB() as the sections:
    // two independent writeGPIOAB() calls would overwrite each other.
    static void update() {
        if (!ENABLE_SECTION_EXPANDER) { HydraulicLift::idle(); return; }
        uint32_t now = (uint32_t)millis();
        if (!found) {
            // Lost (or never found): a cheap presence probe, a full re-initialisation once it answers.
            if (retry.probeDue(now) && present() && initChip()) {
                found = true;
                healthMisses = 0;
                lastHealthMs = now;
                LogBuffer::println("[SECTIONS] MCP23017 is answering again - sections resumed.");
            }
            return;
        }
        // A working chip is checked once per second; two misses in a row = lost (a chip that dropped off the bus or
        // lost power would otherwise stay "found" and the sections would silently stop following AgOpenWeb).
        if ((uint32_t)(now - lastHealthMs) >= HEALTH_MS) {
            if (present()) { healthMisses = 0; lastHealthMs = now; }
            else {
                lastHealthMs = now - HEALTH_MS + HEALTH_RETRY_MS;   // look again shortly
                if (++healthMisses >= HEALTH_MISSES_LOST) {
                    found = false;
                    retry.markLost(now);
                    LogBuffer::println("[SECTIONS] MCP23017 stopped answering on I2C - sections stopped, probing again every 2 s.");
                    return;
                }
            }
        }

        HydraulicLift::update();   // advances the lift (level or timed pulse)
        MachineInputs in;
        in.fresh = MachineCommandReceiver::isFresh();
        in.sections = MachineCommandReceiver::getSectionsMask();
        in.tram = MachineCommandReceiver::getTramState();
        in.geoStop = MachineCommandReceiver::getGeoStop() != 0;
        in.hydUp = HydraulicLift::isActive();
        in.hydDown = HydraulicLift::isDownActive();
        logicalMask = machineLogicalMask(MachinePinConfigReceiver::map(), in);
        const uint16_t mask = machinePhysicalMask(logicalMask, MachineConfigReceiver::getInvertRelay());

        if (mask == lastWrittenMask) return;

        mcp.writeGPIOAB(mask);
        lastWrittenMask = mask;
    }

    static bool isEnabled() { return ENABLE_SECTION_EXPANDER; }
    static bool isFound() { return found; }
    // Which outputs are on (bit n = pin n+1), before "Invert relay" is applied - for the panel.
    static uint16_t getLogicalMask() { return found ? logicalMask : 0; }
};

Adafruit_MCP23X17 SectionExpander::mcp;
bool SectionExpander::found = false;
uint16_t SectionExpander::lastWrittenMask = 0;
uint16_t SectionExpander::logicalMask = 0;
I2cRetry SectionExpander::retry;
uint32_t SectionExpander::lastHealthMs = 0;
uint8_t SectionExpander::healthMisses = 0;
