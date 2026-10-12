# Lista kontrolna pierwszego uruchomienia

[English](../en/first-start.md) · [Pierwsze uruchomienie](getting-started.md) · [Okablowanie](wiring.md) · [Rozwiązywanie problemów](troubleshooting.md) · [Powrót do README](../../README.pl.md)

Rób kroki po kolei i nie przeskakuj: każdy zakłada, że poprzedni jest w porządku. Kroki 1-3 wykonuj z **odłączonym** silnikiem i hydrauliką. W krokach 4-5 silnik może być podłączony do sterownika, ale musi być mechanicznie wolny (odłączony od kierownicy albo koła uniesione), a wyłącznik awaryjny pod ręką.

## 1. Przed zasilaniem

- Rezystory podciągające do masy na wejściach PWM i DIR sterownika silnika, żeby nie wisiały w powietrzu podczas startu modułu (patrz [Okablowanie](wiring.md)).
- Bezpiecznik w zasilaniu silnika i wyłącznik awaryjny odcinający zasilanie silnika.
- Silnik, zawory hydrauliki i płytka przekaźników nadal odłączone.

## 2. Zasilanie i sieć

- Kompilacja przeszła w GitHub Actions, firmware jest wgrany.
- Panel się otwiera; **Diagnostyka** pokazuje połączone WiFi (lub Ethernet) i rozmowę z AgOpenWeb.
- Ustaw PIN lub hasło panelu w **Ustawieniach**, zanim moduł trafi do wspólnej sieci.

## 3. Czujniki

- GPS: jakość fixa i liczba satelitów wyglądają dobrze w **Odczycie**; poprawki RTK docierają do odbiornika.
- IMU i czujnik kąta pokazują sensowne wartości; obrót kierownicy zmienia kąt we właściwą stronę.
- Kalibracja: zapisz ponownie punkty LEWO i PRAWO po każdej zmianie montażu czujnika.

## 4. Łańcuch bezpieczeństwa na stole

- W **Ustawienia → Funkcje** włącz **Sterownik silnika** i **Czujnik prądu** i zapisz (domyślnie oba są wyłączone; bez czujnika prądu nie ma ochrony nadprądowej). Potem przy wyłączonym prowadzeniu naciśnij **Zeruj** i ustaw progi prądu w **Kalibracji**.
- Włącz sterowanie w AgOpenWeb: silnik ma stać, dopóki po restarcie nie zobaczy wyłączonego sterowania (ekran AgOpenWeb o tym informuje).
- Test silnika w **Kalibracji** przy silniku mechanicznie odłączonym od kierownicy: sprawdź kierunek; jeśli odwrotny, użyj *Invert motor* w AgOpenWeb (albo zaznacz *Odwróć kierunek silnika (pin DIR)* w Kalibracji; zapisz przy wyłączonym prowadzeniu).
- Odepnij przewód czujnika kąta przy włączonym sterowaniu: silnik ma stanąć, a ekran AgOpenWeb pokazuje ostrzeżenie.

## 5. Wyjścia maszyny

- Ustaw funkcje pinów (sekcje, HydUp / HydDown, tramlines, geo-stop) i *Invert relay* w AgOpenWeb; **Diagnostyka** pokazuje mapę pinów i włączone wyjścia.
- Sprawdź każde wyjście lampką lub miernikiem, zanim podłączysz zawór albo płytkę przekaźników.

## 6. Pierwsza jazda

- Otwarty teren, bardzo mała prędkość, jedna osoba przy wyłączniku awaryjnym.
- Zacznij od niskiego wzmocnienia P i podnoś małymi krokami; porównuj zadany kąt z rzeczywistym na **Wykresach**.
- Sekcje i podnośnik testuj w polu dopiero wtedy, gdy sterowanie zachowuje się poprawnie.
