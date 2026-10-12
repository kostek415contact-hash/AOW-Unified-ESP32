#!/usr/bin/env bash
# Uruchamia WSZYSTKIE testy na komputerze (tests/test_*.cpp) oraz kontrolę składni skryptu panelu.
# Użycie (z katalogu głównego repo):  tools/run_tests.sh
# Kod wyjścia 0 = wszystko przeszło, 1 = coś nie przeszło (nadaje się do CI).
#
# Testy kompiluje z symulacją makr, które rdzeń ESP32/Arduino definiuje globalnie i które już raz psuły
# kompilację w Arduino IDE (HEX, DEC, BIN, OCT, LOW, HIGH, R_OK, W_OK, X_OK, F_OK, INPUT, OUTPUT):
# identyfikator o takiej nazwie w kodzie kończy się tu błędem, a nie dopiero w IDE.
set -u
cd "$(dirname "$0")/.."
MACROS="-DHEX=16 -DDEC=10 -DBIN=2 -DOCT=8 -DLOW=0 -DHIGH=1 -DR_OK=4 -DW_OK=2 -DX_OK=1 -DF_OK=0 -Itests/mocks"
# SANITIZE=1 tools/run_tests.sh  -> every test is built with AddressSanitizer + UndefinedBehaviorSanitizer (over-reads, overflows, UB)
if [ "${SANITIZE:-0}" = "1" ]; then MACROS="$MACROS -fsanitize=address,undefined -fno-sanitize-recover=undefined -g"; echo "(sanitizer build)"; fi
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
fail=0; total=0; checks=0

for t in tests/test_*.cpp; do
  n="$(basename "$t" .cpp)"; total=$((total+1))
  if ! g++ -std=c++17 -Wall -Wextra $MACROS "$t" -o "$TMP/$n" 2>"$TMP/$n.err"; then
    echo "BŁĄD KOMPILACJI: $t"; head -5 "$TMP/$n.err"; fail=1; continue
  fi
  line="$("$TMP/$n" | tail -1)"
  case "$line" in
    "WYNIK: "*" kontroli OK") ok="${line#WYNIK: }"; ok="${ok%% *}"
        if [ "${ok%%/*}" != "${ok##*/}" ]; then echo "  BŁĄD $n: nie wszystkie kontrole ($ok)"; "$TMP/$n" | grep BLAD | head -5; fail=1; continue; fi
        checks=$((checks+${ok%%/*}))
        printf '  ok   %-28s %s\n' "$n" "$ok" ;;
    *) echo "  BŁĄD $n: $line"; "$TMP/$n" | grep BLAD | head -5; fail=1 ;;
  esac
done

# skrypt panelu (JS wyciągnięty ze stron panelu: web/web_ui.h + web/ui/*.h) - sama składnia
if command -v node >/dev/null && command -v python3 >/dev/null; then
  python3 - "$TMP" <<'PY'
import re, sys
sys.path.insert(0, 'tools')
from panel_src import panel_html
js = "\n".join(re.findall(r'<script[^>]*>(.*?)</script>', panel_html(), re.S))
open(sys.argv[1] + '/panel.js', 'w', encoding='utf-8').write(js)
PY
  if node --check "$TMP/panel.js" 2>"$TMP/js.err"; then echo "  ok   panel (skrypt JS)           składnia"; else echo "  BŁĄD składni skryptu panelu:"; head -5 "$TMP/js.err"; fail=1; fi
else
  echo "  (pominięto kontrolę panelu: brak node lub python3)"
fi

# tlumaczenia panelu: kazdy tekst z web_ui.h ma wpis w kazdym web/lang/lang_*.h (i odwrotnie), zmienne {..} zgodne
if command -v python3 >/dev/null; then
  if python3 tools/i18n_check.py >"$TMP/i18n.out" 2>&1; then tail -1 "$TMP/i18n.out" | sed 's/^/  /'; else echo "  BŁĄD tłumaczeń panelu:"; head -20 "$TMP/i18n.out"; fail=1; fi
fi

# dokumentacja: linki, pary EN/PL, zgodnosc pinow i portow z config.h
if command -v python3 >/dev/null; then
  if python3 tools/docs_check.py >"$TMP/docs.out" 2>&1; then sed 's/^/  /' "$TMP/docs.out"; else echo "  BŁĄD dokumentacji:"; head -20 "$TMP/docs.out"; fail=1; fi
fi

# panel w prawdziwej przegladarce (Chromium przez Playwright): karty, okno potwierdzenia, bledy JS, nieprzetlumaczone klucze.
# Bez Playwrighta/Chromium lokalnie jest pomijany; w CI (PANEL_SMOKE_REQUIRE=1) brak przegladarki to blad.
if command -v python3 >/dev/null; then
  req=""; [ "${PANEL_SMOKE_REQUIRE:-}" = "1" ] && req="--require"
  python3 tools/panel_smoke.py $req >"$TMP/smoke.out" 2>&1; rc=$?
  if [ $rc -eq 0 ]; then cat "$TMP/smoke.out"; elif [ $rc -eq 2 ]; then cat "$TMP/smoke.out"; else cat "$TMP/smoke.out"; fail=1; fi
fi

echo "----"
if [ $fail -eq 0 ]; then echo "WSZYSTKO OK: $total zestawów, $checks kontroli"; else echo "SĄ BŁĘDY (patrz wyżej)"; fi
exit $fail
