# Rozwiązywanie problemów

[English](../en/troubleshooting.md) · [Pierwsze uruchomienie](getting-started.md) · [Instrukcja](user-guide.md) · [Powrót do README](../../README.pl.md)

Najpierw włącz odpowiedni przełącznik w **Diagnostyce** - pokazuje dane danego podsystemu i zapisuje je w **Dzienniku**.

| Objaw | Prawdopodobna przyczyna i co zrobić |
|---|---|
| Panel się nie otwiera | Połącz się z własnym punktem dostępowym modułu (`AOG-ESP32-Setup`) i wejdź na `192.168.4.1`. W sieci spróbuj `http://AOG-ESP32.local` albo IP z *Diagnostyka → WiFi*. Gdy moduł nie połączy się z żadną znaną siecią, sam wraca do punktu dostępowego |
| Moduł gubi WiFi | *Diagnostyka → WiFi* pokazuje liczbę zerwań, ostatni powód rozłączenia i czasy zaników. Sprawdź siłę sygnału, dodaj drugą sieć (do 4) albo użyj Ethernetu |
| Zapomniany PIN / hasło panelu | Ustaw `PANEL_AUTH_RESET_ON_BOOT 1` w `config.h`, wgraj, ustaw nowy kod i przywróć `0` |
| Zgubione hasło punktu dostępowego modułu | Podłącz USB, otwórz monitor portu szeregowego (115200 baud) i naciśnij reset: dopóki moduł jest własnym punktem dostępowym, wypisuje wylosowane hasło przy starcie i powtarza je co minutę (nie dotyczy własnego hasła - wtedy użyj panelu przez swoje WiFi albo wgraj ponownie z nowym `SECRET_AP_PASSWORD`) |
| Wgrywanie na `/update` mówi „wymagane logowanie” | Hasło OTA nie jest ustawione, a panel jest chroniony: zaloguj się najpierw do panelu (w tej samej karcie przeglądarki), potem otwórz stronę aktualizacji. Przy ustawionym `SECRET_OTA_PASSWORD` użyj użytkownika `admin` |
| Zapomniane hasło OTA | Nowe oprogramowanie wgrasz tylko kablem USB; ustaw nowe `SECRET_OTA_PASSWORD` |
| Restart / wgrywanie odrzucone | Prowadzenie jest włączone. Najpierw wyłącz je w AgOpenWeb (reguła bezpieczeństwa) |
| Kafelek *AgOpenWeb* nigdy nie pokazuje „połączono” | Ta sama sieć? Czy moduł nie słucha innego nadawcy (*Ustawienia → Nadawcy AgOpenWeb*)? Sprawdź adres broadcast w *Ustawienia → Sieć* |
| GPS bez fixa / mało satelitów | Widok anteny i kabel; *Diagnostyka → GPS* pokazuje prędkość UART, wykrycie układu i wynik konfiguracji |
| Tylko `RTK FLOAT`, nigdy `FIX` | Sprawdź kafelek NTRIP (strumieniuje? wiek poprawek?). *Diagnostyka → RTCM* wylicza typy komunikatów i błędy CRC. Upewnij się, że mountpoint pasuje do odbiornika |
| NTRIP nie łączy | Kafelek pokazuje powód: sieć, zły użytkownik/hasło, zły mountpoint, odpowiedź z tabelą źródeł. Wpisz dane ponownie (pole hasła jest puste celowo; puste = bez zmian) |
| IMU „brak danych” lub się zawiesza | Sprawdź **PS0 → 3,3 V** i przewód do GPIO6. Słabe zasilanie z USB powodowało zawieszenia w prototypie - zasil płytkę z porządnej przetwornicy 5 V. Panel może zresetować czujnik przez RST (GPIO7) |
| Łącze Ethernet pada pod obciążeniem | Zostaw **RST W5500 niepodłączony** (patrz [piny](pinout.md)) |
| Silnik nie rusza, w logu „Surowy odczyt ... jest daleko poza skalibrowanym zakresem” (Diagnostyka → Czujnik kąta: „Odczyt w skalibrowanym zakresie” = NIE) | Prawdopodobnie przewód czujnika kąta jest zerwany lub luźny albo czujnik przesunięto po kalibracji. Sprawdź okablowanie; po przesunięciu czujnika powtórz kalibrację lewo/prawo |
| Kąt jest odwrócony lub zakres zły | Powtórz kalibrację lewo/prawo i wpisz rzeczywiste kąty skrajne |
| Panel pokazuje „Czujnik kąta: brak odczytu” lub „Ekspander sekcji MCP23017 nie odpowiada” (w logu „ADS1115 / MCP23017 przestał odpowiadać na I2C”) | Układ nie odpowiada na magistrali I2C: sprawdź jego zasilanie 3,3 V i masę, przewody SDA/SCL i złącza (GPIO8/GPIO9). Moduł dalej pracuje, co 2 s sprawdza układ i wznawia pracę sam, gdy tylko odpowie - restart nie jest potrzebny. Do tego czasu silnik pozostaje zablokowany (brak kąta), a sekcje nie podążają za AgOpenWeb. Na biurku bez czujników to stan normalny |
| Silnik kręci w złą stronę | Zaznacz *Odwróć kierunek silnika* w **Kalibracji** i zapisz (prowadzenie musi być wyłączone) |
| Silnik sam się zatrzymuje | Zadziałał detektor ucieczki albo zabezpieczenie prądowe; **Status** i *Diagnostyka* pokazują które. Usuń przyczynę i naciśnij **Skasuj blokadę detektora** / **Skasuj blokadę** w **Kalibracji**. Do AgOpenWeb idzie też kick-out (PGN 250) |
| Wykresy: „historia niedostępna” | Moduł nie ma włączonego PSRAM - wybierz **OPI PSRAM** w opcjach płytki w Arduino IDE |
| Sterowanie ignoruje AgOpenWeb | AgOpenWeb nie wysłał jeszcze ustawień sterowania (*Odczyt → Sterowanie*) albo bit włączenia nie jest rozpoznawany (*Ustawienia → Odczyt włączenia prowadzenia*) |

Nadal nie działa? Zgłoś issue (szablon prosi o wersję oprogramowania, krótki opis i istotne linie logu) - nigdy nie
wklejaj haseł ani tokenów.
