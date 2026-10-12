# Safety / Bezpieczeństwo

**English** · [Polski](#polski)

## English

This firmware steers a vehicle. A fault, a wrong setting or a bug can make a machine turn unexpectedly and
**injure people or damage property**.

- **It is not a certified safety product.** It is a hobby / research project provided under the GPL-3.0 licence
  **without any warranty** (see [LICENSE](LICENSE), sections 15-16). You use it entirely at your own risk.
- **Always keep a way to take over immediately**: a physical steering-override or emergency stop that cuts the motor
  power independently of this software **plus a fuse in the motor supply**, and stay in the seat while steering is engaged.
- **Add pull-down resistors (about 10 kΩ) on the motor driver inputs (PWM and DIR).** While the chip resets or boots, its pins float; the firmware drives them LOW as early as it can, but only a resistor holds them during the reset itself.
- **Test with the motor disconnected or the wheels off the ground first.** Check the direction (left/right), the angle
  calibration and the limits before the first real run. Test at very low speed, in an open area, with nobody nearby.
- The built-in protections (over-current, current-sensor fault, runaway detector, kick-out to AgOpenWeb, refusal to
  restart while engaged, a dead-man timer that cuts the motor about 300 ms after the main loop hangs, and no driving after a restart until steering
  has been switched off and on again in AgOpenWeb) are **additional layers, not a substitute** for a hardware cut-off and an attentive operator. The over-current protection works only when the current sensor is enabled (Settings → Features); it is off by default.
- **Losing a sensor stops the motor, it never guesses.** If the angle sensor (ADS1115) stops answering, its reading goes stale within 250 ms and the motor is blocked;
  the current sensor shares that chip, so over-current protection also reports "no data" and blocks driving. The module probes the chip every 2 s and
  resumes by itself when it answers again, but driving then still needs steering to be switched off and on again if the module restarted.
- **A tripped protection stays tripped until the operator switches steering off.** The runaway and over-current latches are released only after
  AgOpenWeb has been seen with steering OFF for 1 s - a WiFi drop does not release them. The motor also stays blocked until both angle end
  points were saved at least once, and the panel refuses to change motor, current-protection, range, network or calibration settings and the Clear lock buttons while steering is engaged.
- Set a **PIN or password** for the panel and keep the access-point password private (it is random per device unless you set one); anyone on the
  same network can otherwise change settings that affect steering.
- Do not use on public roads. Follow the laws and safety rules that apply to your machine and location.
- After every change of firmware, settings, wiring or hardware, repeat the checks above.

If you find a safety problem, please report it as described in [SECURITY.md](SECURITY.md).

## Polski

To oprogramowanie steruje pojazdem. Usterka, błędne ustawienie lub błąd w kodzie mogą sprawić, że maszyna nieoczekiwanie
skręci i **zrani ludzi albo spowoduje szkody**.

- **To nie jest certyfikowany produkt bezpieczeństwa.** To projekt hobbystyczny / badawczy na licencji GPL-3.0,
  **bez gwarancji** (patrz [LICENSE](LICENSE), punkty 15-16). Używasz go wyłącznie na własną odpowiedzialność.
- **Zawsze miej sposób na natychmiastowe przejęcie kontroli**: fizyczne wyłączenie sterowania lub wyłącznik awaryjny,
  który odcina zasilanie silnika niezależnie od tego oprogramowania, **oraz bezpiecznik w zasilaniu silnika**, i nie opuszczaj fotela przy włączonym prowadzeniu.
- **Dodaj rezystory pull-down (ok. 10 kΩ) na wejściach sterownika silnika (PWM i DIR).** Podczas resetu i startu układu piny pływają; firmware ustawia je na LOW najwcześniej jak się da, ale w trakcie samego resetu trzyma je tylko rezystor.
- **Najpierw testuj z odłączonym silnikiem albo z kołami uniesionymi nad ziemią.** Przed pierwszą jazdą sprawdź kierunek
  (lewo/prawo), kalibrację kąta i ograniczenia. Pierwsze próby: bardzo mała prędkość, otwarty teren, nikogo w pobliżu.
- Wbudowane zabezpieczenia (nadprąd, błąd czujnika prądu, detektor ucieczki, kickout do AgOpenWeb, odmowa restartu przy
  włączonym prowadzeniu, zegar bezpieczeństwa odcinający silnik ok. 300 ms po zawieszeniu głównej pętli oraz brak jazdy po restarcie, dopóki
  sterowanie nie zostanie wyłączone i włączone ponownie w AgOpenWeb) to **dodatkowe warstwy, a nie zamiennik** sprzętowego odcięcia i uważnego operatora. Ochrona nadprądowa działa tylko przy włączonym czujniku prądu (Ustawienia → Funkcje); domyślnie jest wyłączony.
- **Utrata czujnika zatrzymuje silnik, nigdy nie zgaduje.** Jeśli czujnik kąta (ADS1115) przestanie odpowiadać, jego odczyt staje się nieświeży w ciągu 250 ms i silnik jest blokowany;
  czujnik prądu korzysta z tego samego układu, więc ochrona nadprądowa także zgłasza „brak danych” i blokuje jazdę. Moduł co 2 s sprawdza układ i wznawia pracę sam,
  gdy znowu odpowie, ale jeśli moduł się zrestartował, jazda nadal wymaga wyłączenia i ponownego włączenia sterowania.
- **Zadziałana ochrona pozostaje zadziałana, dopóki operator nie wyłączy sterowania.** Zatrzaski ucieczki i nadprądu schodzą dopiero, gdy AgOpenWeb
  przez 1 s pokazuje sterowanie WYŁĄCZONE - zanik WiFi ich nie zwalnia. Silnik jest też zablokowany, dopóki oba punkty kąta nie zostały zapisane
  przynajmniej raz, a panel odmawia zmiany ustawień silnika, ochrony prądowej, zakresu, sieci i kalibracji oraz przycisków Skasuj blokadę przy włączonym prowadzeniu.
- Ustaw **PIN lub hasło** panelu i nie ujawniaj hasła punktu dostępowego (jest losowe dla każdego urządzenia, chyba że ustawisz własne); inaczej każdy w tej samej sieci
  może zmienić ustawienia wpływające na sterowanie.
- Nie używaj na drogach publicznych. Przestrzegaj przepisów i zasad bezpieczeństwa obowiązujących dla twojej maszyny i miejsca.
- Po każdej zmianie oprogramowania, ustawień, okablowania lub sprzętu powtórz powyższe kontrole.

Znalazłeś problem bezpieczeństwa? Zgłoś go zgodnie z [SECURITY.md](SECURITY.md).
