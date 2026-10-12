#pragma once
// ======================================================================
// web/chunked_out.h
// EN: Assembles long responses (e.g. /status JSON) in fixed-size chunks so no large contiguous heap block is
//     needed.
// PL: Składa długie odpowiedzi (np. JSON z /status) w kawałkach stałego rozmiaru, żeby nie wymagać dużego
//     ciągłego bloku sterty.
// Tests: tests/test_chunked_out.cpp (host, plain C++).
// Used by: web/web_server.h
// ======================================================================
#include <stddef.h>
#include <string.h>

// A buffer that assembles a long response in chunks of CAP bytes and hands each chunk to the function `sink(const char*, size_t)`
// (in firmware: server.sendContent). Thanks to this, /status (several KB of JSON) does not need one contiguous heap block
// (formerly reserve(12288) on every request); it takes only CAP bytes on the stack.
// Pure logic, tests: tests/test_chunked_out.cpp.
template <size_t CAP, typename Sink>
class ChunkedOut {
public:
    explicit ChunkedOut(Sink s) : sink(s) {}
    void write(const char* p, size_t n) {
        while (n > 0) {
            size_t room = CAP - used;
            size_t take = n < room ? n : room;
            memcpy(buf + used, p, take);
            used += take; p += take; n -= take; total_ += take;
            if (used == CAP) flush();
        }
    }
    void write(const char* z) { if (z) write(z, strlen(z)); }
    void flush() { if (used > 0) { sink(buf, used); chunks_++; used = 0; } }
    size_t total() const { return total_; }
    size_t chunks() const { return chunks_; }
private:
    char buf[CAP];
    size_t used = 0, total_ = 0, chunks_ = 0;
    Sink sink;
};
