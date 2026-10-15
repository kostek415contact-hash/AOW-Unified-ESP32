#pragma once
// ======================================================================
// config/config.h
// EN: Central configuration: pins, addresses, ports, feature switches, thresholds and default values (panel
//     settings in NVS override them).
// PL: Centralna konfiguracja: piny, adresy, porty, włączniki funkcji, progi i wartości domyślne (ustawienia z
//     panelu w NVS mają pierwszeństwo).
// Used by: AOW-Unified-ESP32.ino, actuators/actuators.h, connectivity/ethernet_handler.h,
//     connectivity/network_manager.h, connectivity/pgn_messages.h, connectivity/pgn_receivers.h,
//     connectivity/wifi_handler.h, diagnostics/diagnostics.h ...
// Depends on: secrets.h
// ======================================================================
// Build date/time - visible in the web panel (System) and in the startup log: the surest way to check which firmware
// is actually running on the device (useful when the local files and what is uploaded drift apart, as once happened).
// A local build uses the compiler's __DATE__/__TIME__ macros. A GitHub release build gets the real build time from CI
// (FIRMWARE_BUILD_STAMP in build_info.h, below), because the ESP32 toolchain may pin those macros to a fixed value.
// Version number - the ONLY place where you change it (semantic versioning: major.minor.patch).
// Shown in the panel (System, Diagnostics) and in the startup log; on a release add an entry to CHANGELOG (the tag and patch number are done by CI).
#define FIRMWARE_MAJOR_MINOR "1.0"   // <- THE ONLY PLACE YOU EDIT BY HAND (major.minor); the last number is added automatically
// Last number (patch): GitHub Actions writes build_info.h with FIRMWARE_PATCH = "N" (and FIRMWARE_BUILD_STAMP) on every release build. A local
// build in the Arduino IDE has no such file and shows "<major.minor>.dev" - a build that is not an official release.
#if __has_include("../build_info.h")
  #include "../build_info.h"
#endif
#ifndef FIRMWARE_PATCH
  #define FIRMWARE_PATCH "dev"
#endif
#ifdef FIRMWARE_BUILD_STAMP
static const char* FIRMWARE_BUILD = FIRMWARE_BUILD_STAMP;
#else
static const char* FIRMWARE_BUILD = __DATE__ " " __TIME__;
#endif
static const char* FIRMWARE_VERSION = FIRMWARE_MAJOR_MINOR "." FIRMWARE_PATCH;
#include <Arduino.h>
#include <IPAddress.h>
#include <WiFiUdp.h>
#include "../sensors/imu_mount.h"

// ===== NETWORK / TRANSPORT =====
// WIFI_SSID/WIFI_PASS/AP_SSID/AP_PASSWORD are now a String, not const char* -
// these are STARTING VALUES (used only on the first run, before you
// save anything). They can be edited live from the web panel (the
// "Settings" section) - see web/runtime_settings.h, which at startup
// overrides them with the version saved in NVS, if one exists.
// --- Secrets (passwords) ---
// We keep them in secrets.h in the project's root directory. That file is in .gitignore, so the password does not end up in
// git. Template: secrets.example.h. When secrets.h is missing, all secrets are empty (WiFi empty -> after startup
// the module starts its own AP and you set the network in the panel; OTA without a password).
#if defined(__has_include)
  #if __has_include("../secrets.h")
    #include "../secrets.h"
  #endif
#endif
#ifndef SECRET_WIFI_SSID
  #define SECRET_WIFI_SSID ""
#endif
#ifndef SECRET_WIFI_PASS
  #define SECRET_WIFI_PASS ""
#endif
#ifndef SECRET_AP_PASSWORD
  #define SECRET_AP_PASSWORD ""   // none given: the module makes a random AP password on first start (see web/runtime_settings.h)
#endif
#ifndef SECRET_OTA_PASSWORD
  #define SECRET_OTA_PASSWORD ""          // empty = /update and ArduinoOTA without a password
#endif
static String WIFI_SSID = SECRET_WIFI_SSID;
static String WIFI_PASS = SECRET_WIFI_PASS;
// Additional, optional networks (several known WiFis - the module connects to the one
// visible with the strongest signal, like a phone). Empty = unused.
// Network no. 1 (above) always stays - this preserves the behaviour from before this change.
static String WIFI_SSID2 = ""; static String WIFI_PASS2 = "";
static String WIFI_SSID3 = ""; static String WIFI_PASS3 = "";
static String WIFI_SSID4 = ""; static String WIFI_PASS4 = "";
// The module obtains the address, gateway and mask ITSELF from the network (DHCP) - this applies to every network, including a phone hotspot, which has a random, changing subnet
// (e.g. 10.20.255.x with gateway 10.20.255.117; addresses differ between phones and sessions). A static address (LOCAL_IP / GATEWAY / SUBNET_MASK below)
// is an OPTION for a single network, e.g. a router without DHCP: tick "force static address" at that slot (1-4). Default: off.
// A change takes effect at the next connection to the network.
static bool WIFI_STATIC[4] = {false, false, false, false};
// How long WifiHandler::begin() waits for a connection to the station (an existing network)
// before it gives up and starts its own access point (AP) - see
// AP_SSID/AP_PASSWORD below and startAP() in wifi_handler.h.
static unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
// Last-resort fallback when neither Ethernet nor a connection to an existing WiFi network
// succeeded - the device starts its OWN access point with this
// SSID/password, so it always stays reachable (e.g. in the field with no coverage of a
// known network). The web panel at the default ESP32 address for AP: 192.168.4.1
static String AP_SSID = "AOG-ESP32-Setup";
static String AP_PASSWORD = SECRET_AP_PASSWORD; // min. 8 characters - a WPA2 requirement; empty here = generated on first start
static bool AP_PASSWORD_AUTO = false;           // true while the AP password is the one generated by the module (printed to Serial)

// LOCAL_IP/GATEWAY/SUBNET_MASK/AGIO_IP are now IPAddress (not const) -
// editable live from the web panel (the "Settings" section), just like
// WIFI_SSID and others - see web/runtime_settings.h. The values here are only
// starting ones. A restart is needed for it to take effect (read once in setup()).
static IPAddress LOCAL_IP(192, 168, 1, 120);
static IPAddress GATEWAY(192, 168, 1, 1);
static IPAddress SUBNET_MASK(255, 255, 255, 0);
static IPAddress AGIO_IP(192, 168, 1, 255);

static const uint16_t AGIO_PORT = 9999;
static const uint16_t MODULE_LISTEN_PORT = 8888;
static const uint16_t AUX_PORT = 5126;

// ===== ETHERNET (W5500) =====
static const uint8_t ETH_CS_PIN = 10;
static const uint8_t ETH_MOSI_PIN = 11;
static const uint8_t ETH_SCK_PIN = 12;
static const uint8_t ETH_MISO_PIN = 13;
// NOTE: since the discovery that a connected RST caused constant transmission errors
// and the link repeatedly dropping (see the comment in ethernet_handler.h) -
// the driver NO LONGER gets this pin (ETH.begin() is called with -1 instead of
// ETH_RST_PIN). This constant stays only as documentation of the physical
// wiring - GPIO14 can be physically left disconnected from the W5500 module.
static const uint8_t ETH_RST_PIN = 14;
// How long Ethernet may be "down" (isConnected()==false) before we
// start WiFi as a backup at all - a test of the hypothesis that merely turning on the
// WiFi radio right next to active SPI/Ethernet may make things worse
// (shared power/clock/RF noise), so we give Ethernet a chance
// to recover on its own from a brief fault before we do that.
static unsigned long ETHERNET_DOWN_GRACE_MS = 10000;

// Ethernet (W5500) is always compiled in; whether it is USED is a runtime setting (web panel: Settings -> Ethernet, needs
// a restart). USE_ETHERNET is only the factory default of that setting (0 = off); with Ethernet off the module behaves
// exactly as before and never touches the W5500.
#ifndef USE_ETHERNET
#define USE_ETHERNET 0
#endif
static bool ENABLE_ETHERNET = (USE_ETHERNET != 0);

using AogUdpClass = WiFiUDP; // ETH.h shares the network stack with WiFi - WiFiUDP works unchanged

// ===== PGN PROTOCOL =====
static const uint8_t SRC_AUTOSTEER = 126;
static const uint8_t SRC_MACHINE   = 123;
static const uint8_t SRC_IMU       = 121;
static const uint8_t SRC_GPS       = 120;
static const uint8_t SRC_GPS2      = 124;

static const uint8_t PGN_HELLO_REQUEST     = 200;
static const uint8_t PGN_SCAN_REQUEST      = 202;
static const uint8_t PGN_SUBNET_REPLY      = 203;
static const uint8_t PGN_AUTOSTEER_HB      = 253;
static const uint8_t PGN_MACHINE_HB        = 237;
static const uint8_t PGN_IMU_HB            = 211;
static const uint8_t PGN_GPS_STATUS_HB     = 218;
static const uint8_t PGN_AUTOSTEER_COMMAND = 254;
static const uint8_t PGN_SENSOR_DATA      = 250; // byte 5 = current/pressure sensor 0-255 (kickout in AgOpenWeb) - see connectivity/kickout_signal.h
static const uint8_t PGN_STEER_SETTINGS    = 252; // gainP,highPWM,lowPWM,minPWM,countsPerDeg,steerOffset,ackermanFix - see connectivity/pgn_receivers.h
static const uint8_t PGN_MACHINE_COMMAND = 239;
static const uint8_t PGN_MACHINE_CONFIG  = 238; // raiseTime, lowerTime, enableToolLift, set0 - see MachineConfigReceiver (connectivity/pgn_receivers.h)
static const uint8_t PGN_MACHINE_PINS    = 236; // 24 pin functions (Sec1..16, HydUp, HydDown, TramLeft, TramRight, GeoStop) - see MachinePinConfigReceiver
static const uint8_t PGN_STEER_CONFIG     = 251; // set0 (invert WAS / relays / motor, driver type ...), trip byte, min speed, set1 - see SteerConfigReceiver
// RTCM wrapped in a standard PGN (0x80 0x81 ... PGN=215 ... RTCM3 data),
// sent to the MAIN port (MODULE_LISTEN_PORT), NOT to the separate RTCM_LISTEN_PORT.
// Confirmed as a second, alternative method of passing RTCM in a
// related project (MrPoke21/AgOpenGPS_ESP32, src/zPackets.cpp case 215) -
// next to the existing raw-broadcast method on port 2233 (RtcmRelay).
// We support BOTH methods in parallel, because we do not know in advance which one
// your version of AgOpenWeb uses.
static const uint8_t PGN_RTCM = 215;

static const unsigned long SLOT_DURATION_MS = 5;
static const uint8_t SLOTS_PER_CYCLE = 40;

// ===== PHYSICAL INPUTS =====
static const uint8_t STEER_BUTTON_PIN = 15;
static const unsigned long STEER_BUTTON_DEBOUNCE_MS = 50;

static const uint8_t WORK_SWITCH_PIN = 18;
static const unsigned long WORK_SWITCH_DEBOUNCE_MS = 50;

// ===== SENSORS =====
static const uint8_t GPS_RX_PIN = 4;
static const uint8_t GPS_TX_PIN = 5;
static const uint32_t GPS_BAUD_RATE = 0; // 0 = auto-detection
static const uint32_t GPS_TARGET_RATE_HZ = 10; // 10 Hz = like gpsUpdateRate in AgOpenWeb; the LG290P also supports 20 Hz + RTK at once

static const uint8_t I2C_SDA_PIN = 8;
static const uint8_t I2C_SCL_PIN = 9;

static const uint8_t ADS1115_I2C_ADDRESS = 0x48;
static const uint8_t STEERING_SENSOR_ADS_CHANNEL = 0;
// RESTORED - "Zero WAS" in AgOpenWeb is currently broken (confirmed:
// it does not zero the displayed angle, WAS Offset grows endlessly with every
// click). We go back to our own calibration on the module as a workaround - see
// sensors/steering_angle_sensor.h. The two below are only the END ANGLES
// (how far the wheel physically turns) - the ADC points themselves (left/right)
// you set through the web panel, not here.
static float STEERING_ANGLE_LEFT_DEG = -45.0f;
static float STEERING_ANGLE_RIGHT_DEG = 45.0f;
// How far outside the calibrated LEFT..RIGHT reading the angle sensor value may be before it is rejected as implausible (a broken or
// loose sensor wire floats to an arbitrary voltage). A fraction of the calibrated span (0.15 = 15%), but never less than 300 ADC counts.
// A rejected reading counts as "no angle": the motor stops at once and the log says why.
static const float STEERING_RAW_RANGE_MARGIN = 0.15f;


// ===== RTCM =====
static const uint16_t RTCM_LISTEN_PORT = 2233;
static const uint16_t RTCM_RELAY_BUFFER_SIZE = 1200;   // an RTCM3 frame is at most 1029 bytes

// ===== OWN NTRIP CLIENT (BYPASSES AgOpenWeb) =====
// This path is COMPLETELY INDEPENDENT of RtcmRelay/PGN215 above -
// instead of waiting for AgOpenWeb to pass on the corrections (which in practice may
// never happen for VRS/multi-type mountpoints, see the documented
// bug in AgOpenGPS PR #1225 - AgOpenWeb probably has the same problem),
// the ESP32 connects to the caster ITSELF, sends GGA and receives RTCM. Off
// by default (it works in parallel with, not instead of, the other methods -
// you can have all of them enabled at once without conflict, each simply
// feeds GpsHandler::writeRtcm() with whatever it happens to receive).
static bool ENABLE_NTRIP_CLIENT = false;
static String NTRIP_HOST = "91.198.76.2";
static uint16_t NTRIP_PORT = 8086;
static String NTRIP_MOUNTPOINT = "RTK4G_MULTI_RTCM32";
static String NTRIP_USER = "";
static String NTRIP_PASS = "";
// How often to send GGA to the caster - VRS/multi-mountpoints REQUIRE
// this in order to start streaming at all (see the rationale above).
// 10 s is a typical, safe value - GGA too often without need is
// unnecessary network traffic, too rarely risks the caster deciding the client is
// dead.
static const unsigned long NTRIP_GGA_INTERVAL_MS = 10000;

// ===== WEB PANEL AND OTA =====
// Reduced from 65536 - copying 64 KB under a critical section (interrupts
// disabled!) on every poll of the web page (/log, every 1 s) was a real
// load, suspected of affecting Ethernet stability under load.
// 8 KB is still several hundred lines of history - quite enough for a live
// preview, and it is 8x cheaper to copy.
static const size_t LOG_BUFFER_SIZE = 8192; // PSRAM

static const char* OTA_HOSTNAME = "AOG-ESP32";
// OTA password: the Arduino IDE asks for it when uploading, and the /update page requires login (user "admin"). Empty: /update then needs the panel PIN/password (if set); the IDE port stays open.
static const char* OTA_PASSWORD = SECRET_OTA_PASSWORD;
static const bool ENABLE_ARDUINO_OTA = true;
// Fast WiFi connecting after an outage (the scan ends at the first network found with the given name). It shortens recovery, but with several access
// points of THE SAME name (mesh) it may pick a weaker one. Not verified on hardware - set false if there is trouble.
// (The name is deliberately different from WIFI_FAST_SCAN: that is the name of an enum value in ESP-IDF.)
static const bool WIFI_FAST_CONNECT = true;

// ===== SECTIONS AND HYDRAULICS (active) =====
static bool ENABLE_SECTION_EXPANDER = true; // editable from the web panel (Settings)
static const uint8_t MCP23017_I2C_ADDRESS = 0x20;
// The hydraulic lift, tram lines and geo-stop are not separate features: they are functions of the MCP23017 pins, chosen in
// AgOpenWeb (Machine -> Pin Config, PGN 236). Until AgOpenWeb sends it, pins 1..16 are Sec1..Sec16. Raise / lower times
// (PGN 238) make the lift outputs timed pulses; time 0 = the output follows the command.

// ===== FUTURE MODULES (inactive - hardware not ordered yet) =====
static bool ENABLE_MOTOR_DRIVER = false;
// Motor direction: the state of the DIR pin depends on how the motor is wired to the wheels. Instead of editing the code, tick in the panel
// (Calibration -> Motor) "Reverse direction" when the runaway detector reports REVERSED DIRECTION.
static bool MOTOR_INVERT_DIR = false;
// Runaway detector (actuators/runaway_guard.h): the motor is driven, and the angle does NOT change in the requested direction => stop.
//   window - how many ms the motor has to move the wheel by at least RUNAWAY_MIN_MOVE_DEG;
//   an error < RUNAWAY_MIN_ERROR_DEG (near the target) is not judged - 3 deg, because a real system can stop a few degrees from the target
//   (backlash, dead zone); real faults (reversed direction, no motion) start with large errors.
static bool ENABLE_RUNAWAY_DETECT = true;
static unsigned long RUNAWAY_WINDOW_MS = 1000;
static float RUNAWAY_MIN_MOVE_DEG = 1.0f;
static float RUNAWAY_MIN_ERROR_DEG = 3.0f;
static const uint8_t MOTOR_PWM_PIN = 16;
static const uint8_t MOTOR_DIR_PIN = 17;

static bool ENABLE_CURRENT_SENSOR = false;
static const uint8_t CURRENT_SENSOR_ADS_CHANNEL = 1;   // ACS71240 output on input A1 of the ADS1115 (channel 0 = angle sensor)
// Motor protection thresholds - changed live in the panel (Calibration -> Current sensor). Sensor: ACS71240KEXBLT-030B3
// (+-30 A, 44 mV/A, zero 1.65 V, accuracy typically +-6%), see sensors/current_math.h. The default values are
// conservative (Cytron MD13S: 13 A continuous) - adjust to your motor after measurements with an ammeter:
//   kickout: current above CURRENT_KICKOUT_A for CURRENT_KICKOUT_MS => motor stop, locked until steering is disabled,
//   hard threshold: >= CURRENT_HARD_LIMIT_A in two consecutive samples => immediately.
static float CURRENT_KICKOUT_A = 8.0f;
static unsigned long CURRENT_KICKOUT_MS = 400;
static float CURRENT_HARD_LIMIT_A = 15.0f;

static bool ENABLE_IMU = true;
// A constant added to yaw. UART-RVC gives yaw RELATIVE TO THE POSITION AT STARTUP
// (not relative to north), and AgOpenWeb computes the IMU<->GPS offset itself (it snaps
// to the GPS course at startup and keeps correcting it) - so this value has no
// bearing on operation. Leave 0.
static float IMU_MOUNTING_OFFSET_DEG = 0.0f;
// Rotation of the IMU board relative to the vehicle axis (degrees) - corrects the MIXING of roll/pitch
// (roll and pitch are absolute, so they depend on how the board sits). Set from the
// web panel: Calibration tab -> "IMU calibration" (tilt the enclosure, the program
// computes the angle itself). 0 = no correction.
static float IMU_MOUNT_ROTATION_DEG = 0.0f;
// Flip the sign of the roll sent to AgOpenWeb (a board mounted the other way round, or roll that tilts the wrong way
// in AgOpenWeb). Changeable in the web panel (Calibration -> IMU), works immediately. false = roll as measured.
static bool IMU_ROLL_INVERT = false;
// Full 3D mounting of the board (sensors/imu_mount.h): the board may sit at ANY fixed angle (e.g. an enclosure on the cab wall with
// the board tilted 45 deg). Set from the web panel: Calibration -> IMU -> "3D mounting" (two poses: level, then tilted sideways).
// When valid it REPLACES the plain board rotation above; heading, pitch and roll are then those of the vehicle.
static ImuMount::Mount IMU_MOUNT3D;   // valid = false: off
// GPIO6 - a free pin, it does not collide with anything currently used. Confirm on the
// silkscreen of your ESP32-S3 board that it is broken out when the
// BNO085 arrives - see uncertainty #3 in sensors/imu_sensor.h.
static const uint8_t IMU_UART_RX_PIN = 6;
// The ESP32 pin connected to the RST input (reset, active low) of the BNO085 board.
// -1 = not used (default; behaviour as before).
// After soldering the RST wire -> GPIO7 set 7 here. The firmware then resets
// the sensor on EVERY start and automatically when the IMU goes silent for >3 s.
// Why: the BNO085 can get stuck in a state that survives an ESP32 restart and even
// a brief power cut (confirmed by measurement: it came back only after about 2
// minutes without power) - the sensor's RST signal alone clears that state immediately.
static int IMU_RESET_PIN = 7;

// Built-in RGB LED (WS2812) on ESP32-S3-N16R8 boards - GPIO38
// (confirmed in the dedicated pinout documentation for exactly this family
// of Waveshare boards "ESP32-S3-DEV-KIT-NxR8" - CircuitState Electronics).
// Earlier we assumed GPIO48 (as on the original Espressif DevKitC-1) -
// that was a DIFFERENT board, hence the LED did not work despite correct code.
//
// UNCERTAINTY: on SOME boards of this family the LED additionally requires
// soldering pads/a jumper on the board (often labelled "RGB" near the
// LED) for it to get electrical power at all - regardless of
// which GPIO drives the data signal. If GPIO38 does not work either,
// check the board from this angle, and possibly try GPIO48
// as an alternative (different revisions/batches of the nominally same board
// are sometimes wired differently - confirmed by many reports from
// users with the identical board model but a different answer).
static bool ENABLE_STATUS_LED = true;
static const uint8_t STATUS_LED_PIN = 38;
// 0-255. Deliberately dimmed by default - full WS2812 brightness can
// dazzle the eyes in a dark cab.
static uint8_t STATUS_LED_BRIGHTNESS = 40;

// ===== DIAGNOSTICS =====
static const unsigned long SERIAL_BAUD = 115200;
static const unsigned long LOOP_STALL_WARN_MS = 50;
// Default 5 s: with a motor on board a hung loop = the motor held at the last PWM, so the restart must
// happen quickly (after the restart PWM = 0). History: the value 25 s (earlier 15 s) was introduced because an observed
// crash had a hang of ~14.4 s - the culprit was the internal W5500 driver thread (w5500_tsk), which under transmission
// errors can take over the loop's core. Ethernet is now disabled by default, so that cause is gone.
// IF you enable the W5500 (panel: Settings -> Ethernet), set the watchdog in the panel back to about 25 s.
// A value stored earlier in memory (Settings -> Thresholds and times) takes precedence - change it in the panel to 5.
// The longest known loop blocks: GPS configuration after a retry (~3 s, it feeds the watchdog between steps).
static uint32_t WATCHDOG_TIMEOUT_S = 5;
// Forgotten panel PIN/password: set 1, flash the firmware (the code will be cleared at startup, the panel will ask for a new one), then go back to 0.
#ifndef PANEL_AUTH_RESET_ON_BOOT
  #define PANEL_AUTH_RESET_ON_BOOT 0
#endif
// All the DEBUG_* below are now bool, not const bool - editable
// live from the web panel (the "Settings" section), with no need to reflash the
// firmware. The values here are only defaults (for the first start) - see
// web/runtime_settings.h.
// Production version: all OFF by default (less loop load and log noise). Enable them in the panel
// (Settings -> Diagnostic logging, or Diagnostics); once enabled, the values are saved in the module's memory
// and take precedence over these defaults.
static bool DEBUG_STEP_TIMING = false;

static bool DEBUG_WIFI              = false;
static bool DEBUG_ETHERNET          = false;
static bool DEBUG_NET               = false;
static bool DEBUG_PGN               = false;
static bool DEBUG_GPS               = false;
static bool DEBUG_RTCM              = false;
static bool DEBUG_STEER_BUTTON      = false;
static bool DEBUG_WORK_SWITCH       = false;
static bool DEBUG_STEERING_SENSOR   = false;
static bool DEBUG_MOTOR             = false;   // Diagnostics -> Motor card and the [MOTOR] log
static bool DEBUG_CURRENT_SENSOR    = false;   // Diagnostics -> Current sensor card and the [CURRENT] log
static bool DEBUG_IMU               = false;
static bool DEBUG_STEER_COMMAND     = false;
static bool DEBUG_STEER_COMMAND_RAW = false;
static bool DEBUG_MACHINE_COMMAND   = false;
static bool DEBUG_SYS               = false;
static bool DEBUG_WEB               = false;

// ======================================================================
// COMPILE-TIME SANITY CHECKS / KONTROLE PRZY KOMPILACJI
// EN: A wiring mistake in the pin constants above (two functions on one pin, a pin the ESP32-S3 needs for flash or USB) is a
//     compile error instead of a motor that does not behave. Ports must differ too, or two sockets would fight for one.
// PL: Pomyłka w stałych pinów (dwie funkcje na jednym pinie, pin zajęty przez flash lub USB w ESP32-S3) to błąd kompilacji, a nie
//     dziwnie zachowujący się silnik. Porty też muszą się różnić, inaczej dwa gniazda biłyby się o jeden.
// ======================================================================
namespace ConfigCheck {
constexpr bool pinUsable(unsigned p) { return p != 0 && p != 19 && p != 20 && !(p >= 26 && p <= 32) && p <= 48; }   // 0 strapping, 19/20 USB, 26-32 flash/PSRAM
constexpr bool allDistinct8(unsigned a, unsigned b, unsigned c, unsigned d, unsigned e, unsigned f, unsigned g, unsigned h) {
    const unsigned v[8] = {a, b, c, d, e, f, g, h};
    for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) if (v[i] == v[j]) return false;
    return true;
}
}  // namespace ConfigCheck
static_assert(ConfigCheck::allDistinct8(MOTOR_PWM_PIN, MOTOR_DIR_PIN, STEER_BUTTON_PIN, WORK_SWITCH_PIN, GPS_RX_PIN, GPS_TX_PIN, I2C_SDA_PIN, I2C_SCL_PIN),
              "config.h: two functions share one pin (motor / buttons / GPS / I2C)");
static_assert(ConfigCheck::allDistinct8(IMU_UART_RX_PIN, STATUS_LED_PIN, ETH_CS_PIN, ETH_MOSI_PIN, ETH_SCK_PIN, ETH_MISO_PIN, MOTOR_PWM_PIN, MOTOR_DIR_PIN),
              "config.h: two functions share one pin (IMU / LED / Ethernet / motor)");
static_assert(ConfigCheck::allDistinct8(IMU_UART_RX_PIN, STATUS_LED_PIN, STEER_BUTTON_PIN, WORK_SWITCH_PIN, GPS_RX_PIN, GPS_TX_PIN, I2C_SDA_PIN, I2C_SCL_PIN),
              "config.h: two functions share one pin (IMU / LED / buttons / GPS / I2C)");
static_assert(ConfigCheck::allDistinct8(ETH_CS_PIN, ETH_MOSI_PIN, ETH_SCK_PIN, ETH_MISO_PIN, STEER_BUTTON_PIN, WORK_SWITCH_PIN, GPS_RX_PIN, GPS_TX_PIN),
              "config.h: two functions share one pin (Ethernet / buttons / GPS)");
static_assert(ConfigCheck::allDistinct8(ETH_CS_PIN, ETH_MOSI_PIN, ETH_SCK_PIN, ETH_MISO_PIN, I2C_SDA_PIN, I2C_SCL_PIN, ETH_RST_PIN, STATUS_LED_PIN),
              "config.h: two functions share one pin (Ethernet / I2C / LED)");
static_assert(ConfigCheck::pinUsable(MOTOR_PWM_PIN) && ConfigCheck::pinUsable(MOTOR_DIR_PIN) && ConfigCheck::pinUsable(STEER_BUTTON_PIN) &&
              ConfigCheck::pinUsable(WORK_SWITCH_PIN) && ConfigCheck::pinUsable(GPS_RX_PIN) && ConfigCheck::pinUsable(GPS_TX_PIN) &&
              ConfigCheck::pinUsable(I2C_SDA_PIN) && ConfigCheck::pinUsable(I2C_SCL_PIN) && ConfigCheck::pinUsable(IMU_UART_RX_PIN) &&
              ConfigCheck::pinUsable(STATUS_LED_PIN) && ConfigCheck::pinUsable(ETH_CS_PIN) && ConfigCheck::pinUsable(ETH_MOSI_PIN) &&
              ConfigCheck::pinUsable(ETH_SCK_PIN) && ConfigCheck::pinUsable(ETH_MISO_PIN),
              "config.h: a pin is reserved on the ESP32-S3 (GPIO0 strapping, GPIO19/20 USB, GPIO26-32 flash/PSRAM)");
static_assert(AGIO_PORT != MODULE_LISTEN_PORT && AGIO_PORT != AUX_PORT && AGIO_PORT != RTCM_LISTEN_PORT &&
              MODULE_LISTEN_PORT != AUX_PORT && MODULE_LISTEN_PORT != RTCM_LISTEN_PORT && AUX_PORT != RTCM_LISTEN_PORT,
              "config.h: two UDP ports are equal");
static_assert(RTCM_RELAY_BUFFER_SIZE >= 512 && RTCM_RELAY_BUFFER_SIZE <= 4096, "config.h: RTCM_RELAY_BUFFER_SIZE out of the sane range");
