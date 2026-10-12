#pragma once
// ======================================================================
// diagnostics/history_sampler.h
// EN: Once-per-second sampler that fills the chart history; series semantics match the panel's SER definitions.
// PL: Próbnik raz na sekundę zasilający historię wykresów; znaczenie serii zgodne z definicjami SER w panelu.
// Used by: connectivity/network_manager.h
// Depends on: history.h
// ======================================================================
// Collects the chart series values ONCE PER SECOND and writes them to History. Called from the main loop
// (network_manager.h). The semantics of each series are the same as in SER in web/ui/js_charts.h (from which the
// browser computes charts from /status) - so the history from the module and the live data
// from the browser are continuous.
// Requires (included earlier in network_manager.h): gps_universal.h, imu_sensor.h,
// steering_angle_sensor.h, current_sensor.h, pgn_receivers.h, ntrip_client.h, wifi_handler.h, ethernet_handler.h
// and the variables coreZeroLoadPercent / lastLoopLoadPercent.
#include "history.h"

inline void nmHistoryTick() {
    unsigned long now = millis();
    if (!History::due(now)) return;

    // Data rates (KB/s) from the growth of the byte counters.
    static uint32_t prevNtrip = 0, prevGps = 0;
    static unsigned long prevMs = 0;
    float dt = (prevMs != 0) ? (float)(now - prevMs) / 1000.0f : 0.0f;
    uint32_t ntripBytes = NtripClient::getTotalBytesRelayed();
    uint32_t gpsBytes = GpsHandler::getDiagBytesTotal();
    float ntripKBs = NAN, gpsKBs = NAN;
    if (dt > 0.2f) {
        if (ntripBytes >= prevNtrip) ntripKBs = (float)(ntripBytes - prevNtrip) / 1024.0f / dt;
        if (gpsBytes >= prevGps) gpsKBs = (float)(gpsBytes - prevGps) / 1024.0f / dt;
    }
    prevNtrip = ntripBytes; prevGps = gpsBytes; prevMs = now;

    float v[HistoryFormat::NS];
    for (int i = 0; i < HistoryFormat::NS; i++) v[i] = NAN;

    float hAcc = GpsHandler::getHAcc();
    v[0] = (hAcc >= 0.0f) ? hAcc * 100.0f : NAN;                       // hAccCm
    int q = GpsHandler::getFixQuality();                               // fixLvl: 0 none,1 GPS,2 DGPS,3 FLOAT,4 FIX
    v[1] = (q == 0) ? 0.0f : (q == 1) ? 1.0f : (q == 2) ? 2.0f : (q == 5) ? 3.0f : (q == 4) ? 4.0f : NAN;
    v[2] = (float)GpsHandler::getSatelliteCount();                     // sats
    v[3] = GpsHandler::getSpeedKnots() * 1.852f;                       // speed [km/h]
    v[4] = ImuSensor::getRollDeg();                                    // roll
    v[5] = ImuSensor::getPitchDeg();                                   // pitch
    v[6] = SteeringAngleSensor::isConnected() ? SteeringAngleSensor::getAngleDeg() : NAN;   // angle
    v[7] = SteerCommandReceiver::isAutoSteerActive()
    ? SteerCommandReceiver::getTargetAngleDeg() : NAN;      // target
    bool ethOn = EthernetHandler::isConnected();
    bool apMode = WifiHandler::isAPMode();
    v[8] = (!ethOn && !apMode && WiFi.status() == WL_CONNECTED) ? (float)WiFi.RSSI() : NAN;   // rssi
    v[9] = ntripKBs;                                                   // ntripKBs
    v[10] = gpsKBs;                                                    // gpsKBs
    v[11] = lastLoopLoadPercent;                                       // cpu1 (main loop)
    v[12] = coreZeroLoadPercent;                                       // cpu0 (WWW/OTA)
    v[13] = (float)ESP.getFreeHeap() / 1024.0f;                        // heap [KB]
    v[14] = (float)ESP.getMinFreeHeap() / 1024.0f;                     // minheap [KB]
    v[15] = (float)ImuSensor::getPacketsPerSec();                      // imuPps
    v[16] = (float)ImuSensor::getBytesPerSec();                        // imuBps
    // curA: the largest motor |current| from the last ~1 s [A]. Only when the sensor works (OK or overcurrent latch) -
    // with the sensor disabled, faulty or without data there is no trustworthy value (NAN = gap in the chart).
    uint8_t cs = CurrentSensor::stateCode();
    v[17] = (cs == 2 || cs == 3) ? CurrentSensor::getRecentPeakA() : NAN;                // curA
    // ageDgps: age of the RTK corrections from GGA [s]. NAN (gap in the chart) when the receiver does not report it: empty field, plain GPS
    // or GGA older than 3 s (see GpsAge / GpsHandler::getAgeDgpsOrNone). MAX aggregation - a dropout of the RTCM stream does not disappear.
    float ageDgps = GpsHandler::getAgeDgpsOrNone();
    v[18] = (ageDgps >= 0.0f) ? ageDgps : NAN;                                           // ageDgps

    History::push(now, v);
}
