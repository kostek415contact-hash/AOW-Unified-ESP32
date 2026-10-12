// ======================================================================
// AOW-Unified-ESP32.ino
// EN: Sketch entry point: setup() logs the version and starts the network manager; loop() runs one manager
//     iteration, warns on stalls and yields 1 tick.
// PL: Punkt wejścia szkicu: setup() loguje wersję i uruchamia menedżera sieci; loop() wykonuje jeden obieg,
//     ostrzega o przestojach i oddaje procesor na 1 tick.
// Depends on: config.h, network_manager.h
// ======================================================================
#include <Arduino.h>
#include "config/config.h"
#include "connectivity/network_manager.h"

void setup() {
  MotorDriver::safePins();   // motor driver inputs LOW before anything slow starts (WiFi, GPS...)
  Serial.begin(SERIAL_BAUD);
  LogBuffer::printf("[SYS] AOW-Unified-ESP32 %s (build %s)\n", FIRMWARE_VERSION, FIRMWARE_BUILD);
  networkManagerBegin();
}

void loop() {
  unsigned long loopStart = millis();

  networkManagerLoop();

  unsigned long loopTime = millis() - loopStart;
  if (loopTime > LOOP_STALL_WARN_MS) {
    LogBuffer::printfIf(DEBUG_SYS, "[LOOP STALL] %lu ms\n", loopTime);
  }

  // Yield the CPU for 1 tick (~1 ms). Without this loop() spins non-stop
  // (~3300 passes/s), and the CPU profile showed that most of core 1's time
  // goes to EMPTY polling (UDP parsePacket, RTCM, LED, GPS, IMU - about
  // 20-30 us per call, even when nothing arrived). The data sources
  // have buffers (IMU 100 Hz/2048 B, GPS 20 Hz/4096 B, UDP in lwIP), and the original
  // loop took ~11 ms per pass - a 1 ms sleep is completely safe for them.
  delay(1);
}
