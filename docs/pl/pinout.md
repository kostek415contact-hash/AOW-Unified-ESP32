# Piny i adresy

[English](../en/pinout.md) · [Okablowanie](wiring.md) · [Powrót do README](../../README.pl.md)

Wszystkie wartości są w `config/config.h`. **GPIO** oznacza piny ESP32-S3.

| Moduł | Interfejs | Pin / adres | Uwagi |
|---|---|---|---|
| GPS LG290P | UART1, 460800 baud | RX **GPIO4**, TX **GPIO5** | Prędkość jest wykrywana automatycznie |
| IMU BNO085 | tryb UART-RVC, 115200 | przewód SDA czujnika → **GPIO6** (RX ESP32), RST → **GPIO7** (`IMU_RESET_PIN`), PS0 → **3,3 V** | PS0 wybiera tryb RVC - przylutuj na stałe. Zasilanie 3,3 V i GND. Kompas nie jest używany |
| ADS1115 | I²C, `0x48` | SDA **GPIO8**, SCL **GPIO9** | kanał 0 = kąt skrętu, kanał 1 = prąd silnika |
| MCP23017 | I²C, `0x20` (ta sama magistrala) | 16 wyjść, piny **0-15**; zanim AgOpenWeb wyśle konfigurację pinów: Sec1-16 na pinach **0-15** | Funkcja każdego pinu wynika z konfiguracji pinów w AgOpenWeb (Machine → Pin Config); „Invert relay” odwraca wszystkie |
| Cytron MD13S | PWM + DIR | PWM **GPIO16**, DIR **GPIO17** | Zasilanie silnika nie przechodzi przez ESP32 |
| Czujnik prądu ACS71240KEXBLT-030B3 (±30 A) | analog → ADS1115 | kanał 1 | Zasilaj z 3,3 V (nie z 5 V - uszkodzi wejście ADS1115) |
| W5500 Ethernet (opcja) | SPI | CS **GPIO10**, MOSI **11**, SCK **12**, MISO **13** | Patrz uwaga o RST niżej. Włączenie w panelu: Ustawienia → Ethernet |
| Przycisk prowadzenia | wejście, podciąganie | **GPIO15** | Wciśnięty = zwarty do GND |
| Przełącznik pracy | wejście, podciąganie | **GPIO18** | Aktywny = zwarty do GND |
| Dioda statusowa WS2812 | 1 przewód | **GPIO38** | |

### W5500: RST celowo niepodłączony

GPIO14 zostaje wolny, a sterownik dostaje `-1` zamiast pinu RST w `ETH.begin()`. Z podłączonym RST łącze cyklicznie
padało pod obciążeniem i pokazywało ciągłe błędy transmisji (potwierdzone testem: 0 błędów z odłączonym RST, stałe
błędy z podłączonym). Moduł ma własny układ power-on-reset na płytce.

### Piny zasilania

Rozłóż zasilanie na kilka pinów listwy: w pierwszej wersji użyto 8 × GND, 3 × 5 V, 2 × 3,3 V.

### Porty sieciowe

| Port | Kierunek | Do czego |
|---|---|---|
| 9999 | moduł → AgOpenWeb | ramki dla AgOpenWeb (PANDA, status sterowania, ...) |
| 8888 | AgOpenWeb → moduł | tu moduł nasłuchuje (komendy sterowania i maszyny, RTCM) |
| 5126 | pomocniczy | pomocniczy port UDP, na którym moduł też nasłuchuje (`AUX_PORT`) |
| 2233 | AgOpenWeb → moduł | surowe poprawki RTCM przekazywane do GPS (`RTCM_LISTEN_PORT`) |
| 80 | przeglądarka → moduł | panel WWW (`/update` to strona wgrywania oprogramowania) |
