# Pierwsze uruchomienie

[English](../en/getting-started.md) · [Okablowanie](wiring.md) · [Instrukcja](user-guide.md) · [Powrót do README](../../README.pl.md)

> ⚠️ Najpierw przeczytaj [SAFETY.md](../../SAFETY.md). Pierwsze testy rób z **odłączonym silnikiem** i uniesionymi kołami.

## 1. Kompilacja i wgranie

1. Zainstaluj **Arduino IDE** i **rdzeń ESP32 Arduino 3.3.11** (przetestowana wersja; tej samej używają buildy Release na GitHubie).
2. Zainstaluj biblioteki: **Adafruit ADS1X15**, **Adafruit MCP23X17**, **Adafruit NeoPixel**, **Adafruit BNO08x RVC** (przetestowane: ADS1X15 2.6.2, MCP23017 2.3.2, NeoPixel 1.15.5, BNO08x RVC 1.0.2, BusIO 1.17.4).
3. Wybierz płytkę **ESP32-S3** i ustaw **PSRAM: OPI PSRAM** (wymagane dla bufora logu i historii wykresów).
4. *(Opcjonalnie, zalecane)* skopiuj `secrets.example.h` do `secrets.h` i uzupełnij:

   | Pole | Znaczenie |
   |---|---|
   | `SECRET_WIFI_SSID`, `SECRET_WIFI_PASS` | startowa sieć WiFi (możesz też wpisać ją później w panelu) |
   | `SECRET_AP_PASSWORD` | hasło własnego punktu dostępowego, min. 8 znaków. Bez niego moduł losuje **hasło** przy pierwszym starcie (patrz niżej) |
   | `SECRET_OTA_PASSWORD` | hasło aktualizacji OTA (Arduino IDE i strona `/update`, użytkownik `admin`). **Zapisz je** - przy zgubionym haśle nowe oprogramowanie wgrasz tylko kablem USB |

   `secrets.h` jest w `.gitignore` i nie trafia do gita. Bez niego projekt też się kompiluje.
5. Otwórz `AOW-Unified-ESP32.ino` (pozostałe pliki `.h` pojawią się jako karty) i **wgraj** kablem USB.

Wolisz nie kompilować? Każde wydanie na GitHubie zawiera gotowe pliki: `*-full-flash.bin` (pierwsze wgranie kablem USB)
i `*-ota.bin` (aktualizacja ze strony `/update` panelu) oraz `SHA256SUMS.txt`.

## 2. Pierwszy start

1. Gdy moduł nie zna żadnej sieci WiFi, tworzy własny punkt dostępowy **`AOG-ESP32-Setup`** (hasło: z `secrets.h`;
   bez niego to wylosowane, które moduł wypisał na **monitorze portu szeregowego USB** (115200 baud) przy starcie
   punktu dostępowego - otwórz monitor i naciśnij przycisk reset, żeby zobaczyć je ponownie).
2. Połącz telefon lub laptop i otwórz **http://192.168.4.1**.
3. Panel zaproponuje ochronę zmian **PIN-em** (4-8 cyfr), **hasłem** (8-63 znaków) albo bez ochrony. Podgląd zostaje
   otwarty; każda zmiana wymaga kodu.
4. Wejdź w **Ustawienia** i wpisz swoją sieć WiFi (do 4 sieci; używana jest ta z najsilniejszym sygnałem, jak w telefonie).
   Kliknij **Zapisz sieć**, potem **Restartuj teraz**.
5. Potem otwieraj panel pod **http://AOG-ESP32.local** albo pod IP z *Diagnostyka → WiFi*.

Działa też **hotspot z telefonu**: wpisz jego nazwę i hasło. Moduł dostaje adres, maskę i bramę przez DHCP, więc zmienna
podsieć hotspotu nie przeszkadza.

## 3. Połączenie z AgOpenWeb

1. Moduł i komputer/tablet z AgOpenWeb muszą być w **tej samej sieci**. Przy **hotspocie z telefonu** użyj
   [forka AgOpenWeb z poprawionym unicastem](https://github.com/kostek415contact-hash/AgOpenWeb); na zwykłym routerze wystarczy wersja oryginalna.
2. Zakładka **Status**: kafelek *AgOpenWeb* pokazuje `połączono`, gdy napływają ramki, razem z IP nadawcy.
3. Jeśli włączenie prowadzenia z AgOpenWeb nie jest rozpoznawane, sprawdź **Ustawienia → Odczyt włączenia
   prowadzenia** (domyślnie bit 0x01 bajtu statusu).

## 4. GPS i RTK

- LG290P jest konfigurowany automatycznie przy starcie. **Status** pokazuje fix (celem jest `RTK FIX`).
- Poprawki: wpisz swój **NTRIP** w **Ustawieniach** (host, port, mountpoint, użytkownik, hasło) i włącz wbudowanego
  klienta; albo pozwól AgOpenWeb przekazywać RTCM do modułu.

## 5. Kalibracja

Postępuj wg [instrukcji](user-guide.md#kalibracja): najpierw czujnik kąta, potem obrót IMU, potem zero czujnika prądu.

## Późniejsze aktualizacje

OTA z Arduino IDE (port sieciowy) albo ze strony `/update`. Restart, reset fabryczny i wgrywanie są **odrzucane przy
włączonym prowadzeniu** - najpierw wyłącz je w AgOpenWeb.
