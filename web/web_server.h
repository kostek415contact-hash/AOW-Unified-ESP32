#pragma once
// ======================================================================
// web/web_server.h
// EN: Web server on port 80: panel page, /status JSON, log, settings endpoints, calibration, /update (OTA),
//     /history, /lang.js.
// PL: Serwer WWW na porcie 80: strona panelu, JSON /status, dziennik, endpointy ustawień, kalibracja, /update
//     (OTA), /history, /lang.js.
// Used by: connectivity/network_manager.h
// Depends on: actuators.h, chunked_out.h, config.h, current_sensor.h, diagnostics.h, ethernet_handler.h,
//     event_log.h, gps_universal.h, heap_trend.h, history.h, json_escape.h ...
// ======================================================================
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "../config/config.h"
#include "log_buffer.h"
#include "runtime_settings.h"
#include "../connectivity/ethernet_handler.h"
#include "../sensors/imu_sensor.h"
#include "../sensors/gps_universal.h"
#include "../sensors/steering_angle_sensor.h"
#include "../connectivity/pgn_receivers.h"
#include "../io/inputs.h"
#include "../actuators/actuators.h"
#include "../io/section_expander.h"
#include "../sensors/current_sensor.h"
#include "../ntrip/rtcm_relay.h"
#include "../ntrip/ntrip_client.h"
#include "../io/status_led.h"
#include "../diagnostics/diagnostics.h"
#include <esp_system.h>
#include "web_ui.h"
#include "lang/languages.h"
#include "msg.h"
#include "../diagnostics/heap_trend.h"
#include "settings_validation.h"
#include "panel_auth.h"
#include "request_guard.h"
#include "chunked_out.h"
#include "json_escape.h"
#include "../diagnostics/history.h"
#include "../diagnostics/event_log.h"
#include "../diagnostics/persist_log.h"
#include "../diagnostics/net_stats.h"
#include "../ntrip/rtcm_arbiter.h"
#include "../ota/ota_status.h"

// Web server on port 80:
//   /        - status panel (WiFi/GPS/RTCM/RAM) + live log preview
//   /status  - status panel data in JSON format (polled every 1 s)
//   /log     - returns the current dump of the log buffer (LogBuffer::snapshot())
//   /restart - restarts the device (confirmation on the client side)
//   /update  - OTA update form and handling (standard Update.h)
class WebServerModule {
private:
  static WebServer server;

  // The panel page is in web/web_ui.h + web/ui/*.h (PROGMEM) - here we only serve it.
  static void handleRoot() {
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.send_P(200, "text/html", WEB_UI_HTML);
  }

  // Panel translation catalog (/lang.js?l=en): a script that sets window.AOG_LANG. It receives only the selected language,
  // loaded synchronously in <head> before the panel script. Unknown language -> 404 (the panel shows English).
  static void handleLang() {
    const String l = server.arg("l");
    const char* js = (l.length() >= 2 && l.length() <= 6) ? webLangCatalog(l.c_str()) : nullptr;
    if (!js) { server.send(404, "text/plain", "no such language"); return; }
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "application/javascript; charset=utf-8", js);
  }

  // The reason for the last restart - readable text for the panel (ASCII, no quotes,
  // because it goes into JSON). It lets you see at once whether the device hung
  // (panic/watchdog) or had a voltage drop (brownout).
  static const char* resetReasonText() {
    switch (esp_reset_reason()) {
      case ESP_RST_POWERON:   return "power-on";
      case ESP_RST_EXT:       return "external reset";
      case ESP_RST_SW:        return bootAfterOta ? "restart after an OTA update" : (bootAfterWifiLoss ? "restart after WiFi loss" : "software restart");
      case ESP_RST_PANIC:     return "PANIC (crash)";
      case ESP_RST_INT_WDT:   return "interrupt watchdog";
      case ESP_RST_TASK_WDT:  return "task watchdog";
      case ESP_RST_WDT:       return "watchdog";
      case ESP_RST_DEEPSLEEP: return "wake-up from deep sleep";
      case ESP_RST_BROWNOUT:  return "brownout";
      default:                return "other/unknown";
    }
  }

  // A text typed by the user (SSID, NTRIP host/user...) written into the JSON: quotes, backslashes and control characters escaped.
  // Without this a name like Jan's "Net" made the whole /status invalid and the panel showed "no connection" for good.
  static String jsonEsc(const String& in) {
    String out;
    out.reserve(in.length() + 8);
    char tmp[8];
    for (size_t i = 0; i < in.length(); i++) {
      size_t n = JsonEscape::escapeChar((uint8_t)in[i], tmp);
      tmp[n] = '\0';
      out += tmp;
    }
    return out;
  }

  static void handleStatus() {
    bool ethOn = EthernetHandler::isConnected();
    bool apMode = WifiHandler::isAPMode();
    bool netConnected = ethOn || apMode || (WiFi.status() == WL_CONNECTED);
    String netType = ethOn ? "Ethernet" : (apMode ? "AP" : "WiFi");
    String ip = ethOn ? ETH.localIP().toString() : (apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
    // The response goes in 1 KB chunks (Transfer-Encoding: chunked) instead of a single String with reserve(12288):
    // no contiguous heap block is needed on every poll, and additional fields do not increase memory use.
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", "");
    auto sink = [](const char* p, size_t n) { server.sendContent(p, n); };
    ChunkedOut<1024, decltype(sink)> out(sink);
    struct { decltype(out)* o; void operator+=(const String& v) { o->write(v.c_str(), v.length()); } void operator+=(const char* v) { o->write(v); } } json = {&out};
    json += "{";
    json += "\"apMode\":" + String(apMode ? "true" : "false") + ",";
    json += "\"netType\":\"" + netType + "\",";
    json += "\"netConnected\":" + String(netConnected ? "true" : "false") + ",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + ip + "\",";
    json += "\"fixQuality\":" + String(GpsHandler::getFixQuality()) + ",";
    json += "\"satellites\":" + String(GpsHandler::getSatelliteCount()) + ",";
    json += "\"gpsRateHz\":" + String(GpsHandler::getMeasuredRateHz(), 1) + ",";
    json += "\"hdop\":" + String(GpsHandler::getHdop(), 1) + ",";
    json += "\"hAcc\":" + String(GpsHandler::getHAcc(), 3) + ",";
    json += "\"vAcc\":" + String(GpsHandler::getVAcc(), 3) + ",";
    json += "\"altitude\":" + String(GpsHandler::getAltitude(), 1) + ",";
    json += "\"lat\":\"" + String(GpsHandler::getLatitude()) + " " + String(GpsHandler::getLatDir()) + "\",";
    json += "\"lon\":\"" + String(GpsHandler::getLongitude()) + " " + String(GpsHandler::getLonDir()) + "\",";
    json += "\"gpsSpeedKmh\":" + String(GpsHandler::getSpeedKnots() * 1.852f, 1) + ",";
    json += "\"gpsHeading\":" + String(GpsHandler::getGpsHeadingDeg(), 1) + ",";
    json += "\"imuConnected\":" + String(ImuSensor::isConnected() ? "true" : "false") + ",";
    json += "\"imuHeading\":" + String(ImuSensor::getHeadingDeg(), 1) + ",";
    json += "\"imuPitch\":" + String(ImuSensor::getPitchDeg(), 1) + ",";
    json += "\"imuRoll\":" + String(ImuSensor::getRollDeg(), 1) + ",";
    json += "\"imuRollRaw\":" + String(ImuSensor::getRawRollDeg(), 2) + ",";
    json += "\"imuPitchRaw\":" + String(ImuSensor::getRawPitchDeg(), 2) + ",";
    json += "\"imuYawRaw\":" + String(ImuSensor::getRawYawDeg(), 1) + ",";
    json += "\"imuRotDeg\":" + String(IMU_MOUNT_ROTATION_DEG, 1) + ",";
    json += "\"imuRollInvert\":" + String(IMU_ROLL_INVERT ? "true" : "false") + ",";
    {   // 3D mounting in its text form (digits, ';' ',' '.' '-' only - safe inside JSON); empty = off
        char m3[160]; ImuMount::format(IMU_MOUNT3D, m3, sizeof(m3));
        json += "\"imuMount3d\":\"" + String(m3) + "\",";
    }
    json += "\"imuAccelX\":" + String(ImuSensor::getAccelX(), 2) + ",";
    json += "\"imuAccelY\":" + String(ImuSensor::getAccelY(), 2) + ",";
    json += "\"imuAccelZ\":" + String(ImuSensor::getAccelZ(), 2) + ",";
    json += "\"gpsDiagBytes\":" + String(GpsHandler::getDiagBytesTotal()) + ",";
    json += "\"gpsDiagDollar\":" + String(GpsHandler::getDiagDollarLines()) + ",";
    json += "\"gpsDiagChecksum\":" + String(GpsHandler::getDiagValidChecksum()) + ",";
    json += "\"gpsDiagRecognized\":" + String(GpsHandler::getDiagRecognized()) + ",";
    json += "\"imuSilentMs\":" + String(ImuSensor::getSilentMs()) + ",";
    json += "\"imuPps\":" + String(ImuSensor::getPacketsPerSec()) + ",";
    json += "\"imuMaxGapMs\":" + String(ImuSensor::getMaxGapMs()) + ",";
    json += "\"imuRestarts\":" + String(ImuSensor::getUartRestartCount()) + ",";
    json += "\"imuBps\":" + String(ImuSensor::getBytesPerSec()) + ",";
    json += "\"imuBytes\":" + String(ImuSensor::getBytesTotal()) + ",";
    json += "\"imuLastByteMs\":" + String(ImuSensor::getMsSinceLastByte()) + ",";
    json += "\"imuRstEn\":" + String(ImuSensor::isResetPinEnabled() ? "true" : "false") + ",";
    json += "\"imuRstCount\":" + String(ImuSensor::getSensorResetCount()) + ",";
    json += "\"imuRstState\":" + String(ImuSensor::getManualResetState()) + ",";
    json += "\"imuRstMs\":" + String(ImuSensor::getManualResetRecoverMs()) + ",";
    json += "\"imuRstYawBefore\":" + String(ImuSensor::getManualResetYawBefore(), 1) + ",";
    json += "\"imuRstYawAfter\":" + String(ImuSensor::getManualResetYawAfter(), 1) + ",";
    json += "\"imuRstWasSilent\":" + String(ImuSensor::getManualResetWasSilent() ? "true" : "false") + ",";
    json += "\"ageDgps\":" + String(GpsHandler::getAgeDgpsOrNone(), 1) + ","; // -1 = none (like hAcc/vAcc)
    json += "\"ageLvl\":" + String(GpsHandler::getAgeLevel()) + ",";
    json += "\"rtcmBytes\":" + String(RtcmRelay::getTotalBytesRelayed()) + ",";
    json += "\"rtcmPackets\":" + String(RtcmRelay::getTotalPackets()) + ",";
    json += "\"ntripEnabled\":" + String(ENABLE_NTRIP_CLIENT ? "true" : "false") + ",";
    json += "\"ntripStreaming\":" + String(NtripClient::isStreaming() ? "true" : "false") + ",";
    json += "\"ntripBytes\":" + String(NtripClient::getTotalBytesRelayed()) + ",";
    json += "\"ntripReconnects\":" + String(NtripClient::getReconnectCount()) + ",";
    json += "\"ntripFails\":" + String(NtripClient::getFailStreak()) + ",";
    json += "\"ntripFailKind\":" + String(NtripClient::getLastFailKind()) + ",";   // 0 network, 3 bad password, 4 no mountpoint, 5 source table, 6 other
    json += "\"ntripRetryMs\":" + String(NtripClient::getRetryDelayMs()) + ",";
    json += "\"ntripRedirected\":" + String(NtripClient::isRedirected() ? "true" : "false") + ",";
    json += "\"ntripGgaReal\":" + String(NtripClient::getRealGgaSentCount()) + ",";
    json += "\"ntripGgaFallback\":" + String(NtripClient::getFallbackGgaSentCount()) + ",";
    json += "\"ntripMsToFloat\":" + String(NtripClient::getMsToFirstFloatOrFix()) + ",";
    json += "\"ntripMsToFix\":" + String(NtripClient::getMsToFirstFix()) + ",";
    json += "\"ntripRtcmFrames\":" + String(RtcmMessageStats::getFramesParsed()) + ",";
    json += "\"ntripRtcmCrcErr\":" + String(RtcmMessageStats::getCrcErrors()) + ",";   // RTCM frames with a bad CRC24Q checksum
    json += "\"ntripRtcmTypes\":[";
    for (uint8_t i = 0; i < RtcmMessageStats::getTypeCount(); i++) {
      if (i > 0) json += ",";
      json += "{\"type\":" + String(RtcmMessageStats::getTypeAt(i)) +
      ",\"count\":" + String(RtcmMessageStats::getCountAt(i)) + "}";
    }
    json += "],";
    json += "\"ntripHost\":\"" + jsonEsc(NTRIP_HOST) + "\",";
    json += "\"ntripPort\":" + String(NTRIP_PORT) + ",";
    json += "\"ntripMount\":\"" + jsonEsc(NTRIP_MOUNTPOINT) + "\",";
    json += "\"ntripUser\":\"" + jsonEsc(NTRIP_USER) + "\",";
    json += "\"ntripPassSet\":" + String(NTRIP_PASS.length() > 0 ? "true" : "false") + ",";   // hasla NIE opuszczaja modulu
    json += "\"steerSensorOk\":" + String(SteeringAngleSensor::isConnected() ? "true" : "false") + ",";
    json += "\"steerCalOk\":" + String(SteeringAngleSensor::isCalibrationValid() ? "true" : "false") + ",";
    json += "\"steerCalSaved\":" + String(SteeringAngleSensor::isCalibrationSaved() ? "true" : "false") + ",";
    json += "\"steerRaw\":" + String(SteeringAngleSensor::getRaw()) + ",";
    json += "\"steerAngleDeg\":" + String(SteeringAngleSensor::getAngleDeg(), 1) + ",";
    json += "\"steerAdcLeft\":" + String(SteeringAngleSensor::getAppliedAdcLeft()) + ",";
    json += "\"steerAdcRight\":" + String(SteeringAngleSensor::getAppliedAdcRight()) + ",";
    json += "\"steerSettingsRx\":" + String(SteerSettingsReceiver::hasReceived() ? "true" : "false") + ",";
    json += "\"steerSettingsStored\":" + String(SteerSettingsReceiver::isFromStorage() ? "true" : "false") + ",";   // values come from the module memory, AgOpenWeb has not resent them yet
    json += "\"steerGainP\":" + String(SteerSettingsReceiver::getGainP()) + ",";        // 0 = the motor will get no PWM (no settings from AgOpenWeb)
    json += "\"steerHighPwm\":" + String(SteerSettingsReceiver::getHighPWM()) + ",";
    json += "\"steerMinPwm\":" + String(SteerSettingsReceiver::getMinPWM()) + ",";
    json += "\"steerCountsPerDeg\":" + String(SteerSettingsReceiver::getCountsPerDeg()) + ",";
    json += "\"steerOffset\":" + String(SteerSettingsReceiver::getSteerOffset()) + ",";
    json += "\"steerAckermanFix\":" + String(SteerSettingsReceiver::getAckermanFix()) + ",";
    json += "\"cmdFresh\":" + String(SteerCommandReceiver::isFresh() ? "true" : "false") + ",";
    json += "\"cmdStatusByte\":" + String(SteerCommandReceiver::getLastStatusByte()) + ",";   // raw PGN 254 status byte
    json += "\"cmdEngageMask\":" + String(SteerCommandReceiver::getEngagedMask()) + ",";
    json += "\"cmdEngaged\":" + String(SteerCommandReceiver::isAutoSteerActive() ? "true" : "false") + ",";
    json += "\"cmdTargetAngle\":" + String(SteerCommandReceiver::getTargetAngleDeg(), 1) + ",";
    json += "\"cmdSpeedKmh\":" + String(SteerCommandReceiver::getSpeedKmh(), 1) + ",";
    json += "\"cmdSections\":" + String((uint16_t)SteerCommandReceiver::getSections1to8() | ((uint16_t)SteerCommandReceiver::getSections9to16() << 8)) + ",";
    json += "\"steerBtnPressed\":" + String(SteerButton::isPressed() ? "true" : "false") + ",";
    json += "\"workSwitchActive\":" + String(WorkSwitch::isActive() ? "true" : "false") + ",";
    json += "\"motorDriverEn\":" + String(MotorDriver::isEnabled() ? "true" : "false") + ",";
    json += "\"sectionExpEn\":" + String(SectionExpander::isFound() ? "true" : "false") + ",";
    json += "\"sectionExpCfg\":" + String(SectionExpander::isEnabled() ? "true" : "false") + ",";   // enabled in config (found or not)
    json += "\"hydLiftEn\":" + String(HydraulicLift::isMapped() ? "true" : "false") + ",";
    json += "\"hydLiftActive\":" + String(HydraulicLift::isActive() ? "true" : "false") + ",";
    json += "\"hydLiftManual\":" + String(HydraulicLift::isManualOverride() ? "true" : "false") + ",";
    json += "\"machineFresh\":" + String(MachineCommandReceiver::isFresh() ? "true" : "false") + ",";
    json += "\"sectionsMask\":" + String(MachineCommandReceiver::getSectionsMask()) + ",";
    json += "\"currentSensEn\":" + String(CurrentSensor::isEnabled() ? "true" : "false") + ",";
    json += "\"motorPwm\":" + String(MotorDriver::getLastPwm()) + ",";                         // last PWM (signed; angle convention)
    json += "\"motorErrDeg\":" + String(MotorDriver::getLastErrorDeg(), 2) + ",";
    json += "\"motorInvert\":" + String(MOTOR_INVERT_DIR ? "true" : "false") + ",";
    json += "\"runEn\":" + String(ENABLE_RUNAWAY_DETECT ? "true" : "false") + ",";
    json += "\"testState\":" + String(MotorDriver::getTestState()) + ",";          // 0 idle, 1 running, 2 finished, 3 aborted
    json += "\"testReason\":" + String(MotorDriver::getTestReason()) + ",";        // when 3: 1 steering enabled, 2 current protection, 3 feature disabled, 4 end of the range reached
    json += "\"testDir\":" + String(MotorDriver::getTestDir()) + ",";
    json += "\"testPwm\":" + String(MotorDriver::getTestPwm()) + ",";
    json += "\"testMs\":" + String(MotorDriver::getTestMs()) + ",";
    json += "\"testDeltaDeg\":" + String(MotorDriver::getTestDeltaDeg(), 2) + ",";  // -999 = no angle reading
    json += "\"testPeakA\":" + String(MotorDriver::getTestPeakA(), 2) + ",";       // -1 = no current measurement
    json += "\"testAgeMs\":" + String(MotorDriver::getTestAgeMs()) + ",";
    json += "\"runState\":" + String(MotorDriver::runawayState()) + ",";                        // 0 off, 1 OK, 2 no progress, 3 reversed direction, 4 frozen angle sensor
    json += "\"runMoveDeg\":" + String(MotorDriver::runawayMoveDeg(), 2) + ",";
    json += "\"motorArmed\":" + String(MotorDriver::isArmed() ? "true" : "false") + ",";           // false until steering was seen switched OFF after start-up
    json += "\"motorDeadmanTrips\":" + String(MotorDriver::deadmanTrips()) + ",";               // times the dead-man timer cut the motor because loop() hung
    json += "\"kickoutActive\":" + String(kickoutSig.active(millis()) ? "true" : "false") + ",";    // PGN 250 = 255 to AgOpenWeb
    json += "\"kickoutCount\":" + String(kickoutSig.count()) + ",";
    json += "\"gpsUtc\":" + String(GpsClock::epochNow(millis())) + ",";   // UTC seconds since 1970 from GPS (RMC), 0 = unknown
    json += "\"kickoutCause\":" + String(kickoutSig.lastCause()) + ",";   // 0 none, 1 overcurrent, 2 current sensor error, 3 no current samples, 4 no motion, 5 reversed direction, 6 frozen angle sensor
    json += "\"runTrips\":" + String(MotorDriver::runawayTrips()) + ",";
    json += "\"runLastReason\":" + String(MotorDriver::runawayLastReason()) + ",";
    json += "\"runLastMoveDeg\":" + String(MotorDriver::runawayLastMoveDeg(), 2) + ",";
    json += "\"runWindowMs\":" + String(RUNAWAY_WINDOW_MS) + ",";
    json += "\"runMinMoveDeg\":" + String(RUNAWAY_MIN_MOVE_DEG, 1) + ",";
    json += "\"runMinErrDeg\":" + String(RUNAWAY_MIN_ERROR_DEG, 1) + ",";
    json += "\"currState\":" + String(CurrentSensor::stateCode()) + ",";           // 0 off, 1 no data, 2 OK, 3 overcurrent (latch), 4 sensor error
    json += "\"currAmps\":" + String(CurrentSensor::getAmps(), 2) + ",";
    json += "\"currAvgAmps\":" + String(CurrentSensor::getAvgAmps(), 2) + ",";
    json += "\"currPeakA\":" + String(CurrentSensor::getPeakAmps(), 2) + ",";
    json += "\"currVolts\":" + String(CurrentSensor::getVolts(), 3) + ",";
    json += "\"currZeroV\":" + String(CurrentSensor::getZeroV(), 3) + ",";
    json += "\"currAgeMs\":" + String(CurrentSensor::getAgeMs()) + ",";
    json += "\"currLatched\":" + String(CurrentSensor::isLatched() ? "true" : "false") + ",";
    json += "\"currCalState\":" + String(CurrentSensor::calStatus()) + ",";         // 0 idle, 1 running, 2 done, 3 failed
    json += "\"currCalPct\":" + String(CurrentSensor::calProgress()) + ",";
    json += "\"currCalReason\":\"" + String(CurrentSensor::calFailReason()) + "\",";
    json += "\"currRecentPeakA\":" + String(CurrentSensor::getRecentPeakA(), 2) + ",";   // peak |current| of the last ~1 s (chart)
    json += "\"currSamples\":" + String(CurrentSensor::getSamples()) + ",";
    json += "\"currTrips\":" + String(CurrentSensor::getTrips()) + ",";
    json += "\"currLastTripA\":" + String(CurrentSensor::getLastTripA(), 2) + ",";
    json += "\"currKickoutA\":" + String(CURRENT_KICKOUT_A, 1) + ",";
    json += "\"currKickoutMs\":" + String(CURRENT_KICKOUT_MS) + ",";
    json += "\"currHardA\":" + String(CURRENT_HARD_LIMIT_A, 1) + ",";
    json += "\"useEthernet\":" + String(ethOn ? "true" : "false") + ",";
    json += "\"enEthernet\":" + String(ENABLE_ETHERNET ? "true" : "false") + ",";
    json += "\"lifeBoots\":" + String(RuntimeSettings::lifeData().boots) + ",";                 // counters that outlive restarts
    json += "\"lifeAbnormal\":" + String(RuntimeSettings::lifeData().abnormalBoots) + ",";
    json += "\"lifeMotorSec\":" + String(RuntimeSettings::lifeData().motorRunSec) + ",";
    json += "\"lifeRunaway\":" + String(RuntimeSettings::lifeData().runawayTrips) + ",";
    json += "\"lifeKickout\":" + String(RuntimeSettings::lifeData().kickoutTrips) + ",";
    json += "\"lifeDeadman\":" + String(RuntimeSettings::lifeData().deadmanTrips) + ",";
    json += "\"uptimeSec\":" + String(millis() / 1000) + ",";
    json += "\"histCapSec\":" + String(History::capacitySec()) + ",";
    json += "\"histStoredSec\":" + String(History::storedSec()) + ",";
    // ---- Diagnostics (cards in the Diagnostics tab): WiFi, network, PGN, GPS, angle sensor, steer command, OTA ----
    json += "\"wifiLost\":" + String(WifiHandler::getLostCount()) + ",";
    json += "\"wifiReason\":" + String(WifiHandler::getLastDisconnectReason()) + ",";
    json += "\"wifiReasonAgeMs\":" + String(WifiHandler::getMsSinceLastDisconnect()) + ",";
    json += "\"wifiScans\":" + String(WifiHandler::getFailoverScans()) + ",";
    json += "\"wifiSwitches\":" + String(WifiHandler::getFailoverSwitches()) + ",";
    json += "\"wifiOutN\":" + String(WifiHandler::getOutageCount()) + ",";
    json += "\"wifiOutLastMs\":" + String(WifiHandler::getOutageLastMs()) + ",";
    json += "\"wifiOutMaxMs\":" + String(WifiHandler::getOutageMaxMs()) + ",";
    json += "\"wifiOutAvgMs\":" + String(WifiHandler::getOutageAvgMs()) + ",";
    json += "\"wifiOutNowMs\":" + String(WifiHandler::getOutageNowMs()) + ",";
    json += "\"wifiFbRestarts\":" + String(wifiFbCount) + ",";
    json += "\"txOk\":" + String(NetStats::txOk) + ",";
    json += "\"txFail\":" + String(NetStats::txFail) + ",";
    json += "\"txFailAgeMs\":" + String(NetStats::msSinceLastTxFail(millis())) + ",";
    json += "\"udpRebuilds\":" + String(NetStats::udpRebuilds) + ",";
    json += "\"rxBad\":" + String(NetStats::badFrames) + ",";
    json += "\"pgnOther\":" + String(NetStats::otherPgn) + ",";
    json += "\"crcOk\":" + String(NetStats::crcOk) + ",";
    json += "\"crcBad\":" + String(NetStats::crcBad) + ",";
    json += "\"crcRejected\":" + String(NetStats::crcRejected) + ",";
    json += "\"pandaTryMaxMs\":" + String(GapStats::pandaTry.maxMs()) + ",\"pandaTryN1\":" + String(GapStats::pandaTry.countOver1s()) + ",\"pandaTryN2\":" + String(GapStats::pandaTry.countOver2s()) + ",";
    json += "\"pandaOkMaxMs\":" + String(GapStats::pandaOk.maxMs()) + ",\"pandaOkN1\":" + String(GapStats::pandaOk.countOver1s()) + ",\"pandaOkN2\":" + String(GapStats::pandaOk.countOver2s()) + ",";
    json += "\"ggaGapMaxMs\":" + String(GapStats::gga.maxMs()) + ",\"ggaGapN1\":" + String(GapStats::gga.countOver1s()) + ",\"ggaGapN2\":" + String(GapStats::gga.countOver2s()) + ",";
    json += "\"pandaBigGapAgeMs\":" + String(GapStats::pandaOk.msSinceLastBigGap(millis())) + ",";
    json += "\"rtcmSource\":" + String(RtcmArbiter::activeSource(millis())) + ",";
    json += "\"rtcmDropBytes\":" + String(RtcmArbiter::droppedBytes) + ",";
    json += "\"rtcmDropPackets\":" + String(RtcmArbiter::droppedPackets) + ",";
    json += "\"pgnRx\":[";
    for (uint8_t i = 0; i < NetStats::used; i++) {
      if (i > 0) json += ",";
      json += "{\"p\":" + String(NetStats::table[i].pgn) + ",\"n\":" + String(NetStats::table[i].count) + "}";
    }
    json += "],";
    json += "\"gpsBaud\":" + String(GpsHandler::getActiveBaud()) + ",";
    json += "\"gpsBaudSrc\":" + String(GpsHandler::getBaudSource()) + ",";
    json += "\"gpsChipset\":" + String(GpsHandler::getChipsetState()) + ",";
    json += "\"gpsCfg\":" + String(GpsHandler::getConfigResult()) + ",";
    json += "\"gpsChipsetInfo\":\"" + String(GpsHandler::getChipsetInfo()) + "\",";
    json += "\"gpsRetryActive\":" + String(GpsHandler::isRetryActive() ? "true" : "false") + ",";
    json += "\"gpsRetryAttempts\":" + String(GpsHandler::getRetryAttempts()) + ",";
    json += "\"gpsValidNmea\":" + String(GpsHandler::hasEverGotValidNmea() ? "true" : "false") + ",";
    json += "\"steerVolts\":" + String(SteeringAngleSensor::getVolts(), 3) + ",";
    json += "\"steerI2cErr\":" + String(SteeringAngleSensor::getI2cErrors()) + ",";
    json += "\"steerReadAgeMs\":" + String(SteeringAngleSensor::getReadAgeMs()) + ",";
    json += "\"steerRawInRange\":" + String(SteeringAngleSensor::isRawInRange() ? "true" : "false") + ",";   // false = the raw value is outside the calibrated range + margin (broken wire?)
    {
      uint8_t raw[16];
      uint8_t rn = SteerCommandReceiver::copyLastRaw(raw, sizeof(raw));
      String hex;
      hex.reserve(rn * 3 + 1);
      for (uint8_t i = 0; i < rn; i++) {
        char b[4];
        snprintf(b, sizeof(b), "%02X", raw[i]);
        if (i > 0) hex += ' ';
        hex += b;
      }
      json += "\"cmdRaw\":\"" + hex + "\",";
    }
    json += "\"cmdRxCount\":" + String(SteerCommandReceiver::getRxCount()) + ",";
    json += "\"cmdDeltaMs\":" + String(SteerCommandReceiver::getLastDeltaMs()) + ",";
    json += "\"cmdMaxDeltaMs\":" + String(SteerCommandReceiver::getMaxDeltaMs()) + ",";
    json += "\"otaState\":" + String(OtaStatus::getState()) + ",";
    json += "\"otaSource\":" + String(OtaStatus::getSource()) + ",";
    json += "\"otaBytes\":" + String(OtaStatus::getBytes()) + ",";
    json += "\"otaTotal\":" + String(OtaStatus::getTotal()) + ",";
    json += "\"otaError\":\"" + String(OtaStatus::getError()) + "\",";
    json += "\"otaAgeMs\":" + String(OtaStatus::msSinceEnd(millis())) + ",";
    json += "\"firmwareVersion\":\"" + String(FIRMWARE_VERSION) + "\",";
    json += "\"mdnsName\":\"" + String(ENABLE_ARDUINO_OTA ? OTA_HOSTNAME : "") + "\",";   // adres http://<nazwa>.local (mDNS z ArduinoOTA)
    json += "\"firmwareBuild\":\"" + String(FIRMWARE_BUILD) + "\",";
    json += "\"coreZeroLoadPct\":" + String(coreZeroLoadPercent, 3) + ",";
    json += "\"loopAvgMs\":" + String(lastAvgLoopMs, 2) + ",";
    if (DEBUG_STEP_TIMING) {
      uint8_t n = CpuProfile::count();
      uint8_t order[CpuProfile::MAX_STEPS];
      for (uint8_t i = 0; i < n; i++) order[i] = i;
      for (uint8_t i = 1; i < n; i++) {           // insertion sort, n <= 24
        uint8_t v = order[i]; int8_t j = i - 1;
        while (j >= 0 && CpuProfile::pctAt(order[j]) < CpuProfile::pctAt(v)) { order[j + 1] = order[j]; j--; }
        order[j + 1] = v;
      }
      json += "\"cpuSteps\":[";
      for (uint8_t k = 0; k < n && k < 14; k++) {
        if (k > 0) json += ",";
        json += "{\"n\":\"" + String(CpuProfile::nameAt(order[k])) + "\",\"p\":" +
        String(CpuProfile::pctAt(order[k]), 1) + ",\"m\":" + String(CpuProfile::maxUsAt(order[k])) + "}";
      }
      json += "],";
    }
    json += "\"loopLoadPct\":" + String(lastLoopLoadPercent, 1) + ",";
    json += "\"wifiSsid\":\"" + jsonEsc(WIFI_SSID) + "\",";
    json += "\"wifiPassSet\":" + String(WIFI_PASS.length() > 0 ? "true" : "false") + ",";   // hasla NIE opuszczaja modulu
    json += "\"wifiSsid2\":\"" + jsonEsc(WIFI_SSID2) + "\",";
    json += "\"wifiPass2Set\":" + String(WIFI_PASS2.length() > 0 ? "true" : "false") + ",";   // hasla NIE opuszczaja modulu
    json += "\"wifiSsid3\":\"" + jsonEsc(WIFI_SSID3) + "\",";
    json += "\"wifiPass3Set\":" + String(WIFI_PASS3.length() > 0 ? "true" : "false") + ",";   // hasla NIE opuszczaja modulu
    json += "\"wifiSsid4\":\"" + jsonEsc(WIFI_SSID4) + "\",";
    json += "\"wifiPass4Set\":" + String(WIFI_PASS4.length() > 0 ? "true" : "false") + ",";   // hasla NIE opuszczaja modulu
    // Which of the 1-4 known networks is CURRENTLY connected/being tried - visible
    // in the panel, so it is clear which of several networks is actually working.
    json += "\"wifiActiveSlot\":" + String((!apMode && !ethOn) ? WifiHandler::getCurrentSlot() : 0) + ",";
    json += "\"apSsid\":\"" + jsonEsc(AP_SSID) + "\",";
    json += "\"apPassSet\":" + String(AP_PASSWORD.length() > 0 ? "true" : "false") + ",";   // hasla NIE opuszczaja modulu
    json += "\"localIp\":\"" + LOCAL_IP.toString() + "\",";
    {   // the ACTUAL network (with DHCP / a phone hotspot it differs from the configured one) and where we send frames to AgOpenWeb
      IPAddress ipN, maskN, gwN;
      nmActiveNet(ipN, maskN, gwN);
      json += "\"ipNow\":\"" + ipN.toString() + "\",";
      json += "\"maskNow\":\"" + maskN.toString() + "\",";
      json += "\"gwNow\":\"" + gwN.toString() + "\",";
      json += "\"agioDest\":\"" + nmAgioDest().toString() + "\",";
      json += "\"agioKnown\":" + String(nmAgioKnownNow() ? "true" : "false") + ",";
      {   // AgOpenWeb senders (connectivity/agio_senders.h): who we talk to, mode, a list with counters - a card in Settings
        uint32_t nowA = (uint32_t)millis();
    json += "\"authMode\":\"" + String(PanelAuth::mode() == PanelAuth::PIN ? "pin" : PanelAuth::mode() == PanelAuth::PASSWORD ? "password" : PanelAuth::mode() == PanelAuth::DECLINED ? "off" : "none") + "\",";
        json += "\"agioMode\":\"" + String(AgioSenders::isManual() ? "manual" : "auto") + "\",";
        json += "\"agioSel\":\"" + IPAddress(AgioSenders::selected()).toString() + "\",";
        json += "\"agioSelActive\":" + String(AgioSenders::selectedActive(nowA) ? "true" : "false") + ",";
        json += "\"agioSwitches\":" + String(AgioSenders::switches) + ",\"agioIgnored\":" + String(AgioSenders::ignoredTotal) + ",";
        json += "\"agioSenders\":[";
        for (uint8_t i = 0, n = AgioSenders::count(); i < n; i++) {
          const AgioSenders::Entry* e = AgioSenders::nth(i);
          if (!e) break;
          if (i) json += ",";
          json += "{\"ip\":\"" + IPAddress(e->ip).toString() + "\",\"n\":" + String(e->frames) + ",\"ign\":" + String(e->ignored) +
                  ",\"age\":" + String(AgioSenders::ageMs(*e, nowA)) + ",\"sel\":" + String(e->ip == AgioSenders::selected() ? "true" : "false") + "}";
        }
        json += "],";
      }
      json += "\"wifiFixed1\":" + String(WIFI_STATIC[0] ? "true" : "false") + ",\"wifiFixed2\":" + String(WIFI_STATIC[1] ? "true" : "false") + ",";
      json += "\"wifiFixed3\":" + String(WIFI_STATIC[2] ? "true" : "false") + ",\"wifiFixed4\":" + String(WIFI_STATIC[3] ? "true" : "false") + ",";
      json += "\"wifiDhcpNow\":" + String(WifiHandler::currentSlotDhcp() ? "true" : "false") + ",";
    }
    json += "\"gateway\":\"" + GATEWAY.toString() + "\",";
    json += "\"subnet\":\"" + SUBNET_MASK.toString() + "\",";
    json += "\"agioIp\":\"" + AGIO_IP.toString() + "\",";
    json += "\"enSection\":" + String(ENABLE_SECTION_EXPANDER ? "true" : "false") + ",";
    json += "\"enMotor\":" + String(ENABLE_MOTOR_DRIVER ? "true" : "false") + ",";
    json += "\"enCurrent\":" + String(ENABLE_CURRENT_SENSOR ? "true" : "false") + ",";
    json += "\"enImu\":" + String(ENABLE_IMU ? "true" : "false") + ",";
    json += "\"enLed\":" + String(ENABLE_STATUS_LED ? "true" : "false") + ",";
    json += "\"machCfgRx\":" + String(MachineConfigReceiver::hasReceived() ? "true" : "false") + ",";   // PGN 238 received
    json += "\"liftRaiseSec\":" + String(MachineConfigReceiver::getRaiseTime()) + ",";
    json += "\"liftLowerSec\":" + String(MachineConfigReceiver::getLowerTime()) + ",";
    json += "\"liftToolEnable\":" + String(MachineConfigReceiver::getEnableToolLift()) + ",";
    json += "\"machInvert\":" + String(MachineConfigReceiver::getInvertRelay() ? "true" : "false") + ",";   // "Invert relay" (PGN 238 set0 bit 0)
    json += "\"machUser\":[" + String(MachineConfigReceiver::getUser(0)) + "," + String(MachineConfigReceiver::getUser(1)) + "," +
            String(MachineConfigReceiver::getUser(2)) + "," + String(MachineConfigReceiver::getUser(3)) + "],";
    json += "\"machPinsRx\":" + String(MachinePinConfigReceiver::hasReceived() ? "true" : "false") + ",";   // PGN 236 received this session
    json += "\"machPins\":[";
    for (uint8_t i = 0; i < MachinePinMap::HW; i++) { if (i) json += ","; json += String(MachinePinConfigReceiver::map().fn[i]); }
    json += "],";
    json += "\"machOut\":" + String(SectionExpander::getLogicalMask()) + ",";   // outputs that are on (bit n = pin n+1)
    json += "\"hydDownActive\":" + String(HydraulicLift::isDownActive() ? "true" : "false") + ",";
    json += "\"steerCfgRx\":" + String(SteerConfigReceiver::hasReceived() ? "true" : "false") + ",";        // PGN 251 received this session
    json += "\"steerCfgSet0\":" + String(SteerConfigReceiver::getSet0()) + ",\"steerCfgSet1\":" + String(SteerConfigReceiver::getSet1()) + ",";
    json += "\"motorInvertAog\":" + String(SteerConfigReceiver::invertMotor() ? "true" : "false") + ",";   // "Invert motor" from AgOpenWeb (applied)
    json += "\"ethGraceMs\":" + String(ETHERNET_DOWN_GRACE_MS) + ",";
    json += "\"wifiTimeoutMs\":" + String(WIFI_CONNECT_TIMEOUT_MS) + ",";
    json += "\"watchdogS\":" + String(WATCHDOG_TIMEOUT_S) + ",";
    json += "\"ledBrightness\":" + String(STATUS_LED_BRIGHTNESS) + ",";
    json += "\"steerLeftDeg\":" + String(STEERING_ANGLE_LEFT_DEG, 1) + ",";
    json += "\"steerRightDeg\":" + String(STEERING_ANGLE_RIGHT_DEG, 1) + ",";
    json += "\"debugStepTiming\":" + String(DEBUG_STEP_TIMING ? "true" : "false") + ",";
    json += "\"debugWifi\":" + String(DEBUG_WIFI ? "true" : "false") + ",";
    json += "\"debugEthernet\":" + String(DEBUG_ETHERNET ? "true" : "false") + ",";
    json += "\"debugNet\":" + String(DEBUG_NET ? "true" : "false") + ",";
    json += "\"debugPgn\":" + String(DEBUG_PGN ? "true" : "false") + ",";
    json += "\"debugGps\":" + String(DEBUG_GPS ? "true" : "false") + ",";
    json += "\"debugRtcm\":" + String(DEBUG_RTCM ? "true" : "false") + ",";
    json += "\"debugSteerButton\":" + String(DEBUG_STEER_BUTTON ? "true" : "false") + ",";
    json += "\"debugWorkSwitch\":" + String(DEBUG_WORK_SWITCH ? "true" : "false") + ",";
    json += "\"debugSteeringSensor\":" + String(DEBUG_STEERING_SENSOR ? "true" : "false") + ",";
    json += "\"debugCurrent\":" + String(DEBUG_CURRENT_SENSOR ? "true" : "false") + ",";
    json += "\"debugMotor\":" + String(DEBUG_MOTOR ? "true" : "false") + ",";
    json += "\"debugImu\":" + String(DEBUG_IMU ? "true" : "false") + ",";
    json += "\"debugSteerCommand\":" + String(DEBUG_STEER_COMMAND ? "true" : "false") + ",";
    json += "\"debugSteerCommandRaw\":" + String(DEBUG_STEER_COMMAND_RAW ? "true" : "false") + ",";
    json += "\"debugMachineCommand\":" + String(DEBUG_MACHINE_COMMAND ? "true" : "false") + ",";
    json += "\"debugSys\":" + String(DEBUG_SYS ? "true" : "false") + ",";
    json += "\"debugWeb\":" + String(DEBUG_WEB ? "true" : "false") + ",";
    // Age of the last packet from AgOpenWeb (-1 = never yet) - the "AgOpenWeb" tile in the panel.
    json += "\"agioAgeMs\":" + String(lastAgioPacketMillis != 0 ? (long)(millis() - lastAgioPacketMillis) : -1L) + ",";
    {   // free-RAM trend: slope [B/h] over the last hours, heapLeak = a leak is likely (diagnostics/heap_trend.h)
      const HeapTrend::Result ht = HeapMonitor::trend.result();
      json += "\"heapTrendValid\":" + String(ht.valid ? "true" : "false") + ",\"heapTrendBph\":" + String((long)ht.slopeBph) +
              ",\"heapTrendBuckets\":" + String(ht.buckets) + ",\"heapTrendNeed\":" + String((unsigned)HeapTrend::MIN_BUCKETS) +
              ",\"heapLeak\":" + String(HeapMonitor::trend.leakSuspect() ? "true" : "false") + ",";
    }
    json += "\"minFreeHeap\":" + String(ESP.getMinFreeHeap()) + ",";
    json += "\"maxAllocHeap\":" + String(ESP.getMaxAllocHeap()) + ",";
    json += "\"chipTempC\":" + String(temperatureRead(), 1) + ",";
    json += "\"resetReason\":\"" + String(resetReasonText()) + "\",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"psramFound\":" + String(psramFound() ? "true" : "false") + ",";
    json += "\"freePsram\":" + String(ESP.getFreePsram()) + ",";
    json += "\"totalPsram\":" + String(ESP.getPsramSize());
    json += "}";
    out.flush();
    server.sendContent("");   // closing the chunked response
  }

  static void handleLog() {
    server.send(200, "text/plain; charset=utf-8", LogBuffer::snapshot());
  }

  // Password from the form. The panel does not receive passwords from the module (/status only reports "set"), so:
  // no argument = unchanged, <name>Clear=1 = delete the password, otherwise a new value.
  static String passArg(const char* name, const String& current) {
    String clearName = String(name) + "Clear";
    if (server.hasArg(clearName) && server.arg(clearName) == "1") return String("");
    if (!server.hasArg(name)) return current;
    return server.arg(name);
  }

  static void handleSettingsSaveNetwork() {
    if (refuseWhileEngaged("network settings")) return;   // about 20 flash writes: they stall the CPU while steering
    if (!(server.hasArg("wifiSsid") && server.hasArg("apSsid"))) {
      server.send(400, "text/plain", MSG("Required fields missing"));
      return;
    }
    // Validation BEFORE saving: among other things an AP password < 8 characters makes it impossible to start the access
    // point (the module would be unreachable when there is no known network), and a bad IP address used to be
    // silently skipped.
    const String pw1 = passArg("wifiPass", WIFI_PASS),   pw2 = passArg("wifiPass2", WIFI_PASS2);
    const String pw3 = passArg("wifiPass3", WIFI_PASS3), pw4 = passArg("wifiPass4", WIFI_PASS4);
    const String pwAp = passArg("apPass", AP_PASSWORD);
    const char* err = SettingsValidation::validateNetwork(
      server.arg("wifiSsid").c_str(), pw1.c_str(),
                                                          server.arg("wifiSsid2").c_str(), pw2.c_str(),
                                                          server.arg("wifiSsid3").c_str(), pw3.c_str(),
                                                          server.arg("wifiSsid4").c_str(), pw4.c_str(),
                                                          server.arg("apSsid").c_str(), pwAp.c_str(),
                                                          server.arg("localIp").c_str(), server.arg("gateway").c_str(),
                                                          server.arg("subnet").c_str(), server.arg("agioIp").c_str());
    if (err) { server.send(400, "text/plain", err); return; }
    // Forced STATIC address for slots 1-4 ("0"/"1"; default 0 = DHCP). No field = unchanged (an older panel page does not know these fields).
    bool fixedIp[4] = {WIFI_STATIC[0], WIFI_STATIC[1], WIFI_STATIC[2], WIFI_STATIC[3]};
    for (int i = 0; i < 4; i++) {
      String key = String("fixed") + String(i + 1);
      if (!server.hasArg(key)) continue;
      if (!SettingsValidation::parseBool01(server.arg(key).c_str(), &fixedIp[i])) { server.send(400, "text/plain", MSG("Static address: value 0 or 1")); return; }
    }
    RuntimeSettings::saveNetwork(server.arg("wifiSsid"), pw1,
                                 server.arg("wifiSsid2"), pw2,
                                 server.arg("wifiSsid3"), pw3,
                                 server.arg("wifiSsid4"), pw4,
                                 server.arg("apSsid"), pwAp,
                                 server.arg("localIp"), server.arg("gateway"),
                                 server.arg("subnet"), server.arg("agioIp"),
                                 fixedIp[0], fixedIp[1], fixedIp[2], fixedIp[3]);
    server.send(200, "text/plain", "OK");
  }

  static void handleSettingsSaveNtrip() {
    if (!(server.hasArg("ntripHost") && server.hasArg("ntripPort") && server.hasArg("ntripMount"))) {
      server.send(400, "text/plain", MSG("Required fields missing"));
      return;
    }
    uint16_t port = 0;
    const String ntripPw = passArg("ntripPass", NTRIP_PASS);
    const char* err = SettingsValidation::validateNtrip(
      server.arg("ntripHost").c_str(), server.arg("ntripPort").c_str(), server.arg("ntripMount").c_str(),
                                                        server.arg("ntripUser").c_str(), ntripPw.c_str(), &port);
    if (err) { server.send(400, "text/plain", err); return; }
    RuntimeSettings::saveNtripSettings(server.hasArg("enNtrip") && server.arg("enNtrip") == "1",
                                       server.arg("ntripHost"), port,
                                       server.arg("ntripMount"), server.arg("ntripUser"), ntripPw);
    NtripClient::settingsChanged();   // reconnect with the new data now, not after the old back-off
    server.send(200, "text/plain", "OK");
  }

  static void handleSettingsSaveDebug() {
    const char* keys[] = {"dStepTiming", "dWifi", "dEth", "dNet", "dPgn", "dGps", "dRtcm",
      "dSteerBtn", "dWorkSw", "dSteerSens", "dCurrent", "dMotor", "dImu",
      "dSteerCmd", "dSteerCmdRaw", "dMachCmd", "dSys", "dWeb"};
      for (const char* key : keys) {
        if (server.hasArg(key)) {
          RuntimeSettings::setDebugFlag(key, server.arg(key) == "1");
        }
      }
      server.send(200, "text/plain", "OK");
  }

  static void handleSettingsSaveFeatures() {
    if (refuseWhileEngaged("feature switches")) return;
    RuntimeSettings::saveFeatureFlags(
      server.hasArg("enSection") && server.arg("enSection") == "1",
                                      server.hasArg("enMotor") && server.arg("enMotor") == "1",
                                      server.hasArg("enCurrent") && server.arg("enCurrent") == "1",
                                      server.hasArg("enImu") && server.arg("enImu") == "1",
                                      server.hasArg("enLed") && server.arg("enLed") == "1");
    server.send(200, "text/plain", "OK");
  }

  // Ethernet on/off (W5500): saved now, used after a restart. Refused while steering is engaged only for the restart itself
  // (that has its own check); saving the flag cannot interrupt anything.
  static void handleSettingsSaveEthernet() {
    if (refuseWhileEngaged("Ethernet settings")) return;
    if (!server.hasArg("enEthernet") || (server.arg("enEthernet") != "0" && server.arg("enEthernet") != "1")) {
      server.send(400, "text/plain", MSG("Required fields missing"));
      return;
    }
    RuntimeSettings::saveEthernet(server.arg("enEthernet") == "1");
    server.send(200, "text/plain", "OK");
  }

  static void handleSettingsSaveThresholds() {
    if (refuseWhileEngaged("thresholds and times")) return;
    if (!(server.hasArg("ethGraceMs") && server.hasArg("wifiTimeoutMs") && server.hasArg("watchdogS") && server.hasArg("ledBrightness"))) {
      server.send(400, "text/plain", MSG("Required fields missing"));
      return;
    }
    // Previously toInt() turned any text into 0 - watchdog = 0 is a restart loop after saving.
    SettingsValidation::Thresholds th;
    const char* err = SettingsValidation::validateThresholds(
      server.arg("ethGraceMs").c_str(), server.arg("wifiTimeoutMs").c_str(),
                                                             server.arg("watchdogS").c_str(), server.arg("ledBrightness").c_str(), &th);
    if (err) { server.send(400, "text/plain", err); return; }
    RuntimeSettings::saveThresholds(th.ethGraceMs, th.wifiTimeoutMs, th.watchdogS, th.ledBrightness);
    server.send(200, "text/plain", "OK");
  }

  // Chart history recorded by the MODULE (see diagnostics/history.h) - available also when
  // the browser was closed. A binary response (format: history_store.h):
  //   /history?range=<seconds>&points=<max number of points>
  // For long ranges the module aggregates to <= points points, keeping the WORST value
  // in the bucket (an IMU dropout, fix drop, CPU spike do not vanish when averaging).
  // Events for the chart markers: {"nowMs":uptime,"events":[{"a":age_ms,"t":type,"v":arg},...]} from the oldest.
  // The age (a) is counted from the moment of the response - the browser computes the clock from it. Types: see diagnostics/event_log.h.
  static void handleEvents() {
    EventLog::Event ev[EventLog::MAX];
    uint8_t n = EventLog::snapshot(ev, EventLog::MAX);
    uint32_t now = millis();
    String json;
    json.reserve(120 + ((size_t)n + PersistLog::prevCount()) * 40);
    json += "{\"nowMs\":" + String(now) + ",\"events\":[";
    for (uint8_t i = 0; i < n; i++) {
      if (i) json += ",";
      json += "{\"a\":" + String((uint32_t)(now - ev[i].ms)) + ",\"t\":" + String((int)ev[i].type) + ",\"v\":" + String((int)ev[i].arg) + "}";
    }
    // Previous run (RTC memory): b = how many ms BEFORE the end of operation the event occurred, up = uptime of that run.
    json += "],\"prev\":{\"n\":" + String(PersistLog::prevCount()) + ",\"sess\":" + String(PersistLog::prevSession()) +
            ",\"up\":" + String(PersistLog::prevEndMs()) + ",\"utc\":" + String(PersistLog::prevUtc()) + ",\"ev\":[";
    for (uint8_t i = 0; i < PersistLog::prevCount(); i++) {
      const PersistLog::Entry& e = PersistLog::prevAt(i);
      uint32_t before = PersistLog::prevEndMs() >= e.ms ? PersistLog::prevEndMs() - e.ms : 0;
      if (i) json += ",";
      json += "{\"b\":" + String(before) + ",\"t\":" + String((int)e.type) + ",\"v\":" + String((int)e.arg) + "}";
    }
    json += "]}}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
  }

  static void handleHistory() {
    if (!History::isReady()) {
      server.send(503, "text/plain", MSG("history unavailable (no PSRAM)"));
      return;
    }
    long range = 3600, points = 720;
    if (server.hasArg("range") && !SettingsValidation::parseLong(server.arg("range").c_str(), 1, 172800, &range)) {
      server.send(400, "text/plain", MSG("range: number of seconds 1-172800"));
      return;
    }
    if (server.hasArg("points") && !SettingsValidation::parseLong(server.arg("points").c_str(), 10, 4000, &points)) {
      server.send(400, "text/plain", MSG("points: a number 10-4000"));
      return;
    }
    HistoryStore::Plan plan;
    if (!History::plan((uint32_t)range, (uint32_t)points, &plan)) {
      server.send(500, "text/plain", MSG("history plan error"));
      return;
    }
    uint8_t hdr[320];
    if (HistoryStore::headerSize() > sizeof(hdr)) {
      server.send(500, "text/plain", MSG("history header too large"));
      return;
    }
    // A buffer for one series (<= 4000 points = 8 KB) from the ordinary heap - the web task's stack is only 8 KB.
    int16_t* seriesBuf = (plan.nPoints > 0) ? (int16_t*)malloc((size_t)plan.nPoints * sizeof(int16_t)) : nullptr;
    if (plan.nPoints > 0 && seriesBuf == nullptr) {
      server.send(503, "text/plain", MSG("out of memory for the response"));
      return;
    }
    size_t hl = History::writeHeader(hdr, plan);
    size_t total = hl + (size_t)HistoryFormat::NS * plan.nPoints * sizeof(int16_t);
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(total);
    server.send(200, "application/octet-stream", "");
    server.sendContent((const char*)hdr, hl);
    for (int s = 0; s < HistoryFormat::NS && plan.nPoints > 0; s++) {
      History::series(plan, s, seriesBuf);
      server.sendContent((const char*)seriesBuf, (size_t)plan.nPoints * sizeof(int16_t));
    }
    free(seriesBuf);
  }

  // Manual reset of the IMU sensor through RST (a test of the RST wire and manual recovery).
  // Refused when steering is enabled: the reset returns yaw to 0 and would change the heading
  // while driving.
  static void handleImuReset() {
    if (SteerCommandReceiver::isAutoSteerActive()) {
      server.send(409, "text/plain", MSG("steering is engaged - resetting the sensor would change the heading while driving"));
      return;
    }
    if (ImuSensor::requestManualReset()) {
      LogBuffer::println("[IMU] Manual sensor reset (RST test) requested from the web panel.");
      server.send(200, "text/plain", "OK");
    } else {
      server.send(409, "text/plain", MSG("reset unavailable (IMU_RESET_PIN = -1, IMU disabled or a reset is already in progress)"));
    }
  }

  static void handleSettingsSaveImuRotation() {
    if (refuseWhileEngaged("IMU rotation")) return;   // changes the roll/pitch used for steering compensation
    if (!server.hasArg("rotDeg")) { server.send(400, "text/plain", MSG("Required fields missing")); return; }
    float rot = 0.0f;
    const char* err = SettingsValidation::validateImuRotation(server.arg("rotDeg").c_str(), &rot);
    if (err) { server.send(400, "text/plain", err); return; }
    int rollInv = -1;   // optional field: absent = leave the roll sign unchanged
    if (server.hasArg("rollInvert")) {
      const String v = server.arg("rollInvert");
      if (v != "0" && v != "1") { server.send(400, "text/plain", MSG("Invalid value")); return; }
      rollInv = (v == "1") ? 1 : 0;
    }
    RuntimeSettings::saveImuRotation(rot, rollInv);
    server.send(200, "text/plain", "OK");
  }

  // One pose of the 3D mounting calibration: <pre>Pitch, <pre>Roll (the sensor's raw angles, deg) and <pre>Ax/Ay/Az (accelerometer, m/s2).
  static bool readMountPose(const char* pre, ImuMount::Pose* p) {
    const char* suffix[5] = {"Pitch", "Roll", "Ax", "Ay", "Az"};
    const float lim[5] = {90.0f, 180.0f, 200.0f, 200.0f, 200.0f};
    float v[5];
    for (int i = 0; i < 5; i++) {
      const String name = String(pre) + suffix[i];
      if (!server.hasArg(name) || !SettingsValidation::parseFloatRange(server.arg(name).c_str(), -lim[i], lim[i], &v[i])) return false;
    }
    *p = ImuMount::Pose{v[0], v[1], ImuMount::V3{v[2], v[3], v[4]}};
    return true;
  }

  // Full 3D mounting of the IMU board (sensors/imu_mount.h): solved from two poses (A level, B tilted), then saved. Works immediately.
  static void handleImuMountSolve() {
    if (refuseWhileEngaged("IMU 3D mounting")) return;   // changes the heading, roll and pitch used for steering
    ImuMount::Pose a, b;
    if (!readMountPose("a", &a) || !readMountPose("b", &b) || !server.hasArg("kind")) {
      server.send(400, "text/plain", MSG("Required fields missing or invalid"));
      return;
    }
    const String kind = server.arg("kind");
    if (kind != "0" && kind != "1") { server.send(400, "text/plain", MSG("Invalid value")); return; }
    ImuMount::Mount m;
    const ImuMount::Result r = ImuMount::solve(a, b, kind == "1" ? ImuMount::Tilt::NOSE_UP : ImuMount::Tilt::SIDE_RIGHT_DOWN, m);
    if (r != ImuMount::Result::OK) { server.send(400, "text/plain", ImuMount::resultText(r)); return; }
    RuntimeSettings::saveImuMount3d(m);
    server.send(200, "text/plain", "OK");
  }

  // Sets the 3D mounting from its text form (settings backup) or switches it off (m empty). Works immediately.
  static void handleSettingsSaveImuMount3d() {
    if (refuseWhileEngaged("IMU 3D mounting")) return;
    if (!server.hasArg("m")) { server.send(400, "text/plain", MSG("Required fields missing")); return; }
    ImuMount::Mount m;
    if (!ImuMount::parse(server.arg("m").c_str(), m)) { server.send(400, "text/plain", MSG("Invalid value")); return; }
    RuntimeSettings::saveImuMount3d(m);
    server.send(200, "text/plain", "OK");
  }

  // Choosing the AgOpenWeb sender: ip = address (pinning, only this sender) or empty (automatic mode). Works immediately.
  // ---- panel protection (PIN / password): see web/panel_auth.h ----
  // Every state-CHANGING handler is registered through SECURED(): without a valid token (X-Auth header) it answers 401.
  // When the panel is not protected (unset or disabled), everything passes as before.
  // Cross-site guard (web/request_guard.h): a POST sent by a page from ANOTHER site is refused even when the panel has no PIN.
  static bool originOk() {
    if (RequestGuard::hostAllowed(server.header("Host").c_str(), OTA_HOSTNAME) &&
        RequestGuard::sameOrigin(server.header("Origin").c_str(), server.header("Host").c_str())) return true;
    LogBuffer::println("[WEB] Refused a request from another site (Origin does not match).");
    server.send(403, "text/plain", MSG("Request from another site refused"));
    return false;
  }

  static bool requireAuth() {
    if (!originOk()) return false;
    if (PanelAuth::authorized(server.header("X-Auth").c_str(), millis())) return true;
    server.send(401, "text/plain", MSG("Login required (panel PIN or password)"));
    return false;
  }

  static void randomWords(uint32_t out[4]) { for (int i = 0; i < 4; i++) out[i] = esp_random(); }

  // 429: too many wrong login attempts - how many more seconds to wait (a template for translation: the msg_* key in logsrc.h)
  static void sendLockedOut(uint32_t leftMs) {
    char b[80];
    snprintf(b, sizeof(b), MSG("Too many wrong attempts - try again in %lu s"), (unsigned long)((leftMs + 999) / 1000));
    server.send(429, "text/plain", b);
  }

  static void handleAuthLogin() {
    if (!originOk()) return;
    if (!PanelAuth::isProtected()) { server.send(400, "text/plain", MSG("The panel is not protected")); return; }
    uint32_t rnd[4]; randomWords(rnd);
    char tok[33]; uint32_t left = 0;
    PanelAuth::LoginResult r = PanelAuth::login(server.arg("secret").c_str(), millis(), rnd, tok, &left);
    if (r == PanelAuth::LOGIN_OK) { server.send(200, "text/plain", tok); return; }
    if (r == PanelAuth::LOGIN_LOCKED) {
      LogBuffer::println("[PANEL] Login blocked after too many wrong attempts.");
      sendLockedOut(left);
      return;
    }
    LogBuffer::println("[PANEL] Wrong login code.");
    server.send(403, "text/plain", PanelAuth::mode() == PanelAuth::PIN ? MSG("Wrong PIN") : MSG("Wrong password"));
  }

  // mode = pin | password | off, secret = new code, current = current code. The first setup (panel open) does not require login;
  // a change requires the token AND the current code (once more).
  static void handleAuthSetup() {
    if (!originOk()) return;
    // Changing/disabling an existing protection: log in AND confirm the current code (the token could have stayed on an unlocked phone).
    if (PanelAuth::isProtected()) {
      if (!requireAuth()) return;
      uint32_t left = 0;
      PanelAuth::LoginResult cr = PanelAuth::check(server.arg("current").c_str(), millis(), &left);
      if (cr == PanelAuth::LOGIN_LOCKED) { sendLockedOut(left); return; }
      if (cr != PanelAuth::LOGIN_OK) {
        LogBuffer::println("[PANEL] Protection change rejected: wrong current code.");
        server.send(403, "text/plain", PanelAuth::mode() == PanelAuth::PIN ? MSG("Wrong current PIN") : MSG("Wrong current password"));
        return;
      }
    }
    String m = server.arg("mode"), sec = server.arg("secret");
    PanelAuth::Mode mode;
    if (m == "pin") mode = PanelAuth::PIN;
    else if (m == "password") mode = PanelAuth::PASSWORD;
    else if (m == "off") mode = PanelAuth::DECLINED;
    else { server.send(400, "text/plain", MSG("Unknown protection mode")); return; }
    const char* err = PanelAuth::validateSecret(mode, sec.c_str());
    if (err) { server.send(400, "text/plain", err); return; }
    if (!RuntimeSettings::saveAuth(mode, sec.c_str())) { server.send(400, "text/plain", MSG("Protection not saved")); return; }
    if (mode == PanelAuth::DECLINED) { server.send(200, "text/plain", "OK"); return; }
    uint32_t rnd[4]; randomWords(rnd);
    char tok[33]; PanelAuth::issue(millis(), rnd, tok);   // the authorized user need not log in again right away
    server.send(200, "text/plain", tok);
  }

  static void handleSettingsSaveAgioSender() {
    if (refuseWhileEngaged("AgOpenWeb sender selection")) return;
    if (!server.hasArg("ip")) { server.send(400, "text/plain", MSG("Required fields missing")); return; }
    String ip = server.arg("ip");
    ip.trim();
    const char* err = SettingsValidation::validateAgioSender(ip.c_str());
    if (err) { server.send(400, "text/plain", err); return; }
    IPAddress addr(0, 0, 0, 0);
    if (ip.length() > 0 && !addr.fromString(ip)) { server.send(400, "text/plain", MSG("AgOpenWeb sender address: invalid IP address")); return; }
    RuntimeSettings::saveAgioSender(addr);
    nmAgioSelectionChanged();
    server.send(200, "text/plain", "OK");
  }

  static void handleSettingsSaveAgioEngage() {
    if (refuseWhileEngaged("engage-reading mode")) return;
    if (!server.hasArg("mask")) { server.send(400, "text/plain", MSG("Required fields missing")); return; }
    uint8_t m = 0;
    const char* err = SettingsValidation::validateAgioEngageMask(server.arg("mask").c_str(), &m);
    if (err) { server.send(400, "text/plain", err); return; }
    RuntimeSettings::saveAgioEngageMask(m);
    server.send(200, "text/plain", "OK");
  }

  static void handleSettingsSaveSteeringRange() {
    if (refuseWhileEngaged("steering range")) return;
    if (!(server.hasArg("leftDeg") && server.hasArg("rightDeg"))) { server.send(400, "text/plain", MSG("Required fields missing")); return; }
    float l = 0.0f, r = 0.0f;
    const char* err = SettingsValidation::validateSteeringRange(server.arg("leftDeg").c_str(), server.arg("rightDeg").c_str(), &l, &r);
    if (err) { server.send(400, "text/plain", err); return; }
    RuntimeSettings::saveSteeringRange(l, r);
    server.send(200, "text/plain", "OK");
  }

  static void handleSettingsSaveCurrent() {
    if (refuseWhileEngaged("current protection settings")) return;
    if (!(server.hasArg("currKickoutA") && server.hasArg("currKickoutMs") && server.hasArg("currHardA"))) { server.send(400, "text/plain", MSG("Required fields missing")); return; }
    float a = 0.0f, hard = 0.0f; long ms = 0;
    const char* err = SettingsValidation::validateCurrent(server.arg("currKickoutA").c_str(), server.arg("currKickoutMs").c_str(),
                                                          server.arg("currHardA").c_str(), &a, &ms, &hard);
    if (err) { server.send(400, "text/plain", err); return; }
    RuntimeSettings::saveCurrentSettings(a, (unsigned long)ms, hard);
    server.send(200, "text/plain", "OK");
  }

  // Zeroing the current sensor: only with steering disabled (the motor must stand still, otherwise the zero would come out wrong).
  // Calibration takes about 1-2 s and happens in the main loop; the result is in /status (currCalState).
  static void handleCurrentZero() {
    if (!ENABLE_CURRENT_SENSOR) { server.send(409, "text/plain", MSG("The current sensor is disabled (Settings -> Features).")); return; }
    if (SteerCommandReceiver::isAutoSteerActive()) { server.send(409, "text/plain", MSG("Zeroing only while steering is disengaged (the motor must be stopped).")); return; }
    CurrentSensor::requestZeroCalibration();
    server.send(200, "text/plain", "OK");
  }

  static void handleCurrentReset() {
    if (refuseWhileEngaged("current latch reset")) return;
    if (!ENABLE_CURRENT_SENSOR) { server.send(409, "text/plain", MSG("The current sensor is disabled (Settings -> Features).")); return; }
    CurrentSensor::requestReset();
    server.send(200, "text/plain", "OK");
  }

  static void handleSettingsSaveMotor() {
    if (refuseWhileEngaged("motor settings")) return;
    if (!(server.hasArg("runEn") && server.hasArg("runWindowMs") && server.hasArg("runMinMoveDeg") && server.hasArg("runMinErrDeg") && server.hasArg("motorInvert"))) {
      server.send(400, "text/plain", MSG("Required fields missing")); return; }
    SettingsValidation::MotorSettings m;
    const char* err = SettingsValidation::validateMotor(server.arg("runEn").c_str(), server.arg("runWindowMs").c_str(), server.arg("runMinMoveDeg").c_str(),
                                                        server.arg("runMinErrDeg").c_str(), server.arg("motorInvert").c_str(), &m);
    if (err) { server.send(400, "text/plain", err); return; }
    RuntimeSettings::saveMotorSettings(m.runEn, (unsigned long)m.windowMs, m.moveDeg, m.errDeg, m.invert);
    server.send(200, "text/plain", "OK");
  }

  // Manual motor test: a single Left/Right pulse of limited time and PWM (stopped by the firmware).
  static void handleMotorTest() {
    if (!(server.hasArg("dir") && server.hasArg("pwm") && server.hasArg("ms"))) { server.send(400, "text/plain", MSG("Required fields missing")); return; }
    int dir = 0; long pwm = 0, ms = 0;
    const char* err = SettingsValidation::validateMotorTest(server.arg("dir").c_str(), server.arg("pwm").c_str(), server.arg("ms").c_str(), &dir, &pwm, &ms);
    if (err) { server.send(400, "text/plain", err); return; }
    switch (MotorDriver::requestTestPulse(dir, (int)pwm, (unsigned long)ms)) {
      case MotorDriver::MT_OK:       server.send(200, "text/plain", "OK"); break;
      case MotorDriver::MT_DISABLED: server.send(409, "text/plain", MSG("The motor driver is disabled (Settings -> Features).")); break;
      case MotorDriver::MT_ENGAGED:  server.send(409, "text/plain", MSG("Steering in AgOpenWeb is engaged - the test only works while it is disengaged.")); break;
      case MotorDriver::MT_BUSY:     server.send(409, "text/plain", MSG("The previous test is still running.")); break;
      case MotorDriver::MT_CURRENT:  server.send(409, "text/plain", MSG("Current protection is blocking the motor (overcurrent, sensor error or no data) - check the Current sensor card.")); break;
      case MotorDriver::MT_ENDSTOP:  server.send(409, "text/plain", MSG("The wheel is at its end stop in this direction - test the other way.")); break;
      default:                       server.send(400, "text/plain", MSG("Invalid test parameters")); break;
    }
  }

  // Satellites: C/N0 per satellite and signal (+ whether used in the solution) for the bar chart in the panel (Diagnostics -> GPS).
  // A separate endpoint, so as not to bloat /status (it is even ~100 entries). An array [sys,prn,elev,azim,snr,signal,used]; snr = -1 when the
  // satellite is visible but has no signal. gsv/gsa: counters of received sentences (0 = the receiver does not send them).
  static void handleGpsSats() {
    GpsSats::Sat sats[GpsSats::MAX];
    bool usedKnown = false;
    int n = GpsSats::snapshot(sats, GpsSats::MAX, millis(), &usedKnown);
    String json;
    json.reserve(160 + n * 30);
    json += "{\"gsv\":" + String(GpsSats::rxGsv) + ",\"gsa\":" + String(GpsSats::rxGsa) + ",\"usedKnown\":" + String(usedKnown ? "true" : "false") + ",\"sats\":[";
    for (int i = 0; i < n; i++) {
      char b[40];
      snprintf(b, sizeof b, "%s[%u,%u,%u,%u,%d,%u,%u]", i ? "," : "", sats[i].sys, sats[i].prn, sats[i].elev, sats[i].az,
               sats[i].snr == GpsSats::SNR_NONE ? -1 : (int)sats[i].snr, sats[i].sig, sats[i].used ? 1 : 0);
      json += b;
    }
    json += "]}";
    server.send(200, "application/json", json);
  }

  static void handleRunawayReset() {
    if (refuseWhileEngaged("runaway latch reset")) return;
    if (!ENABLE_MOTOR_DRIVER) { server.send(409, "text/plain", MSG("The motor driver is disabled (Settings -> Features).")); return; }
    MotorDriver::requestRunawayReset();
    server.send(200, "text/plain", "OK");
  }

  // Restart, factory reset and firmware upload with steering ENABLED would stop the module while driving
  // (the motor loses control, AgOpenWeb loses the module). Refused with 409 - first disable steering in AgOpenWeb.
  static bool refuseWhileEngaged(const char* what) {
    if (!SteerCommandReceiver::isAutoSteerActive()) return false;
    LogBuffer::printf("[SYS] Refused: %s while steering is engaged.\n", what);
    server.send(409, "text/plain", MSG("Steering is engaged - disengage it in AgOpenWeb, then try again."));
    return true;
  }

  static void handleFactoryReset() {
    if (refuseWhileEngaged("factory reset")) return;
    RuntimeSettings::factoryReset();
    server.send(200, "text/plain", "OK");
    delay(300);
    ESP.restart();
  }

  static void handleRestart() {
    if (refuseWhileEngaged("restart")) return;
    server.send(200, "text/plain", "Restarting...");
    LogBuffer::printlnIf(DEBUG_SYS, "[SYS] Restart requested from the web page.");
    delay(300);
    ESP.restart();
  }

  // Without a HydUp / HydDown pin in the AgOpenWeb Pin Config a manual command would set an override that nothing will ever clear.
  static bool hydLiftOn() {
    if (HydraulicLift::isMapped()) return true;
    server.send(409, "text/plain", MSG("No HydUp / HydDown pin in the AgOpenWeb Pin Config"));
    return false;
  }

  static void handleHydLiftManualOn() {
    if (!hydLiftOn()) return;
    HydraulicLift::setManual(true);
    server.send(200, "text/plain", "OK");
  }

  static void handleHydLiftManualOff() {
    if (!hydLiftOn()) return;
    HydraulicLift::setManual(false);
    server.send(200, "text/plain", "OK");
  }

  static void handleHydLiftAuto() {
    HydraulicLift::setAuto();
    server.send(200, "text/plain", "OK");
  }

  // Saving a calibration point: 409 when the sensor has not delivered a reading; when after saving the LEFT-RIGHT
  // span is too small, the 200 response carries a warning (the panel shows it).
  static void sendCalResult(bool saved) {
    if (!saved) {
      server.send(409, "text/plain", MSG("the angle sensor is not responding - nothing saved"));
    } else if (!SteeringAngleSensor::isCalibrationValid()) {
      server.send(200, "text/plain", MSG("saved, but the LEFT-RIGHT span is too small (min. 200) - save the other point at the end stop"));
    } else {
      server.send(200, "text/plain", "OK");
    }
  }
  // Saving an end point rewrites the angle mapping: with steering engaged the angle would jump and the motor would follow.
  static void handleSteeringCalLeft() { if (refuseWhileEngaged("angle calibration")) return; sendCalResult(SteeringAngleSensor::captureLeft()); }
  static void handleSteeringCalRight() { if (refuseWhileEngaged("angle calibration")) return; sendCalResult(SteeringAngleSensor::captureRight()); }

  // OTA password (secrets.h -> OTA_PASSWORD). When set, /update requires login (user "admin");
  // the same password applies when uploading from the Arduino IDE (ota/arduino_ota.h). Empty = no password (as before).
  // Without an OTA password, a panel with a PIN/password requires the panel token (X-Auth, sent by the /update page);
  // only a panel left open on purpose lets /update through (see RequestGuard::otaAllowed).
  static bool otaAuthOk() {
    if (!RequestGuard::sameOrigin(server.header("Origin").c_str(), server.header("Host").c_str())) return false;
    const bool pwSet = strlen(OTA_PASSWORD) > 0;
    return RequestGuard::otaAllowed(pwSet, pwSet && server.authenticate("admin", OTA_PASSWORD),
                                    PanelAuth::isProtected(), PanelAuth::authorized(server.header("X-Auth").c_str(), millis()));
  }

  static void handleUpdatePage() {
    // The page itself is harmless; with an OTA password the browser asks for the login here, otherwise the upload checks the panel token.
    if (strlen(OTA_PASSWORD) > 0 && !server.authenticate("admin", OTA_PASSWORD)) { server.requestAuthentication(); return; }
    static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OTA Update</title>
<script>
/* language as in the panel (localStorage "lang", otherwise the browser language); catalogs from /lang.js, English as a fallback */
(function(){var l=null;try{l=localStorage.getItem("lang");}catch(e){}
if(!l||!/^[a-z]{2,5}$/.test(l))l=/^pl/i.test(navigator.language||"")?"pl":"en";
document.documentElement.lang=l;
document.write('<script src="/lang.js?l=en"><\/script>');
if(l!=="en")document.write('<script src="/lang.js?l='+l+'"><\/script>');})();
</script>
<style>
:root{--bg:#0b0f0d;--panel:#121815;--border:#26332c;--text:#e8ece9;--label:#8a9891;--accent:#d99a3f;--bad:#e0584f}
*{box-sizing:border-box}
body{background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;
  margin:0;display:flex;align-items:center;justify-content:center;min-height:100vh}
.card{max-width:380px;width:100%;margin:16px;padding:22px;border:1px solid var(--border);border-radius:12px;background:var(--panel)}
h3{margin:0 0 16px;font-size:16px;font-weight:600}
.drop{display:flex;flex-direction:column;align-items:center;gap:8px;margin-bottom:14px;padding:22px 14px;border:2px dashed var(--border);border-radius:10px;text-align:center;transition:border-color .15s,background .15s}
.drop.over{border-color:var(--accent);background:rgba(217,154,63,.08)}
.drop-t{font-size:14px;font-weight:600}
.drop-o{font-size:12px;color:var(--label)}
.vh{position:absolute;width:1px;height:1px;opacity:0;overflow:hidden}
.btn{display:block;width:100%;min-height:44px;border:1px solid var(--accent);border-radius:8px;background:var(--accent);color:#1a1408;
  font-weight:600;font-size:14px;font-family:inherit;cursor:pointer}
.btn:disabled{opacity:.5;cursor:default}
.btn.sec{display:inline-flex;align-items:center;justify-content:center;width:auto;padding:0 16px;background:transparent;color:var(--text);border-color:var(--border);white-space:nowrap}
.btn.sec:hover{border-color:var(--accent)}
.fn{font-size:13px;color:var(--label);max-width:100%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.fn.set{color:var(--text)}
.inp{width:100%;min-height:44px;margin:0 0 12px;padding:0 12px;border:1px solid var(--border);border-radius:8px;background:var(--bg);color:var(--text);font:inherit;font-size:14px}
.inp:focus{outline:none;border-color:var(--accent)}
.hint{font-size:13px;color:var(--label);margin:0 0 12px;line-height:1.4}
.st{font-size:13px;color:var(--label);min-height:1.2em;margin:14px 0 0}
.st.bad{color:var(--bad)}
a{color:var(--accent);text-decoration:none;font-size:13px}
.back{display:block;margin-top:18px}
[hidden]{display:none!important}
</style></head><body>
<div class="card">
<h3 data-i18n="firmware_update_ota">Firmware update (OTA)</h3>
<form id="f" method="POST" action="/update" enctype="multipart/form-data">
<div id="dz" class="drop">
<div class="drop-t" data-i18n="drop_the_bin_file_here">Drop the .bin file here</div>
<div class="drop-o" data-i18n="or">or</div>
<label class="btn sec"><input class="vh" type="file" name="firmware" accept=".bin"><span data-i18n="choose_file">Choose file</span></label>
<div id="fn" class="fn" data-i18n="no_file_selected">No file selected</div>
</div>
<button class="btn" type="submit" data-i18n="upload">Upload</button>
</form>
<div id="lg" hidden>
<p class="hint" data-i18n="log_in_with_the_panel_pin_or_password"></p>
<input id="lgp" class="inp" type="password" autocomplete="current-password" placeholder="PIN / password">
<button id="lgb" class="btn" type="button" data-i18n="log_in">Log in</button>
</div>
<p id="st" class="st"></p>
<a class="back" href="/" data-i18n-html="back_to_the_panel_2">&larr; Back to the panel</a>
</div>
<script>
var L=document.documentElement.lang,CAT=window.AOG_CAT||{},LA=CAT[L]||{},FB=CAT.en||{};
function TL(k,p){var r=LA[k];if(typeof r!=="string")r=FB[k];if(typeof r!=="string")r=k;return p?r.replace(/\{(\w+)\}/g,function(m,x){return(x in p)?String(p[x]):m;}):r;}
document.querySelectorAll("[data-i18n]").forEach(function(e){e.textContent=TL(e.getAttribute("data-i18n"));});
document.querySelectorAll("[data-i18n-html]").forEach(function(e){e.innerHTML=TL(e.getAttribute("data-i18n-html"));});
document.querySelectorAll("[data-i18n-attr]").forEach(function(e){var a=e.getAttribute("data-i18n-attr").split(":");e.setAttribute(a[0],TL(a[1]));});
document.title=TL("firmware_update_ota");
var st=document.getElementById("st"),frm=document.getElementById("f"),lg=document.getElementById("lg"),lgp=document.getElementById("lgp"),fn=document.getElementById("fn");
function showLogin(on){lg.hidden=!on;frm.hidden=on;st.className=on?"st bad":"st";if(on)lgp.focus();}
/* Drag and drop: drop a .bin file on the dashed area (a file dropped elsewhere must not make the browser open it). */
var dz=document.getElementById("dz");
["dragover","drop"].forEach(function(n){window.addEventListener(n,function(e){e.preventDefault();});});
["dragenter","dragover"].forEach(function(n){dz.addEventListener(n,function(){dz.classList.add("over");});});
["dragleave","drop"].forEach(function(n){dz.addEventListener(n,function(){dz.classList.remove("over");});});
dz.addEventListener("drop",function(e){
  var f=e.dataTransfer&&e.dataTransfer.files&&e.dataTransfer.files[0];
  if(!f)return;
  if(!/\.bin$/i.test(f.name)){st.textContent=TL("choose_a_firmware_file_bin");return;}
  try{var dt=new DataTransfer();dt.items.add(f);frm.elements.firmware.files=dt.files;}catch(x){st.textContent=TL("choose_a_firmware_file_bin");return;}
  frm.elements.firmware.onchange.call(frm.elements.firmware);
});
frm.elements.firmware.onchange=function(){var f=this.files[0];fn.textContent=f?f.name:TL("no_file_selected");fn.className=f?"fn set":"fn";st.textContent="";};
lgp.placeholder="PIN / "+TL("password");
function tok(){try{return sessionStorage.getItem("authTok")||"";}catch(e){return "";}}
/* Ask the module BEFORE sending the file whether this tab may upload; if not, show the login box (cb runs only when allowed). */
function check(cb){
  var h={};if(tok())h["X-Auth"]=tok();
  fetch("/update_check",{headers:h}).then(function(r){
    if(r.status===401){st.textContent=TL("login_required_2");showLogin(true);return;}
    showLogin(false);if(r.ok&&cb)cb();
  }).catch(function(){st.textContent=TL("connection_lost_the_module_may_be_restarting");});
}
function send(){
  var fl=frm.elements.firmware.files[0];
  if(!fl){st.textContent=TL("choose_a_firmware_file_bin");return;}
  var x=new XMLHttpRequest();x.open("POST","/update");
  if(tok())x.setRequestHeader("X-Auth",tok());
  x.upload.onprogress=function(e){if(e.lengthComputable)st.textContent=TL("uploading_p_pct",{p:Math.round(e.loaded*100/e.total)});};
  x.onload=function(){
    if(x.status===401){st.textContent=TL("login_required_2");showLogin(true);return;}
    st.textContent=x.status===200?TL("update_ok_the_module_is_restarting"):TL("update_failed_details_in_the_panel");};
  x.onerror=function(){st.textContent=TL("connection_lost_the_module_may_be_restarting");};
  var fd=new FormData();fd.append("firmware",fl);x.send(fd);
}
frm.onsubmit=function(ev){ev.preventDefault();if(!frm.elements.firmware.files[0]){st.textContent=TL("choose_a_firmware_file_bin");return;}check(send);};
check();   /* on load: show the login box at once when it is needed */
/* No token yet (the panel was not opened in this tab): the module answered 401 - log in here and repeat the upload. */
function login(){
  var b=document.getElementById("lgb");b.disabled=true;
  fetch("/auth_login",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"secret="+encodeURIComponent(lgp.value)})
   .then(function(r){return r.text().then(function(t){return[r.ok,t.trim()];});})
   .then(function(a){
     b.disabled=false;
     if(a[0]){try{sessionStorage.setItem("authTok",a[1]);}catch(e){}lgp.value="";showLogin(false);st.textContent="";if(frm.elements.firmware.files[0])send();}
     else{st.textContent=a[1];lgp.select();}})
   .catch(function(){b.disabled=false;st.textContent=TL("connection_lost_the_module_may_be_restarting");});
}
document.getElementById("lgb").onclick=login;
lgp.onkeydown=function(e){if(e.key==="Enter")login();};
</script>
</body></html>)HTML";
server.send_P(200, "text/html", PAGE);
  }

  // Asked by the update page BEFORE it sends the file: 200 = this browser may upload, 401 = log in first (no megabytes sent in vain).
  static void handleUpdateCheck() {
    if (otaAuthOk()) { server.send(200, "text/plain", "OK"); return; }
    if (strlen(OTA_PASSWORD) > 0) server.requestAuthentication();
    else server.send(401, "text/plain", MSG("Login required (panel PIN or password)"));
  }

  static void handleUpdateUpload() {
    // WebServer calls this handler BEFORE the final handler, so authorization must be checked here -
    // otherwise an unauthorized file would be written to flash before handleUpdateResult() refuses.
    static bool authorized = false;
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      authorized = otaAuthOk();
      if (authorized && SteerCommandReceiver::isAutoSteerActive()) {
        authorized = false;
        OtaStatus::start(OtaStatus::WEB, 0, millis());
        OtaStatus::fail(MSG("steering engaged - disengage it before the update"), millis());
        LogBuffer::println("[OTA] Upload rejected: steering is engaged.");
        return;
      }
      if (!authorized) {
        OtaStatus::start(OtaStatus::WEB, 0, millis());
        OtaStatus::fail(MSG("not authorised (OTA password)"), millis());
        LogBuffer::println("[OTA] Upload rejected: not authorised.");
        return;
      }
      StatusLed::setOtaActive(true);
      OtaStatus::start(OtaStatus::WEB, 0, millis());
      LogBuffer::printfIf(DEBUG_WEB, "[OTA] Update started: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        StatusLed::setOtaActive(false);
        OtaStatus::fail(Update.errorString(), millis());
        LogBuffer::printf("[OTA] Start error: %s\n", Update.errorString());
      }
    } else if (!authorized) {
      return;   // the rest of the upload without authorization: we store nothing
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      OtaStatus::progress((uint32_t)upload.totalSize);
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        StatusLed::setOtaActive(false);
        OtaStatus::fail(Update.errorString(), millis());
        LogBuffer::printf("[OTA] Write error: %s\n", Update.errorString());
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      StatusLed::setOtaActive(false);
      // The upload takes tens of seconds: steering may have been engaged in the meantime. The restart would drop the assist while driving,
      // so the finished image is discarded instead (the module keeps running; the update can be repeated).
      if (SteerCommandReceiver::isAutoSteerActive()) {
        Update.abort();
        OtaStatus::fail(MSG("steering engaged - disengage it before the update"), millis());
        LogBuffer::println("[OTA] Update discarded: steering was engaged during the upload.");
        return;
      }
      if (Update.end(true)) {
        OtaStatus::success((uint32_t)upload.totalSize, millis());   // the module is about to restart (handleUpdateResult)
        LogBuffer::printfIf(DEBUG_WEB, "[OTA] %u bytes written OK, restarting...\n", (unsigned)upload.totalSize);
      } else {
        OtaStatus::fail(Update.errorString(), millis());
        LogBuffer::printf("[OTA] Final error: %s\n", Update.errorString());
      }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      // The browser aborted the upload: without this the OTA LED stayed on and the state was not recorded.
      Update.end();
      StatusLed::setOtaActive(false);
      OtaStatus::fail(MSG("upload aborted"), millis());
      LogBuffer::println("[OTA] Upload aborted by the browser.");
    }
  }

  static void handleUpdateResult() {
    if (!otaAuthOk()) {
      if (strlen(OTA_PASSWORD) > 0) server.requestAuthentication();
      else server.send(401, "text/plain", MSG("Login required (panel PIN or password)"));
      return;
    }
    server.sendHeader("Connection", "close");
    // Restart ONLY after a successful write. After an error the module keeps running, and the panel (Diagnostics -> Web/OTA) shows
    // the reason - after a restart that information would vanish.
    if (OtaStatus::getState() != OtaStatus::OK || Update.hasError()) {
      server.send(500, "text/plain", "Update error! Details: panel, Diagnostics, Web/OTA.");
      return;
    }
    server.send(200, "text/plain", "OK, restarting the device...");
    delay(500);
    ESP.restart();
  }

public:
  // A wrapper for a state-changing handler: first the token check (requireAuth), only then the actual handler.
  #define SECURED(h) ([]() { if (!requireAuth()) return; h(); })
  static void begin() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/lang.js", HTTP_GET, handleLang);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/log", HTTP_GET, handleLog);
    server.on("/settings_save_network", HTTP_POST, SECURED(handleSettingsSaveNetwork));
    server.on("/settings_save_ntrip", HTTP_POST, SECURED(handleSettingsSaveNtrip));
    server.on("/settings_save_debug", HTTP_POST, SECURED(handleSettingsSaveDebug));
    server.on("/settings_save_features", HTTP_POST, SECURED(handleSettingsSaveFeatures));
    server.on("/settings_save_ethernet", HTTP_POST, SECURED(handleSettingsSaveEthernet));
    server.on("/settings_save_thresholds", HTTP_POST, SECURED(handleSettingsSaveThresholds));
    server.on("/settings_save_steering_range", HTTP_POST, SECURED(handleSettingsSaveSteeringRange));
    server.on("/settings_save_current", HTTP_POST, SECURED(handleSettingsSaveCurrent));
    server.on("/settings_save_motor", HTTP_POST, SECURED(handleSettingsSaveMotor));
    server.on("/runaway_reset", HTTP_POST, SECURED(handleRunawayReset));
    server.on("/motor_test", HTTP_POST, SECURED(handleMotorTest));
    server.on("/gps_sats", HTTP_GET, handleGpsSats);
    server.on("/current_zero", HTTP_POST, SECURED(handleCurrentZero));
    server.on("/current_reset", HTTP_POST, SECURED(handleCurrentReset));
    server.on("/settings_save_imu_rotation", HTTP_POST, SECURED(handleSettingsSaveImuRotation));
    server.on("/imu_mount_solve", HTTP_POST, SECURED(handleImuMountSolve));
    server.on("/settings_save_imu_mount3d", HTTP_POST, SECURED(handleSettingsSaveImuMount3d));
    server.on("/settings_save_agio_sender", HTTP_POST, SECURED(handleSettingsSaveAgioSender));
    server.on("/settings_save_agio_engage", HTTP_POST, SECURED(handleSettingsSaveAgioEngage));
    server.on("/imu_reset", HTTP_POST, SECURED(handleImuReset));
    server.on("/history", HTTP_GET, handleHistory);
    server.on("/events", HTTP_GET, handleEvents);
    server.on("/restart", HTTP_POST, SECURED(handleRestart));
    server.on("/factory_reset", HTTP_POST, SECURED(handleFactoryReset));
    server.on("/hydlift_manual_on", HTTP_POST, SECURED(handleHydLiftManualOn));
    server.on("/hydlift_manual_off", HTTP_POST, SECURED(handleHydLiftManualOff));
    server.on("/hydlift_auto", HTTP_POST, SECURED(handleHydLiftAuto));
    server.on("/steering_cal_left", HTTP_POST, SECURED(handleSteeringCalLeft));
    server.on("/steering_cal_right", HTTP_POST, SECURED(handleSteeringCalRight));
    server.on("/auth_login", HTTP_POST, handleAuthLogin);
    server.on("/auth_setup", HTTP_POST, handleAuthSetup);
    server.on("/update", HTTP_GET, handleUpdatePage);
    server.on("/update_check", HTTP_GET, handleUpdateCheck);
    server.on("/update", HTTP_POST, handleUpdateResult, handleUpdateUpload);
    static const char* collect[] = {"X-Auth", "Origin", "Host"};
    server.collectHeaders(collect, 3);   // panel login token + headers for the cross-site check
    server.begin();
    LogBuffer::printlnIf(DEBUG_WEB, "[WEB] Server started on port 80 (/ = panel, /update = OTA)");
  }

  // Call every loop() pass.
  static void update() {
    server.handleClient();
  }
};

WebServer WebServerModule::server(80);
