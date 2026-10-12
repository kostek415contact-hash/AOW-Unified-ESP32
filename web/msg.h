#pragma once
// MSG("text") marks a message that the module sends back to the panel (validation error, HTTP response, error reason) - it does NOT translate it itself.
// The text in code is in English; the panel shows it in the selected language using the msg_* keys (web/lang/logsrc.h + lang_<code>.h).
// The marker lets tools/i18n_check.py find the message and require a translation. Description: web/lang/README.md.
#define MSG(s) (s)
