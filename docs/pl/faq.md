# FAQ

[English](../en/faq.md) · [Rozwiązywanie problemów](troubleshooting.md) · [Instrukcja](user-guide.md) · [Powrót do README](../../README.pl.md)

Krótkie odpowiedzi na najczęstsze pytania. Objawy i naprawy są w [Rozwiązywaniu problemów](troubleshooting.md).

### Czy działa przez hotspot z telefonu?

Tak. Wpisz nazwę i hasło hotspotu w Ustawienia → Sieć; moduł bierze adres, maskę i bramę z DHCP, więc zmienna podsieć
hotspotu nie jest problemem. W hotspocie nie ma użytecznego broadcastu, więc ramki trafiają do AgOpenWeb unicastem - użyj
[forka AgOpenWeb z poprawionym unicastem](https://github.com/kostek415contact-hash/AgOpenWeb). Na zwykłym routerze wystarczy
standardowy AgOpenWeb.

### Czy potrzebny jest internet?

Nie. Panel serwuje sam moduł, po polsku i angielsku, bez aplikacji i bez chmury. Internet jest potrzebny tylko do poprawek
RTK (NTRIP) - i te mogą przychodzić przez AgOpenWeb zamiast własnego klienta modułu.

### Status pokazuje, że NTRIP jest wyłączony, a poprawki mam. Dlaczego?

Gdy własny klient NTRIP modułu jest wyłączony, a poprawki są przekazywane z AgOpenWeb (surowy RTCM na porcie 2233), Status
pokazuje „przekaz z AgOpenWeb”, a znaczek mówi, że poprawki płyną przez AgOpenWeb. To normalna konfiguracja.

### Włączyłem sterowanie w AgOpenWeb, a silnik po restarcie się nie rusza

Tak ma być. Po każdym restarcie silnik stoi, dopóki nie zobaczy przynajmniej raz **wyłączonego** sterowania w AgOpenWeb:
wyłącz je i włącz ponownie. Dzięki temu maszyna nie zaczyna sama skręcać po awarii albo zaniku zasilania. Stan widać w panelu
w Diagnostyka → Silnik („Sterowanie uzbrojone”).

### Czy po każdym restarcie trzeba wysyłać ustawienia steer z AgOpenWeb?

Nie. Wartości od AgOpenWeb (wzmocnienie, limity PWM, impulsy na stopień, ...) moduł zapamiętuje i przywraca przy starcie.
Po aktualizacji ze starszego firmware wyślij je raz z AgOpenWeb. Nigdy nie są zapisywane przy włączonym sterowaniu.

### Które ustawienia z AgOpenWeb moduł naprawdę wykorzystuje?

Tylko część. Reszta zostaje w AgOpenWeb.

| Ekran w AgOpenWeb | Ustawienie | Czy moduł używa? |
|---|---|---|
| Auto Steer | Wzmocnienie P, maksymalny i minimalny PWM | **Tak.** Bez nich silnik nie dostaje PWM. Moduł pamięta je po restarcie. |
| Auto Steer | WAS Offset, Counts Per Degree, Ackermann | Odbierane i zapisywane w logu, **nieużywane**. Kąt pochodzi z kalibracji lewo/prawo w panelu (zakładka Kalibracja). |
| Auto Steer / kreator Steer | Invert motor | **Tak.** Odwraca kierunek silnika razem z ustawieniem w panelu (Kalibracja → Silnik pokazuje wynik). Działa tylko przy wyłączonym sterowaniu. |
| Auto Steer / kreator Steer | Invert WAS, typ sterownika, inne opcje sprzętu | Odbierane i pokazywane (Diagnostyka). Znak kąta wynika z kalibracji lewo/prawo w panelu; sterownik to Cytron (PWM + DIR). |
| Auto Steer | Zero WAS, SmartWAS, kreator counts per degree | Nie używaj: zakładają, że moduł stosuje offset i counts per degree do surowego odczytu, a ten moduł kalibruje lokalnie, więc kąt się nie zmienia, a offset rośnie bez końca. Kalibruj w panelu. |
| Auto Steer | Max Steer Angle | Zostaje w AgOpenWeb, nie jest wysyłany do modułu. |
| Tool, Switches | Work switch / Steer switch, active low lub high | Tylko AgOpenWeb: decyduje, jak odczytać stan zgłaszany przez moduł. Użyj, jeśli przełącznik działa odwrotnie. |
| Machine, Module | Raise time, Lower time | **Tak**: po każdym poleceniu HydUp jest włączone przez czas podnoszenia, HydDown przez czas opuszczania. Przy czasie 0 wyjście podąża za poleceniem. |
| Machine, Module | Enable | Enable wyłączone oznacza brak automatycznych poleceń hydrauliki z AgOpenWeb; ręczne Podnieś / Opuść w panelu nadal działają, o ile HydUp / HydDown jest przypisane do pinu. |
| Machine, Module | Invert relay | **Tak.** Płytka przekaźników jest aktywna stanem niskim: wszystkie 16 wyjść jest odwróconych i spoczynkowo ma stan wysoki. Zapamiętywane w module. |
| Machine, Module | User 1-4 | Odbierane i pokazywane (Diagnostyka). Nic na płytce ich nie używa. |
| Machine, Module | Look ahead | Zostaje w AgOpenWeb (decyduje, kiedy wysłać polecenie podnośnika). |
| Machine, Pin Config | Przypisanie pinów 1-16 | **Tak.** Każde z 16 wyjść może mieć funkcję Sec1-16, HydUp, HydDown, TramLeft, TramRight albo GeoStop. Zapamiętywane w module; widoczne w Diagnostyce. Piny 17-24 nie mają sprzętu. Zanim AgOpenWeb coś wyśle: Sec1-16 na pinach 1-16 (piny MCP23017 0-15), bez wyjść podnośnika. |

### Dlaczego wykresy są puste po restarcie?

Historia leży w PSRAM modułu i po restarcie zaczyna się od nowa. Wymaga włączonego OPI PSRAM przy kompilacji; bez niego panel
informuje, że moduł nie ma pamięci na historię, i rysuje tylko dane na żywo.

### Czy mogę używać bez silnika, tylko do GPS i IMU?

Tak. Każda funkcja (silnik, sekcje, czujnik prądu, IMU, LED) to przełącznik w Ustawienia → Funkcje, a Ethernet
ma własny w Ustawienia → Ethernet. Zostaw wyłączone to, czego nie podłączyłeś.

### Które porty sieciowe muszą być otwarte?

UDP 9999 (moduł do AgOpenWeb), 8888 i 5126 (do modułu), 2233 (surowy RTCM do modułu) i TCP 80 dla panelu. Zobacz tabelę
w [Pinach i adresach](pinout.md).

### Czy zrzuty ekranu są prawdziwe?

Nie. Wszystkie zrzuty ekranu i schemat połączeń w tym repozytorium zostały wygenerowane przez AI z danych demo, nie z
prawdziwej maszyny. Sam panel wygląda właśnie tak.

### Czy można tego bezpiecznie używać na maszynie?

To projekt hobbystyczny, bez gwarancji i nie jest certyfikowanym produktem bezpieczeństwa. Przed pierwszym uruchomieniem
przeczytaj [SAFETY.md](../../SAFETY.md): zachowaj sprzętowy sposób przejęcia kontroli, dodaj rezystory pull-down na wejściach
sterownika silnika i testuj z kołami uniesionymi nad ziemią.
