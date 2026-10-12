// ======================================================================
// web/ui/js_run.h
// EN: Script part 6: navigation, polling, satellites and start-up. Part of the panel page (web/web_ui.h joins the parts in order; they are one string in flash).
// PL: Skrypt cz. 6: nawigacja, odpytywanie, satelity i start. Część strony panelu (web/web_ui.h skleja części po kolei; w pamięci flash to jeden napis).
// ======================================================================
R"HTML(/* ================= navigation and polling ================= */
/* ===== "masonry" card layout: each card, in code order, goes to the shortest column (related cards stay close together). A layout change is animated (FLIP), and after cards are collapsed/expanded the layout fixes itself. ===== */
const MAS=['readGrid','diagGrid','calGrid'];
function masPlan(hs,n){
  const idx=hs.map((_,i)=>i),h=new Array(n).fill(0),col=new Array(hs.length);
  idx.forEach(i=>{let k=0;for(let j=1;j<n;j++)if(h[j]<h[k])k=j;col[i]=k;h[k]+=hs[i]+12;});
  return{col,spread:Math.max(...h)-Math.min(...h)};
}
function masonry(id,force,anim){
  const g=document.getElementById(id);if(!g||!g.offsetParent)return;
  const cards=g._m||(g._m=[...g.children]);if(!cards.length)return;
  const n=Math.max(1,Math.floor((g.clientWidth+12)/(270+12)));
  const sig=n+':'+cards.length;if(!force&&g._sig===sig)return;g._sig=sig;
  const first=anim?cards.map(c=>c.getBoundingClientRect()):null;
  const plan=masPlan(cards.map(c=>c.offsetHeight),n);
  const cols=[];for(let i=0;i<n;i++){const c=document.createElement('div');c.className='mcol';cols.push(c);}
  g.replaceChildren(...cols);g.classList.add('mas');
  cards.forEach((c,i)=>cols[plan.col[i]].appendChild(c));
  if(first&&!(window.matchMedia&&matchMedia('(prefers-reduced-motion:reduce)').matches)){
    const mv=[];
    cards.forEach((c,i)=>{const r=c.getBoundingClientRect(),dx=first[i].left-r.left,dy=first[i].top-r.top;if(Math.abs(dx)>1||Math.abs(dy)>1){c.style.transition='none';c.style.transform='translate('+dx+'px,'+dy+'px)';mv.push(c);}});
    if(mv.length){g.offsetWidth;requestAnimationFrame(()=>mv.forEach(c=>{c.style.transition='transform .3s ease';c.style.transform='';setTimeout(()=>{c.style.transition='';},350);}));}
  }
}
/* After the card heights change (e.g. disabling GPS data collapses cards) the layout is corrected after a moment of calm, but only when this
   clearly reduces the column imbalance - small changes in live data do not move the cards. */
function masonryCheck(id){
  const g=document.getElementById(id);if(!g||!g.offsetParent||!g._m||!g.classList.contains('mas'))return;
  const cols=[...g.children],n=cols.length;if(n<2)return;
  const cur=cols.map(c=>[...c.children].reduce((a,x)=>a+x.offsetHeight+12,0)),spread=Math.max(...cur)-Math.min(...cur);
  if(spread-masPlan(g._m.map(c=>c.offsetHeight),n).spread>80)masonry(id,true,true);
}
let masRO=null,masRT={};
function masonryWatch(){
  if(!window.ResizeObserver)return;
  if(!masRO)masRO=new ResizeObserver(es=>{es.forEach(e=>{const g=e.target.closest('.grid.mas');if(!g)return;clearTimeout(masRT[g.id]);masRT[g.id]=setTimeout(()=>masonryCheck(g.id),250);});});
  MAS.forEach(id=>{const g=document.getElementById(id);if(g&&g._m&&!g._ro){g._ro=1;g._m.forEach(c=>masRO.observe(c));}});
}
function masonryTab(){MAS.forEach(id=>masonry(id,false,true));}
let masT=0;
let curTab='status';
const TABS=['status','readings','charts','diag','calibration','settings','log'];
const LEGACY_TABS={odczyt:'readings',wykresy:'charts',kalibracja:'calibration',ustawienia:'settings',dziennik:'log'};   // old bookmarks (#odczyt ...)
function showTab(t,noHash){t=LEGACY_TABS[t]||t;
  if(!TABS.includes(t))t='status';curTab=t;try{localStorage.setItem('aog.tab',t);}catch(e){}
  $$('.tab').forEach(e=>e.classList.toggle('active',e.id==='tab-'+t));
  $$('#nav button').forEach(b=>{const on=b.dataset.tab===t;b.classList.toggle('active',on);if(on)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current');});
  try{const ab=$('#nav button.active');if(ab&&ab.scrollIntoView)ab.scrollIntoView({block:'nearest',inline:'center'});}catch(e){}
  if(!noHash&&location.hash!=='#'+t)history.replaceState(null,'','#'+t);
  window.scrollTo(0,0);if(S)updStrip(S);renderCurrent();MAS.forEach(id=>masonry(id,true));masonryWatch();clearTimeout(masT);masT=setTimeout(()=>{MAS.forEach(id=>masonry(id,true,true));masonryWatch();},700);if(t==='log'){renderLog(true);pollLog();}
}
function renderCurrent(){
  if(!S)return;
  if(curTab==='status')updStatus(S);
  else if(curTab==='readings'){cardsRead.forEach((c,i)=>updRows(c._list,S));}
  else if(curTab==='charts')drawAll();
  else if(curTab==='diag'){updDiag(S);maybeSats();}
  else if(curTab==='calibration')updCal(S);
  else if(curTab==='settings'){syncFlagSwitches();syncActiveWifiBadge(S);updSenders(S);updEngage(S);updAuthCard(S);}
}
/* ===== satellites: C/N0 per satellite and signal (Diagnostics -> GPS) ===== */
let SATS=null,satsBusy=false,satsAt=0;
const SAT_SYS=['GPS','GLONASS',TL('galileo'),'BeiDou','QZSS',TL('other'),TL('sbas_corrections_not_for_rtk')];
function satGrp(sys,prn){return(sys===0&&prn>=33&&prn<=64)?6:sys;}   // NMEA numbering: 33-64 is SBAS (PRN 120-151), e.g. EGNOS
function satCls(v){return v<0?'n':v>=38?'g':v>=28?'y':'r';}
async function fetchSats(){
  if(satsBusy)return;satsBusy=true;
  try{const r=await fetch('/gps_sats',{cache:'no-store'});SATS=r.ok?await r.json():{err:r.status};}catch(e){SATS={err:TL('network_2')};}
  satsBusy=false;satsAt=Date.now();
  if(cardsDiag.dGps)renderSats(cardsDiag.dGps.extra);
}
function maybeSats(){if(curTab==='diag'&&flagState.dGps&&Date.now()-satsAt>=900)fetchSats();}
function renderSats(box){
  let w=box.querySelector('.sats');
  if(!w){w=document.createElement('div');w.className='sats';box.appendChild(w);}
  w.textContent='';
  const note=txt=>{const p=document.createElement('p');p.className='note';p.textContent=txt;w.appendChild(p);};
  if(!SATS){note(TL('loading_satellites'));return;}
  if(SATS.err){note(TL('satellites_unavailable_older_module',{e:SATS.err}));return;}
  const L=SATS.sats||[];
  if(!L.length){note(TL('no_gsv_messages_from_the_receiver')+(SATS.gsv===0?TL('counter_0'):'')+TL('the_module_enables_them_while_configuring'));return;}
  // one CLUSTER per satellite (sys+prn): all its signals side by side; the number above the cluster = the best signal
  const sats={};
  L.forEach(e=>{const k=e[0]+'-'+e[1];(sats[k]=sats[k]||{sys:e[0],grp:satGrp(e[0],e[1]),prn:e[1],el:e[2],az:e[3],used:false,sig:[]});sats[k].sig.push({id:e[5],snr:e[4]});if(e[6])sats[k].used=true;});
  const all=Object.values(sats);all.forEach(s=>{s.sig.sort((a,b)=>a.id-b.id);s.best=Math.max.apply(null,s.sig.map(x=>x.snr));});
  let strong=0,mid=0,weak=0,none=0,used=0,sbas=0;
  all.forEach(s=>{if(s.grp===6){sbas++;return;}if(s.used)used++;if(s.best<0)none++;else if(s.best>=38)strong++;else if(s.best>=28)mid++;else weak++;});   // SBAS excluded from the count (not used for RTK)
  const sum=document.createElement('p');sum.className='satsum';
  sum.textContent=TL('satellites_n_strong_38_db_hz_a_medium_28',{n:all.length-sbas,a:strong,b:mid,c:weak,d:none,u:SATS.usedKnown?used:TL('unknown_no_gsa')})+(sbas?TL('sbas_not_for_rtk_n',{n:sbas}):'');
  w.appendChild(sum);
  const bars=document.createElement('div');bars.className='satbars';
  for(let sy=0;sy<SAT_SYS.length;sy++){
    const grp=all.filter(s=>s.grp===sy).sort((a,b)=>a.prn-b.prn);if(!grp.length)continue;
    const g=document.createElement('div');g.className='satgrp';
    const h=document.createElement('h4');h.textContent=SAT_SYS[sy];g.appendChild(h);
    const row=document.createElement('div');row.className='satrow';
    grp.forEach(s=>{
      const c=document.createElement('div');c.className='satcl'+(SATS.usedKnown&&!s.used?' u0':'');c.dataset.k=s.sys+'-'+s.prn;
      c.title=TL('sys_prn_elevation_el_deg_azimuth_az_deg',{sys:SAT_SYS[s.grp],prn:s.prn,el:s.el,az:s.az})+s.sig.map(x=>'s'+x.id+' '+(x.snr<0?TL('none_2'):x.snr+' dB‑Hz')).join(', ')+(SATS.usedKnown?(s.used?TL('used_in_the_solution'):TL('not_used')):'');
      const sn=document.createElement('span');sn.className='sn';sn.textContent=s.best<0?'–':String(s.best);
      const bs=document.createElement('div');bs.className='bars';
      s.sig.forEach(x=>{const v=document.createElement('div');v.className='sv '+satCls(x.snr);v.dataset.sig=x.id;v.style.height=(x.snr<0?2:Math.max(2,Math.min(70,Math.round(x.snr/55*70))))+'px';bs.appendChild(v);});
      const sp=document.createElement('span');sp.className='sp';sp.textContent=String(s.prn);
      c.append(sn,bs,sp);row.appendChild(c);
    });
    g.appendChild(row);bars.appendChild(g);
  }
  w.appendChild(bars);
  note(TL('c_n0_in_db_hz_0_55_green_38_yellow_28_37')+(SATS.usedKnown?TL('dashed_outline_visible_but_not_used_in_the'):''));
}

let fails=0,wasDown=false;
const POLL_MS=Math.max(200,+(new URLSearchParams(location.search).get('poll'))||1000);
function setOffline(v){$('#offline').hidden=!v;document.body.classList.toggle('stale',!!v);}   // .stale dims the data: it is not current
async function poll(){
  if(document.hidden){setTimeout(poll,2000);return;}
  const ac=new AbortController();const to=setTimeout(()=>ac.abort(),4000);
  let ok=false;
  try{
    const r=await fetch('/status',{signal:ac.signal,cache:'no-store'});
    if(!r.ok)throw new Error(r.status);
    const s=await r.json();S=s;ok=true;fails=0;record(s);updAuthBtn(s);
    if(wasDown&&rebooting){location.reload();return;}
    if(wasDown){loadDeviceHistory(true);loadEvents();}   // after a connection gap, fetch what is missing from the module
    wasDown=false;setOffline(false);
    // GPS data rate (bytes/s) - computed on the browser side
    const now=Date.now();if(gpsPrev&&s.gpsDiagBytes>=gpsPrev.b&&now>gpsPrev.t)gpsBps=(s.gpsDiagBytes-gpsPrev.b)*1000/(now-gpsPrev.t);gpsPrev={b:s.gpsDiagBytes,t:now};
    if(isNum(s.currSamples)){if(curPrev&&s.currSamples>=curPrev.n&&now>curPrev.t)curSps=(s.currSamples-curPrev.n)*1000/(now-curPrev.t);curPrev={n:s.currSamples,t:now};}
    if(!settingsFilled){FLAGS.forEach(([k,,jf])=>flagState[k]=!!s[jf]);fillSettings(s);syncFlagSwitches();}
    syncCurBands(s);updHeader(s);renderCurrent();maybeAuthPrompt();
    document.querySelectorAll('[data-post^="/hydlift"]').forEach(b=>{b.disabled=!s.hydLiftEn&&b.dataset.post!=='/hydlift_auto';});   // "Auto" always works   // the lift is switched off in Settings
  }catch(e){fails++;if(fails>=2){wasDown=true;setOffline(!rebooting);}}
  clearTimeout(to);setTimeout(poll,ok?POLL_MS:2000);
}

/* ================= start ================= */
function init(){
  buildRead();buildCharts();buildDiag();buildCal();buildSet();buildLog();HERO.cv=$('#heroSpark');
  loadDeviceHistory();setInterval(()=>{if(curTab==='charts'&&!document.hidden&&!chPaused)refreshLong(false);},5000);
  loadEvents();setInterval(()=>{if(!document.hidden)loadEvents();},10000);
  $('#nav').addEventListener('click',e=>{const b=e.target.closest('button[data-tab]');if(b)showTab(b.dataset.tab);});
  // keyboard: Enter / Space on a focused tile or the GPS card opens its tab (they are links, not buttons)
  document.addEventListener('keydown',e=>{if((e.key==='Enter'||e.key===' ')&&e.target.matches&&e.target.matches('[data-go]')){e.preventDefault();showTab(e.target.dataset.go);}});
  document.addEventListener('click',async e=>{
    const g=e.target.closest('[data-go]');if(g&&g.dataset.go){showTab(g.dataset.go);return;}
    const p=e.target.closest('[data-post]');if(p&&!p.disabled){if(p.dataset.confirm&&!(await askConfirm(TL('confirm_hydlift_manual'))))return;}if(p&&!p.disabled)post(p.dataset.post,null,p.textContent.trim()?TL('command_sent_n',{n:p.textContent.trim()}):null);
  });
  $('#flagsAllOn').onclick=()=>{FLAGS.forEach(([k])=>flagState[k]=true);saveFlags();updDiag(S||{});};
  $('#flagsAllOff').onclick=()=>{FLAGS.forEach(([k])=>flagState[k]=false);saveFlags();updDiag(S||{});};
  const ts=$('#themeSelect');ts.value=document.documentElement.getAttribute('data-theme');
  ts.onchange=()=>{document.documentElement.setAttribute('data-theme',ts.value);try{localStorage.setItem('theme',ts.value);}catch(e){}renderCurrent();};
  window.addEventListener('hashchange',()=>showTab(location.hash.slice(1),true));
  window.addEventListener('resize',()=>{fixHeaderH();masonryTab();if(curTab==='charts')drawAll();if(curTab==='status'&&S)updStatus(S);});
  document.addEventListener('visibilitychange',()=>{if(!document.hidden){loadDeviceHistory();if(curTab==='log')pollLog();}});
  const fb=$('#fieldBtn'),setFm=on=>{document.documentElement.classList.toggle('fm',on);fb.setAttribute('aria-pressed',on?'true':'false');try{localStorage.setItem('aog.field',on?'1':'0');}catch(e){}fixHeaderH();masonryTab();};
  fb.title=TL('field_mode');fb.setAttribute('aria-label',TL('field_mode'));fb.setAttribute('aria-pressed',document.documentElement.classList.contains('fm')?'true':'false');
  fb.onclick=()=>setFm(!document.documentElement.classList.contains('fm'));
  $('#rstrip').onclick=()=>showTab('status');
  let tab0='';try{tab0=localStorage.getItem('aog.tab')||'';}catch(e){}
  showTab(location.hash.slice(1)||tab0||'status',true);
  poll();
}
init();
</script>
</body>
</html>
)HTML"
