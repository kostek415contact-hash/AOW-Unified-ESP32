#pragma once
// ======================================================================
// web/runtime_settings.h
// EN: Loads/saves the settings editable in the panel (network, NTRIP, features, thresholds, debug flags) in
//     NVS.
// PL: Wczytuje i zapisuje ustawienia edytowalne z panelu (sieć, NTRIP, funkcje, progi, flagi debug) w NVS.
// Used by: connectivity/network_manager.h, web/web_server.h
// Depends on: agio_senders.h, config.h, log_buffer.h, panel_auth.h, pgn_receivers.h, settings_lock.h,
//     settings_validation.h
// ======================================================================
#include <Arduino.h>
#include <Preferences.h>
#include "../config/config.h"
#include "log_buffer.h"
#include "settings_validation.h"
#include "settings_lock.h"
#include "../connectivity/agio_senders.h"
#include "../connectivity/pgn_receivers.h"
#include "panel_auth.h"
#include "../connectivity/ap_password.h"
#include "../diagnostics/lifetime_stats.h"

// Loads/saves in NVS ("settings") whatever is editable from the web panel
// (the "Settings" section) without reflashing the firmware: network data
// (WIFI_SSID/WIFI_PASS/AP_SSID/AP_PASSWORD) and all the DEBUG_* flags.
//
// The pattern is identical to the angle calibration (see
// sensors/steering_angle_sensor.h) - the values in config.h are only defaults
// (for the first start), begin() below overrides them with the saved version, if
// one exists.
//
// IMPORTANT: begin() must be called VERY EARLY in networkManagerBegin()
// - BEFORE EthernetHandler::begin()/WifiHandler::begin() - otherwise those
// two functions would already have read the (old, default) WIFI_SSID/PASS
// before we could override them with the saved version.
//
// Changing the network data (WIFI_SSID/PASS/AP_*) requires a device RESTART
// to take effect - EthernetHandler/WifiHandler read them only once, in
// setup(). Changing the DEBUG_* flags works IMMEDIATELY, without a restart - they are read
// on the fly at every LogBuffer::printfIf().
class RuntimeSettings {
private:
    static Preferences prefs;
    static LifetimeStats life;   // counters that outlive restarts (diagnostics/lifetime_stats.h), kept in NVS keys lsBoots ... lsDead

public:
    static void begin() {
        SettingsLock::begin();   // lock for the settings texts (panel on core 0 vs loop on core 1)
        prefs.begin("settings", false);

        if (prefs.isKey("wifiSsid")) WIFI_SSID = prefs.getString("wifiSsid", WIFI_SSID);
        if (prefs.isKey("wifiPass")) WIFI_PASS = prefs.getString("wifiPass", WIFI_PASS);
        if (prefs.isKey("wifiSsid2")) WIFI_SSID2 = prefs.getString("wifiSsid2", WIFI_SSID2);
        if (prefs.isKey("wifiPass2")) WIFI_PASS2 = prefs.getString("wifiPass2", WIFI_PASS2);
        if (prefs.isKey("wifiSsid3")) WIFI_SSID3 = prefs.getString("wifiSsid3", WIFI_SSID3);
        if (prefs.isKey("wifiPass3")) WIFI_PASS3 = prefs.getString("wifiPass3", WIFI_PASS3);
        if (prefs.isKey("wifiSsid4")) WIFI_SSID4 = prefs.getString("wifiSsid4", WIFI_SSID4);
        if (prefs.isKey("wifiPass4")) WIFI_PASS4 = prefs.getString("wifiPass4", WIFI_PASS4);
        if (prefs.isKey("wStatic1")) WIFI_STATIC[0] = prefs.getBool("wStatic1", WIFI_STATIC[0]);
        if (prefs.isKey("wStatic2")) WIFI_STATIC[1] = prefs.getBool("wStatic2", WIFI_STATIC[1]);
        if (prefs.isKey("wStatic3")) WIFI_STATIC[2] = prefs.getBool("wStatic3", WIFI_STATIC[2]);
        if (prefs.isKey("wStatic4")) WIFI_STATIC[3] = prefs.getBool("wStatic4", WIFI_STATIC[3]);
        if (prefs.isKey("apSsid")) AP_SSID = prefs.getString("apSsid", AP_SSID);
        if (prefs.isKey("apPass")) AP_PASSWORD = prefs.getString("apPass", AP_PASSWORD);
        // No AP password from secrets.h and none saved: make a random one for THIS device and keep it. The AP is the
        // only way in when no known network is in range, so a shared public default would be a hole.
        if (!ApPassword::usable(AP_PASSWORD.c_str())) {
            char gen[ApPassword::LENGTH + 1];
            ApPassword::generate([]() -> uint32_t { return esp_random(); }, gen);
            AP_PASSWORD = gen;
            prefs.putString("apPass", AP_PASSWORD);
            prefs.putBool("apAuto", true);
        }
        AP_PASSWORD_AUTO = prefs.getBool("apAuto", false);

        // IP addresses stored as text ("192.168.1.120") - IPAddress has
        // built-in parsing of this format (fromString).
        IPAddress tmp;
        if (prefs.isKey("localIp") && tmp.fromString(prefs.getString("localIp"))) LOCAL_IP = tmp;
        if (prefs.isKey("gateway") && tmp.fromString(prefs.getString("gateway"))) GATEWAY = tmp;
        if (prefs.isKey("subnet") && tmp.fromString(prefs.getString("subnet"))) SUBNET_MASK = tmp;
        if (prefs.isKey("agioIp") && tmp.fromString(prefs.getString("agioIp"))) AGIO_IP = tmp;
        // AgOpenWeb sender pinned in the panel (Settings -> AgOpenWeb senders); no entry = automatic mode.
#if PANEL_AUTH_RESET_ON_BOOT
        prefs.remove("authMode"); prefs.remove("authSecret");
        LogBuffer::println("[PANEL] PANEL_AUTH_RESET_ON_BOOT: the panel code was cleared.");
#endif
        PanelAuth::load(prefs.getUChar("authMode", PanelAuth::NOT_SET), prefs.getString("authSecret", "").c_str());
        if (prefs.isKey("agioPin") && tmp.fromString(prefs.getString("agioPin"))) AgioSenders::setPinned((uint32_t)tmp);
        SteerCommandReceiver::setEngagedMask(prefs.getUChar("agioEng", 0x01));   // how AgOpenWeb marks steering as enabled

        // Feature switches - they work immediately (read every loop pass),
        // without a restart.
        ENABLE_SECTION_EXPANDER = prefs.getBool("enSection", ENABLE_SECTION_EXPANDER);
        ENABLE_MOTOR_DRIVER = prefs.getBool("enMotor", ENABLE_MOTOR_DRIVER);
        ENABLE_CURRENT_SENSOR = prefs.getBool("enCurrent", ENABLE_CURRENT_SENSOR);
        ENABLE_RUNAWAY_DETECT = prefs.getBool("runEn", ENABLE_RUNAWAY_DETECT);
        RUNAWAY_WINDOW_MS = prefs.getULong("runMs", RUNAWAY_WINDOW_MS);
        RUNAWAY_MIN_MOVE_DEG = prefs.getFloat("runDeg", RUNAWAY_MIN_MOVE_DEG);
        RUNAWAY_MIN_ERROR_DEG = prefs.getFloat("runErr", RUNAWAY_MIN_ERROR_DEG);
        MOTOR_INVERT_DIR = prefs.getBool("motInv", MOTOR_INVERT_DIR);
        CURRENT_KICKOUT_A = prefs.getFloat("curKickA", CURRENT_KICKOUT_A);
        CURRENT_KICKOUT_MS = prefs.getULong("curKickMs", CURRENT_KICKOUT_MS);
        CURRENT_HARD_LIMIT_A = prefs.getFloat("curHardA", CURRENT_HARD_LIMIT_A);
        ENABLE_IMU = prefs.getBool("enImu", ENABLE_IMU);
        ENABLE_NTRIP_CLIENT = prefs.getBool("enNtrip", ENABLE_NTRIP_CLIENT);
        if (prefs.isKey("ntripHost")) NTRIP_HOST = prefs.getString("ntripHost", NTRIP_HOST);
        NTRIP_PORT = prefs.getUShort("ntripPort", NTRIP_PORT);
        if (prefs.isKey("ntripMount")) NTRIP_MOUNTPOINT = prefs.getString("ntripMount", NTRIP_MOUNTPOINT);
        if (prefs.isKey("ntripUser")) NTRIP_USER = prefs.getString("ntripUser", NTRIP_USER);
        if (prefs.isKey("ntripPass")) NTRIP_PASS = prefs.getString("ntripPass", NTRIP_PASS);
        ENABLE_STATUS_LED = prefs.getBool("enLed", ENABLE_STATUS_LED);
        {   // steering settings AgOpenWeb sent last time (PGN 252: gainP, highPWM, ...) - the motor works right after a restart
            uint8_t blob[SteerSettingsData::BYTES];
            if (prefs.getBytesLength("steerCfg") == sizeof(blob) && prefs.getBytes("steerCfg", blob, sizeof(blob)) == sizeof(blob)) {
                SteerSettingsReceiver::restore(SteerSettingsData::unpack(blob));
                LogBuffer::printf("[STEER SETTINGS] Restored from the module memory: gainP=%u highPWM=%u minPWM=%u (AgOpenWeb values replace them when it sends them).\n",
                                  (unsigned)SteerSettingsReceiver::getGainP(), (unsigned)SteerSettingsReceiver::getHighPWM(), (unsigned)SteerSettingsReceiver::getMinPWM());
            }
        }
        {   // what AgOpenWeb last told the machine module: pin functions (PGN 236), "Invert relay" (PGN 238), "Invert motor" (PGN 251)
            uint8_t pins[MachinePinMap::PINS];
            if (prefs.getBytesLength("machPins") == sizeof(pins) && prefs.getBytes("machPins", pins, sizeof(pins)) == sizeof(pins)) {
                MachinePinMap m; m.load(pins);
                MachinePinConfigReceiver::restore(m);
            }
            MachineConfigReceiver::restoreInvertRelay(prefs.getBool("machInv", false));
            SteerConfigReceiver::restoreInvertMotor(prefs.getBool("aogMotInv", false));
        }
        ENABLE_ETHERNET = prefs.getBool("enEth", ENABLE_ETHERNET);   // read in begin(), BEFORE networkManagerBegin() chooses the link

        // Thresholds/times - ETHERNET_DOWN_GRACE_MS and WIFI_CONNECT_TIMEOUT_MS
        // work immediately, WATCHDOG_TIMEOUT_S requires a restart (read once
        // in watchdogBegin()).
        ETHERNET_DOWN_GRACE_MS = prefs.getULong("ethGrace", ETHERNET_DOWN_GRACE_MS);
        WIFI_CONNECT_TIMEOUT_MS = prefs.getULong("wifiTimeout", WIFI_CONNECT_TIMEOUT_MS);
        WATCHDOG_TIMEOUT_S = prefs.getUInt("watchdogS", WATCHDOG_TIMEOUT_S);
        STATUS_LED_BRIGHTNESS = prefs.getUChar("ledBright", STATUS_LED_BRIGHTNESS);

        // Steering angle range - works immediately.
        STEERING_ANGLE_LEFT_DEG = prefs.getFloat("steerLeftDeg", STEERING_ANGLE_LEFT_DEG);
        IMU_MOUNTING_OFFSET_DEG = prefs.getFloat("imuOffset", IMU_MOUNTING_OFFSET_DEG);
        IMU_MOUNT_ROTATION_DEG = prefs.getFloat("imuRot", IMU_MOUNT_ROTATION_DEG);
        IMU_ROLL_INVERT = prefs.getBool("imuRollInv", IMU_ROLL_INVERT);
        {   // 3D mounting: stored as its text form (sensors/imu_mount.h), checked on load
            const String txt = prefs.getString("imuM3d", "");
            ImuMount::Mount mm;
            if (ImuMount::parse(txt.c_str(), mm)) IMU_MOUNT3D = mm;
            else {
                LogBuffer::println("[SETTINGS] WARNING: the saved IMU 3D mounting is invalid - switched off.");
                IMU_MOUNT3D = ImuMount::Mount(); prefs.remove("imuM3d");
            }
        }
        STEERING_ANGLE_RIGHT_DEG = prefs.getFloat("steerRightDeg", STEERING_ANGLE_RIGHT_DEG);

        DEBUG_STEP_TIMING = prefs.getBool("dStepTiming", DEBUG_STEP_TIMING);
        DEBUG_WIFI = prefs.getBool("dWifi", DEBUG_WIFI);
        DEBUG_ETHERNET = prefs.getBool("dEth", DEBUG_ETHERNET);
        DEBUG_NET = prefs.getBool("dNet", DEBUG_NET);
        DEBUG_PGN = prefs.getBool("dPgn", DEBUG_PGN);
        DEBUG_GPS = prefs.getBool("dGps", DEBUG_GPS);
        DEBUG_RTCM = prefs.getBool("dRtcm", DEBUG_RTCM);
        DEBUG_STEER_BUTTON = prefs.getBool("dSteerBtn", DEBUG_STEER_BUTTON);
        DEBUG_WORK_SWITCH = prefs.getBool("dWorkSw", DEBUG_WORK_SWITCH);
        DEBUG_STEERING_SENSOR = prefs.getBool("dSteerSens", DEBUG_STEERING_SENSOR);
        DEBUG_CURRENT_SENSOR = prefs.getBool("dCurrent", DEBUG_CURRENT_SENSOR);
        DEBUG_MOTOR = prefs.getBool("dMotor", DEBUG_MOTOR);
        DEBUG_IMU = prefs.getBool("dImu", DEBUG_IMU);
        DEBUG_STEER_COMMAND = prefs.getBool("dSteerCmd", DEBUG_STEER_COMMAND);
        DEBUG_STEER_COMMAND_RAW = prefs.getBool("dSteerCmdRaw", DEBUG_STEER_COMMAND_RAW);
        DEBUG_MACHINE_COMMAND = prefs.getBool("dMachCmd", DEBUG_MACHINE_COMMAND);
        DEBUG_SYS = prefs.getBool("dSys", DEBUG_SYS);
        DEBUG_WEB = prefs.getBool("dWeb", DEBUG_WEB);

        sanitizeLoadedValues();
    }

    // Defence against invalid values stored in memory (e.g. by an older version without
    // validation, or after corruption): watchdog = 0 would cause a restart loop BEFORE the web panel gets
    // to start (the only way out is reflashing with a memory erase), and NaN in the angles
    // would end up in $PANDA. An invalid value reverts to the default and is repaired in memory.
    static void sanitizeLoadedValues() {
        using namespace SettingsValidation;
        uint32_t w = clampOrDefault(WATCHDOG_TIMEOUT_S, 3, 300, 5);
        if (w != WATCHDOG_TIMEOUT_S) {
            LogBuffer::printf("[SETTINGS] WARNING: the saved watchdog=%lu s is invalid - restored %lu s.\n", (unsigned long)WATCHDOG_TIMEOUT_S, (unsigned long)w);
            WATCHDOG_TIMEOUT_S = w; prefs.putUInt("watchdogS", w);
        }
        unsigned long e = clampOrDefault(ETHERNET_DOWN_GRACE_MS, 1000, 60000, 10000);
        if (e != ETHERNET_DOWN_GRACE_MS) {
            LogBuffer::printf("[SETTINGS] WARNING: the saved Ethernet time=%lu ms is invalid - restored %lu ms.\n", ETHERNET_DOWN_GRACE_MS, e);
            ETHERNET_DOWN_GRACE_MS = e; prefs.putULong("ethGrace", e);
        }
        unsigned long wt = clampOrDefault(WIFI_CONNECT_TIMEOUT_MS, 3000, 120000, 15000);
        if (wt != WIFI_CONNECT_TIMEOUT_MS) {
            LogBuffer::printf("[SETTINGS] WARNING: the saved WiFi timeout=%lu ms is invalid - restored %lu ms.\n", WIFI_CONNECT_TIMEOUT_MS, wt);
            WIFI_CONNECT_TIMEOUT_MS = wt; prefs.putULong("wifiTimeout", wt);
        }
        if (NTRIP_PORT == 0) {
            LogBuffer::println("[SETTINGS] WARNING: the saved NTRIP port=0 is invalid - restored 8086.");
            NTRIP_PORT = 8086; prefs.putUShort("ntripPort", NTRIP_PORT);
        }
        if (!steeringRangeOk(STEERING_ANGLE_LEFT_DEG, STEERING_ANGLE_RIGHT_DEG)) {
            LogBuffer::println("[SETTINGS] WARNING: the saved steering angle range is invalid - restored -45...45.");
            STEERING_ANGLE_LEFT_DEG = -45.0f; STEERING_ANGLE_RIGHT_DEG = 45.0f;
            prefs.putFloat("steerLeftDeg", STEERING_ANGLE_LEFT_DEG); prefs.putFloat("steerRightDeg", STEERING_ANGLE_RIGHT_DEG);
        }
        float rot = finiteOrDefault(IMU_MOUNT_ROTATION_DEG, -180.0f, 180.0f, 0.0f);
        if (rot != IMU_MOUNT_ROTATION_DEG) {
            LogBuffer::println("[SETTINGS] WARNING: the saved IMU board rotation is invalid - restored 0.");
            IMU_MOUNT_ROTATION_DEG = rot; prefs.putFloat("imuRot", rot);
        }
        float off = finiteOrDefault(IMU_MOUNTING_OFFSET_DEG, -360.0f, 360.0f, 0.0f);
        if (off != IMU_MOUNTING_OFFSET_DEG) {
            LogBuffer::println("[SETTINGS] WARNING: the saved IMU mounting correction is invalid - restored 0.");
            IMU_MOUNTING_OFFSET_DEG = off; prefs.putFloat("imuOffset", off);
        }
        if (!runawayParamsOk((long)RUNAWAY_WINDOW_MS, RUNAWAY_MIN_MOVE_DEG, RUNAWAY_MIN_ERROR_DEG)) {
            LogBuffer::println("[SETTINGS] WARNING: the saved runaway detector parameters are invalid - restored 1000 ms / 1.0 deg / 3.0 deg.");
            RUNAWAY_WINDOW_MS = 1000; RUNAWAY_MIN_MOVE_DEG = 1.0f; RUNAWAY_MIN_ERROR_DEG = 3.0f;
            prefs.putULong("runMs", RUNAWAY_WINDOW_MS); prefs.putFloat("runDeg", RUNAWAY_MIN_MOVE_DEG); prefs.putFloat("runErr", RUNAWAY_MIN_ERROR_DEG);
        }
        if (!currentLimitsOk(CURRENT_KICKOUT_A, (long)CURRENT_KICKOUT_MS, CURRENT_HARD_LIMIT_A)) {
            LogBuffer::println("[SETTINGS] WARNING: the saved current thresholds are invalid - restored 8 A / 400 ms / 15 A.");
            CURRENT_KICKOUT_A = 8.0f; CURRENT_KICKOUT_MS = 400; CURRENT_HARD_LIMIT_A = 15.0f;
            prefs.putFloat("curKickA", CURRENT_KICKOUT_A); prefs.putULong("curKickMs", CURRENT_KICKOUT_MS); prefs.putFloat("curHardA", CURRENT_HARD_LIMIT_A);
        }
    }

    // Saves the network data - requires a restart to take effect (see
    // the note above the class). IP addresses as text ("192.168.1.120") - unparsable
    // values are silently skipped (the previous/default one stays).
    static void saveNetwork(const String& wifiSsid, const String& wifiPass,
                            const String& wifiSsid2, const String& wifiPass2,
                            const String& wifiSsid3, const String& wifiPass3,
                            const String& wifiSsid4, const String& wifiPass4,
                            const String& apSsid, const String& apPass,
                            const String& localIp, const String& gateway,
                            const String& subnet, const String& agioIp,
                            bool fix1, bool fix2, bool fix3, bool fix4) {
        if (apPass != AP_PASSWORD) { prefs.putBool("apAuto", false); AP_PASSWORD_AUTO = false; }   // the owner chose their own
        {   // assignments under the lock - the main loop (core 1) copies these texts, it must not catch a half-done change
            SettingsWriteGuard guard;
            WIFI_SSID = wifiSsid; WIFI_PASS = wifiPass;
            WIFI_SSID2 = wifiSsid2; WIFI_PASS2 = wifiPass2;
            WIFI_SSID3 = wifiSsid3; WIFI_PASS3 = wifiPass3;
            WIFI_SSID4 = wifiSsid4; WIFI_PASS4 = wifiPass4;
            AP_SSID = apSsid;
            AP_PASSWORD = apPass;
            WIFI_STATIC[0] = fix1; WIFI_STATIC[1] = fix2; WIFI_STATIC[2] = fix3; WIFI_STATIC[3] = fix4;   // WifiHandler copies them together with the network names
        }
        prefs.putString("wifiSsid", wifiSsid);
        prefs.putString("wifiPass", wifiPass);
        prefs.putString("wifiSsid2", wifiSsid2);
        prefs.putString("wifiPass2", wifiPass2);
        prefs.putString("wifiSsid3", wifiSsid3);
        prefs.putString("wifiPass3", wifiPass3);
        prefs.putString("wifiSsid4", wifiSsid4);
        prefs.putString("wifiPass4", wifiPass4);
        prefs.putBool("wStatic1", fix1); prefs.putBool("wStatic2", fix2); prefs.putBool("wStatic3", fix3); prefs.putBool("wStatic4", fix4);
        prefs.putString("apSsid", apSsid);
        prefs.putString("apPass", apPass);

        IPAddress tmp;
        if (tmp.fromString(localIp)) { LOCAL_IP = tmp; prefs.putString("localIp", localIp); }
        if (tmp.fromString(gateway)) { GATEWAY = tmp; prefs.putString("gateway", gateway); }
        if (tmp.fromString(subnet)) { SUBNET_MASK = tmp; prefs.putString("subnet", subnet); }
        if (tmp.fromString(agioIp)) { AGIO_IP = tmp; prefs.putString("agioIp", agioIp); }

        LogBuffer::printf("[SETTINGS] Network data saved (forced static address: slots %s%s%s%s; the rest DHCP) - a restart is required for it to take effect.\n", fix1 ? "1 " : "", fix2 ? "2 " : "", fix3 ? "3 " : "", fix4 ? "4 " : "");
                            }

                            // Own NTRIP client - works immediately, without a restart (NtripClient::update()
                            // reads these values every pass, reconnecting if they changed and
                            // the device was not connected at the moment).
                            static void saveNtripSettings(bool enabled, const String& host, uint16_t port,
                                                          const String& mount, const String& user, const String& pass) {
                                {   // same as above - NtripClient::startConnection() copies host/login/password under the same lock
                                    SettingsWriteGuard guard;
                                    ENABLE_NTRIP_CLIENT = enabled;
                                    NTRIP_HOST = host;
                                    NTRIP_PORT = port;
                                    NTRIP_MOUNTPOINT = mount;
                                    NTRIP_USER = user;
                                    NTRIP_PASS = pass;
                                }
                                prefs.putBool("enNtrip", enabled);
                                prefs.putString("ntripHost", host);
                                prefs.putUShort("ntripPort", port);
                                prefs.putString("ntripMount", mount);
                                prefs.putString("ntripUser", user);
                                prefs.putString("ntripPass", pass);
                                LogBuffer::println("[SETTINGS] NTRIP client data saved.");
                                                          }

                                                          // Feature switches - they work immediately, without a restart.
                                                          static void saveFeatureFlags(bool section, bool motor, bool current, bool imu, bool statusLed) {
                                                              ENABLE_SECTION_EXPANDER = section;
                                                              ENABLE_MOTOR_DRIVER = motor;
                                                              ENABLE_CURRENT_SENSOR = current;
                                                              ENABLE_IMU = imu;
                                                              ENABLE_STATUS_LED = statusLed;
                                                              prefs.putBool("enSection", section);
                                                              prefs.putBool("enMotor", motor);
                                                              prefs.putBool("enCurrent", current);
                                                              prefs.putBool("enImu", imu);
                                                              prefs.putBool("enLed", statusLed);
                                                              LogBuffer::println("[SETTINGS] Feature switches saved.");
                                                          }

                                                          // Called every loop pass: writes the steering settings from AgOpenWeb to NVS once they have stopped
                                                          // changing. Not while steering is engaged (a flash write stalls the CPU briefly) - it retries later.
                                                          static void steerSettingsTick() {
                                                              auto& g = SteerSettingsReceiver::gate();
                                                              if (!g.shouldSave(millis()) || SteerCommandReceiver::isAutoSteerActive()) return;
                                                              uint8_t blob[SteerSettingsData::BYTES];
                                                              g.pendingValues().pack(blob);
                                                              prefs.putBytes("steerCfg", blob, sizeof(blob));
                                                              g.markSaved();
                                                              LogBuffer::println("[STEER SETTINGS] Saved to the module memory.");
                                                          }

                                                          // Called every loop pass: writes the machine settings received from AgOpenWeb (pin functions, Invert relay,
                                                          // Invert motor) to NVS when they changed. Not while steering is engaged (a flash write stalls the CPU briefly).
                                                          static void machineCfgTick() {
                                                              if (SteerCommandReceiver::isAutoSteerActive()) return;
                                                              if (MachinePinConfigReceiver::needsSave()) {
                                                                  prefs.putBytes("machPins", MachinePinConfigReceiver::map().fn, MachinePinMap::PINS);
                                                                  MachinePinConfigReceiver::markSaved();
                                                                  LogBuffer::println("[MACHINE PINS] Saved to the module memory.");
                                                              }
                                                              if (MachineConfigReceiver::invertNeedsSave()) {
                                                                  prefs.putBool("machInv", MachineConfigReceiver::getInvertRelay());
                                                                  MachineConfigReceiver::markInvertSaved();
                                                                  LogBuffer::printf("[MACHINE CFG] Invert relay %s - saved to the module memory.\n", MachineConfigReceiver::getInvertRelay() ? "ON" : "OFF");
                                                              }
                                                              if (SteerConfigReceiver::invertNeedsSave()) {
                                                                  prefs.putBool("aogMotInv", SteerConfigReceiver::invertMotor());
                                                                  SteerConfigReceiver::markInvertSaved();
                                                              }
                                                          }

                                                          // ---- Counters that outlive restarts (Diagnostics -> Lifetime). Loaded once, saved when safe (never while steering is engaged). ----
                                                          static void lifeBegin(bool abnormalBoot) {
                                                              LifetimeStats::Data d;
                                                              d.boots = prefs.getUInt("lsBoots", 0);
                                                              d.abnormalBoots = prefs.getUInt("lsAbn", 0);
                                                              d.motorRunSec = prefs.getUInt("lsRunS", 0);
                                                              d.runawayTrips = prefs.getUInt("lsRunaway", 0);
                                                              d.kickoutTrips = prefs.getUInt("lsKick", 0);
                                                              d.deadmanTrips = prefs.getUInt("lsDead", 0);
                                                              life.load(d);
                                                              life.noteBoot(abnormalBoot);
                                                          }
                                                          static const LifetimeStats::Data& lifeData() { return life.data(); }
                                                          static void lifeTick(bool motorDriven, uint32_t runawayNow, uint32_t kickoutNow, uint32_t deadmanNow) {
                                                              const uint32_t now = millis();
                                                              life.tick(now, motorDriven, runawayNow, kickoutNow, deadmanNow);
                                                              if (!life.shouldSave(now, !SteerCommandReceiver::isAutoSteerActive())) return;
                                                              const LifetimeStats::Data& d = life.data();
                                                              prefs.putUInt("lsBoots", d.boots);
                                                              prefs.putUInt("lsAbn", d.abnormalBoots);
                                                              prefs.putUInt("lsRunS", d.motorRunSec);
                                                              prefs.putUInt("lsRunaway", d.runawayTrips);
                                                              prefs.putUInt("lsKick", d.kickoutTrips);
                                                              prefs.putUInt("lsDead", d.deadmanTrips);
                                                              life.markSaved(now);
                                                          }

                                                          // Ethernet on/off - takes effect after a restart (the link is chosen once, at start-up).
                                                          static void saveEthernet(bool enabled) {
                                                              prefs.putBool("enEth", enabled);
                                                              if (enabled) LogBuffer::println("[SETTINGS] Ethernet enabled (applies after a restart).");
                                                              else LogBuffer::println("[SETTINGS] Ethernet disabled (applies after a restart).");
                                                          }

                                                          // ethGraceMs/wifiTimeoutMs/ledBrightness work immediately; watchdogS requires a restart.
                                                          static void saveThresholds(unsigned long ethGraceMs, unsigned long wifiTimeoutMs, uint32_t watchdogS, uint8_t ledBrightness) {
                                                              ETHERNET_DOWN_GRACE_MS = ethGraceMs;
                                                              WIFI_CONNECT_TIMEOUT_MS = wifiTimeoutMs;
                                                              WATCHDOG_TIMEOUT_S = watchdogS;
                                                              STATUS_LED_BRIGHTNESS = ledBrightness;
                                                              prefs.putULong("ethGrace", ethGraceMs);
                                                              prefs.putULong("wifiTimeout", wifiTimeoutMs);
                                                              prefs.putUInt("watchdogS", watchdogS);
                                                              prefs.putUChar("ledBright", ledBrightness);
                                                              LogBuffer::println("[SETTINGS] Thresholds/times saved (the watchdog needs a restart).");
                                                          }

                                                          // Works immediately.
                                                          // Motor runaway detector and direction reversal - work immediately (MotorDriver::update() reads them every pass).
                                                          static void saveMotorSettings(bool runEn, unsigned long windowMs, float moveDeg, float errDeg, bool invert) {
                                                              ENABLE_RUNAWAY_DETECT = runEn;
                                                              RUNAWAY_WINDOW_MS = windowMs;
                                                              RUNAWAY_MIN_MOVE_DEG = moveDeg;
                                                              RUNAWAY_MIN_ERROR_DEG = errDeg;
                                                              MOTOR_INVERT_DIR = invert;
                                                              prefs.putBool("runEn", runEn);
                                                              prefs.putULong("runMs", windowMs);
                                                              prefs.putFloat("runDeg", moveDeg);
                                                              prefs.putFloat("runErr", errDeg);
                                                              prefs.putBool("motInv", invert);
                                                              LogBuffer::printf("[SETTINGS] Motor saved: detector %s, window %lu ms, movement >= %.1f deg, evaluated from error %.1f deg, direction %s.\n",
                                                                                runEn ? "ON" : "OFF", windowMs, moveDeg, errDeg, invert ? "REVERSED" : "normal");
                                                          }

                                                          // Current protection thresholds - work immediately (CurrentSensor::update() copies them every pass).
                                                          static void saveCurrentSettings(float kickoutA, unsigned long kickoutMs, float hardA) {
                                                              CURRENT_KICKOUT_A = kickoutA;
                                                              CURRENT_KICKOUT_MS = kickoutMs;
                                                              CURRENT_HARD_LIMIT_A = hardA;
                                                              prefs.putFloat("curKickA", kickoutA);
                                                              prefs.putULong("curKickMs", kickoutMs);
                                                              prefs.putFloat("curHardA", hardA);
                                                              LogBuffer::printf("[SETTINGS] Current thresholds saved: kickout %.1f A / %lu ms, hard %.1f A.\n", kickoutA, kickoutMs, hardA);
                                                          }

                                                          static void saveSteeringRange(float leftDeg, float rightDeg) {
                                                              STEERING_ANGLE_LEFT_DEG = leftDeg;
                                                              STEERING_ANGLE_RIGHT_DEG = rightDeg;
                                                              prefs.putFloat("steerLeftDeg", leftDeg);
                                                              prefs.putFloat("steerRightDeg", rightDeg);
                                                              LogBuffer::println("[SETTINGS] Steering angle range saved.");
                                                          }

                                                          // Works immediately.
                                                          static void saveImuOffset(float offsetDeg) {
                                                              IMU_MOUNTING_OFFSET_DEG = offsetDeg;
                                                              prefs.putFloat("imuOffset", offsetDeg);
                                                              LogBuffer::println("[SETTINGS] IMU mounting angle correction saved.");
                                                          }

                                                          // Works immediately.
                                                          // rollInvert: 1 / 0 = set, -1 = leave unchanged.
                                                          static void saveImuRotation(float rotDeg, int rollInvert = -1) {
                                                              IMU_MOUNT_ROTATION_DEG = rotDeg;
                                                              prefs.putFloat("imuRot", rotDeg);
                                                              if (rollInvert >= 0) { IMU_ROLL_INVERT = (rollInvert != 0); prefs.putBool("imuRollInv", IMU_ROLL_INVERT); }
                                                              LogBuffer::printf("[SETTINGS] IMU board rotation saved: %.1f deg.\n", rotDeg);
                                                          }

                                                          // 3D mounting (sensors/imu_mount.h). Works immediately. An invalid mount = off.
                                                          static void saveImuMount3d(const ImuMount::Mount& m) {
                                                              if (m.valid && ImuMount::mountOk(m)) {
                                                                  char buf[160];
                                                                  ImuMount::format(m, buf, sizeof(buf));
                                                                  prefs.putString("imuM3d", buf);
                                                                  IMU_MOUNT3D = m;
                                                                  LogBuffer::println("[SETTINGS] IMU 3D mounting saved.");
                                                              } else {
                                                                  prefs.remove("imuM3d");
                                                                  IMU_MOUNT3D = ImuMount::Mount();
                                                                  LogBuffer::println("[SETTINGS] IMU 3D mounting switched off.");
                                                              }
                                                          }

                                                          // Panel protection (PIN / password / none). Validation and authorization - in the handler (web_server.h).
                                                          static bool saveAuth(PanelAuth::Mode m, const char* secret) {
                                                              if (!PanelAuth::set(m, secret)) return false;
                                                              prefs.putUChar("authMode", (uint8_t)m);
                                                              if (m == PanelAuth::PIN || m == PanelAuth::PASSWORD) prefs.putString("authSecret", secret);
                                                              else prefs.remove("authSecret");
                                                              LogBuffer::printf("[PANEL] Panel protection: %s.\n", m == PanelAuth::PIN ? "PIN" : m == PanelAuth::PASSWORD ? "password" : "off");
                                                              return true;
                                                          }

                                                          // Choosing the AgOpenWeb sender: address 0.0.0.0 = automatic mode (the first active one), any other = pinned permanently (only that address,
                                                          // even when it is briefly silent). Works immediately; remembered after a restart.
                                                          static void saveAgioSender(IPAddress ip) {
                                                              if (ip == IPAddress(0, 0, 0, 0)) {
                                                                  AgioSenders::clearPin();
                                                                  prefs.remove("agioPin");
                                                                  LogBuffer::println("[SETTINGS] AgOpenWeb sender: automatic mode (first active one).");
                                                              } else {
                                                                  AgioSenders::setPinned((uint32_t)ip);
                                                                  prefs.putString("agioPin", ip.toString());
                                                                  LogBuffer::printf("[SETTINGS] AgOpenWeb sender pinned: %s.\n", ip.toString().c_str());
                                                              }
                                                          }

                                                          // Which bits of the PGN 254 status byte mean "steering enabled": 1, 4 (default) or 5. Works immediately.
                                                          static void saveAgioEngageMask(uint8_t m) {
                                                              SteerCommandReceiver::setEngagedMask(m);
                                                              prefs.putUChar("agioEng", SteerCommandReceiver::getEngagedMask());
                                                              LogBuffer::printf("[SETTINGS] Reading the steering engage from AgOpenWeb: mask 0x%02X.\n", (unsigned)SteerCommandReceiver::getEngagedMask());
                                                          }

                                                          // Clears ALL saved settings (network, switches, thresholds,
                                                          // debug, angle calibration) and restores the startup values from
                                                          // config.h - after a restart (required, called separately by the caller).
                                                          // Clears the two separate NVS namespaces used in the project.
                                                          static void factoryReset() {
                                                              prefs.clear();

                                                              Preferences steeringPrefs;
                                                              steeringPrefs.begin("steering", false);
                                                              steeringPrefs.clear();
                                                              steeringPrefs.end();

                                                              Preferences currentPrefs;   // current sensor zero calibration
                                                              currentPrefs.begin("current", false);
                                                              currentPrefs.clear();
                                                              currentPrefs.end();

                                                              LogBuffer::println("[SETTINGS] Defaults restored (a restart is required).");
                                                          }

                                                          // Saves one DEBUG_* flag by name (the key from the web form -
                                                          // see web_server.h) - works immediately, without a restart.
                                                          static void setDebugFlag(const String& name, bool value) {
                                                              if (name == "dStepTiming") { DEBUG_STEP_TIMING = value; prefs.putBool("dStepTiming", value); }
                                                              else if (name == "dWifi") { DEBUG_WIFI = value; prefs.putBool("dWifi", value); }
                                                              else if (name == "dEth") { DEBUG_ETHERNET = value; prefs.putBool("dEth", value); }
                                                              else if (name == "dNet") { DEBUG_NET = value; prefs.putBool("dNet", value); }
                                                              else if (name == "dPgn") { DEBUG_PGN = value; prefs.putBool("dPgn", value); }
                                                              else if (name == "dGps") { DEBUG_GPS = value; prefs.putBool("dGps", value); }
                                                              else if (name == "dRtcm") { DEBUG_RTCM = value; prefs.putBool("dRtcm", value); }
                                                              else if (name == "dSteerBtn") { DEBUG_STEER_BUTTON = value; prefs.putBool("dSteerBtn", value); }
                                                              else if (name == "dWorkSw") { DEBUG_WORK_SWITCH = value; prefs.putBool("dWorkSw", value); }
                                                              else if (name == "dSteerSens") { DEBUG_STEERING_SENSOR = value; prefs.putBool("dSteerSens", value); }
                                                              else if (name == "dCurrent") { DEBUG_CURRENT_SENSOR = value; prefs.putBool("dCurrent", value); }
                                                              else if (name == "dMotor") { DEBUG_MOTOR = value; prefs.putBool("dMotor", value); }
                                                              else if (name == "dImu") { DEBUG_IMU = value; prefs.putBool("dImu", value); }
                                                              else if (name == "dSteerCmd") { DEBUG_STEER_COMMAND = value; prefs.putBool("dSteerCmd", value); }
                                                              else if (name == "dSteerCmdRaw") { DEBUG_STEER_COMMAND_RAW = value; prefs.putBool("dSteerCmdRaw", value); }
                                                              else if (name == "dMachCmd") { DEBUG_MACHINE_COMMAND = value; prefs.putBool("dMachCmd", value); }
                                                              else if (name == "dSys") { DEBUG_SYS = value; prefs.putBool("dSys", value); }
                                                              else if (name == "dWeb") { DEBUG_WEB = value; prefs.putBool("dWeb", value); }
                                                          }
};

Preferences RuntimeSettings::prefs;
LifetimeStats RuntimeSettings::life;
