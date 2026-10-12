// Host test of connectivity/slot_schedule.h (plain C++).
//     g++ -std=c++17 -Wall -Wextra tests/test_slot_schedule.cpp -o /tmp/tss && /tmp/tss
#include <cstdio>
#include <vector>
#include "../connectivity/slot_schedule.h"
static int fails = 0, total = 0;
#define CHECK(c, n) do { total++; if (!(c)) { fails++; printf("  BLAD: %s\n", n); } } while (0)
using namespace SlotSchedule;
static std::vector<int> slotsOf(Kind k) { std::vector<int> v; for (int s = 0; s < CYCLE; s++) if (kindOf((uint8_t)s) == k) v.push_back(s); return v; }
// every gap between consecutive slots of the same kind (cyclic) must equal 'gap'
static bool evenSpacing(const std::vector<int>& v, int gap) {
  if (v.empty()) return false;
  for (size_t i = 0; i < v.size(); i++) { int next = v[(i + 1) % v.size()]; int d = (next - v[i] + CYCLE) % CYCLE; if (v.size() == 1) d = CYCLE; if (d != gap) return false; }
  return true;
}
int main() {
  CHECK(slotsOf(K_STEER).size() == 5 && evenSpacing(slotsOf(K_STEER), 8), "AutoSteer: 5 slotow co 8 (40 ms)");
  CHECK(slotsOf(K_MACHINE).size() == 5 && evenSpacing(slotsOf(K_MACHINE), 8), "Machine: 5 slotow co 8 (40 ms)");
  CHECK(slotsOf(K_SENSOR).size() == 5 && evenSpacing(slotsOf(K_SENSOR), 8), "Sensor/kickout: 5 slotow co 8 (40 ms)");
  CHECK(slotsOf(K_PANDA).size() == 2 && evenSpacing(slotsOf(K_PANDA), 20), "PANDA: 2 sloty co 20 (100 ms)");
  CHECK(slotsOf(K_IMU).size() == 2 && evenSpacing(slotsOf(K_IMU), 20), "IMU: 2 sloty co 20 (100 ms)");
  CHECK(slotsOf(K_GPS_STATUS).size() == 2 && evenSpacing(slotsOf(K_GPS_STATUS), 20), "Status GPS: 2 sloty co 20 (100 ms) - slot 24 nie jest juz przeslonięty przez AutoSteer");
  int used = 0; for (int s = 0; s < CYCLE; s++) if (kindOf((uint8_t)s) != K_NONE) used++;
  CHECK(used == 5 + 5 + 5 + 2 + 2 + 2, "razem 21 zajetych slotow, zadne dwa rodzaje nie dziela slotu");
  CHECK(kindOf(4) == K_NONE && kindOf(24) == K_STEER && kindOf(5) == K_GPS_STATUS, "slot 4 wolny, 24 = AutoSteer, 5 = status GPS");
  CHECK(slotsOf(K_STEER).front() == 0 && slotsOf(K_MACHINE).front() == 3, "AutoSteer od 0, Machine przesuniety o 3");
  printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
  return fails ? 1 : 0;
}
