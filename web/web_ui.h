#pragma once
// ======================================================================
// web/web_ui.h
// EN: The web panel page (HTML + CSS + JS in PROGMEM): tabs, live charts, diagnostics, settings, log, i18n
//     engine (TL / plN / logTr).
// PL: Strona panelu WWW (HTML + CSS + JS w PROGMEM): zakładki, wykresy na żywo, diagnostyka, ustawienia,
//     dziennik, silnik tłumaczeń (TL / plN / logTr).
// Used by: web/web_server.h
// ======================================================================
#include <pgmspace.h>

// The web panel page (HTML + CSS + JS in one file). Served from flash
// memory (PROGMEM) by WebServerModule::handleRoot() - it takes no RAM.
//
// The page itself polls /status (data), /log (journal) and sends settings
// with POST to the existing endpoints - see web/web_server.h.
// Themes: dark / light / blue (remembered in the browser).
static const char WEB_UI_HTML[] PROGMEM =
#include "ui/head.h"
#include "ui/style.h"
#include "ui/body.h"
#include "ui/js_core.h"
#include "ui/js_defs.h"
#include "ui/js_build.h"
#include "ui/js_backup.h"
#include "ui/js_charts.h"
#include "ui/js_run.h"
;
