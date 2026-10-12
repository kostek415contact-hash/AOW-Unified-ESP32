# AOW-Unified-ESP32

**Język / Language:** Polski · [English](README.md)

[![Licencja: GPL v3](https://img.shields.io/badge/licencja-GPL--3.0-blue.svg)](LICENSE)
![Płytka](https://img.shields.io/badge/p%C5%82ytka-ESP32--S3-orange)
![Framework](https://img.shields.io/badge/framework-Arduino-00979d)
![Języki panelu](https://img.shields.io/badge/panel-PL%20%7C%20EN-green)

> ⚠️ **Przeczytaj [SAFETY.md](SAFETY.md) przed pierwszym użyciem.** To sterowanie kierownicą maszyny - nie jest to certyfikowany produkt bezpieczeństwa.
> Licencja [GPL-3.0](LICENSE), bez gwarancji.

**Jeden moduł ESP32-S3: autosteer, sekcje, hydraulika i GPS dla [AgOpenWeb](https://github.com/AgOpenWeb)** - z panelem WWW
na żywo, własnym klientem NTRIP, IMU i aktualizacją przez WiFi. Bez aplikacji i bez internetu: panel serwuje sam moduł,
po polsku i angielsku.

![Zakładka Status panelu WWW](docs/img/panel-status-pl.png)

<table>
<tr>
<td valign="top"><img src="docs/img/panel-status-tablet-pl.png" alt="Zakładka Status na tablecie" width="420"></td>
<td valign="top"><img src="docs/img/panel-status-phone-pl.png" alt="Zakładka Status na telefonie" width="200"></td>
</tr>
</table>
<sub>Zakładka Status. Wszystkie zrzuty ekranu i schemat połączeń w tym repozytorium zostały <b>wygenerowane przez AI</b> z danych demo, nie z prawdziwej maszyny.</sub>

## Najważniejsze

- **GPS + RTK:** LG290P 10 Hz, automatyczne wykrywanie prędkości, nieblokująca konfiguracja, własny klient NTRIP z diagnostyką RTCM.
- **Autosteer:** czujnik kąta kół, sterownik silnika z regulatorem P, ochrona prądowa, detektor ucieczki, kick-out do AgOpenWeb.
- **Sekcje + podnośnik hydrauliczny** przez ekspander I²C (MCP23017).
- **IMU** BNO085 (roll/pitch/kurs) z korekcją obrotu płytki.
- **Sieć:** do 4 sieci WiFi, własny punkt dostępowy jako awaria, przyjazne hotspotowi z telefonu (DHCP), opcjonalny Ethernet (W5500).
- **Panel WWW:** status, odczyt, 24-godzinne wykresy zapisywane przez moduł, diagnostyka, kalibracja, ustawienia, dziennik. Zmiany chronione PIN-em/hasłem.
- **Bezpieczne aktualizacje:** OTA z Arduino IDE lub z panelu; odrzucane przy włączonym prowadzeniu.
- **Testowane:** testy jednostkowe na komputerze (`tools/run_tests.sh`) przy każdym pushu.

## Wersja AgOpenWeb (hotspot / unicast)

Przy hotspocie z telefonu (losowa podsieć, broadcast bywa bezużyteczny) ramki trafiają do AgOpenWeb **unicastem**. Fork
poniżej zawiera poprawiony unicast dla hotspotu: **[kostek415contact-hash/AgOpenWeb](https://github.com/kostek415contact-hash/AgOpenWeb)**.
Na zwykłym routerze (działa broadcast) wystarczy wersja oryginalna.

## Szybki start

1. **Arduino IDE** z **rdzeniem ESP32 3.3.11** (przetestowana wersja); płytka **ESP32-S3**, PSRAM **OPI PSRAM**.
2. Zainstaluj biblioteki: *Adafruit ADS1X15*, *Adafruit MCP23X17*, *Adafruit NeoPixel*, *Adafruit BNO08x RVC* (przetestowane: ADS1X15 2.6.2, MCP23017 2.3.2, NeoPixel 1.15.5, BNO08x RVC 1.0.2, BusIO 1.17.4).
3. *(Zalecane)* skopiuj `secrets.example.h` do `secrets.h` i ustaw hasło AP oraz OTA (nigdy nie trafia do gita).
4. Otwórz `AOW-Unified-ESP32.ino` i wgraj. Albo wgraj gotowy plik ze strony **Releases**.
5. Połącz się z WiFi **`AOG-ESP32-Setup`**, otwórz **http://192.168.4.1**, wpisz swoje WiFi w **Ustawieniach**, zrestartuj.

Szczegóły, zrzuty ekranu i pierwsza kalibracja: **[docs/pl/getting-started.md](docs/pl/getting-started.md)**.

## Dokumentacja

| | Polski | English |
|---|---|---|
| Pierwsze uruchomienie | [pierwsze uruchomienie](docs/pl/getting-started.md) | [getting-started](docs/en/getting-started.md) |
| Okablowanie i części | [okablowanie](docs/pl/wiring.md) | [wiring](docs/en/wiring.md) |
| Piny i porty | [piny](docs/pl/pinout.md) | [pinout](docs/en/pinout.md) |
| Instrukcja (panel, kalibracja) | [instrukcja](docs/pl/user-guide.md) | [user-guide](docs/en/user-guide.md) |
| Rozwiązywanie problemów | [problemy](docs/pl/troubleshooting.md) | [troubleshooting](docs/en/troubleshooting.md) |
| Lista kontrolna pierwszego uruchomienia | [pierwszy start](docs/pl/first-start.md) | [first-start](docs/en/first-start.md) |
| FAQ | [faq](docs/pl/faq.md) | [faq](docs/en/faq.md) |

Ponadto: [SAFETY.md](SAFETY.md) · [SECURITY.md](SECURITY.md) · [CONTRIBUTING.md](CONTRIBUTING.md) · [CHANGELOG.md](CHANGELOG.md) ·
[ROADMAP.md](ROADMAP.md) (plan prac).

## Sprzęt w skrócie

| Moduł | Interfejs | Pin / adres |
|---|---|---|
| GPS LG290P | UART1, 460800 | RX **GPIO4**, TX **GPIO5** |
| IMU BNO085 (UART-RVC) | UART2, 115200 | RX **GPIO6**, RST **GPIO7**, PS0 do 3,3 V |
| ADS1115 (kąt kan. 0, prąd kan. 1) | I²C | SDA **GPIO8**, SCL **GPIO9**, `0x48` |
| MCP23017 (sekcje + hydLift) | I²C | `0x20`, hydLift = pin 15 |
| Cytron MD13S (silnik) | PWM + DIR | **GPIO16** / **GPIO17** |
| W5500 Ethernet (opcja) | SPI | CS 10, MOSI 11, SCK 12, MISO 13 |
| Przycisk prowadzenia / przełącznik pracy | wejścia | **GPIO15** / **GPIO18** |
| Dioda statusowa WS2812 | 1 przewód | **GPIO38** |

Pełna tabela, uwagi i porty sieciowe: [docs/pl/pinout.md](docs/pl/pinout.md). Schemat: [docs/pl/wiring.md](docs/pl/wiring.md).

## Stan funkcji

| Funkcja | Stan |
|---|---|
| WiFi (do 4 znanych sieci) + własny AP (awaryjnie) | 🟢 działa, potwierdzone w polu |
| Ethernet (W5500) | 🟡 opcja - włącz w panelu (Ustawienia → Ethernet) po podłączeniu; przy błędzie wraca na WiFi |
| GPS 10 Hz, autobaud, ponowienie po późnym starcie | 🟢 działa |
| Własny klient NTRIP (VRS) + diagnostyka RTCM | 🟢 działa, RTK FLOAT i FIX potwierdzone |
| IMU BNO085 | 🟢 działa, 100 pakietów/s |
| Czujnik kąta skrętu | 🟢 działa (kalibracja w panelu) |
| Sekcje + hydLift | 🟢 działa |
| Panel WWW | 🟢 działa |
| Silnik + regulator P | 🟡 kod gotowy, silnik jeszcze niezamontowany |
| Czujnik prądu (ACS71240KEXBLT-030B3, ±30 A) | 🟡 podłączony, kalibracja zera i odczyt działają; **nie sprawdzony pod obciążeniem silnika** |

## Struktura repozytorium

```
config/         config.h - WSZYSTKIE ustawienia (piny, adresy, włącz/wyłącz funkcje)
secrets.h       hasła startowe (poza gitem; wzór: secrets.example.h)
connectivity/   WiFi, PGN wysyłane i odbierane, główna pętla (network_manager.h)
sensors/        GPS (nieblokujący automat konfiguracji), IMU, czujnik kąta, czujnik prądu
io/             przycisk + przełącznik, ekspander sekcji (MCP23017), dioda
actuators/      silnik, hydraulika
ntrip/          własny klient NTRIP, przekazywanie RTCM z AgOpenWeb
web/            panel WWW (web_ui.h + ui/ = strona w częściach, web_server.h = serwer i /status), ustawienia w NVS, bufor logu, katalogi języków
diagnostics/    watchdog, profil CPU, trend wolnego RAM, czasy zaników WiFi
ota/            aktualizacja przez WiFi
tests/          testy jednostkowe na komputerze (czysty C++)
tools/          run_tests.sh, i18n_check.py, docs_check.py, log_extract.py
docs/           instrukcje (pl, en) i obrazy
```

## Zasady, których trzymamy się w kodzie

- `loop()` **nigdy nie blokuje** (żadnych `delay()`/czekania w pętli).
- Diagnostyka mierzy **czas**, nie liczbę obiegów pętli.
- Zmiany robimy **po jednej**, każda potwierdzona na sprzęcie.
- Kod i klucze tłumaczeń są angielskie / neutralne; polski to tylko jeden z katalogów językowych (`web/lang/README.md`).

## Wkład, bezpieczeństwo, licencja

- [CONTRIBUTING.md](CONTRIBUTING.md) - jak zbudować, przetestować i dodać język
- [SECURITY.md](SECURITY.md) - zgłaszanie podatności
- [SAFETY.md](SAFETY.md) - **przeczytaj** przed podłączeniem do maszyny
- [LICENSE](LICENSE) - **GNU GPL v3.0**; rozpowszechniane prace pochodne muszą mieć tę samą licencję. Bez gwarancji.
