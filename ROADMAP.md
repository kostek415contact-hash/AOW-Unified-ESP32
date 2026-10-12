# Roadmap - AOW-Unified-ESP32

Stan na **10.10.2026**. Aktualizowana ręcznie - szczegóły każdej zmiany w `git log`.

Legenda: 🟢 działa · 🟡 kod jest, czeka na sprzęt/test/włączenie · 🔴 do zrobienia · ➖ opcjonalne

---

## 🎯 Następny krok

Stan na 10.10.2026: moduł w obudowie, RTK w terenie działa, panel i firmware są dwujęzyczne (PL + EN), zabezpieczenia silnika
(sekcja 5, priorytet 1) i odporność na zanik czujników I2C są w kodzie, a build z GitHuba działa jak z Arduino IDE (środowisko
przypięte, patrz „Decyzje architektoniczne”). **Teraz** (kolejność):
1. Na sprzęcie: zapis kalibracji kąta LEWO/PRAWO od nowa po podłączeniu czujnika, sprawdzenie w panelu, że ADS1115, MCP23017 i IMU odpowiadają.
2. Silnik i czujnik kąta NTY, pull-downy 10 kΩ na PWM/DIR, bezpiecznik, E-stop; test na stanowisku (sekcja 7) i pierwsza jazda z bardzo małą prędkością (sekcja 6).
3. Po jeździe: strojenie regulatora P, test sekcji i hydLift w polu; kod wg backlogu (sekcja 5, otwarte) i ewentualny podział `web_ui.h` (ok. 2200 linii; ryzykowny, dopiero po jeździe).
4. Na samym końcu: przygotowanie wydania (5b B): nowe PRYWATNE repo bez historii.

Pamiętaj o `secrets.h` obok `.ino` (plik poza gitem - nie wysyłaj go w archiwach).

---

## 1. Gdzie jesteśmy

| Obszar | Stan | Uwagi |
|---|---|---|
| WiFi + własny AP (fallback) | 🟢 | tryb AP ma osobny sygnał na diodzie |
| GPS LG290P (10 Hz, autobaud, ponawianie) | 🟢 | GST -> hAcc/vAcc |
| Własny klient NTRIP (VRS) + diagnostyka RTCM | 🟢 | RTK FLOAT i FIX potwierdzone w polu |
| IMU BNO085 (UART-RVC) | 🟢 | 100 pakietów/s, samoleczenie, korekta obrotu płytki; dawne zawieszenie czujnika wynikało ze zbyt słabego zasilania z USB (09.10.2026: na zewnętrznej przetwornicy działa poprawnie) |
| Czujnik kąta (ADS1115, odczyt nieblokujący) | 🟢 | kalibracja lokalna w panelu; czujnik docelowy: patrz sprzęt |
| Wyjścia maszyny (MCP23017) | 🟡 | wszystko wg Pin Config z AgOpenWeb (PGN 236): sekcje, HydUp/HydDown (impuls z raise/lower time), tory, geo-stop, Invert relay; bez własnych przełączników; do sprawdzenia na sprzęcie |
| Dioda statusowa | 🟢 | kolory wg jakości RTK / OTA / AP |
| Panel WWW | 🟢 | nowy (faza 1): Status z kafelkami i uwagami, Odczyt, Diagnostyka per flaga, Kalibracja, Ustawienia z walidacją, Dziennik z filtrami. Faza 2 (dane z firmware w kartach Diagnostyki) - kod gotowy, do sprawdzenia na sprzęcie |
| Silnik: regulator P + ochrony | 🟡 | regulator P, ochrona skrajnych położeń, ochrona prądowa i **detektor ucieczki** (odwrócony kierunek / brak ruchu) + przełącznik kierunku w panelu; sprawdzone symulacją pętli zamkniętej na komputerze, **brak silnika do testu na sprzęcie** |
| Czujnik prądu (ACS71240KEXBLT-030B3) | 🟡 | kod gotowy i przetestowany na komputerze (odczyt na A1 ADS1115, kalibracja zera, kickout, panel); **podłączony (09.10.2026): kalibracja zera i odczyt działają; zostaje test pod obciążeniem silnika** |
| Ethernet W5500 | ➖ | **nieplanowany** (decyzja 08.10.2026): kod zostaje jako opcja (włączana w panelu: Ustawienia → Ethernet, po restarcie; fallback na WiFi), moduł niepodłączony i bez planów podłączenia. Po jego włączeniu ustaw watchdog z powrotem na ok. 25 s |

## 2. Sprzęt

| Element | Jest? | Uwagi |
|---|---|---|
| ESP32-S3, GPS LG290P, BNO085, ADS1115, MCP23017, przetwornice | ✅ | działają |
| Sterownik Cytron MD13S | ✅ | czeka na silnik |
| Czujnik prądu ACS71240KEXBLT-030B3 (±30 A, 44 mV/A) | ✅ | na miejscu, do zamontowania; wyjście na wejście **A1** ADS1115, zasilanie **3,3 V** |
| **Silnik** (JGB37-550) | ❌ | patrz "Stanowisko testowe" - można przetestować na zamienniku |
| **Czujnik kąta NTY** | ❌ | obecnie potencjometr/dzielnik testowy |
| Moduł W5500 (Ethernet) | ➖ | nieplanowany; kod zostaje, piny w README |

> ⚠️ **Czujnik kąta zasilaj z tego samego napięcia co ADS1115.** Wejścia ADS1115
> nie mogą przekroczyć VDD + 0,3 V - czujnik 5 V przy ADS na 3,3 V go uszkodzi.

---

## 3. Kolejność prac

| # | Zadanie | Wielkość | Po co |
|---|---|---|---|
| 1 | 🖥️ **Panel WWW - faza 1** (nowa strona) | ✅ gotowe | do wgrania i sprawdzenia na telefonie |
| 2 | 🖥️ **Panel WWW - faza 2**: dane z firmware dla kart z dopiskiem "dojdzie w kolejnej aktualizacji" (sekcja 4) | ✅ gotowe | do sprawdzenia na sprzęcie; zostały drobne braki (sekcja 4) |
| 3 | 🛡️ **Poprawki bezpieczeństwa (część)**: walidacja ustawień **w firmware** (panel już waliduje w przeglądarce, ale firmware musi też) + dzielenie przez zero w kalibracji kąta | ✅ gotowe | `settings_validation.h` (96 testów) i `steering_math.h`; kąt nieważny zamiast NaN, silnik zablokowany |
| 4 | ⚡ **Czujnik prądu**: odczyt ADS1115 kanał 1 (w przerwach między odczytami kąta), przeliczenie na A, kalibracja zera, kickout, panel | ✅ kod gotowy | test na sprzęcie wg listy w sekcji 7 |
| 5 | 🛡️ **Poprawki bezpieczeństwa (reszta)**: ✅ podwójne źródło RTCM, ✅ PGN 215, ✅ liczenie CRC, ✅ wyścig rdzeni (`String`), ✅ hasła poza `/status`, ✅ `secrets.h`, ✅ hasło OTA (mechanizm; włącza się w `secrets.h`); **zostało:** odrzucanie ramek ze złym CRC, filtr źródła PGN, logowanie do panelu | ✅ prawie | stabilność |
| 6 | 🧪 **Detektor ucieczki + stanowisko testowe silnika** | ✅ kod gotowy | detektor, przełącznik kierunku (Kalibracja → Silnik) i symulacja pętli zamkniętej gotowe; test na stanowisku z silnikiem odłożony |
| 7 | 🚜 **Montaż w obudowie i test w polu** | - | sekcja 6 |

---

## 4. Panel WWW

**Zasada:** każda flaga `DEBUG_*` włączona (w Ustawieniach albo przełącznikiem w zakładce
**Diagnostyka**) pokazuje osobną kartę **ze wszystkimi danymi** danego podsystemu.

**Układ (faza 1, gotowe):** `Status` (kafle zielone/żółte/czerwone + lista uwag) · `Odczyt` ·
`Diagnostyka` (karta na każdą flagę, przełącznik w nagłówku) · `Kalibracja` (czujnik kąta ze
wskaźnikiem położenia, obrót IMU krok po kroku) · `Ustawienia` (walidacja pól) ·
`Dziennik` (filtry, szukanie, pauza, kopiuj, pobierz). Nawigacja: dolny pasek na telefonie,
zakładki u góry na komputerze. Zapytania tylko dla widocznej zakładki; baner przy utracie
połączenia; po restarcie strona sama się odświeża.

### Stan kart Diagnostyki

| Flaga | Karta | Brakuje w firmware (faza 2) |
|---|---|---|
| `STEP_TIMING` | 🟢 | - |
| `IMU` | 🟢 | - |
| `SYS` | 🟢 | - (jest: min. RAM, największy blok, temperatura, powód restartu) |
| `STEER_COMMAND` | 🟢 | - |
| `STEER_BUTTON` / `WORK_SWITCH` | 🟢 | - |
| `RTCM` (NTRIP + relay) | 🟢 | - |
| `GPS` | 🟢 | jest: prędkość UART + skąd ją mamy, chipset (wersja z PQTMVER), stan ponawiania, wiek poprawek RTK (kolor + uwaga na Statusie). Zostało: wiek GGA/GST, historia zmian fixa |
| `STEERING_SENSOR` | 🟢 | - (jest: napięcie, błędy I2C, wiek odczytu) |
| `MOTOR` | 🟢 | - (jest: sterownik, kierunek DIR, PWM i błąd kąta, stan detektora ucieczki, ruch w oknie, zadziałania z przyczyną i ruchem, parametry). Zadziałanie widać też na Statusie (kafelek „Sterowanie”: STOP + uwaga) |
| `CURRENT` | 🟢 | - (jest: stan, prąd chwilowy/średni, szczyt z sekundy i od skasowania, napięcie/zero, wiek odczytu, tempo próbkowania, zadziałania ochrony, progi, kalibracja). Prąd jest też kafelkiem na Statusie i wykresem (`curA`, szczyt |I| z sekundy, z pasmami progów) zapisywanym w historii modułu |
| `WIFI` | 🟢 | jest: utraty połączenia, powód ostatniego rozłączenia (kod + opis), skany i przełączenia failoveru, restarty awaryjne. jest też czas zaniku (ostatni / najdłuższy / średni oraz trwający, `diagnostics/wifi_outage.h`, 13 testów) i log `[WIFI] Reconnected after N ms`. Szybkie łączenie (`WIFI_FAST_CONNECT` w `config.h`) - **do sprawdzenia na sprzęcie: czy czas powrotu w tym wierszu jest krótszy niż przed zmianą; przy kilku punktach dostępowych o tej samej nazwie (mesh) wyłącz** |
| `NET` | 🟢 | - (jest: TX udane/błędy, odbudowy gniazd UDP, odrzucone ramki) |
| `PGN` | 🟢 | - (jest: liczniki ramek wg PGN wraz z Hello/Scan/RTCM, odstępy komend steer) |
| `MACHINE_COMMAND` | 🟡 | odstęp między komendami, surowy bajt hydLift, tram, geoStop |
| `ETHERNET` | 🟡 | link, liczba nieudanych `ETH.begin()` (tylko przy włączonym Ethernecie) |
| `STEER_COMMAND_RAW` | 🟢 | - (jest: ostatnia ramka PGN254 w hex, licznik, odstępy) |
| `WEB` | 🟡 | jest: wynik ostatniej próby OTA (IDE i strona `/update`), błąd, rozmiar, „restart po aktualizacji OTA”. **Postępu na żywo nie da się pokazać** - serwer WWW i wgrywanie działają w tym samym zadaniu, więc podczas przesyłania panel nie odpowiada |

---

## 5. Backlog (kod)

### Priorytet 1 - stabilność i bezpieczeństwo
- [x] Walidacja wartości z panelu (zakresy: watchdog, czasy, jasność, porty, długości SSID/hasła)
- [x] Kalibracja kąta: odrzucać identyczne punkty LEWO/PRAWO, zabezpieczyć dzielenie
- [x] Wyścig rdzeni: panel (rdzeń 0) przypisuje `String` czytane w pętli (rdzeń 1) - `web/settings_lock.h` (mutex + migawka; test ThreadSanitizer: stary kod 3 wyścigi, nowy 0)
- [x] `RtcmRelay` i PGN215 ignorować, gdy płynie własny NTRIP (`ntrip/rtcm_arbiter.h`; okno 3 s ciszy oddaje kanał AgOpenWeb)
- [x] Odbiór PGN: niezgodności CRC są liczone (panel: karta PGN); bufor 1 KB, długość PGN215 odporna na zawinięcie bajtu
- [x] Odbiór PGN: ramki sterujące (254, 239, 252) z błędną sumą są odrzucane (`PgnFrame::shouldRejectOnCrc`, `tests/test_pgn_frame.cpp`); decyzja po teście 08.10.2026: 88 451 ramek, 0 błędnych. Licznik odrzuceń w Diagnostyka -> PGN. Filtr po IP AgIO: wciąż do rozważenia
- [x] `/status` nie zwraca haseł (tylko flagi `*PassSet`); hasło WiFi przeniesione do `secrets.h` (`.gitignore`)
- [x] `/update` i ArduinoOTA z hasłem z `secrets.h` (`SECRET_OTA_PASSWORD`); strona `/update` restartuje moduł tylko po sukcesie
- [x] Zabezpieczenie panelu PIN-em (4-8 cyfr) albo hasłem (8-63 znaki), do wyboru przy pierwszym uruchomieniu (`web/panel_auth.h`, 66 testów): chronione są wszystkie endpointy ZMIENIAJĄCE stan (zapisy ustawień, restart, factory reset, kalibracje, test silnika, hydLift ręczny); podgląd (status, wykresy, dziennik) otwarty. Żeton logowania 12 h w nagłówku `X-Auth`; zmiana lub wyłączenie kodu wymaga żetonu ORAZ ponownego wpisania obecnego kodu; blokada 30 s po 5 błędnych kodach (potem coraz dłuższa, max 5 min). Zapomniany kod: `PANEL_AUTH_RESET_ON_BOOT 1` w `config.h` i wgranie firmware. **Do sprawdzenia na sprzęcie** (kompilacja ESP32 i działanie żetonu przez prawdziwy `WebServer`). `/update` nadal chroni osobne hasło OTA
- [x] Silnik: detektor ucieczki (`actuators/runaway_guard.h`): odwrócony kierunek -> stop po 0,2-0,6 s, brak ruchu -> stop po oknie (domyślnie 1 s); zatrzask schodzi po wyłączeniu prowadzenia na 1 s albo z panelu; parametry i **przełącznik "Odwróć kierunek"** w panelu (Kalibracja -> Silnik)
- [x] Czujnik prądu: zatrzask nadprądowy schodzi dopiero, gdy prąd spadnie poniżej progu (świeży, wiarygodny odczyt) ORAZ prowadzenie jest wyłączone przez 1 s - wcześniej przy trwającym nadprądzie (np. niepodłączony czujnik przy włączonej funkcji) zatrzask flapował co sekundę i zalewał dziennik
- [x] Silnik: włączenie funkcji w panelu PO starcie inicjalizuje PWM i DIR (wcześniej `begin()` kończył się od razu i silnik nie dostawał sygnału), a wyłączenie w trakcie ruchu zeruje PWM (wcześniej ostatnie wypełnienie zostawało na pinie i silnik jechał dalej)
- [x] Silnik: gdy AgOpenWeb nie wysłał ustawień sterowania (PGN 252), gainP/highPWM/minPWM = 0 i PWM = 0 (celowo bezpiecznie); panel pokazuje teraz wartości tych ustawień i wprost mówi, dlaczego silnik nie rusza (karta Kalibracja -> Silnik, Diagnostyka, uwaga na Statusie)
- [x] Silnik: ręczny test z panelu (Lewo/Prawo) - impuls o ograniczonym czasie i PWM, odmowa przy włączonym prowadzeniu / wyłączonym sterowniku / blokadzie prądowej / skrajnym położeniu, przerwanie przy włączeniu prowadzenia, wynik z interpretacją (prąd, kąt, kierunek)
- [x] Flaga "prowadzenie włączone" nie wygasała, gdy AgOpenWeb przestawał nadawać (zostawało WŁĄCZONE do restartu): wprowadzone `isAutoSteerActive()` = flaga ORAZ świeże komendy i użyte w silniku, teście ręcznym, panelu, zatrzaskach (prąd, detektor), zerowaniu czujnika prądu i historii
- [x] Panel: detektor ucieczki mówi wprost, gdy czujnik kąta wygląda na niepodłączony (napięcie ~0 V) lub minPWM jest bardzo niski; przyciski testu silnika pokazują powód blokady
- [x] Sieć: moduł SAM pobiera adres, bramę i maskę (DHCP) w KAŻDEJ sieci, także w hotspocie telefonu, który ma losową, zmienną podsieć (np. 10.20.255.x z bramą 10.20.255.117); wcześniej każda sieć dostawała stały adres 192.168.1.x. Stały adres jest opcją tylko dla slotu z wymuszeniem (router bez DHCP). Adres docelowy ramek do AgOpenWeb liczony z FAKTYCZNEJ podsieci (broadcast /24, /28 na iPhone), poznany adres AgOpenWeb nie jest używany w innej podsieci (wcześniej `agioIpKnown` nigdy się nie zerowało) i wygasa po 5 s ciszy; odpowiedź PGN 203 podaje faktyczny adres i maskę; Diagnostyka -> WiFi pokazuje adres/maskę/bramę, źródło adresu i dokąd idą ramki
- [x] GPS: wykres słupkowy C/N0 osobno dla każdego satelity i sygnału (Diagnostyka -> GPS, flaga GPS): grupy wg systemu, kolor wg siły sygnału, wyszarzone satelity nieużywane w rozwiązaniu (GSA), podsumowanie wg najlepszego sygnału satelity. Moduł włącza w odbiorniku GSV i GSA raz na sekundę (PQTMCFGMSGRATE, dzielnik = częstotliwość), endpoint /gps_sats, limit pól NMEA podniesiony z 20 do 24 (GSV z id sygnału ma 22). **Po wgraniu sprawdź w karcie GPS pozycję Przepływ danych: ma wzrosnąć tylko o 2-3 KB/s; skok o kilkadziesiąt oznacza, że dzielnik działa inaczej, niż zakładamy**
- [x] GPS: wiek poprawek RTK (GGA pole 13) w panelu rozróżnia „brak” od „0 s” (`sensors/gps_age.h`, 32 testy): puste pole, zwykły GPS albo GGA starsze niż 3 s dają „brak”, a nie fałszywe 0,0 s. Kolor i uwaga na Statusie: powyżej 10 s „spóźnione”, powyżej 120 s „stanęły” (limit z noty Quectel). `$PANDA` do AgOpenWeb bez zmian (przy braku nadal 0). **Przy teście RTK patrz na ten wiersz: przy działającym VRS ma być ok. 1 s; rosnący wiek przy FLOAT oznacza, że problem jest w strumieniu RTCM, a nie w antenie**
- [x] Wykresy v2: dymek przy kursorze/palcu (godzina, „−12 min 36 s”, wartości wszystkich serii), oś czasu w godzinach zegarowych z delikatną siatką, min / śr. / maks pod każdym wykresem, jakość fixa jako kolorowy pas (FIX zielony, FLOAT/DGPS/GPS żółty, brak czerwony) z **udziałem czasu w FIX/FLOAT/GPS**, nowy wykres **wiek poprawek RTK** (pasma: >10 s żółte, >120 s czerwone; seria `ageDgps` zapisywana też w historii modułu, agregacja MAX), znacznik „teraz” na końcu linii, mocniejsza linia zera (roll, pitch, kąt), drugi kolor serii `--c2` zamiast szarości (zwalidowany walidatorem palet: separacja dla daltonistów ΔE ≈ 21–24), oś wielkości nieujemnych zaczyna się od 0 (wcześniej prąd i wiek schodziły poniżej zera). **Po wgraniu sprawdź na sprzęcie:** pamięć historii w PSRAM urosła o ok. 5,5 % (19 serii zamiast 18), a w logu startowym `[HISTORIA]` ma nadal pokazać 24 h
- [x] Wykresy v3: wspólny kursor na wszystkich wykresach, grupy (GPS i RTK / Ruch i sterowanie / System i łączność), pasek podsumowania RTK (% FIX, najdłuższy FIX, liczba utrat FIX, maks. wiek poprawek), pasma progów na hAcc (stałe `HACC_GOOD_CM`=2,5 i `HACC_WARN_CM`=10 w `web_ui.h` - założenia do potwierdzenia na polu), pełny ekran per wykres (Esc zamyka), zapamiętany zakres, wiek poprawek w nagłówku Status. Wciąż otwarte: znaczniki zdarzeń, zoom przeciąganiem
- [x] Wykresy: znaczniki zdarzeń jako pionowe linie (`diagnostics/event_log.h` + `event_detector.h`, 29 testów): restart modułu (z powodem: watchdog, panika, brownout, OTA), zmiana jakości fixa (po 2 s utrzymania), utrata/powrót NTRIP, sieci i AgOpenWeb, zmiana nadawcy, prowadzenie wł./wył., zadziałanie ochrony silnika (zatrzask prądu, detektor ucieczki). Dziennik 64 zdarzeń w RAM modułu (`/events`), zapis raz na sekundę także bez przeglądarki; panel: linie + trójkąty na wykresach, opis w dymku przy kursorze, przełącznik „Zdarzenia”, lista pod wykresami. Restart modułu kasuje dziennik (tak jak historię), przeglądarka trzyma swoją kopię. **Do sprawdzenia na sprzęcie**
- [x] Silnik: regulator P w `actuators/pwm_math.h` (23 testy): `minPWM` większe od `highPWM` jest przycinane (wcześniej podciąganie łamało limit highPWM), NaN/Inf w błędzie lub wzmocnieniu daje PWM 0
- [x] Watchdog 5 s: konfiguracja GPS po ponowieniu (najdłuższa znana blokada pętli, ok. 3 s) karmiła watchdoga między krokami (od 09.10.2026 konfiguracja GPS jest nieblokująca, patrz niżej). OTA z IDE i strona `/update` chodzą w osobnym zadaniu (rdzeń 0), więc nie blokują pętli
- [x] Silnik: zamrożony czujnik kąta wykrywany szybciej niż oknem: surowy odczyt ADC bit w bit taki sam przez 12 kolejnych NOWYCH próbek (ok. 0,4 s) podczas napędu => STOP, stan 4 „czujnik kąta zamrożony” w panelu i logu (`actuators/runaway_guard.h`, `tests/test_runaway_frozen.cpp`, 18 testów). **Do potwierdzenia na sprzęcie:** czy żywy czujnik zawsze szumi o kilka LSB (gdyby nie, a koło stoi mimo napędu, i tak zadziała stary detektor braku ruchu, tylko wcześniej)
- [x] Sygnał do AgOpenWeb po zadziałaniu ochrony silnika (`connectivity/kickout_signal.h`, 27 testów): sprawdzone w źródłach AgOpenWeb - bit 0x02 w PGN 253 to tylko informacja o przełączniku i niczego nie wyłącza; prowadzenie wyłącza **PGN 250** (bajt 5 = wartość czujnika 0-255 → %), gdy w AgOpenWeb włączono „Czujnik prądu” z progiem. Moduł wysyła PGN 250 co 40 ms: po zadziałaniu zatrzasku prądu, błędzie/braku danych czujnika albo detektora ucieczki **255 (100%)** i trzyma to jeszcze 3 s po ustaniu przyczyny; w normalnej pracy obciążenie 0-150 (maks. ok. 59%). AgOpenWeb nie włącza prowadzenia sam po kickoucie - operator włącza je ponownie. Panel: Diagnostyka → Silnik (sygnał, licznik), uwaga na Statusie, log `[KICKOUT]`. **Wymaga ustawienia w AgOpenWeb: Konfiguracja AutoSteer → Czujnik prądu włączony, próg 80-100%. Do sprawdzenia na sprzęcie**
- [x] Silnik, zabezpieczenia A-G (10.10.2026): zegar bezpieczeństwa (`actuators/motor_safety.h`, zadanie na rdzeniu 0 odcina PWM, gdy `update()` nie był wołany 300 ms), piny silnika LOW od pierwszej linii `setup()` (`MotorDriver::safePins()`), uzbrojenie po restarcie (jazda dopiero po świeżej ramce z wyłączonym prowadzeniem), odrzucanie odczytu kąta spoza skalibrowanego zakresu (`SteeringMath::rawPlausible`, przerwany przewód), 20 ms postoju przy odwróceniu kierunku (`DirGuard`), przerwanie testu impulsowego na skrajnym położeniu; wiersze w panelu (Uzbrojenie, Zawieszenia pętli, Ochrona prądowa, Odczyt w zakresie). Testy: `tests/test_motor_safety.cpp`
- [x] I2C: brakujący lub znikający ADS1115 / MCP23017 nie spowalnia pętli (`sensors/i2c_retry.h`, 14 testów): ponawianie z rosnącymi przerwami (od razu / 250 ms / 1 s), po 10 porażkach z rzędu układ jest „utracony" i tylko sprawdzany co 2 s, a po powrocie wznawia się sam; ekspander sekcji sprawdzany co 1 s. Panel: „nie odpowiada" (czerwone) i uwaga na Statusie. Silnik pozostaje zablokowany bez świeżego kąta
- [x] Przegląd kodu 10.10.2026, paczka 1 (bezpieczeństwo): silnik zablokowany, dopóki oba punkty kąta nie zostały zapisane (`calComplete()`); zmiany ustawień silnika/prądu/zakresu/flag/kalibracji i skasowanie zatrzasków odrzucane przy włączonym prowadzeniu (409); zatrzaski schodzą tylko po potwierdzonym „prowadzenie wyłączone” (`SteerCommandReceiver::isDisengagedConfirmed()`), nie po utracie łączności; reset uzbrojenia po wyłączeniu funkcji silnika; ramki sterujące 254/239/238/252 bez potwierdzonej sumy (także bez bajtu CRC) odrzucane; teksty użytkownika w `/status` escapowane (`web/json_escape.h`, 10 testów). Reszta ustaleń przeglądu: kolejne paczki (pętla GPS/UDP, OTA i sieć, panel, testy/CI)
- [x] Przegląd kodu 10.10.2026, paczka 2 (pętla): ponawianie prędkości GPS bez blokowania pętli (`gps_baud_retry.h`, 18 testów), okna świeżości PANDA = 5 okresów GPS (0,5-2 s) i IMU 500 ms (`live_window.h`, 9 testów), PGN 218 co 100 ms (`slot_schedule.h`, 9 testów), odbiór UDP do 16 datagramów na przebieg, `delay(1)` tylko dla W5500, limit odpowiedzi na Hello. **Do sprawdzenia na sprzęcie** (GPS przy odłączonym odbiorniku, diagnostyka odstępów)
- [x] Przegląd kodu 10.10.2026, paczka 3 (OTA i sieć): `/update` wymaga logowania panelu, gdy nie ma hasła OTA, a panel jest chroniony; odrzucanie żądań z obcej strony po nagłówku `Origin` (`web/request_guard.h`, 19 testów); NTRIP: wykrycie martwego strumienia (20 s bez RTCM, `StallWatch`), natychmiastowe połączenie po zapisie ustawień i zamknięcie połączenia po wyłączeniu klienta; sprawdzanie planu adresów (adres modułu, brama w podsieci, adres AgOpenWeb); hasło AP powtarzane na USB co minutę. **Do sprawdzenia na sprzęcie** (wgranie firmware z panelu z PIN-em i bez, zmiana danych NTRIP w trakcie strumienia)
- [x] Przegląd kodu 10.10.2026, paczka 4 (część 1: testy i CI): parser NMEA wyniesiony do `sensors/nmea.h` (55 testów: suma, dzielenie, GGA/VTG/RMC/GST, puste pola, za krótkie zdania, przepełnienie buforów), ścisła kontrola sumy (dokładnie dwie cyfry hex); testy w CI nie są już uruchamiane podwójnie na `main`, limit czasu 10 min. **Do sprawdzenia na sprzęcie**: GPS nadal ma fix, licznik „poprawna suma” rośnie
- [x] Przegląd kodu 10.10.2026, paczka 4 (część 2: panel): ochrona przed podwójnym dotknięciem i blokada przycisków, czytelne błędy sieci, hydraulika (blokada przy wyłączonej funkcji + potwierdzenie + 409 z modułu), potwierdzenie zapisu końca zakresu kąta, szybsza i wyraźniejsza informacja o braku łączności (przyciemnienie danych), nakładka restartu znika po 60 s, rzadsze pobieranie historii (137 KB). **Odłożone (większe, osobna paczka):** budowanie `/status` bez `String` (C2), wydzielenie stałych ustawień z `/status` do osobnego punktu (C3), przyrostowy `/log` (C4), dostępność z klawiatury (D7)
- [x] Przegląd kodu, paczka 4 (część 3: potwierdzenia): własne okno potwierdzenia dla wszystkich akcji nadpisujących kalibrację, zerujących czujnik, kasujących blokady, ruszających silnikiem lub zmieniających ustawienia bezpieczeństwa (IMU punkt A/B i zapis, prąd, silnik, detektor ucieczki, test silnika, zakres kąta, funkcje, progi, sieć, Ethernet, tryb załączania, nadawca); mocniejsze ostrzeżenie przy wyłączeniu detektora, odwróceniu kierunku i wyłączeniu ochrony prądowej; blokada podwójnego dotknięcia; serwer odrzuca zmianę obrotu IMU przy załączonym sterowaniu
- [x] Przegląd kodu, paczka 4 (część 4: klawiatura): kafelki Status i karta GPS działają z klawiatury (Tab, Enter, spacja), widoczny fokus, okna dialogowe z pułapką fokusu, Esc i powrotem fokusu. **Odrzucone:** przyrostowy `/log` (C4) - bufor ma tylko 8 KB, zysk byłby znikomy. **Dalej odłożone:** `/status` bez `String` (C2) i ustawienia stałe poza `/status` (C3)

### Priorytet 2
- [x] Diagnostyka rozłączeń GPS w AgOpenWeb (08.10.2026: AgOpenWeb rozłączał moduł GPS, gdy przerwa w `$PANDA` przekraczała ok. 2 s; przy sprawdzeniu TX FAIL = 0 z 41 549, pętla bez zawieszek - komenda steer max odstęp 117 ms): trzy liczniki przerw (`diagnostics/gap_tracker.h`, 12 testów) w Diagnostyka -> Sieć: GGA z odbiornika, próby wysłania `$PANDA`, udane wysłania; log `[PANDA] przerwa ...` przy przerwie > 1 s. **Jeśli rozłączenie wróci:** odczytaj te trzy wiersze - przerwy w GGA = odbiornik/UART, w próbach = pętla, tylko w udanych = UDP/sieć, wszystkie w normie = sieć między modułem a AgOpenWeb (hotspot, uśpiony telefon)
- [x] GPS: konfiguracja odbiornika to teraz **nieblokująca maszyna stanów** (`sensors/gps_config_fsm.h`, 23 testy; `autoConfigure()` tylko ją uruchamia, kroki wykonuje `update()`): najpierw obserwacja strumienia NMEA, potem PQTMVERNO/PQTMVER, a **zapis ustawień i `PQTMSAVEPAR` tylko wtedy, gdy strumień nie zgadza się z zadanym** (GGA z docelową częstotliwością ±20 % oraz VTG, RMC, GST, GSA, GSV); dawniej zapis do flasha odbiornika przy każdym starcie. UBX-CFG-RATE idzie tylko do modułu, który nie odpowiedział jak Quectel. Panel: Diagnostyka -> GPS, wiersz „Konfiguracja GPS” (zapisano / bez zmian / inny moduł). **Do sprawdzenia na sprzęcie: (1) pierwszy start po wgraniu może zapisać ustawienia (log „the module will be configured”), kolejne starty mają pokazać „settings stay untouched” i brak PQTMSAVEPAR; (2) po wymianie/resecie odbiornika konfiguracja nadal się zapisuje; (3) przy opóźnionym starcie GPS pętla nie staje (panel CPU)**
- [x] Heading w `$PANDA` znormalizowany do 0-359,9 (`sensors/heading_math.h`, 23 testy; IMU dawał -180...180, GPS bywał 360,0; NaN daje 65535 = brak). Panel nadal pokazuje kurs IMU w -180...180
- [x] Wybór nadawcy AgOpenWeb (`connectivity/agio_senders.h`, 51 testów + 10 walidacji): moduł rozmawia z jednym nadawcą (pierwszy, od którego przyszła ramka), ramki drugiego (np. telefon z AgOpenWeb w tle) ignoruje; lista nadawców i ręczne przypięcie w Ustawienia -> Nadawcy AgOpenWeb (zapis w pamięci modułu), uwaga na Statusie. Przyczyna: drugi AgOpenWeb przełączał adres docelowy i oba dostawały ok. połowy ramek. **Do sprawdzenia na sprzęcie**
- [x] Ustawienia sterowania z AgOpenWeb (PGN 252: gainP/highPWM/minPWM...) zapisywane w pamięci modułu i przywracane po restarcie (`connectivity/steer_settings_store.h`, 24 testy; klucz NVS `steerCfg`): zapis tylko przy zmianie, po 3 s bez zmian, nigdy przy włączonym prowadzeniu; Odczyt i Kalibracja pokazują „z pamięci modułu”, dopóki AgOpenWeb nie wyśle nowych
- [x] Znak rollu: przełącznik „Odwróć znak rollu” w Kalibracja → IMU (`IMU_ROLL_INVERT`, działa od razu, w kopii ustawień); domyślnie roll idzie do `$PANDA` bez zmian
- [x] Domyślne flagi debug w `config.h` wyłączone (wersja produkcyjna). Wartość zapisana wcześniej w pamięci modułu (przełącznik w panelu) ma pierwszeństwo - po aktualizacji sprawdź Ustawienia -> Logowanie diagnostyczne i wyłącz to, czego nie potrzebujesz
- [x] Testy na hoście: `RtcmMessageStats` wydzielony do `ntrip/rtcm_stats.h` (patrz niżej: testy parsera RTCM); pozostałe parsery mają testy od wcześniej
- [x] Wolny RAM: monitor trendu (`diagnostics/heap_trend.h`, 11 testów): minimum co 5 min, regresja liniowa z ostatnich 2-6 h; wyciek = spadek ≥ 2 KB/h przy R² ≥ 0,7. `/status` (`heapTrend*`, `heapLeak`), Diagnostyka → System („Trend wolnego RAM”) i uwaga na Statusie. **Do sprawdzenia po kilku godzinach pracy:** wiersz pokazuje „stabilny”; spadek ≥ 2 KB/h zgłosi ostrzeżenie (wtedy dołącz wykres „Wolny RAM” z kilku godzin)

### Priorytet 3
- [x] NTRIP (`ntrip/ntrip_logic.h`, 52 testy): rosnące przerwy między próbami (5, 10, 20, 40, 80, max 120 s; seria zeruje się po strumieniu ≥ 30 s), zły login/hasło (401/403) czeka 120 s, potem 300 s - ochrona konta ASG-EUPOS przed blokadą; zły mountpoint (404 albo „SOURCETABLE” zamiast strumienia) 60 s, z czytelną przyczyną w logu i panelu; przekierowania 301/302/303/307/308 na `http://host[:port]` (do 3 skoków, https nieobsługiwane). Panel: Diagnostyka → NTRIP (niepowodzenia z rzędu, przerwa, przyczyna) i alarm na Statusie przy złych danych logowania. **Do sprawdzenia na sprzęcie**
- [x] `/status` budowany w kawałkach (`web/chunked_out.h`, 13 testów): odpowiedź (257 pól, ok. 6 KB) idzie porcjami po 1 KB przez `sendContent` (Transfer-Encoding: chunked) zamiast jednego `String` z `reserve(12288)`; koniec ciągłego bloku sterty przy każdym odpytaniu, a nowe pola nie zwiększają zużycia pamięci. **Do sprawdzenia na sprzęcie: panel ładuje się i odświeża normalnie (w razie kłopotów patrz minFreeHeap/maxAllocHeap w Diagnostyce)**
- [x] Testy parsera RTCM (`ntrip/rtcm_stats.h` wydzielony z `ntrip_client.h`, 19 testów z niezależnym CRC24Q i wektorem kontrolnym): parser liczy typy tylko dla ramek ze zgodną sumą CRC24Q, błędne idą do licznika (panel: Diagnostyka → NTRIP, „Ramki RTCM: poprawne / błędne CRC”; powyżej 5% uszkodzonych na czerwono = słaby zasięg albo zerwane TCP); odrzuca nagłówki z niezerowymi bitami zarezerwowanymi. **Do sprawdzenia na sprzęcie: przy zdrowym strumieniu błędnych ma być 0**
- [x] Drobne usprawnienia (09.10.2026): (1) restart, reset fabryczny, wgrywanie przez `/update` i przez Arduino IDE **odrzucane przy włączonym prowadzeniu** (409 z komunikatem w panelu; trwająca już aktualizacja IDE jest dokańczana) - dotąd jedno kliknięcie w czasie jazdy zatrzymywało moduł; (2) panel pod `http://AOG-ESP32.local` (mDNS z ArduinoOTA + ogłoszenie usługi HTTP; adres widać w Diagnostyka → WiFi) - **do sprawdzenia na telefonie w hotspocie**; (3) `FIRMWARE_VERSION` w `config.h` jako jedyne miejsce numeru wersji, w panelu i w logu startu; (4) `tools/run_tests.sh` - jedno polecenie na wszystkie testy + składnia panelu. **Do sprawdzenia na sprzęcie**
- [x] Przyczyna kickoutu (`kickoutCause` w `connectivity/kickout_signal.h`): wpis `[KICKOUT]` w dzienniku, `/status` (`kickoutCause`), Status, karta Gotowość i Diagnostyka podają, CO zatrzymało silnik (nadprąd, błąd czujnika prądu, brak próbek prądu, brak ruchu koła, odwrócony kierunek, zamrożony czujnik kąta). Brak próbek prądu w pierwszych 5 s po starcie nie wysyła już sygnału kickout do AgOpenWeb (silnik i tak stoi, bo ochrona w sterowniku działa niezależnie). **Do sprawdzenia: po starcie nie ma wpisu `[KICKOUT]`, jeśli czujnik prądu jest podłączony**
- [x] Kopia ustawień (Ustawienia → „Kopia ustawień”): eksport do pliku JSON i przywracanie z pliku, tylko w panelu. Bez haseł (WiFi, AP, NTRIP, PIN/hasło panelu), bez loginu NTRIP i bez kalibracji sprzętowej (ADC kąta, zero prądu). Import pokazuje, co się zmieni, i zapisuje zwykłymi endpointami `/settings_save_*` (ta sama walidacja i autoryzacja; odmowa przy włączonym prowadzeniu; grupy z błędnymi polami są pomijane). **Do sprawdzenia: pobierz kopię, zmień mountpoint NTRIP, wczytaj kopię - ma wrócić stary**
- [x] Czas z zegara GPS (`sensors/gps_clock.h`, RMC → UTC): `/status` ma `gpsUtc`; zdarzenia „Przed ostatnim restartem” pokazują godziny (lokalnie) i godzinę końca pracy, o ile odbiornik znał datę. Przeżywa restart razem z zapisem RTC. **Do sprawdzenia: karta „Przed ostatnim restartem” ma godziny po restarcie przy działającym GPS**
- [x] ArduinoOTA z Arduino IDE (port sieciowy `AOG-ESP32`): wgrywanie z IDE działa (log wgrania: 100% Done…). Napis „Not connected” przy porcie sieciowym to tylko etykieta IDE dla portów sieciowych, nie błąd. Wcześniejsze znikanie portu po chwili (09.10 rano) naprawił `ota/ota_keeper.h`; możliwa dodatkowa przyczyna po stronie PC to zapora Fedory (zwrotne połączenie TCP espota) albo mDNS po restarcie komputera
- [x] Trwały zapis zdarzeń przez restart (`diagnostics/persist_log.h`, pamięć RTC): ostatnie 32 zdarzenia (restart, fix, NTRIP, sieć, AgOpenWeb, prowadzenie, silnik) przeżywają restart programowy, watchdog, panikę i brownout, bez zużycia flasha. Zakładka Wykresy → karta „Przed ostatnim restartem” pokazuje, co działo się tuż przed restartem (czas przed restartem, numer uruchomienia). Nie przeżywa zaniku zasilania; pamięta tylko poprzednie uruchomienie (przy pętli restartów starsze znikają). **Do sprawdzenia: wywołaj restart z panelu i zobacz kartę; potem „Reset” przyciskiem/wyłączenie zasilania - karta ma się ukryć**
- [x] Karta „Gotowość do jazdy” na górze Statusu (`readiness()` w panelu): werdykt „Gotowy do jazdy” / „Gotowy, z uwagami (n)” / „Niegotowy (n)” i kafelki kontroli: sieć, AgOpenWeb, GPS (RTK FIX), poprawki RTK, czujnik kąta, IMU, silnik i jego ochrony; kafelek przenosi do odpowiedniej zakładki; wyłączone funkcje są wyszarzone, a IMU pomijane, gdy wyłączone. Tylko panel (bez zmian w firmware). Zrzuty sprawdzone na pulpicie i telefonie. **Do sprawdzenia: werdykt zgadza się z tym, co widzisz w AgOpenWeb (RTK FIX = zielony, brak RTCM = czerwony)**
- [x] ArduinoOTA/mDNS znikały po chwili od startu (port w IDE i `AOG-ESP32.local`, a panel pod IP działał): `ota/ota_keeper.h` ponownie uruchamia nasłuch OTA i ogłoszenie mDNS przy zmianie adresu IP i co 30 s (objaw: port w IDE był widoczny tylko tuż po starcie albo przy otwartym panelu - wpis mDNS wygasał w komputerze po ok. 2 min; nie w trakcie wgrywania ani przy włączonym prowadzeniu), wpis w dzienniku „[OTA-IDE] Ponowne ogłoszenie…”. **Do sprawdzenia: port sieciowy w IDE i `AOG-ESP32.local` trzymają się dłużej niż kilka minut; jeśli nadal znikają, wyślij fragment dziennika**
- [x] Odczyt „prowadzenie włączone” z AgOpenWeb wybierany w panelu (Ustawienia → Odczyt włączenia prowadzenia; `PgnFrame::engagedFromStatus`, testy): na zrzutach (09.10.2026) AgOpenWeb przy zielonej kierownicy wysyłał w bajcie stanu PGN 254 wartość `01`, a przy czerwonej `00`; moduł szukał bitu 0x04, więc nie uznawał prowadzenia za włączone (silnik by stał, blokada restartu nie działała). Źródła AgOpenWeb `develop` (`PgnBuilder.BuildAutoSteerPgn`, test `PgnBuilderEngageBitTests`) potwierdzają: bajt stanu to dokładnie 1 = steruj, 0 = nie (jak w AgOpenGPS i firmware AiO), więc domyślna maska to 0x01. Opcje: bit 0x01 (domyślnie), bit 0x04, oba. Zapis w pamięci modułu. Status ostrzega, gdy przychodzi `01` z kątem zadanym, a wybrane jest 0x04. **Do sprawdzenia: po wgraniu (maska domyślnie 0x01; jeśli w pamięci jest stara wartość 0x04, wybierz 0x01) włącz prowadzenie w AgOpenWeb - panel ma pokazać „WŁĄCZONE”, restart odmowę**
- [x] Ethernet: włączanie z panelu zamiast flagi kompilacji (kod ETH jest zawsze w buildzie; Ustawienia → Ethernet, po restarcie; `ENABLE_ETHERNET`, klucz NVS `enEth`)
- [x] `raiseTime`/`lowerTime` z PGN238: odbiór (`MachineConfigReceiver`, widoczne w Odczycie) i **impuls czasowy** wyjść podnośnika (`actuators/lift_pulse.h`, 8 testów): wyjście włączone przez `raiseTime` s po komendzie „podnieś”, od razu opada po jej zdjęciu. Od PGN 236 także `lowerTime` (osobne wyjście HydDown). **Do sprawdzenia z AgOpenWeb:** czy wysyła PGN 238 i w jakim układzie bajtów (przyjęty jak w module AgOpenGPS: raise, lower, enableToolLift, set0)

## 5b. Wielojęzyczność (panel PL+EN gotowy, firmware: etap 4) i wydanie publiczne na GitHubie (zaplanowane)

Decyzja z 09.10.2026: zapisane do późniejszego dodania. Kolejność: najpierw przygotowanie repo (B), potem wielojęzyczność (A) - albo odwrotnie, części są niezależne.

### A. Wybór języka panelu i łatwe dodawanie języków

Cel: użytkownik wybiera język w panelu (przełącznik w nagłówku i w Ustawieniach, zapamiętany w przeglądarce; opcjonalnie domyślny zapisany w module), a **nowy język dodaje się przez dodanie jednego pliku językowego** - bez ruszania kodu panelu ani firmware.

- [x] **Pliki językowe**: jeden plik na język (np. `lang/pl.json`, `lang/en.json`), klucz -> tekst; metadane w pliku (kod, nazwa własna języka, autor, wersja). Nowy język = skopiowanie `en.json`, przetłumaczenie wartości, wpisanie pliku do listy (najlepiej automatycznie: skrypt budujący wstawia wszystkie pliki z `lang/` do firmware)
- [x] **Klucze zamiast tekstów na sztywno** (panel i firmware zrobione: etapy 4a i 4b niżej; identyfikatory HTML też angielskie od 09.10.2026, stare zakładki/motywy zapamiętane w przeglądarce są mapowane): dziś polskie teksty są wpisane bezpośrednio w `web/web_ui.h` (JS: karty, etykiety, uwagi, dymki, opisy zdarzeń, okna logowania) oraz w firmware (komunikaty błędów walidacji w `settings_validation.h`, powody restartu, odpowiedzi serwera). Plan: w panelu funkcja `t('klucz', {n: 5})` z parametrami i liczbą mnogą; w firmware zwracać **kody** błędów, a tekst składać w przeglądarce (dziennik modułu `LogBuffer` może zostać po polsku/angielsku - to logi dla twórcy)
- [x] **Zawsze działa awaryjnie**: brak klucza w wybranym języku -> angielski -> klucz; brak pliku -> angielski. Język domyślny przy pierwszym uruchomieniu: z `navigator.language`, inaczej angielski
- [x] **Rozmiar**: `web_ui.h` ma ok. 150 KB; do przeglądarki wysyłać tylko wybrany język (osobny endpoint `/lang/xx.json`, pliki w PROGMEM/LittleFS, ewentualnie gzip), nie wszystkie naraz
- [~] **Szczegóły językowe** (UTF-8 i liczby mnogie są; format liczb i dat bez zmian): UTF-8 w całym łańcuchu (polskie znaki, inne alfabety), liczby mnogie (PL ma 3 formy), format liczb i dat (przecinek/kropka, 24 h), kierunek pisma RTL tylko jeśli ktoś doda taki język
- [x] **Test kontrolny na komputerze** (`tests/` albo `tools/`): każdy plik językowy ma komplet kluczy z `en.json`, zgodne parametry `{n}` i brak nieużywanych kluczy; błąd = lista brakujących tłumaczeń. Dzięki temu tłumacz od razu widzi, czego brakuje
- [x] **Dokumentacja dla tłumaczy**: krótka instrukcja w README ("jak dodać język w 5 krokach") i szablon pliku z komentarzem przy każdym kluczu, gdzie jest używany
- [x] **Etapy 1, 2, 3, 5 zrobione (09.10.2026), panel po polsku i angielsku**: ok. 840 tekstów panelu ma **neutralne klucze** (`TL('log_in_to_the_panel')`, `plN(n,'setting')`, `data-i18n="…"`), a teksty leżą w równorzędnych plikach `web/lang/lang_pl.h` i `lang_en.h` (żaden język nie jest kluczem, każdy może być pominięty poza zapasowym angielskim). Moduł serwuje wybrany język i angielski pod `/lang.js?l=…` (`web/lang/languages.h`), przełącznik w nagłówku (zapamiętany w przeglądarce, domyślnie język przeglądarki), liczby mnogie z formami `a|b|c`, kontrola `tools/i18n_check.py` (w `tools/run_tests.sh`; brak klucza w którymś języku, osierocone klucze, zmienne `{n}`, formy mnogie), test `tests/test_languages.cpp`, instrukcja `web/lang/README.md`. Wygląd po polsku bez zmian (porównanie 98 widoków przed i po w przeglądarce). **Do sprawdzenia na module: wygląd po angielsku na telefonie, czas ładowania katalogów (angielski + polski)**
- [x] **Etap 4a (09.10.2026): dziennik i powód restartu** tłumaczone w panelu bez zmian w firmware: komunikaty w kodzie firmware przepisane na angielski, szablony w `web/lang/logsrc.h` (172 komunikaty, klucze `log_<znacznik>_<nn>`), tłumaczenia (pl, en) w katalogach, dopasowanie w przeglądarce (`logTr` w `web_ui.h`); `tools/i18n_check.py` pilnuje, żeby każdy `LogBuffer::print*` miał wpis (`--add-log` dopisuje)
- [x] **Etap 4b (09.10.2026): komunikaty i błędy odsyłane przez moduł** (walidacja ustawień, odpowiedzi HTTP, powody błędów OTA i kalibracji prądu): w kodzie po angielsku w `MSG("...")`, tłumaczone w panelu kluczami `msg_*` (89 komunikatów); strona `/update` (OTA) tłumaczona jak panel

### B. Przygotowanie kodu do wydania publicznego na GitHubie

- [x] **Dwujęzyczne opisy w kodzie (PL + EN)** - **etap 2 zrobiony 09.10.2026:** komentarze w treści wszystkich plików firmware (`.h`/`.ino`) są po angielsku; zostają polskie bloki PL w nagłówkach oraz komentarze w `tests/` i `tools/`. **Etap 1 zrobiony 09.10.2026:** każdy plik ma na początku blok EN + PL (co robi, testy, kto woła, od czego zależy), nieaktualne opisy („nieprzetestowane”, „nie zamówiony”) poprawione; **zostaje** przetłumaczenie długich komentarzy w treści plików (poza nagłówkami są nadal po polsku) i opisy publicznych klas/funkcji. Pierwotny zakres:: na początku każdego pliku jednolity blok - najpierw angielski, potem polski (co robi plik, od czego zależy, kto go woła, ograniczenia); to samo dla publicznych klas i funkcji. Pozostałe komentarze w treści mogą zostać po polsku i być tłumaczone stopniowo. Skrypt kontrolny (`tools/check_headers.py`) sprawdza, że każdy `.h/.ino` ma oba bloki
- [x] **README dwujęzyczne**: `README.md` po angielsku (główne) i `README.pl.md` po polsku, z linkiem do drugiego; opis funkcji, zdjęcia/zrzuty panelu, schemat połączeń i lista części (BOM), instrukcja instalacji (Arduino IDE, wersja rdzenia ESP32, biblioteki z wersjami, ustawienia płytki ESP32-S3), pierwsze uruchomienie (`secrets.h`, panel, PIN/hasło) **Zrobione 09.10.2026:** `README.md` (EN) + `README.pl.md` (PL); zostały zrzuty panelu, schemat połączeń i BOM.
- [x] **Licencja**: wybrać i dodać `LICENSE` (decyzja właściciela: np. MIT, Apache-2.0 albo GPL-3.0 - GPL wymusza otwartość pochodnych); sprawdzić zgodność z licencjami użytych bibliotek i z protokołem AgOpenGPS/AgOpenWeb **Zrobione 09.10.2026: GPL-3.0** (`LICENSE`). Do sprawdzenia przed upublicznieniem: czy nie skopiowano cudzego kodu na innej licencji (np. z firmware AiO).
- [ ] **Higiena repo przed upublicznieniem**: decyzja z 09.10.2026: powstanie **NOWE, PRYWATNE repo** (bez starej historii). Repo zostaje prywatne; upublicznienie to osobna, późniejsza decyzja - wtedy jeszcze raz przejrzeć całość (`git filter-repo` niepotrzebne, jeśli nowa historia jest czysta); sprawdzić całą historię pod kątem haseł i kluczy (`secrets.h` jest poza gitem, ale mogły być wcześniej); przejrzeć `config.h`, README, ROADMAP, docs i testy pod kątem danych prywatnych (nazwy sieci, adresy IP, login ASG-EUPOS, nazwy mountpointów); `secrets.example.h` z opisem każdego pola
- [x] **Automatyczne testy**: GitHub Actions uruchamiające wszystkie `tests/test_*.cpp` (g++ -std=c++17 -Wall -Wextra) przy każdym pushu i pull requeście, plus kontrola panelu (`node --check`); znaczek statusu w README **Zrobione 09.10.2026:** `.github/workflows/tests.yml` (uruchamia `tools/run_tests.sh`); znaczek statusu w README po założeniu nowego repo.
- [x] **Pliki projektu**: `CHANGELOG.md` (wersje wg numeru w firmware), tagi i wydania (Releases) z gotowym plikiem `.bin`, `CONTRIBUTING.md` (jak zgłaszać błędy, styl kodu, jak uruchomić testy), `SECURITY.md` (HTTP bez szyfrowania, PIN/hasło chroni sieć, nie dostęp fizyczny; jak zgłaszać luki), szablony zgłoszeń (issue/PR), `docs/` (architektura, mapa pinów po angielsku, protokół PGN, opis panelu) **Zrobione 09.10.2026:** `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`; zostają tagi/Releases z `.bin`.
- [x] **Ostrzeżenie bezpieczeństwa w README**: urządzenie steruje silnikiem kierownicy maszyny rolniczej - wymagany E-stop sprzętowy, test na stanowisku przed jazdą, autor nie ponosi odpowiedzialności (to samo w LICENSE/README PL i EN) **Zrobione 09.10.2026:** `SAFETY.md` (PL+EN), link na górze README.
- [ ] **Porządek nazw** (README, docs, szablony i `Mapa pin.txt` → `docs/` zrobione 09.10.2026; zostaje nazwa nowego repo): spójna nazwa projektu (repo, `.ino`, README, opisy), numer wersji w jednym miejscu (`FIRMWARE_BUILD` + wersja semantyczna), usunięcie z repo plików roboczych

---

---

## 6. Test w obudowie i w polu

- [x] Montaż w obudowie (08.10.2026: moduł zamknięty w obudowie)
- [x] Kalibracja obrotu IMU w obudowie (panel: Kalibracja -> Punkt A / B) - działa (08.10.2026)
- [x] Na równym terenie **Roll: Zero Now** w AgOpenWeb (09.10.2026: działa; razem z testem dwóch AgOpenWeb naraz, RTK w terenie i kompilacją w Arduino IDE)
- [x] Watchdog domyślnie 5 s (było 25 s; wartość 25 s wzięła się z awarii sterownika W5500, który jest wyłączony). Jeśli w panelu zapisano wcześniej inną wartość, ma ona pierwszeństwo - ustaw 5 w Ustawienia -> Progi i czasy
- [ ] Silnik: pull-downy 10 kΩ na PWM/DIR, bezpiecznik na zasilaniu, E-stop sprzętowy
- [x] Test z prawdziwym fixem RTK pod niebem (08.10.2026: FLOAT po 28 s, FIX po 35 s od startu, hAcc mediana 0,9 cm, wiek poprawek 0,7-2,0 s, bez utraty FIX przez 6,5 min, także w ruchu do 4,5 km/h)
- [ ] Strojenie regulatora P, test sekcji i hydLift w polu

## 7. Stanowisko testowe silnika (zamiast JGB37-550)

- **Silnik:** dowolny szczotkowy 12 V z przekładnią (np. od wycieraczek, podnośnika szyby, wkrętarki)
- **Czujnik kąta:** potencjometr liniowy 10 kΩ (albo czujnik położenia przepustnicy) sprzężony z wyjściem przekładni; zasilany jak ADS1115
- **Kolejność testów:**
  1. odsprzęgnięty silnik: sprawdzić kierunek DIR - **panel: Kalibracja -> Silnik -> Test silnika (Lewo / Prawo)**: jeden impuls (PWM 10-150, 100-1000 ms), zatrzymywany przez moduł; pokazuje kąt, szczyt prądu i czy koło poszło w zadaną stronę
  2. znak sprzężenia: dodatni błąd ma **zbliżać** do celu (odwrócony = ucieczka). Detektor zatrzyma silnik w ułamku sekundy, a w panelu zaznaczasz "Odwróć kierunek silnika" (bez przekompilowania)
  3. ochrony: odłączyć czujnik (silnik musi stanąć), dojechać do skraju zakresu (silnik musi stanąć)
  4. prąd: ACS71240 w linii silnika, porównać z multimetrem (lista kontrolna czujnika: niżej)

---

## 7b. Hotspot telefonu (praca w polu)

1. Włącz hotspot w telefonie. W panelu (Ustawienia -> Sieć) wpisz jego nazwę i hasło w jednym ze slotów 2-4, zapisz i zrestartuj moduł. **Niczego więcej nie trzeba ustawiać**: moduł sam pobiera adres, bramę i maskę z telefonu (adresy hotspotu są losowe i zmienne).
2. Adres panelu zobaczysz w liście urządzeń hotspotu w telefonie albo w Diagnostyce -> WiFi (flaga "WiFi"). Awaryjny AP pod 192.168.4.1 działa jak dotąd.
3. Diagnostyka -> WiFi pokazuje faktyczny adres/maskę/bramę, czy adres jest z DHCP i dokąd moduł wysyła ramki: broadcast podsieci, dopóki nie dotrze pierwszy pakiet od AgOpenWeb, potem unicast (po 5 s ciszy znów broadcast).
4. Stały adres to opcja per sieć ("Wymuś stały adres IP"), tylko dla sieci bez DHCP; pola Adres IP/brama/maska w Ustawieniach dotyczą wtedy wyłącznie takiej sieci.
5. Telefon i urządzenie z AgOpenWeb muszą być w tej samej sieci hotspotu. Część telefonów włącza izolację klientów - wtedy urządzenia nie widzą się nawzajem; w razie braku łączności sprawdź to ustawienie hotspotu.

## 8. Czujnik prądu - lista kontrolna przy pierwszym podłączeniu

1. **Zasilanie 3,3 V** (nie 5 V - uszkodzi ADS1115). Wyjście czujnika do wejścia **A1** ADS1115.
2. Włącz **Ustawienia -> Funkcje -> Czujnik prądu**. Bez silnika i bez prądu karta Kalibracja -> Czujnik prądu ma pokazać stan **OK**, napięcie ok. **1,65 V** i prąd w okolicy 0 A. Stan "BŁĄD CZUJNIKA" (napięcie poza 0,15-3,15 V) oznacza niepodłączone/niezasilane wyjście.
3. **Zeruj** (silnik i prowadzenie wyłączone): po 1-2 s kalibracja "gotowa", prąd ok. 0,0 A.
4. Obciążenie o znanym prądzie (żarówka 12 V, silnik z amperomierzem): odczyt w panelu ma zgadzać się z multimetrem. Datasheet podaje dokładność typowo ±6%.
5. Dopiero potem ustaw progi (domyślnie 8 A / 400 ms / twardy 15 A) z zapasem względem prądu Twojego silnika. Silnik **stoi** przy nadprądzie, błędzie czujnika i braku danych z ADS. **Nie włączaj funkcji czujnika prądu, dopóki czujnik nie jest podłączony:** pływające wejście A1 daje stały, bezsensowny odczyt (np. 0,59 V = −24 A) i zatrzask nadprądowy.
6. Kickout: w AgOpenWeb włącz Konfiguracja AutoSteer → Czujnik prądu i ustaw próg 80-100%. Po zadziałaniu ochrony moduł wysyła PGN 250 = 255 i AgOpenWeb sam wyłącza prowadzenie (bez tego ustawienia tylko zatrzymujemy silnik - wyłącz prowadzenie ręcznie).

## Decyzje architektoniczne

- **Konwencja kąta w sterowaniu silnikiem jest jedna:** PWM dodatni = w stronę dodatniego kąta (PRAWO). Przełącznik "Odwróć kierunek" odwraca tylko fizyczny pin DIR, więc regulator i detektor ucieczki nie muszą nic wiedzieć o okablowaniu.
- **Detektor ucieczki ocenia związek "napęd -> zmiana kąta", nie sam błąd** - bo cel w AgOpenWeb się porusza i błąd może rosnąć legalnie. Domyślny próg oceny 3 deg (blisko celu regulator dreptuje).

- **`loop()` nie blokuje** - żadnych `delay()`/czekania w pętli (lekcja: ponawianie GPS zamroziło system na 7,3 s). Kroki mierzy panel CPU.
- **Diagnostyka oparta na czasie, nie na liczbie obiegów** - licznik "razy z rzędu" rósł z szybkością pętli i wprowadzał w błąd.
- **IMU: UART-RVC** - yaw względny (AgOpenWeb sam wylicza offset do GPS), roll/pitch bezwzględne, więc obrót płytki koryguje się programowo.
- **Własny klient NTRIP** - obejście błędu VRS w AgOpenWeb (PR #1225).
- **Kalibracja kąta lokalnie** - "Zero WAS" w AgOpenWeb jest zepsuty.
- **hydLift: osobne wyjścia podnoszenia i opuszczania** (HydUp / HydDown z Pin Config); wcześniejsze założenie „jedno wyjście” nie pasowało do protokołu AgOpenWeb.
- **Ethernet W5500 zostaje w kodzie jako opcja** (przełącznik w panelu, domyślnie wyłączony; `USE_ETHERNET` w `config.h` to tylko wartość fabryczna). RST modułu celowo niepodłączony - podłączony powodował błędy transmisji pod obciążeniem.
- **Środowisko builda w CI jest przypięte do tego, na którym projekt testowano w Arduino IDE** (rdzeń ESP32 3.3.11, wersje bibliotek, opcje płytki w `.github/workflows/release.yml`). Lekcja z 10.10.2026: plik zbudowany w CI na starszym rdzeniu zawieszał moduł (znikały ADS1115, MCP23017 i IMU, pętla stała), a ten sam kod z Arduino IDE działał. Przy zmianie rdzenia lub bibliotek najpierw build testowy i sprawdzenie na płytce.
