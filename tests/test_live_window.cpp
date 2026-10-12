// Host test of sensors/live_window.h (plain C++).
//     g++ -std=c++17 -Wall -Wextra tests/test_live_window.cpp -o /tmp/tlw && /tmp/tlw
#include <cstdio>
#include <cmath>
#include <limits>
#include "../sensors/live_window.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
int main() {
  CHECK(LiveWindow::forGps(100.0f) == 500, "10 Hz: 5 okresow = 500 ms");
  CHECK(LiveWindow::forGps(50.0f) == 500, "20 Hz: 250 ms podniesione do minimum 500 ms");
  CHECK(LiveWindow::forGps(200.0f) == 1000, "5 Hz: 1000 ms");
  CHECK(LiveWindow::forGps(1000.0f) == 2000, "1 Hz: 5000 ms obciete do 2000 ms");
  CHECK(LiveWindow::forGps(0.0f) == 1000 && LiveWindow::forGps(-5.0f) == 1000, "nie zmierzono: 1000 ms");
  CHECK(LiveWindow::forGps(std::numeric_limits<float>::quiet_NaN()) == 1000, "NaN: 1000 ms");
  CHECK(LiveWindow::forGps(std::numeric_limits<float>::infinity()) == 1000, "Inf: 1000 ms");
  CHECK(LiveWindow::forGps(100.4f) == 502, "okno rosnie ciagle, bez schodkow");
  CHECK(LiveWindow::IMU_MS == 500, "IMU: 500 ms");
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
