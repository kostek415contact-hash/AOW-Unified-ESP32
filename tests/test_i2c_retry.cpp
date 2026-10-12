// Host test of sensors/i2c_retry.h (plain C++).
//     g++ -std=c++17 -Wall -Wextra tests/test_i2c_retry.cpp -o /tmp/ti2c && /tmp/ti2c
#include <cstdio>
#include "../sensors/i2c_retry.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  FAIL: %s\n", n); } } while (0)
int main() {
  { I2cRetry r;
    CHECK(r.due(0) && !r.lost(), "fresh: due at once, not lost");
    r.fail(100); r.fail(101);
    CHECK(r.due(101), "two failures: still retried at once");
    r.fail(102);
    CHECK(!r.due(200) && r.due(352), "3 failures: wait 250 ms");
    r.fail(400); r.fail(700); r.fail(1000);
    CHECK(r.failures() == 6 && !r.due(1500) && r.due(2000), "6 failures: wait 1 s");
    bool lostNow = false;
    lostNow |= r.fail(2000); lostNow |= r.fail(3000); lostNow |= r.fail(4000);
    CHECK(!lostNow && !r.lost(), "9 failures: not lost yet");
    CHECK(r.fail(5000) && r.lost(), "10th failure: lost, reported once");
    CHECK(!r.fail(6000), "11th failure: loss not reported again");
    r.recovered();
    CHECK(!r.lost() && r.due(6001) && r.failures() == 0, "recovered: clean state"); }
  { I2cRetry r; r.fail(10); r.fail(20); r.ok(); r.fail(30); r.fail(40);
    CHECK(r.failures() == 2 && r.due(40), "ok() resets the streak"); }
  { I2cRetry r; r.markLost(1000);
    CHECK(r.lost() && !r.probeDue(1500) && !r.probeDue(2999) && r.probeDue(3000), "lost: probe every 2 s");
    CHECK(!r.probeDue(3001) && r.probeDue(5000), "lost: next probe 2 s later"); }
  { I2cRetry r; 
    CHECK(r.probeDue(0), "first probe is immediate"); }
  { I2cRetry r; for (int i = 0; i < 4; i++) r.fail(4294967000u);
    CHECK(!r.due(4294967100u) && r.due(100u), "millis() wrap-around: 250 ms wait honoured"); }
  { I2cRetry r; for (int i = 0; i < 400; i++) r.fail(i);
    CHECK(r.failures() == 255 && r.lost(), "counter saturates, no wrap to zero"); }
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
