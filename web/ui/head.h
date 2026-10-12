// ======================================================================
// web/ui/head.h
// EN: Page head: theme and language bootstrap scripts. Part of the panel page (web/web_ui.h joins the parts in order; they are one string in flash).
// PL: Początek strony: skrypty motywu i wyboru języka. Część strony panelu (web/web_ui.h skleja części po kolei; w pamięci flash to jeden napis).
// ======================================================================
R"HTML(<!DOCTYPE html>
<html lang="pl" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>AOG ESP32</title>
<script>
(function(){var t='dark';try{t=localStorage.getItem('theme')||'dark';}catch(e){}t=({ciemny:'dark',jasny:'light',niebieski:'blue'})[t]||t;   /* saved by an older version */
document.documentElement.setAttribute('data-theme',t);
try{if(localStorage.getItem('aog.field')==='1')document.documentElement.classList.add('fm');}catch(e){}})();
</script>
<script>
/* panel language: the remembered choice, and without it the browser language (Polish, otherwise English). Translation catalogs (the selected one + English as a fallback, plus the log templates) are added synchronously from the module */
(function(){var L=[['pl','Polski'],['en','English']],l=null;try{l=localStorage.getItem('lang');}catch(e){}
if(!L.some(function(x){return x[0]===l;}))l=/^pl/i.test(navigator.language||'')?'pl':'en';
window.AOG_LANGS=L;document.documentElement.lang=l;
document.write('<script src="/lang.js?l=en"><\/script>');
if(l!=='en')document.write('<script src="/lang.js?l='+l+'"><\/script>');
document.write('<script src="/lang.js?l=logsrc"><\/script>');})();   /* log message templates from the code (English) - translated through the log_* keys */
</script>
)HTML"
