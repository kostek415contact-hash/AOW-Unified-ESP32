// ======================================================================
// web/ui/style.h
// EN: Styles (CSS) of the panel. Part of the panel page (web/web_ui.h joins the parts in order; they are one string in flash).
// PL: Style (CSS) panelu. Część strony panelu (web/web_ui.h skleja części po kolei; w pamięci flash to jeden napis).
// ======================================================================
R"HTML(<style>
:root{
  --bg:#0b0f0d;--panel:#121815;--panel2:#0e1311;--border:#26332c;--text:#e8ece9;--label:#8a9891;--accent:#d99a3f;
  --ok:#6fbf73;--warn:#e0a940;--bad:#e0584f;--c2:#4a98d2;--r:10px;
  --mono:ui-monospace,SFMono-Regular,Menlo,Consolas,"Liberation Mono",monospace;
  --sans:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;
}
:root[data-theme="light"]{--bg:#f5f3ef;--panel:#ffffff;--panel2:#f0ede7;--border:#d8d3c8;--text:#1c211d;--label:#6b7a70;--accent:#b5762a;--ok:#2f8f3a;--warn:#b5762a;--bad:#c23b32;--c2:#2570b0}
:root[data-theme="blue"]{--bg:#0b0e14;--panel:#121722;--panel2:#0e131c;--border:#232c3d;--text:#e7ebf2;--label:#7c8aa0;--accent:#4f8ddb;--ok:#5fb86a;--warn:#d9a441;--bad:#e05b52;--c2:#c9722b}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--text);font:14px/1.4 var(--sans);-webkit-font-smoothing:antialiased}
button,input,select{font-family:inherit;font-size:14px;color:var(--text)}
[hidden]{display:none!important}
.mono{font-family:var(--mono)}
/* --- top bar --- */
.top{position:sticky;top:0;z-index:30;display:flex;align-items:center;justify-content:space-between;gap:10px;
  padding:10px 14px;padding-top:calc(10px + env(safe-area-inset-top));background:var(--bg);border-bottom:1px solid var(--border)}
.brand{display:flex;align-items:center;gap:10px;min-width:0}
.brand h1{margin:0;font-size:15px;font-weight:600;white-space:nowrap}
.dot{width:10px;height:10px;border-radius:50%;background:var(--label);flex:none}
.dot.ok{background:var(--ok);box-shadow:0 0 8px 1px rgba(111,191,115,.5)}
.dot.warn{background:var(--warn);box-shadow:0 0 8px 1px rgba(224,169,64,.5)}
.dot.bad{background:var(--bad);box-shadow:0 0 8px 1px rgba(224,88,79,.5)}
.chip{font-size:12px;color:var(--label);border:1px solid var(--border);border-radius:999px;padding:2px 9px;white-space:nowrap}
.top-actions{display:flex;align-items:center;gap:8px}
.brand .chip{overflow:hidden;text-overflow:ellipsis;min-width:0}
@media(max-width:520px){.top{gap:6px}.top-actions{gap:6px}.top-actions select{padding:6px 3px;font-size:13px;max-width:84px}.auth-btn{padding:6px 7px;font-size:13px}}   /* phone: two selectors (language, theme) next to the brand */
select{background:var(--panel);border:1px solid var(--border);border-radius:8px;padding:7px 8px;min-height:36px}
.auth-btn{background:var(--panel);border:1px solid var(--border);border-radius:8px;padding:7px 12px;min-height:36px;cursor:pointer;font-weight:600;white-space:nowrap}
.auth-btn.in{color:var(--accent)}
.auth-btn[hidden]{display:none}
.banner{position:sticky;top:53px;z-index:29;padding:9px 14px;font-size:13px;font-weight:600;text-align:center}
.banner.bad{background:var(--bad);color:#fff}
/* --- navigation --- */
nav.tabs{position:fixed;left:0;right:0;bottom:0;z-index:30;display:flex;background:var(--panel);border-top:1px solid var(--border);
  padding-bottom:env(safe-area-inset-bottom)}
nav.tabs button{flex:1;display:flex;flex-direction:column;align-items:center;gap:3px;padding:8px 2px 7px;min-height:56px;background:none;border:0;
  color:var(--label);font-size:10.5px;font-weight:600;cursor:pointer;border-top:2px solid transparent}
nav.tabs button.active{color:var(--accent);border-top-color:var(--accent)}
.ico{width:22px;height:22px;stroke:currentColor;fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
@media(min-width:800px){
  nav.tabs{position:sticky;top:53px;bottom:auto;border-top:0;border-bottom:1px solid var(--border);padding:0 14px;justify-content:flex-start;gap:4px}
  nav.tabs button{flex:none;flex-direction:row;gap:8px;padding:0 16px;min-height:44px;font-size:13.5px;border-top:0;border-bottom:2px solid transparent}
  nav.tabs button.active{border-bottom-color:var(--accent)}
  .banner{top:98px}
  .grid.tiles{grid-template-columns:repeat(4,1fr)}
  main{padding-bottom:30px!important}
}
main{max-width:1180px;margin:0 auto;padding:14px 14px 84px}
.tab{display:none}.tab.active{display:block}
h2{font-size:12px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;color:var(--label);margin:20px 2px 8px}
h2:first-child{margin-top:4px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(270px,1fr));gap:12px;align-items:start}
.grid.tiles{grid-template-columns:repeat(auto-fill,minmax(165px,1fr))}
/* cards of different heights: columns (masonry) instead of grid rows - no empty gaps under short cards */
.grid.mas{display:flex;align-items:flex-start}
.mcol{flex:1 1 0;min-width:0;display:flex;flex-direction:column;gap:12px}
/* --- cards and rows --- */
.card{background:var(--panel);border:1px solid var(--border);border-radius:var(--r);padding:12px 14px}
.card h3{margin:0 0 6px;font-size:12px;font-weight:600;color:var(--label);letter-spacing:.03em}
.card-h{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:6px}
.card-h h3{margin:0;font-size:13px;color:var(--text)}
.row{display:flex;align-items:baseline;justify-content:space-between;gap:12px;padding:5px 0;border-top:1px solid var(--border)}
.row:first-child{border-top:0}
.rl{font-size:12.5px;color:var(--label)}
.rv{font-family:var(--mono);font-size:14px;text-align:right;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}
.rv.ok,.ok{color:var(--ok)}.rv.warn,.warn{color:var(--warn)}.rv.bad,.bad{color:var(--bad)}.rv.muted{color:var(--label)}
.note{font-size:12px;color:var(--label);margin:8px 0 0}
/* Settings: sections */
.setpage{max-width:1180px;margin:0 auto}
.setnav{display:flex;flex-wrap:wrap;gap:6px;margin:0 0 4px}
.sech{margin:26px 0 4px;font-size:17px;font-weight:650;scroll-margin-top:70px;padding-bottom:6px;border-bottom:1px solid var(--border)}
.setnav+.sech{margin-top:14px}
.secd{margin:6px 0 10px;font-size:13px;color:var(--label);line-height:1.5;max-width:70ch}
.setsec{display:grid;grid-template-columns:repeat(auto-fill,minmax(min(100%,340px),1fr));gap:12px;align-items:start;margin-top:10px}
.setsec .card{padding:14px 16px}
.setsec .card h3{font-size:14px;color:var(--text);margin:0 0 8px}
.setsec .note{font-size:13px;line-height:1.5}
.fold{margin:8px 0 0}
.fold>summary{cursor:pointer;font-size:12.5px;color:var(--accent);list-style:none;padding:4px 0}
.fold>summary::-webkit-details-marker{display:none}
.fold>summary::before{content:"\25B8  "}
.fold[open]>summary::before{content:"\25BE  "}
.fold>p,.fold>ol{margin-top:4px}
.snd-head{margin:0 0 4px;font-size:13px}
.snd-head b{color:var(--text)}
.snd-row{display:grid;grid-template-columns:1fr auto;gap:2px 12px;align-items:baseline;padding:9px 0 9px 10px;border-top:1px solid var(--border);border-left:3px solid transparent}
.snd-row.sel{border-left-color:var(--ok)}
.snd-ip{font-size:14px;overflow-wrap:anywhere}
.snd-st{justify-self:end;font-size:12.5px;font-weight:600;text-align:right}
.snd-st.ok{color:var(--ok)}.snd-st.warn{color:var(--warn)}.snd-st.muted{color:var(--label)}
.snd-sub{grid-column:1/-1;font-size:12px;color:var(--label)}
.snd-row .btn{grid-column:1/-1;margin-top:6px}
.sats{margin-top:10px}
.satsum{font-size:12px;line-height:1.35;margin:0 0 6px}
.satbars{display:flex;flex-direction:column;gap:5px}
.satgrp h4{margin:0 0 1px;font-size:11px;font-weight:600;color:var(--label)}
.satrow{display:flex;gap:3px;border-bottom:1px solid var(--border)}
.satcl{flex:1 1 0;min-width:0;max-width:36px;height:94px;display:flex;flex-direction:column;justify-content:flex-end;align-items:center}
.satcl .sn{font-size:9px;line-height:11px;color:var(--text);white-space:nowrap}
.satcl .bars{display:flex;align-items:flex-end;justify-content:center;gap:1px;width:100%}
.satcl .sv{flex:1 1 0;max-width:10px;min-width:2px;min-height:2px;border-radius:2px 2px 0 0}
.satcl .sp{font-size:9px;line-height:11px;color:var(--text)}
.sv.g{background:var(--ok)}.sv.y{background:var(--warn)}.sv.r{background:var(--bad)}.sv.n{background:var(--label)}
.satcl.u0 .sv{opacity:.4;outline:1px dashed var(--label);outline-offset:-1px}
.off-hint{font-size:12.5px;color:var(--label);margin:2px 0 0}
/* --- status tiles --- */
.hero{background:var(--panel);background:radial-gradient(120% 140% at 0% 0%,color-mix(in srgb,var(--accent) 10%,transparent),transparent 60%),var(--panel);
  border:1px solid var(--border);border-left:5px solid var(--c,var(--label));border-radius:var(--r);padding:16px 18px;margin-bottom:12px;cursor:pointer}
.hero .hl{font-size:12px;color:var(--label)}
.hero .hv{font-family:var(--mono);font-size:38px;font-weight:600;line-height:1.25;color:var(--c,var(--text))}
.hero .hs{font-size:13px;color:var(--label);margin-top:2px}
.tile{background:var(--panel);border:1px solid var(--border);border-left:4px solid var(--c,var(--label));border-radius:var(--r);padding:11px 12px;cursor:pointer;min-height:92px;
  display:flex;flex-direction:column;gap:2px}
.tile:active{background:var(--panel2)}
.tile .tn{font-size:11.5px;color:var(--label)}
.tile .ts{font-size:19px;font-weight:600;color:var(--c,var(--text));line-height:1.2}
.tile .tsub{font-size:12px;color:var(--label);margin-top:auto;overflow-wrap:anywhere}
.ready{background:var(--panel);border:1px solid var(--border);border-left:5px solid var(--c,var(--label));border-radius:var(--r);padding:12px 14px;margin-bottom:12px}
.ready .rh{display:flex;flex-wrap:wrap;align-items:baseline;gap:4px 12px}
.ready .rv{font-size:20px;font-weight:600;color:var(--c,var(--text))}
.ready .rs{font-size:12.5px;color:var(--label)}
.ready .rc{display:grid;grid-template-columns:repeat(auto-fill,minmax(138px,1fr));gap:6px;margin-top:10px}
.ready .ck{display:grid;grid-template-columns:9px 1fr;grid-template-rows:auto auto;column-gap:8px;row-gap:1px;align-items:center;padding:7px 10px;background:var(--panel2);border:1px solid var(--border);border-radius:8px;cursor:pointer;min-width:0}
.ready .ck i{grid-row:1/3;width:9px;height:9px;border-radius:50%;background:var(--c)}
.ready .ck b{font-size:11.5px;font-weight:500;color:var(--label)}
.ready .ck span{font-size:13.5px;font-weight:600;color:var(--c,var(--text));overflow-wrap:break-word}
.attn{background:var(--panel);border:1px solid var(--border);border-radius:var(--r);padding:10px 14px;margin-bottom:12px}
.attn .ai{display:flex;gap:9px;align-items:center;padding:3px 0;font-size:13.5px}
.attn .ai i{width:9px;height:9px;border-radius:50%;flex:none;background:var(--c)}
/* --- forms --- */
.field{display:flex;flex-direction:column;gap:4px;margin-top:10px}
.field>span{font-size:12.5px;color:var(--label)}
input[type=text],input[type=number],input[type=password]{width:100%;min-height:42px;background:var(--panel2);border:1px solid var(--border);border-radius:8px;padding:8px 11px;font-size:16px}
input:focus{outline:none;border-color:var(--accent)}
.field.err input{border-color:var(--bad)}
.field .em{font-size:12px;color:var(--bad);min-height:0}
.field .hint{font-size:11.5px;color:var(--label)}
.field .pwclear{align-self:flex-start;margin-top:4px;min-height:32px;padding:0 10px;font-size:12px;border-radius:6px;border:1px solid var(--border);background:transparent;color:var(--label);cursor:pointer}
.field input:disabled{opacity:.55}
.field .hint.active{color:var(--ok);font-weight:600}
.btn{min-height:44px;padding:0 16px;border-radius:8px;border:1px solid var(--border);background:var(--panel2);cursor:pointer;font-weight:600}
.btn:hover,.btn:focus-visible{border-color:var(--accent);color:var(--accent);outline:none}
/* a clearly visible keyboard focus on everything that can be reached with Tab */
.btn:focus-visible,.tile:focus-visible,.hero:focus-visible,.pill:focus-visible,nav.tabs button:focus-visible,.xb:focus-visible,select:focus-visible,.pwclear:focus-visible,a:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.btn.primary{background:var(--accent);border-color:var(--accent);color:#1a1408}
.btn.primary:hover{color:#1a1408;filter:brightness(1.08)}
.btn.danger{border-color:var(--bad);color:var(--bad)}
.btn:disabled{opacity:.5;cursor:default}
.btns{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}.btns .btn{flex:1;min-width:120px}
/* switches */
.sw{position:relative;display:inline-flex;align-items:center;gap:10px;cursor:pointer;user-select:none}
.sw input{position:absolute;opacity:0;width:0;height:0}
.sw .tr{width:42px;height:24px;border-radius:999px;background:var(--panel2);border:1px solid var(--border);position:relative;transition:background .15s;flex:none}
.sw .tr::after{content:"";position:absolute;left:2px;top:2px;width:18px;height:18px;border-radius:50%;background:var(--label);transition:transform .15s,background .15s}
.sw input:checked~.tr{background:var(--panel2);background:color-mix(in srgb,var(--accent) 30%,var(--panel2));border-color:var(--accent)}
.sw input:checked~.tr::after{transform:translateX(18px);background:var(--accent)}
.sw input:focus-visible~.tr{outline:2px solid var(--accent);outline-offset:2px}
.sw .st{font-size:13.5px}
.swrow{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:9px 0;border-top:1px solid var(--border)}
.swrow:first-of-type{border-top:0}
.flags{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));column-gap:24px}
/* calibration */
.big{font-family:var(--mono);font-size:44px;font-weight:600;line-height:1.2;color:var(--accent)}
.bigs{font-size:12.5px;color:var(--label)}
.gauge{position:relative;height:14px;border-radius:7px;background:var(--panel2);border:1px solid var(--border);margin:14px 0 4px}
.gauge i{position:absolute;top:-4px;width:6px;height:20px;border-radius:3px;background:var(--accent);transform:translateX(-3px);transition:left .2s}
.gauge b{position:absolute;top:0;bottom:0;width:1px;background:var(--label);opacity:.5}
.gl{display:flex;justify-content:space-between;font-size:11.5px;color:var(--label)}
.steps{margin:8px 0 0;padding-left:20px;color:var(--label);font-size:13px}.steps li{margin:4px 0}
.result{margin-top:10px;padding:9px 11px;border-radius:8px;background:var(--panel2);border:1px solid var(--border);font-size:13px}
.result.bad{border-color:var(--bad);color:var(--bad)}
.bits{display:grid;grid-template-columns:repeat(16,1fr);gap:3px;margin-top:8px}
.bits i{height:16px;border-radius:3px;background:var(--panel2);border:1px solid var(--border)}
.bits i.on{background:var(--ok);border-color:var(--ok)}
.bits.pins{grid-template-columns:repeat(8,1fr)}
.bits.pins i{height:auto;padding:2px 0 3px;text-align:center;font-style:normal;font-size:11px;line-height:1.2;font-family:ui-monospace,monospace}
.bits.pins i b{display:block;font-weight:400;font-size:9px;opacity:.65}
.bits.pins i.on{color:#0b0f0d}
.bits.pins i.idle{opacity:.4}
.tl{margin:0;padding:0;list-style:none}
/* log */
.evcard{margin-top:14px}.evcard h3{margin-bottom:6px}
.ev-row{display:grid;grid-template-columns:auto 1fr auto;gap:2px 10px;align-items:baseline;padding:6px 0;border-top:1px solid var(--border)}
.ev-row:first-of-type{border-top:0}.ev-row i{width:9px;height:9px;border-radius:50%;background:var(--c);align-self:center}
.ev-row .et{color:var(--text)}.ev-row .eh{color:var(--label);font:12px var(--mono);white-space:nowrap}
@media(max-width:520px){.ev-row{grid-template-columns:auto 1fr}.ev-row .eh{grid-column:2;white-space:normal}}   /* narrow screen: the time under the event description, no horizontal scrolling */
.logbar{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:10px;align-items:center}
.pill{min-height:34px;padding:0 12px;border-radius:999px;border:1px solid var(--border);background:var(--panel);cursor:pointer;font-size:13px;color:var(--label);font-weight:600}
.pill.active{border-color:var(--accent);color:var(--accent)}
.logbar input{flex:1;min-width:120px;min-height:34px;padding:4px 10px;font-size:14px;border-radius:999px}
#log{margin:0;white-space:pre-wrap;word-break:break-word;background:#080b09;color:#d5dcd7;border:1px solid var(--border);border-radius:var(--r);
  padding:12px;font:12.5px/1.45 var(--mono);height:calc(100vh - 250px);min-height:240px;overflow:auto}
:root[data-theme="light"] #log{background:#fdfcfa;color:#222}
.l-fail{color:var(--bad)}.l-wifi{color:var(--warn)}.l-gps{color:var(--ok)}.l-rtcm{color:#7ea6c9}.l-imu{color:#c39be0}.l-sys{color:var(--label)}
/* toast */
#toast{position:fixed;left:50%;bottom:calc(76px + env(safe-area-inset-bottom));transform:translateX(-50%);z-index:60;display:flex;flex-direction:column;gap:6px;align-items:center;pointer-events:none;width:calc(100% - 24px);max-width:420px}
@media(min-width:800px){#toast{bottom:24px}}
.toast{background:var(--panel);border:1px solid var(--border);border-left:4px solid var(--c,var(--accent));padding:10px 14px;border-radius:8px;box-shadow:0 6px 24px rgba(0,0,0,.35);font-size:13.5px;width:100%}
.overlay{position:fixed;top:0;left:0;right:0;bottom:0;z-index:80;background:rgba(0,0,0,.72);display:flex;align-items:center;justify-content:center;padding:20px;text-align:center}
.overlay .box{background:var(--panel);border:1px solid var(--border);border-radius:var(--r);padding:22px 26px;max-width:340px}
.overlay .box.auth{max-width:380px;width:100%;text-align:left}
.overlay .box.auth h3{margin:0 0 8px;font-size:16px}
.overlay .box.auth input{margin:6px 0}
.overlay .box.auth .btns{margin-top:10px}
.seg{display:flex;gap:8px;flex-wrap:wrap;margin:8px 0}.seg .btn{min-height:38px;padding:0 12px}.seg .btn.on{border-color:var(--accent);color:var(--accent)}
.auth-err{color:var(--bad);min-height:1.2em;font-size:13px;margin:4px 0 0}
/* charts */
.hero{position:relative}
#heroSpark{position:absolute;right:14px;top:14px;width:34%;max-width:230px;height:54px;pointer-events:none}
.grid.charts{grid-template-columns:repeat(auto-fill,minmax(320px,1fr))}
.chart canvas{width:100%;height:170px;display:block;touch-action:pan-y;cursor:crosshair}
.chart .card-h{margin-bottom:2px}
.chart .tm{font:12px var(--mono);color:var(--label)}
article.chart{position:relative}
.chsum{margin:0 0 2px;grid-template-columns:repeat(4,1fr)}@media(max-width:700px){.chsum{grid-template-columns:repeat(2,1fr)}}.chsum .tile{cursor:default;min-height:82px}.chsum .tile:active{background:var(--panel)}
#chGrid h2{margin:18px 2px 8px}#chGrid h2:first-child{margin-top:14px}
/* button to enlarge a chart to full screen (phone: the chart is only 170 px high) */
.chart .xb{flex:none;width:30px;height:28px;border:1px solid var(--border);background:none;color:var(--label);border-radius:8px;cursor:pointer;font-size:15px;line-height:1;padding:0}
.chart .xb:hover{color:var(--text);border-color:var(--label)}
article.chart.big{position:fixed;left:0;top:0;right:0;bottom:0;z-index:70;border-radius:0;overflow:auto;background:var(--panel);padding:calc(14px + env(safe-area-inset-top)) 14px calc(14px + env(safe-area-inset-bottom))}
article.chart.big canvas{height:calc(100vh - 250px);min-height:240px}
body.noscroll{overflow:hidden}
body.stale main{opacity:.5;filter:grayscale(.6);transition:opacity .3s}
/* min / avg / max under the chart (or time shares in states for fix quality) */
.chart .st{display:flex;flex-direction:column;gap:2px;margin-top:7px;font:11.5px var(--mono);color:var(--label)}
.chart .st.shares{flex-direction:row;flex-wrap:wrap;gap:3px 14px}
.chart .st .sr,.chart .st .sh{display:flex;align-items:center;gap:6px;flex-wrap:wrap}
.chart .st i{display:inline-block;width:10px;height:3px;border-radius:2px;flex:none}
.chart .st .sh i{width:8px;height:8px;border-radius:50%}
.chart .st .sn{color:var(--text)}
/* tooltip at the cursor: the value in the foreground, the series name after it */
.tip{position:absolute;z-index:5;pointer-events:none;background:var(--panel);border:1px solid var(--border);border-radius:8px;padding:7px 10px;box-shadow:0 6px 20px rgba(0,0,0,.35);font-size:12px;min-width:120px;max-width:230px}
.tip .tt{font:11.5px var(--mono);color:var(--label);margin-bottom:4px;white-space:nowrap}
.tip .te{font-size:12px;margin:0 0 4px;max-width:230px;white-space:normal;line-height:1.3}
.tip .tr{display:flex;align-items:center;gap:7px;line-height:1.55}
.tip .tr i{width:10px;height:3px;border-radius:2px;flex:none}
.tip .tr b{font:600 13px var(--mono);color:var(--text);white-space:nowrap}
.tip .tr span{color:var(--label)}
.lg{display:flex;flex-wrap:wrap;gap:4px 14px;font-size:12px;color:var(--label);margin:0 0 6px}
.lg i{display:inline-block;width:12px;height:3px;border-radius:2px;margin-right:6px;vertical-align:middle}
.lg b{font-family:var(--mono);font-weight:600;color:var(--text);margin-left:6px}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
</style>
)HTML"
