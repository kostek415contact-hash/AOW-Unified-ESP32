// ======================================================================
// web/ui/body.h
// EN: Page markup: top bar, tabs, containers. Part of the panel page (web/web_ui.h joins the parts in order; they are one string in flash).
// PL: Znaczniki strony: pasek górny, zakładki, kontenery. Część strony panelu (web/web_ui.h skleja części po kolei; w pamięci flash to jeden napis).
// ======================================================================
R"HTML(</head>
<body>
<svg width="0" height="0" style="position:absolute" aria-hidden="true"><defs>
<symbol id="i-status" viewBox="0 0 24 24"><rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/></symbol>
<symbol id="i-readings" viewBox="0 0 24 24"><path d="M3 12h4l2.5-7 5 14 2.5-7H21"/></symbol>
<symbol id="i-diag" viewBox="0 0 24 24"><circle cx="10.5" cy="10.5" r="6.5"/><path d="M15.5 15.5L21 21"/></symbol>
<symbol id="i-calibration" viewBox="0 0 24 24"><path d="M4 7h9M17 7h3M4 17h3M11 17h9"/><circle cx="15" cy="7" r="2"/><circle cx="9" cy="17" r="2"/></symbol>
<symbol id="i-settings" viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M4.9 19.1L7 17M17 7l2.1-2.1"/></symbol>
<symbol id="i-log" viewBox="0 0 24 24"><path d="M4 6h16M4 12h16M4 18h10"/></symbol>
<symbol id="i-charts" viewBox="0 0 24 24"><path d="M3 3v18h18"/><path d="M7 15l4-5 3 3 5-7"/></symbol>
</defs></svg>

<header class="top">
  <div class="brand"><span class="dot" id="mainDot"></span><h1>AOG ESP32</h1><span class="chip" id="netChip">—</span></div>
  <div class="top-actions">
    <button id="authBtn" class="auth-btn" type="button" hidden></button>
    <button id="fieldBtn" class="auth-btn fbtn" type="button" aria-pressed="false">A+</button>
    <select id="langSelect" aria-label="Language" data-i18n-attr="aria-label:language"></select>
    <select id="themeSelect" aria-label="Theme" data-i18n-attr="aria-label:theme"><option value="dark" data-i18n="dark">Dark</option><option value="light" data-i18n="light">Light</option><option value="blue" data-i18n="blue">Blue</option></select>
  </div>
  <button type="button" class="rstrip" id="rstrip" hidden><span class="rv"></span><span class="rs"></span></button>
</header>
<div class="banner bad" id="offline" hidden data-i18n="no_connection_to_the_module_retrying">No connection to the module — retrying…</div>

<nav class="tabs" id="nav">
  <button data-tab="status"><svg class="ico"><use href="#i-status"/></svg><span data-i18n="status">Status</span></button>
  <button data-tab="readings"><svg class="ico"><use href="#i-readings"/></svg><span data-i18n="readings">Readings</span></button>
  <button data-tab="charts"><svg class="ico"><use href="#i-charts"/></svg><span data-i18n="charts">Charts</span></button>
  <button data-tab="diag"><svg class="ico"><use href="#i-diag"/></svg><span data-i18n="diagnostics">Diagnostics</span></button>
  <button data-tab="calibration"><svg class="ico"><use href="#i-calibration"/></svg><span data-i18n="calibration">Calibration</span></button>
  <button data-tab="settings"><svg class="ico"><use href="#i-settings"/></svg><span data-i18n="settings">Settings</span></button>
  <button data-tab="log"><svg class="ico"><use href="#i-log"/></svg><span data-i18n="log">Log</span></button>
</nav>

<main>
  <section class="tab" id="tab-status">
    <div class="ready" id="ready"></div>
    <div class="hero" id="hero" data-go="readings" tabindex="0" role="link"><canvas id="heroSpark" aria-hidden="true"></canvas><div class="hl" data-i18n="gps_position">GPS position</div><div class="hv" id="heroV">—</div><div class="hs" id="heroS" data-i18n="loading">loading…</div></div>
    <div class="attn" id="attn"></div>
    <div class="grid tiles" id="tiles"></div>
  </section>

  <section class="tab" id="tab-readings">
    <div class="grid" id="readGrid"></div>
  </section>

  <section class="tab" id="tab-charts">
    <div class="logbar" id="chBar"></div>
    <div class="grid tiles chsum" id="chSum"></div>
    <div id="chGrid"></div>
    <article class="card evcard" id="evCard" hidden><h3 data-i18n="events_vertical_lines_on_the_charts">Events (vertical lines on the charts)</h3><div id="evList"></div></article>
    <article class="card evcard" id="prevCard" hidden><h3 id="prevTitle" data-i18n="before_the_last_restart">Before the last restart</h3><div id="prevList"></div><p class="note" id="prevNote" style="margin:8px 0 0"></p></article>
    <p class="note" id="chNote" style="margin:12px 2px 0" data-i18n="loading_history_from_the_module">Loading history from the module…</p>
  </section>

  <section class="tab" id="tab-diag">
    <p class="note" style="margin:0 2px 12px" data-i18n-html="each_switch_enables_logging_of_the_given">Each switch enables logging of the given subsystem <b>and</b> shows all its data here live.</p>
    <div class="btns" style="margin:0 0 12px;max-width:420px"><button class="btn" id="flagsAllOn" data-i18n="enable_all">Enable all</button><button class="btn" id="flagsAllOff" data-i18n="disable_all">Disable all</button></div>
    <div class="grid" id="diagGrid"></div>
  </section>

  <section class="tab" id="tab-calibration">
    <div class="grid" id="calGrid"></div>
  </section>

  <section class="tab" id="tab-settings">
    <div id="setGrid"></div>
  </section>

  <section class="tab" id="tab-log">
    <div class="logbar" id="logbar"></div>
    <pre id="log" data-i18n="loading_2">Loading…</pre>
  </section>
</main>
<div id="toast" aria-live="polite"></div>

)HTML"
