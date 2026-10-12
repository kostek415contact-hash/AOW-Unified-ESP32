#pragma once
// EN: Host stand-in for heap_caps_malloc (falls back to malloc).  PL: Zamiennik heap_caps_malloc na komputerze.
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 0
inline void* heap_caps_malloc(size_t n, int) { return malloc(n); }
