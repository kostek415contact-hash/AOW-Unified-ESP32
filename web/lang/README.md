# Języki panelu WWW

Teksty panelu (`web/ui/*.h`, sklejane w `web/web_ui.h`) nie są wpisane na sztywno w żadnym języku. W kodzie są **neutralne klucze**, a teksty
leżą w plikach językowych `web/lang/lang_<kod>.h` (teraz `lang_pl.h` i `lang_en.h`).

```js
TL('log_in_to_the_panel')                 // tekst w wybranym języku
TL('error_n', {n: 404})                   // z parametrem: "Error {n}" -> "Error 404"
plN(count, 'setting')                     // liczba mnoga: "setting|settings" (pl: trzy formy)
```
```html
<span data-i18n="status">Status</span>                  <!-- tekst elementu -->
<p data-i18n-html="each_switch_enables_logging">…</p>   <!-- tekst z <b>…</b> -->
<select aria-label="Theme" data-i18n-attr="aria-label:theme">   <!-- atrybut:klucz -->
```
Tekst w znaczniku HTML jest tylko wartością początkową (angielską); panel zamienia go na wybrany język.

Jeśli tekstu nie ma w wybranym języku, panel pokazuje **angielski**, a gdy i tam go brak, sam klucz. Język wybiera się
w nagłówku (zapamiętany w przeglądarce); bez wyboru panel bierze język przeglądarki (polski, inaczej angielski).
Moduł wysyła tylko wybrany język i angielski (`/lang.js?l=<kod>`); katalogi leżą w pamięci flash, nie w RAM.

## Dodanie tekstu w kodzie

1. Wymyśl klucz: małe litery, cyfry i `_` (np. `motor_stopped_overcurrent`), opisujący znaczenie, nie język.
2. Użyj go w `web/ui/*.h` (`TL('klucz')`).
3. Dopisz klucz do **każdego** pliku `lang_*.h` (zacznij od `lang_en.h`). `python3 tools/i18n_check.py --sync pl` dopisuje
   brakujące klucze z tekstem angielskim.
4. `python3 tools/i18n_check.py` (albo `tools/run_tests.sh`) wskaże brakujące, puste, osierocone i niezgodne wpisy.

## Dodanie języka (np. niemiecki, kod `de`)

1. Skopiuj `web/lang/lang_en.h` do `web/lang/lang_de.h`. W nim zmień `WEB_LANG_EN` na `WEB_LANG_DE` oraz
   `window.AOG_CAT.en=` na `window.AOG_CAT.de=`.
2. Przetłumacz **wartości** (po dwukropku). Klucze zostaw bez zmian.
3. W `web/lang/languages.h` dodaj `#include "lang_de.h"` i linijkę `if (strcmp(code, "de") == 0) return WEB_LANG_DE;`.
4. W `web/ui/head.h`, w skrypcie wyboru języka w `<head>`, dopisz `['de','Deutsch']` do listy `L`.
5. Uruchom `python3 tools/i18n_check.py`.

## Zasady tłumaczenia

- **Zmienne `{nazwa}` zostają bez zmian** (kolejność w zdaniu można zmienić).
- **Spacje na początku i na końcu** tekstu (np. `" • logged in"`) muszą zostać.
- **Liczba mnoga** to formy rozdzielone znakiem `|`: angielski `"setting|settings"`, polski
  `"ustawienie|ustawienia|ustawień"`. Języki z inną liczbą form wymagają reguły w `PLURAL` w `web/ui/js_core.h`
  i w `PLURAL_FORMS` w `tools/i18n_check.py`.
- **Znaczniki HTML** (`<b>…</b>`) zostają w tym samym miejscu.
- Nazwy własne i skróty (AgOpenWeb, NTRIP, RTK, PGN, ADC) zostają bez zmian.
- Plik musi być poprawnym JSON w UTF-8 (cudzysłowy `"`, bez przecinka po ostatnim wpisie).

## Dziennik (zakładka „Dziennik”) i powód restartu

Komunikaty w kodzie firmware są po **angielsku** (`LogBuffer::printf("[MOTOR] ...")`) - to tekst dla programisty, widoczny
też na Serialu. Panel pokazuje je w wybranym języku (polski, angielski i każdy dodany) przez klucze:

- `logsrc.h` trzyma szablony z kodu (`"log_motor_03": "[MOTOR] Runaway detector lock cleared from the panel."`) oraz słowa
  wstawiane jako `%s` (`"logarg_right": "RIGHT"`, powody odrzucenia NTRIP, powód restartu).
- Każdy katalog języka (także `lang_pl.h` i `lang_en.h`) ma te klucze z tłumaczeniem; kolejne argumenty printf to `{1}`, `{2}`...
  (w kolejności z kodu, można je przestawiać). Klucz: `log_<znacznik>_<nn>`.
- Linia, której nie da się dopasować (np. urwana przy 255 znakach), zostaje taka, jak wysłał firmware (po angielsku).

**Dopisałeś / zmieniłeś komunikat w kodzie?** Uruchom `python3 tools/i18n_check.py` - zgłosi brakujący wpis, potem
`python3 tools/i18n_check.py --add-log` dopisze szablon do `logsrc.h` i do wszystkich katalogów (tekst z kodu do przetłumaczenia);
stare wpisy po zmianie tekstu usuń ręcznie (kontrola o nich przypomni). Kontrola pilnuje też, żeby żaden szablon nie
przesłaniał innego.

## Komunikaty i błędy odsyłane przez moduł (`MSG`)

Błędy walidacji ustawień, odpowiedzi HTTP (400/403/409/429...), powody błędu OTA i kalibracji prądu są w kodzie po angielsku
i opakowane w `MSG("...")` (`web/msg.h`; makro nic nie robi, tylko oznacza komunikat dla narzędzia):

```cpp
if (!validPort(p)) return MSG("NTRIP port: a number from 1 to 65535");
server.send(409, "text/plain", MSG("The previous test is still running."));
```

Panel pokazuje taki tekst w wybranym języku tym samym mechanizmem co dziennik (klucze `msg_<obszar>_<nn>` w `logsrc.h`,
tłumaczenia w katalogach). Nowy `MSG(...)` w kodzie: `python3 tools/i18n_check.py --add-log`, potem tłumaczenie w
`lang_<kod>.h`. Komunikat z liczbą: `MSG("... %lu s")` i `snprintf` (tak jest w `sendLockedOut`).

Strona `/update` (OTA) ma własny, mały skrypt i klucze jak reszta panelu (`data-i18n`, `TL('...')`), język bierze z tego samego
wyboru co panel.

Poza tłumaczeniem zostają: komunikaty biblioteki `Update` (np. „Wrong Magic Byte”), nazwa chipsetu GPS i inne teksty
pochodzące z urządzeń zewnętrznych.
