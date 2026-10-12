#pragma once
// ======================================================================
// sensors/gps_config_fsm.h
// EN: Non-blocking GPS receiver configuration state machine; writes and saves settings only when the observed
//     NMEA stream differs from the target.
// PL: Nieblokująca maszyna stanów konfiguracji odbiornika GPS; zapisuje ustawienia tylko, gdy obserwowany
//     strumień NMEA różni się od zadanego.
// Tests: tests/test_gps_config_fsm.cpp (host, plain C++).
// Used by: sensors/gps_universal.h
// ======================================================================
// Automatic GPS receiver configuration as a (non-blocking) STATE MACHINE. Pure C++ (no Arduino), host-tested
// (tests/test_gps_config_fsm.cpp). GpsHandler calls tick() every loop pass and carries out the returned action (sending a
// command), and feeds received lines to onLine() / onNmea().
//
// WHY: the old version (autoConfigure) waited in the main loop for up to ~3 s for receiver replies (listening windows),
// and after a late GPS start it did so in the middle of operation - the loop stalled, and with it the IMU, $PANDA and steering.
// Now every step takes microseconds, and waiting for a reply is just a time comparison.
//
// WRITING TO THE RECEIVER'S MEMORY (PQTMSAVEPAR): formerly on EVERY start, i.e. needless wear on the receiver's flash. Now
// we first OBSERVE the NMEA stream: if the receiver already sends what we want (GGA at the target rate plus
// VTG, RMC, GST, GSA, GSV), we write and save nothing. When the stream differs (first start, replaced or
// reset receiver, different rate), we write the settings and save them ONCE.
//
// Flow: OBSERVE -> VERSION (PQTMVERNO, then PQTMVER) -> [Quectel: WRITE+SAVE or skip] -> PROTO (diagnostic
// query) -> DONE; for a non-Quectel receiver a one-time UBX-CFG-RATE (u-blox) instead.
#include <cstdint>
#include <cstring>
#include <cstdio>

class GpsConfigFsm {
public:
    enum Phase : uint8_t { IDLE, OBSERVE, VER1, VER2, WRITE, PROTO, UBX, DONE };
    enum ActionType : uint8_t { A_NONE, A_SEND, A_UBX };
    // Configuration result (for the panel): 0 = not started / in progress, 1 = new settings written, 2 = the receiver already had the correct
    // settings (nothing written), 3 = not Quectel (UBX-CFG-RATE sent)
    enum Result : uint8_t { R_NONE = 0, R_WRITTEN = 1, R_MATCHED = 2, R_OTHER = 3 };
    // Events for the log (GpsHandler turns them into messages)
    enum Event : uint8_t { EV_NONE, EV_OBSERVED_MATCH, EV_OBSERVED_DIFFERS, EV_QUECTEL, EV_NOT_QUECTEL, EV_WROTE, EV_PROTO_REPLY, EV_PROTO_NONE, EV_UBX, EV_DONE };
    struct Action { ActionType type; char cmd[48]; };

    // sentence type masks (talker ignored)
    static constexpr uint8_t T_GGA = 1, T_VTG = 2, T_RMC = 4, T_GST = 8, T_GSA = 16, T_GSV = 32;
    static constexpr uint8_t T_REQUIRED = T_VTG | T_RMC | T_GST | T_GSA | T_GSV;

    static constexpr uint32_t OBSERVE_MIN_MS = 1500;   // we do not judge sooner
    static constexpr uint32_t OBSERVE_MAX_MS = 3500;   // we do not wait longer (e.g. the receiver is silent)
    static constexpr uint32_t REPLY_MS       = 700;    // time for the PQTMVERNO / PQTMVER reply
    static constexpr uint32_t PROTO_MS       = 1000;
    static constexpr uint32_t SEND_GAP_MS    = 50;     // gap between write commands (like the old delay(50))
    static constexpr uint32_t UBX_GAP_MS     = 100;

    void start(uint32_t nowMs, uint32_t targetHz) {
        *this = GpsConfigFsm();
        hz_ = targetHz > 0 ? targetHz : 1;
        intervalMs_ = 1000UL / hz_;
        phase_ = OBSERVE; phaseStart_ = nowMs;
    }

    Phase phase() const { return phase_; }
    bool running() const { return phase_ != IDLE && phase_ != DONE; }
    bool done() const { return phase_ == DONE; }
    Result result() const { return result_; }
    bool quectel() const { return quectel_; }
    const char* verLine() const { return verLine_; }
    const char* protoLine() const { return protoLine_; }
    bool streamMatches() const { return match_; }
    uint32_t observedGga() const { return ggaCount_; }
    float observedRateHz() const {
        if (ggaCount_ < 2 || lastGga_ == firstGga_) return 0.0f;
        return (float)(ggaCount_ - 1) * 1000.0f / (float)(uint32_t)(lastGga_ - firstGga_);
    }

    // A NMEA sentence with a valid checksum (called for each, but it counts only in the OBSERVE phase).
    void onNmea(uint8_t typeBit, uint32_t nowMs) {
        if (phase_ != OBSERVE) return;
        seen_ |= typeBit;
        if (typeBit == T_GGA) {
            if (ggaCount_ == 0) firstGga_ = nowMs;
            lastGga_ = nowMs; ggaCount_++;
        }
    }

    // A line from the receiver starting with "$PQTM" (replies to commands).
    void onLine(const char* line) {
        if (phase_ == VER1 || phase_ == VER2) {
            if (std::strstr(line, "PQTMVER")) { quectel_ = true; copy(verLine_, sizeof(verLine_), line); gotReply_ = true; }
        } else if (phase_ == PROTO) {
            if (std::strstr(line, "PQTMCFGPROT")) { copy(protoLine_, sizeof(protoLine_), line); gotReply_ = true; }
        }
    }

    // Call every loop pass. Returns the action to carry out (A_NONE = nothing).
    Action tick(uint32_t nowMs) {
        Action a{A_NONE, {0}};
        const uint32_t el = nowMs - phaseStart_;
        switch (phase_) {
        case IDLE: case DONE: break;
        case OBSERVE:
            if (el >= OBSERVE_MAX_MS || (el >= OBSERVE_MIN_MS && ggaCount_ >= 4)) {
                match_ = streamOk();
                push(match_ ? EV_OBSERVED_MATCH : EV_OBSERVED_DIFFERS);
                enter(VER1, nowMs);
                send(a, "PQTMVERNO");
            }
            break;
        case VER1:
            if (gotReply_) { afterVersion(nowMs, a); }
            else if (el >= REPLY_MS) { enter(VER2, nowMs); send(a, "PQTMVER"); }
            break;
        case VER2:
            if (gotReply_ || el >= REPLY_MS) afterVersion(nowMs, a);
            break;
        case WRITE:
            if (el >= SEND_GAP_MS && writeIdx_ < writeCount()) {
                nextWrite(a);
                phaseStart_ = nowMs; writeIdx_++;
            } else if (writeIdx_ >= writeCount() && el >= SEND_GAP_MS) {
                result_ = R_WRITTEN; push(EV_WROTE);
                enterProto(nowMs, a);
            }
            break;
        case PROTO:
            if (gotReply_) { push(EV_PROTO_REPLY); finish(); }
            else if (el >= PROTO_MS) { push(EV_PROTO_NONE); finish(); }
            break;
        case UBX:
            if (el >= UBX_GAP_MS) { push(EV_UBX); result_ = R_OTHER; finish(); }
            break;
        }
        return a;
    }

    uint8_t ubxIntervalMsLo() const { return (uint8_t)(intervalMs_ & 0xFF); }
    uint16_t intervalMs() const { return (uint16_t)intervalMs_; }

    // Takes the next log event (EV_NONE = none)
    Event popEvent() {
        if (evRead_ == evWrite_) return EV_NONE;
        Event e = ev_[evRead_]; evRead_ = (evRead_ + 1) % EV_MAX; return e;
    }

private:
    static constexpr uint8_t EV_MAX = 12;
    Phase phase_ = IDLE;
    uint32_t phaseStart_ = 0, hz_ = 1, intervalMs_ = 1000;
    uint8_t seen_ = 0;
    uint32_t ggaCount_ = 0, firstGga_ = 0, lastGga_ = 0;
    bool match_ = false, quectel_ = false, gotReply_ = false;
    Result result_ = R_NONE;
    uint8_t writeIdx_ = 0;
    char verLine_[64] = {0}, protoLine_[128] = {0};
    Event ev_[EV_MAX] = {EV_NONE};
    uint8_t evRead_ = 0, evWrite_ = 0;

    static void copy(char* dst, size_t n, const char* s) { std::strncpy(dst, s, n - 1); dst[n - 1] = '\0'; }
    void push(Event e) { ev_[evWrite_] = e; evWrite_ = (evWrite_ + 1) % EV_MAX; }
    void enter(Phase p, uint32_t now) { phase_ = p; phaseStart_ = now; gotReply_ = false; }
    void finish() { phase_ = DONE; push(EV_DONE); }
    static void send(Action& a, const char* c) { a.type = A_SEND; copy(a.cmd, sizeof(a.cmd), c); }

    bool streamOk() const {
        if (ggaCount_ < 4) return false;                        // too few sentences to judge the rate (e.g. a 1 Hz receiver)
        if ((seen_ & T_REQUIRED) != T_REQUIRED) return false;   // one of the required sentences is missing
        const float r = observedRateHz(), t = (float)hz_;
        return r >= t * 0.8f && r <= t * 1.2f;
    }

    void afterVersion(uint32_t now, Action& a) {
        if (quectel_) {
            push(EV_QUECTEL);
            if (match_) { result_ = R_MATCHED; enterProto(now, a); }
            else { enter(WRITE, now); writeIdx_ = 0; phaseStart_ = now - SEND_GAP_MS; }   // the first command immediately
        } else {
            push(EV_NOT_QUECTEL);
            enter(UBX, now);
            a.type = A_UBX;
        }
    }
    void enterProto(uint32_t now, Action& a) { enter(PROTO, now); send(a, "PQTMCFGPROT,R,1,3"); }   // UART3

    // GGA/VTG/GST/RMC every epoch, GSV and GSA once per second (divider = rate), FIXRATE, and finally a save to the receiver's memory.
    static constexpr uint8_t FIXED_WRITES = 7;   // 4 x rate=1, GSV, GSA, FIXRATE
    uint8_t writeCount() const { return FIXED_WRITES + 1; }   // + PQTMSAVEPAR
    void nextWrite(Action& a) const {
        char c[48];
        switch (writeIdx_) {
        case 0: std::snprintf(c, sizeof(c), "PQTMCFGMSGRATE,W,GGA,1"); break;
        case 1: std::snprintf(c, sizeof(c), "PQTMCFGMSGRATE,W,VTG,1"); break;
        case 2: std::snprintf(c, sizeof(c), "PQTMCFGMSGRATE,W,GST,1"); break;
        case 3: std::snprintf(c, sizeof(c), "PQTMCFGMSGRATE,W,RMC,1"); break;
        case 4: std::snprintf(c, sizeof(c), "PQTMCFGMSGRATE,W,GSV,%lu", (unsigned long)hz_); break;
        case 5: std::snprintf(c, sizeof(c), "PQTMCFGMSGRATE,W,GSA,%lu", (unsigned long)hz_); break;
        case 6: std::snprintf(c, sizeof(c), "PQTMCFGFIXRATE,W,%lu", (unsigned long)intervalMs_); break;
        default: std::snprintf(c, sizeof(c), "PQTMSAVEPAR"); break;
        }
        send(a, c);
    }
};
