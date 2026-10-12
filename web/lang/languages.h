#pragma once
#include <string.h>
#include "lang_pl.h"
#include "lang_en.h"
#include "logsrc.h"

// Katalogi tłumaczeń panelu WWW. Każdy język to plik web/lang/lang_<kod>.h (obiekt JSON "klucz": "tekst");
// tu wystarczy dopisać jedną linijkę. Opis dla tłumaczy i programistów: web/lang/README.md.
//
// Panel pobiera katalog wybranego języka oraz angielski (zapasowy, gdy w wybranym czegoś brakuje) z
// /lang.js?l=<kod> (patrz WebServerModule::handleLang). Katalogi leżą w pamięci flash, nie w RAM.
static inline const char* webLangCatalog(const char* code) {
  if (!code) return nullptr;
  if (strcmp(code, "pl") == 0) return WEB_LANG_PL;
  if (strcmp(code, "en") == 0) return WEB_LANG_EN;
  if (strcmp(code, "logsrc") == 0) return WEB_LANG_LOGSRC;   // oryginalne szablony dziennika (patrz tools/i18n_check.py)
  return nullptr;   // nieznany język -> 404, panel pokaże angielski
}
