#pragma once
// ======================================================================
// connectivity/hw_message.h
// EN: PGN 221 "hardware message" - a short text the module shows on the AgOpenWeb screen (e.g. a lost sensor), plus the
//     edge gate that keeps such a message from being repeated every pass. Pure C++, host-tested.
// PL: PGN 221 „komunikat sprzętowy” - krótki tekst pokazywany na ekranie AgOpenWeb (np. zanik czujnika) oraz bramka
//     zbocza, żeby taki komunikat nie powtarzał się w każdym obiegu. Czysty C++, testowany na komputerze.
// Tests: tests/test_hw_message.cpp (host, plain C++).
// Used by: connectivity/network_manager.h
// ======================================================================
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace HwMessage {

static const uint8_t PGN = 221;
static const size_t MAX_TEXT = 60;

// Frame as AgOpenWeb reads it (AutoSteerService.TryParseHardwareMessage): 0x80 0x81 src 221 len secs colour text... crc,
// len = text bytes + 2, colour 0 = warning. Returns the frame size, 0 when the buffer is too small or the text is empty.
inline size_t build(uint8_t* out, size_t cap, uint8_t src, const char* text, uint8_t seconds, bool warning) {
    if (!text || !*text) return 0;
    size_t n = strlen(text);
    if (n > MAX_TEXT) n = MAX_TEXT;
    const size_t total = 5 + 2 + n + 1;
    if (cap < total) return 0;
    out[0] = 0x80; out[1] = 0x81; out[2] = src; out[3] = PGN; out[4] = (uint8_t)(n + 2);
    out[5] = seconds; out[6] = warning ? 0 : 1;
    memcpy(out + 7, text, n);
    uint8_t crc = 0;
    for (size_t i = 2; i < total - 1; i++) crc = (uint8_t)(crc + out[i]);
    out[total - 1] = crc;
    return total;
}

// Why the module restarted, as far as the operator needs to know: only the abnormal reasons produce a message
// (a power-on, an OTA update or a normal software restart say nothing).
enum BootKind : uint8_t { BK_NORMAL = 0, BK_PANIC, BK_WATCHDOG, BK_BROWNOUT };
inline const char* bootAlertText(BootKind k) {
    switch (k) {
        case BK_PANIC:    return "Module restarted after a crash";
        case BK_WATCHDOG: return "Module restarted by the watchdog";
        case BK_BROWNOUT: return "Module restarted: power dip - check the supply";
        default:          return nullptr;
    }
}

// Fires once when the condition becomes true, then again every repeatMs while it stays true; re-arms when it clears.
class AlertGate {
public:
    bool update(bool active, uint32_t nowMs, uint32_t repeatMs) {
        if (!active) { armed_ = true; return false; }
        if (armed_ || (nowMs - lastMs_) >= repeatMs) { armed_ = false; lastMs_ = nowMs; return true; }
        return false;
    }
private:
    bool armed_ = true;
    uint32_t lastMs_ = 0;
};

// "Was fine once, is broken now": avoids nagging about hardware that was never connected (a bench setup).
class LostAfterOk {
public:
    bool update(bool ok) { if (ok) everOk_ = true; return everOk_ && !ok; }
private:
    bool everOk_ = false;
};

}  // namespace HwMessage
