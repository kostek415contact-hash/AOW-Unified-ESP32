// Testy jednostkowe bufora historii wykresow (bez Arduino). Uruchomienie z katalogu glownego projektu:
//     g++ -std=c++17 -Wall -Wextra tests/test_history.cpp -o /tmp/test_history && /tmp/test_history
// Sprawdza kodowanie, zawijanie bufora, agregacje (MIN/MAX/srednia), luki, zachowanie krotkich zdarzen
// przy agregacji oraz format naglowka odpowiedzi /history.
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <cstdlib>
#include "../diagnostics/history_store.h"
using namespace HistoryFormat;
static int fails = 0, total = 0;
#define CHECK(cond, name) do { total++; if (!(cond)) { fails++; printf("  BLAD: %s\n", name); } } while (0)

static void makeRow(int16_t* row, int i) {
    for (int s = 0; s < NS; s++) row[s] = (int16_t)(i * 10 + s);       // wartosc jednoznacznie zalezy od (probka, seria)
}
static uint32_t rd32(const uint8_t* b) { return b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24); }
static uint16_t rd16(const uint8_t* b) { return (uint16_t)(b[0] | (b[1] << 8)); }

int main() {
    // ---- encode ----
    CHECK(encode(0, 2.34f) == 23, "encode: hAccCm 2.34 cm -> 23 (skala 10)");
    CHECK(encode(0, 2.36f) == 24, "encode: zaokraglenie");
    CHECK(encode(3, 5.4f) == 540, "encode: speed skala 100");
    CHECK(encode(4, -3.9f) == -390, "encode: ujemna wartosc");
    CHECK(encode(1, NAN) == INVALID && encode(1, INFINITY) == INVALID && encode(1, -INFINITY) == INVALID, "encode: NaN/inf -> INVALID");
    CHECK(encode(15, 1e9f) == 32767 && encode(15, -1e9f) == -32767, "encode: obciecie do +-32767 (nigdy INVALID)");
    CHECK(encode(15, 100.0f) == 100 && encode(16, 1912.0f) == 1912, "encode: imuPps/imuBps skala 1");
    CHECK(encode(13, 204.6f) == 2046, "encode: heap [KB] skala 10");

    // ---- nazwy i skale ----
    { int n = 1; for (const char* c = NAMES; *c; ++c) if (*c == ',') n++; CHECK(n == NS, "NAMES ma dokladnie NS pozycji"); }
    CHECK(sizeof(SCALE) / sizeof(SCALE[0]) == (size_t)NS && sizeof(AGG) / sizeof(AGG[0]) == (size_t)NS, "SCALE i AGG maja NS pozycji");

    // ---- bufor niepelny ----
    { std::vector<int16_t> mem(100 * NS); HistoryStore st; CHECK(st.init(mem.data(), 100), "init");
        int16_t row[NS]; for (int i = 0; i < 30; i++) { makeRow(row, i); st.pushRow(row); }
        CHECK(st.count() == 30 && st.capacity() == 100, "count=30 przy niepelnym buforze");
        HistoryStore::Plan p; CHECK(st.plan(10, 100, &p) && p.nPoints == 10 && p.step == 1, "plan: ostatnie 10 s, bez agregacji");
        std::vector<int16_t> out(p.nPoints); st.series(p, 0, out.data());
        bool ok = true; for (int i = 0; i < 10; i++) if (out[i] != (int16_t)((20 + i) * 10 + 0)) ok = false;
        CHECK(ok, "seria 0: probki 20..29 od najstarszej do najnowszej");
        st.series(p, 5, out.data()); CHECK(out[9] == (int16_t)(29 * 10 + 5) && out[0] == (int16_t)(20 * 10 + 5), "seria 5: poprawna kolumna");
        CHECK(st.plan(1000, 100, &p) && p.nPoints == 30, "zakres wiekszy niz dane -> tyle, ile jest (30)");
        CHECK(!st.plan(0, 100, &p) && !st.plan(10, 0, &p), "plan: zakres 0 / punkty 0 odrzucone");
    }
    // ---- zawijanie ----
    { std::vector<int16_t> mem(20 * NS); HistoryStore st; st.init(mem.data(), 20);
        int16_t row[NS]; for (int i = 0; i < 50; i++) { makeRow(row, i); st.pushRow(row); }
        CHECK(st.count() == 20, "count zatrzymuje sie na pojemnosci po zawinieciu");
        HistoryStore::Plan p; st.plan(100, 100, &p);
        CHECK(p.nPoints == 16, "pelny bufor: 4 najstarsze pomijane (moga byc nadpisywane), zostaje 16");
        std::vector<int16_t> out(p.nPoints); st.series(p, 0, out.data());
        bool ok = true; for (int i = 0; i < 16; i++) if (out[i] != (int16_t)((34 + i) * 10)) ok = false;
        CHECK(ok, "po zawinieciu: 34..49 w prawidlowej kolejnosci");
    }
    // ---- agregacja: MAX / MIN / MEAN ----
    { std::vector<int16_t> mem(100 * NS); HistoryStore st; st.init(mem.data(), 100);
        int16_t row[NS]; for (int i = 0; i < 40; i++) { for (int s = 0; s < NS; s++) row[s] = (int16_t)i; st.pushRow(row); }
        HistoryStore::Plan p; st.plan(40, 10, &p);
        CHECK(p.step == 4 && p.nPoints == 10, "40 s / 10 punktow => kubelki po 4 s");
        std::vector<int16_t> mx(10), mn(10), me(10);
        st.series(p, 0, mx.data());   // hAccCm  = AGG_MAX
        st.series(p, 1, mn.data());   // fixLvl  = AGG_MIN
        st.series(p, 3, me.data());   // speed   = AGG_MEAN
        bool okMax = true, okMin = true, okMean = true;
        for (int b = 0; b < 10; b++) { if (mx[b] != 4 * b + 3) okMax = false; if (mn[b] != 4 * b) okMin = false; if (me[b] != 4 * b + 2) okMean = false; }
        CHECK(okMax, "MAX kubelka = najwieksza wartosc (4b+3)");
        CHECK(okMin, "MIN kubelka = najmniejsza wartosc (4b)");
        CHECK(okMean, "MEAN kubelka = srednia zaokraglona (4b+1.5 -> 4b+2)");
        // niepodzielny zakres: 10 s / 3 punkty -> kubelki po 4, pelne 2 kubelki od najnowszej
        st.plan(10, 3, &p); CHECK(p.step == 4 && p.nPoints == 2, "10 s / 3 pkt => 2 pelne kubelki po 4 s");
        std::vector<int16_t> o2(2); st.series(p, 0, o2.data());
        CHECK(o2[1] == 39 && o2[0] == 35, "kubelki liczone od najnowszej probki: [32..35] i [36..39]");
    }
    // ---- seria pradu silnika (curA): indeks 17, skala 0,01 A, agregacja MAX ----
    {
        { std::string nm(NAMES); size_t pos = nm.find("curA"); int idx = 0; for (size_t i = 0; i < pos; i++) if (nm[i] == ',') idx++;
          CHECK(pos != std::string::npos && idx == 17, "curA ma indeks 17 w NAMES (nowe serie dochodza na koncu)"); }
        CHECK(SCALE[17] == 100 && AGG[17] == AGG_MAX, "curA: skala 100 (0,01 A), agregacja MAX (krotki pik nie moze zniknac)");
        CHECK(encode(17, 12.34f) == 1234 && encode(17, 0.0f) == 0 && encode(17, NAN) == INVALID, "curA: 12,34 A -> 1234, 0 -> 0, NaN -> brak danych");
        CHECK(encode(17, 500.0f) == 32767, "curA: 500 A obciete do zakresu int16 (327,67 A) bez przepelnienia");
        std::vector<int16_t> mem(100 * NS); HistoryStore st; st.init(mem.data(), 100);
        int16_t row[NS]; for (int i = 0; i < 40; i++) { for (int s = 0; s < NS; s++) row[s] = 0; row[17] = encode(17, (i == 5) ? 9.5f : 1.0f); st.pushRow(row); }
        HistoryStore::Plan p; st.plan(40, 10, &p);
        std::vector<int16_t> cur(10); st.series(p, 17, cur.data());
        CHECK(cur[1] == 950 && cur[0] == 100 && cur[2] == 100, "pik 9,5 A w jednej sekundzie widoczny w kubelku 4 s jako 9,50 A (nie usredniony do ok. 3,1 A)");
    }
    // ---- seria wieku poprawek RTK (ageDgps): ostatnia w formacie, skala 0,1 s, agregacja MAX ----
    {
        CHECK(NS == 19 && std::string(NAMES).rfind("ageDgps") == std::string(NAMES).size() - 7, "ageDgps jest ostatnia nazwa, NS = 19");
        CHECK(std::string(NAMES).find("curA,ageDgps") != std::string::npos, "ageDgps zaraz po curA (dotychczasowe indeksy bez zmian)");
        CHECK(SCALE[18] == 10 && AGG[18] == AGG_MAX, "ageDgps: skala 10 (0,1 s), agregacja MAX (najgorszy wiek w kubelku)");
        CHECK(encode(18, 1.0f) == 10 && encode(18, 0.0f) == 0 && encode(18, 15.5f) == 155 && encode(18, NAN) == INVALID,
              "ageDgps: 1 s -> 10, 0 s (swieze poprawki) -> 0, brak -> INVALID");
        CHECK(encode(18, 5000.0f) == 32767, "ageDgps: powyzej 3276,7 s obciete do zakresu int16 bez przepelnienia");
        std::vector<int16_t> mem(100 * NS); HistoryStore st; st.init(mem.data(), 100);
        int16_t row[NS];
        for (int i = 0; i < 40; i++) { for (int s = 0; s < NS; s++) row[s] = 0; row[18] = encode(18, (i >= 5 && i < 8) ? (float)(i - 3) : 1.0f); st.pushRow(row); }
        HistoryStore::Plan p; st.plan(40, 10, &p);
        std::vector<int16_t> age(10); st.series(p, 18, age.data());
        CHECK(age[1] == encode(18, 4.0f) && age[0] == encode(18, 1.0f) && age[2] == encode(18, 1.0f),
              "zanik strumienia RTCM (wiek 2..4 s w 3 sekundach) widoczny w kubelku 4 s jako najgorsze 4,0 s (nie usredniony)");
    }
    // ---- MEAN dla liczb ujemnych, zaokraglenie od zera ----
    { std::vector<int16_t> mem(50 * NS); HistoryStore st; st.init(mem.data(), 50);
        int16_t row[NS]; int vals[2] = { -3, -2 };
        for (int i = 0; i < 2; i++) { for (int s = 0; s < NS; s++) row[s] = (int16_t)vals[i]; st.pushRow(row); }
        HistoryStore::Plan p; st.plan(2, 1, &p); int16_t o; st.series(p, 3, &o);
        CHECK(p.step == 2 && o == -3, "srednia -2.5 zaokraglona od zera do -3");
    }
    // ---- dziury (INVALID) ----
    { std::vector<int16_t> mem(60 * NS); HistoryStore st; st.init(mem.data(), 60);
        int16_t row[NS];
        for (int i = 0; i < 4; i++) { for (int s = 0; s < NS; s++) row[s] = 100; st.pushRow(row); }
        st.pushGap(4);
        for (int i = 0; i < 4; i++) { for (int s = 0; s < NS; s++) row[s] = 300; st.pushRow(row); }
        HistoryStore::Plan p; st.plan(12, 3, &p);
        CHECK(p.step == 4 && p.nPoints == 3, "12 s / 3 pkt");
        std::vector<int16_t> o(3); st.series(p, 3, o.data());
        CHECK(o[0] == 100 && o[1] == INVALID && o[2] == 300, "kubelek zlozony tylko z dziur => INVALID, sasiednie bez zmian");
        st.plan(12, 2, &p); std::vector<int16_t> o2(p.nPoints); st.series(p, 3, o2.data());
        CHECK(p.step == 6 && o2[0] == 100 && o2[1] == 300, "kubelek 'polowa dziur' liczy sie tylko z waznych probek");
        st.pushGap(100000);
        CHECK(st.count() == 60, "pushGap wiekszy niz pojemnosc jest obcinany do pojemnosci");
        st.plan(60, 60, &p); std::vector<int16_t> o3(p.nPoints); st.series(p, 15, o3.data());
        bool allInvalid = true; for (uint32_t i = 0; i < p.nPoints; i++) if (o3[i] != INVALID) allInvalid = false;
        CHECK(allInvalid, "po ogromnej przerwie cala historia to dziury");
    }
    // ---- zdarzenie krotsze niz kubelek nie ginie (zanik IMU) ----
    { std::vector<int16_t> mem(3000 * NS); HistoryStore st; st.init(mem.data(), 3000);
        int16_t row[NS];
        for (int i = 0; i < 2000; i++) { for (int s = 0; s < NS; s++) row[s] = 0; row[15] = (i >= 1000 && i < 1003) ? 0 : 100; row[11] = (i == 700) ? 9500 : 3000; st.pushRow(row); }
        HistoryStore::Plan p; st.plan(2000, 100, &p);
        std::vector<int16_t> o(p.nPoints), c(p.nPoints); st.series(p, 15, o.data()); st.series(p, 11, c.data());
        int zeros = 0, spikes = 0; for (uint32_t i = 0; i < p.nPoints; i++) { if (o[i] == 0) zeros++; if (c[i] == 9500) spikes++; }
        CHECK(p.step == 20 && zeros >= 1, "3-sekundowy zanik IMU widoczny po agregacji 20:1 (MIN)");
        CHECK(spikes == 1, "1-sekundowy skok CPU widoczny po agregacji 20:1 (MAX)");
    }
    // ---- naglowek ----
    { std::vector<int16_t> mem(100 * NS); HistoryStore st; st.init(mem.data(), 100);
        int16_t row[NS]; for (int i = 0; i < 57; i++) { makeRow(row, i); st.pushRow(row); }
        HistoryStore::Plan p; st.plan(57, 100, &p);
        std::vector<uint8_t> h(HistoryStore::headerSize() + 16);
        size_t n = st.writeHeader(h.data(), p, 1234);
        CHECK(n == HistoryStore::headerSize(), "writeHeader zapisuje dokladnie headerSize() bajtow");
        CHECK(h[0] == 'A' && h[1] == 'O' && h[2] == 'G' && h[3] == 'H' && h[4] == 1 && h[5] == NS, "magic 'AOGH', wersja 1, NS");
        CHECK(rd16(&h[6]) == 57 && rd16(&h[8]) == 1 && rd32(&h[12]) == 1234 && rd32(&h[16]) == 100 && rd32(&h[20]) == 57, "nPoints, step, lastAgeMs, capacity, stored");
        uint16_t nl = rd16(&h[24]);
        CHECK(nl % 2 == 0 && nl == HistoryStore::namesPadded(), "dlugosc nazw parzysta (wyrownanie int16)");
        CHECK(strncmp((const char*)&h[26], NAMES, strlen(NAMES)) == 0 && h[26 + strlen(NAMES)] == 0, "nazwy serii zakonczone zerem");
        size_t so = 26 + nl; bool scOk = true; for (int s = 0; s < NS; s++) if (rd16(&h[so + 2 * s]) != SCALE[s]) scOk = false;
        CHECK(scOk, "skale serii w naglowku");
        CHECK((n % 2) == 0, "dane zaczynaja sie od parzystego offsetu (Int16Array w przegladarce)");
    }
    // ---- pusty bufor ----
    { std::vector<int16_t> mem(50 * NS); HistoryStore st; st.init(mem.data(), 50);
        HistoryStore::Plan p; CHECK(st.plan(100, 100, &p) && p.nPoints == 0, "pusty bufor: plan z 0 punktow");
        CHECK(!HistoryStore().plan(10, 10, &p), "niezainicjowany bufor: plan odrzucony");
        CHECK(!st.init(nullptr, 100) || true, "init(nullptr) nie zawiesza");
    }
    printf("WYNIK: %d/%d kontroli OK\n", total - fails, total);
    return fails ? 1 : 0;
}
