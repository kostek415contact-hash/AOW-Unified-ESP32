// ======================================================================
// web/ui/js_charts.h
// EN: Script part 5: log and the charts (history, events, cursor, RTK summary). Part of the panel page (web/web_ui.h joins the parts in order; they are one string in flash).
// PL: Skrypt cz. 5: dziennik i wykresy (historia, zdarzenia, kursor, podsumowanie RTK). Część strony panelu (web/web_ui.h skleja części po kolei; w pamięci flash to jeden napis).
// ======================================================================
R"HTML(/* --- log --- */
const LOGF={all:null,gps:/\[GPS|\[RTCM|\[NTRIP/,net:/\[WIFI|\[NET\]|\[TX FAIL|\[PGN|\[ETHERNET|\[STEER CMD|\[MACHINE CMD/,imu:/\[IMU/,err:/ERROR|FAIL|WARNING|Could not|not found|NO DATA|No valid|No response|No speed|rejected|too long|not saved|STOP:/i,sys:/\[SYS|\[CPU|\[LOOP|\[STEP|\[WATCHDOG|\[STATUS/};
const LOGN={all:TL('all'),gps:'GPS/NTRIP',net:TL('network_pgn_2'),imu:'IMU',err:TL('errors'),sys:TL('system')};
let logFilter='all',logPaused=false,logRaw='',logShown='',logQ='';
function logClass(l){if(/\[TX FAIL\]|ERROR/i.test(l))return 'l-fail';if(/\[WIFI|\[NET\]/.test(l))return 'l-wifi';if(/\[GPS/.test(l))return 'l-gps';if(/\[RTCM|\[NTRIP/.test(l))return 'l-rtcm';if(/\[IMU/.test(l))return 'l-imu';if(/\[SYS\]|\[CPU\]/.test(l))return 'l-sys';return '';}
function buildLog(){
  const bar=$('#logbar');
  Object.keys(LOGF).forEach(k=>{const b=document.createElement('button');b.className='pill'+(k===logFilter?' active':'');b.textContent=LOGN[k];b.dataset.f=k;b.onclick=()=>{logFilter=k;$$('.pill[data-f]',bar).forEach(x=>x.classList.toggle('active',x.dataset.f===k));renderLog(true);};bar.appendChild(b);});
  const q=document.createElement('input');q.type='text';q.placeholder=TL('search');q.addEventListener('input',()=>{logQ=q.value.toLowerCase();renderLog(true);});bar.appendChild(q);
  const p=document.createElement('button');p.className='pill';p.textContent=TL('pause');p.onclick=()=>{logPaused=!logPaused;p.classList.toggle('active',logPaused);p.textContent=logPaused?TL('resume'):TL('pause');};bar.appendChild(p);
  const cp=document.createElement('button');cp.className='pill';cp.textContent=TL('copy');cp.onclick=copyLog;bar.appendChild(cp);
  const dl=document.createElement('button');dl.className='pill';dl.textContent=TL('download');dl.onclick=()=>{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([logRaw.split('\n').map(logTr).join('\n')],{type:'text/plain'}));a.download='aog-log.txt';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);};bar.appendChild(dl);
  const dr=document.createElement('button');dr.className='pill';dr.textContent=TL('diagnostic_report');dr.onclick=downloadDiagReport;bar.appendChild(dr);
  const cr=document.createElement('button');cr.className='pill';cr.textContent=TL('copy_report');cr.onclick=()=>copyText(diagReport(S,logRaw,EVT),TL('report_copied'));bar.appendChild(cr);
}
/* One text file for a bug report: versions, the readiness verdict, the whole /status (secrets removed), recent events and the log in the
   firmware's own English. Nothing leaves the browser except the file the user chooses to send. */
function diagReport(s,logText,events){
  const SECRET=/pass|token|secret|psk|ntripUser|panelPin|authPin/i;
  const clean={};Object.keys(s||{}).forEach(k=>{clean[k]=SECRET.test(k)?'(hidden)':s[k];});
  const L=[];const h=t=>{L.push('','=== '+t+' ===');};
  L.push('AOW-Unified-ESP32 diagnostic report',new Date().toISOString());
  h('Firmware');L.push('version '+(s&&s.firmwareVersion)+'  build '+(s&&s.firmwareBuild)+'  uptime '+(s&&s.uptimeSec)+' s  last restart: '+(s&&s.resetReason));
  h('Ready to drive');
  try{const r=readiness(s||{});L.push(r.v.toUpperCase()+' ('+r.bads+' bad, '+r.warns+' warnings)');r.k.forEach(x=>L.push('  ['+x.c+'] '+x.n+': '+x.t));}catch(e){L.push('(could not be computed)');}
  h('Status');Object.keys(clean).sort().forEach(k=>L.push(k+' = '+JSON.stringify(clean[k])));
  h('Events (newest last)');(events||[]).forEach(e=>L.push(new Date(e.ts).toISOString()+'  type '+e.t+'  value '+e.v));
  h('Log');L.push(logText||'(empty)');
  return L.join('\n');
}
function downloadDiagReport(){
  const txt=diagReport(S,logRaw,EVT);
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([txt],{type:'text/plain'}));
  const d=new Date(),p=n=>String(n).padStart(2,'0');a.download='aog-diagnostic-'+d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+'-'+p(d.getHours())+p(d.getMinutes())+'.txt';
  document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(a.href),1500);
  toast(TL('diagnostic_report_downloaded'),'ok');
}
function copyText(txt,msg){
  const done=()=>toast(msg,'ok');
  if(navigator.clipboard&&window.isSecureContext){navigator.clipboard.writeText(txt).then(done,()=>fallbackCopy(txt,done));}else fallbackCopy(txt,done);
}
function copyLog(){copyText(logShown||logRaw,TL('log_copied'));}
function fallbackCopy(t,done){const ta=document.createElement('textarea');ta.value=t;ta.style.position='fixed';ta.style.opacity='0';document.body.appendChild(ta);ta.select();try{document.execCommand('copy');done();}catch(e){toast(TL('copy_failed'),'bad');}ta.remove();}
function renderLog(force){
  const el=$('#log');const re=LOGF[logFilter];
  let lines=logRaw.split('\n').filter(l=>l&&(!re||re.test(l))).map(l=>[l,logTr(l)]).filter(p=>!logQ||p[1].toLowerCase().includes(logQ)||p[0].toLowerCase().includes(logQ));
  if(lines.length>600)lines=lines.slice(-600);
  const txt=lines.map(p=>p[1]).join('\n');if(!force&&txt===logShown)return;logShown=txt;
  const atBottom=el.scrollTop+el.clientHeight>=el.scrollHeight-40;
  const fr=document.createDocumentFragment();
  lines.forEach(p=>{const s=document.createElement('span');const c=logClass(p[0]);if(c)s.className=c;s.textContent=p[1]+'\n';fr.appendChild(s);});
  el.textContent='';el.appendChild(fr);if(atBottom||force)el.scrollTop=el.scrollHeight;
}
let logTimer=null;
let logBusy=false;
async function pollLog(){
  clearTimeout(logTimer);
  if(curTab!=='log'||document.hidden||logBusy){return;}
  logBusy=true;
  try{
    const ac=new AbortController();const to=setTimeout(()=>ac.abort(),4000);
    const r=await fetch('/log',{signal:ac.signal,cache:'no-store'});clearTimeout(to);
    if(r.ok&&!logPaused){logRaw=await r.text();renderLog(false);}
  }catch(e){}
  logBusy=false;
  logTimer=setTimeout(pollLog,1500);
}

/* ================= charts (our own, lightweight code - works without internet too) ================= */
const HMAX=3600,GAP=4500;
const HIST={t:[],d:{}};let prevS=null,prevT=0;
// LONG: aggregated history from the module for ranges > 60 min (6 h, 24 h). histInfo: what the module remembers.
const LONG={t:[],d:{},gap:0};let longRange=0,longFetchedAt=0,longBusy=false,histInfo=null,histBusy=false;
const rateKB=k=>(s,p,dt)=>(p&&dt>0&&isNum(s[k])&&isNum(p[k])&&s[k]>=p[k])?(s[k]-p[k])/1024/dt:null;
const SER={
  hAccCm:s=>s.hAcc>=0?s.hAcc*100:null,
  fixLvl:s=>({0:0,1:1,2:2,5:3,4:4})[s.fixQuality],
  sats:s=>s.satellites,speed:s=>s.gpsSpeedKmh,
  roll:s=>s.imuRoll,pitch:s=>s.imuPitch,
  angle:s=>s.steerSensorOk?s.steerAngleDeg:null,
  target:s=>(s.cmdFresh&&s.cmdEngaged)?s.cmdTargetAngle:null,
  rssi:s=>(s.netType==='WiFi'&&s.netConnected)?s.rssi:null,
  ntripKBs:rateKB('ntripBytes'),gpsKBs:rateKB('gpsDiagBytes'),
  cpu1:s=>s.loopLoadPct,cpu0:s=>s.coreZeroLoadPct,
  heap:s=>s.freeHeap/1024,minheap:s=>isNum(s.minFreeHeap)?s.minFreeHeap/1024:null,
  imuPps:s=>s.imuPps,
  imuBps:s=>isNum(s.imuBps)?s.imuBps:null,
  curA:s=>(s.currentSensEn&&isNum(s.currRecentPeakA)&&(s.currState===2||s.currState===3))?s.currRecentPeakA:null,   // peak |current| of the last second (as in the module history)
  ageDgps:s=>s.ageDgps>=0?s.ageDgps:null   // RTK corrections age [s]; none = a gap in the chart (as in the module history)
};
function record(s,ts){
  ts=ts||Date.now();const dt=prevT?(ts-prevT)/1000:0,d=HIST.d;
  for(const k in SER){let v=null;try{v=SER[k](s,prevS,dt);}catch(e){}(d[k]||(d[k]=[])).push(isNum(v)?v:null);}
  HIST.t.push(ts);
  if(HIST.t.length>HMAX){HIST.t.shift();for(const k in d)d[k].shift();}
  prevS=s;prevT=ts;
}
// Bands on the current chart (yellow: between the soft and hard threshold, red: above the hard one) - the thresholds change from the panel, so we compute the bands from /status.
function syncCurBands(s){const c=CHARTS.find(x=>x.series&&x.series[0].k==='curA');if(!c||!isNum(s.currKickoutA)||!isNum(s.currHardA))return;c.bands=[[s.currKickoutA,s.currHardA,'--warn'],[s.currHardA,1000,'--bad']];}
// RTK chart thresholds. Corrections age: the same numbers as in sensors/gps_age.h (WARN_S, BAD_S). hAcc: a nominal target for RTK FIX
// (the LG290P in FIX usually drops to 1-2 cm) - change it here if you want a different boundary.
const AGE_WARN_S=10,AGE_BAD_S=120,HACC_GOOD_CM=2.5,HACC_WARN_CM=10;
const CHARTS=[
 {t:TL('hacc_accuracy_target_n_cm',{n:String(HACC_GOOD_CM).replace('.',',')}),u:' cm',dec:1,min0:true,bands:[[0,HACC_GOOD_CM,'--ok'],[HACC_WARN_CM,100000,'--warn']],series:[{k:'hAccCm',l:'hAcc',c:'--ok',fill:true}]},
 {t:TL('gps_fix_quality'),yMin:0,yMax:4,ticks:{0:TL('none'),1:'GPS',2:'DGPS',3:'FLOAT',4:'FIX'},fixLbl:true,series:[{k:'fixLvl',l:TL('state_2'),c:'--accent',colorBy:v=>v===4?'--ok':v===0?'--bad':'--warn'}]},
 {t:TL('rtk_correction_age_normal_about_1_s'),u:' s',dec:1,min0:true,minSpan:12,bands:[[AGE_WARN_S,AGE_BAD_S,'--warn'],[AGE_BAD_S,100000,'--bad']],series:[{k:'ageDgps',l:TL('correction_age'),c:'--c2'}]},
 {t:TL('satellites'),dec:0,min0:true,series:[{k:'sats',l:TL('satellites_2'),c:'--accent',step:true}]},
 {t:TL('gps_speed'),u:' km/h',dec:1,min0:true,series:[{k:'speed',l:TL('speed_2'),c:'--accent',fill:true}]},
 {t:TL('imu_roll_and_pitch'),u:'°',dec:1,zero:true,series:[{k:'roll',l:'roll',c:'--accent'},{k:'pitch',l:'pitch',c:'--c2'}]},
 {t:TL('steering_angle_measured_and_target'),u:'°',dec:1,zero:true,series:[{k:'angle',l:TL('measured'),c:'--accent'},{k:'target',l:TL('target_aog'),sn:TL('target'),c:'--c2'}]},
 {t:TL('motor_current_peak_in_the_last_second'),u:' A',dec:1,min0:true,minSpan:10,bands:[],series:[{k:'curA',l:TL('text'),c:'--accent',fill:true}]},
 {t:TL('wifi_signal'),u:' dBm',dec:0,minSpan:12,bands:[[-120,-80,'--bad'],[-80,-70,'--warn']],series:[{k:'rssi',l:'RSSI',c:'--ok'}]},
 {t:TL('data_throughput'),u:' KB/s',dec:1,min0:true,series:[{k:'ntripKBs',l:'NTRIP',c:'--accent'},{k:'gpsKBs',l:'GPS UART',c:'--c2'}]},
 {t:TL('core_load'),u:' %',dec:1,yMin:0,yMax:100,series:[{k:'cpu1',l:TL('core_1_loop'),sn:TL('core_1'),c:'--accent'},{k:'cpu0',l:TL('core_0_web'),sn:TL('core_0'),c:'--c2'}]},
 {t:TL('free_ram'),u:' KB',dec:0,minSpan:20,series:[{k:'heap',l:TL('free'),c:'--ok'},{k:'minheap',l:TL('minimum_since_start'),sn:TL('minimum'),c:'--warn'}]},
 {t:TL('imu_packets_per_second'),dec:0,yMin:0,yMax:120,series:[{k:'imuPps',l:TL('pkts_s'),c:'--ok'}]},
 {t:TL('imu_raw_uart_bytes_healthy_1900_b_s'),u:' B/s',dec:0,min0:true,minSpan:200,series:[{k:'imuBps',l:'B/s',c:'--accent',fill:true}]}
];
/* ---- history recorded by the MODULE (/history): charts accumulate even with the page closed ---- */
// Format: see diagnostics/history_store.h (the 'AOGH' header + series names + scales + int16 series, little-endian).
function decodeHistory(buf){
  const dv=new DataView(buf);
  if(buf.byteLength<26||dv.getUint8(0)!==65||dv.getUint8(1)!==79||dv.getUint8(2)!==71||dv.getUint8(3)!==72)throw new Error(TL('bad_history_format'));
  if(dv.getUint8(4)!==1)throw new Error(TL('unknown_history_version'));
  const ns=dv.getUint8(5),n=dv.getUint16(6,true),step=dv.getUint16(8,true),lastAge=dv.getUint32(12,true),cap=dv.getUint32(16,true),stored=dv.getUint32(20,true),nl=dv.getUint16(24,true);
  let o=26,names='';
  for(let i=0;i<nl;i++){const c=dv.getUint8(o+i);if(!c)break;names+=String.fromCharCode(c);}
  o+=nl;
  const keys=names.split(',');
  if(keys.length!==ns)throw new Error(TL('mismatched_number_of_series'));
  const scale=[];for(let i=0;i<ns;i++){scale.push(dv.getUint16(o,true));o+=2;}
  if(buf.byteLength<o+ns*n*2)throw new Error(TL('truncated_history_response'));
  const d={};
  for(let s=0;s<ns;s++){const arr=new Array(n);for(let i=0;i<n;i++){const v=dv.getInt16(o,true);o+=2;arr[i]=(v===-32768)?null:v/scale[s];}d[keys[s]]=arr;}
  return {step,n,lastAge,cap,stored,keys,d};
}
async function fetchHistory(rangeSec,points){
  const r=await fetch('/history?range='+rangeSec+'&points='+points,{cache:'no-store'});
  if(r.status===404||r.status===503){const e=new Error('history '+r.status);e.code=r.status;throw e;}
  if(!r.ok)throw new Error('HTTP '+r.status);
  return decodeHistory(await r.arrayBuffer());
}
// Inserts data from the module into HIST / LONG (in place - the arrays are used by the drawing).
function fillFrom(h,dst){
  const now=Date.now(),stepMs=h.step*1000;
  dst.t.length=0;for(let i=0;i<h.n;i++)dst.t.push(now-h.lastAge-(h.n-1-i)*stepMs);
  for(const k of Object.keys(SER)){const src=h.d[k],arr=dst.d[k]||(dst.d[k]=[]);arr.length=0;for(let i=0;i<h.n;i++)arr.push(src?src[i]:null);}
  dst.gap=Math.max(GAP,stepMs*2.5);
}
const fmtDur=sec=>{const h=Math.floor(sec/3600),m=Math.floor(sec%3600/60);return h>0?h+' h '+m+' min':m+' min';};
function showHistNote(){
  const el=$('#chNote');if(!el)return;
  if(!histInfo){el.textContent=TL('loading_history_from_the_module');return;}
  if(histInfo.none)el.textContent=histInfo.none===503
    ?TL('the_module_has_no_memory_for_history_no')
    :TL('this_firmware_does_not_store_history_the');
  else el.textContent=TL('the_history_is_stored_by_the_module_itself',{a:fmtDur(histInfo.stored),b:fmtDur(histInfo.cap)});
}
// The last hour from the module (1 s) -> HIST. Called at startup, on returning to the tab and after
// the connection is regained - so a browser in the background or closed does not leave gaps.
let histLoadedAt=0;
async function loadDeviceHistory(force){
  if(histBusy)return;
  if(!force&&Date.now()-histLoadedAt<30000)return;   // ~137 KB: not again within 30 s (returning to the tab, a short dropout)
  histBusy=true;
  try{
    const h=await fetchHistory(3600,3600);
    fillFrom(h,HIST);histInfo={stored:h.stored,cap:h.cap};histLoadedAt=Date.now();
    showHistNote();if(S)renderCurrent();
  }catch(e){if(e.code){histInfo={none:e.code};showHistNote();}}
  finally{histBusy=false;}
}
// Ranges > 60 min: the module aggregates itself (~700 points), refreshed every 20 s.
async function refreshLong(force){
  if(chRange<=3600000||longBusy)return;
  if(!force&&longRange===chRange&&Date.now()-longFetchedAt<20000)return;
  longBusy=true;
  try{
    const h=await fetchHistory(Math.round(chRange/1000),700);
    fillFrom(h,LONG);longRange=chRange;longFetchedAt=Date.now();histInfo={stored:h.stored,cap:h.cap};showHistNote();drawAll();
  }catch(e){if(e.code){histInfo={none:e.code};showHistNote();}}
  finally{longBusy=false;}
}
/* ===== events from the module (restart, fix, NTRIP, network, AgOpenWeb, steering, motor) = vertical lines on the charts ===== */
const RST_TXT={1:TL('power_on'),2:TL('external_reset'),3:TL('software_restart'),4:TL('panic_crash'),5:TL('interrupt_watchdog'),6:TL('task_watchdog'),7:TL('watchdog_2'),8:TL('wake_up_from_deep_sleep'),9:TL('brownout'),100:TL('after_ota_update'),101:TL('after_wifi_loss')};
const FIX_TXT=[TL('no_fix'),'GPS','DGPS','RTK FLOAT','RTK FIX'];
function evDesc(e){
  const v=e.v;
  switch(e.t){
    case 1:return{txt:TL('module_restart_r',{r:RST_TXT[v]||TL('unknown_reason')}),c:'--warn',w:[4,5,6,7,9].includes(v)};
    case 2:return{txt:TL('fix_quality_q',{q:FIX_TXT[v]||v}),c:v===4?'--ok':v===0?'--bad':'--warn'};
    case 3:return v?{txt:TL('ntrip_corrections_flowing'),c:'--ok'}:{txt:TL('ntrip_correction_stream_lost'),c:'--bad'};
    case 4:return v?{txt:TL('network_connected'),c:'--ok'}:{txt:TL('network_lost'),c:'--bad'};
    case 5:return v?{txt:TL('agopenweb_connection_restored'),c:'--ok'}:{txt:TL('agopenweb_no_frames_5_s'),c:'--warn'};
    case 6:return{txt:TL('agopenweb_sender_changed'),c:'--warn'};
    case 7:return{txt:v?TL('steering_engaged_2'):TL('steering_disengaged'),c:'--label'};
    case 8:return{txt:v===2?TL('motor_runaway_detector_tripped'):TL('motor_overcurrent_latch'),c:'--bad'};
  }
  return{txt:TL('event_t',{t:e.t}),c:'--label'};
}
let EVT=[],evBusy=false,evShow=true;
try{evShow=localStorage.getItem('aog.evShow')!=='0';}catch(e){}
async function loadEvents(){
  if(evBusy)return;evBusy=true;
  try{
    const r=await fetch('/events',{cache:'no-store'});if(!r.ok)return;
    const j=await r.json(),tNow=Date.now();let added=false;
    for(const e of (j.events||[])){
      const ts=tNow-e.a;
      if(EVT.some(x=>x.t===e.t&&x.v===e.v&&Math.abs(x.ts-ts)<3500))continue;   // the same entry from the previous fetch (the clock is computed from the age +-ms)
      EVT.push({ts,t:e.t,v:e.v});added=true;
    }
    if(added){EVT.sort((a,b)=>a.ts-b.ts);const lim=tNow-48*3600e3;EVT=EVT.filter(x=>x.ts>=lim).slice(-300);}
    renderPrev(j.prev);
    if(added||!EVT.length)renderEvList();
    if(added&&curTab==='charts')drawAll();
  }catch(e){}
  finally{evBusy=false;}
}
function renderEvList(){
  const card=$('#evCard'),box=$('#evList');if(!card||!box)return;
  card.hidden=!EVT.length;if(!EVT.length)return;
  box.textContent='';
  EVT.slice(-15).reverse().forEach(e=>{
    const d=evDesc(e),row=document.createElement('div');row.className='ev-row';
    const i=document.createElement('i');i.style.setProperty('--c','var('+d.c+')');
    const t=document.createElement('span');t.className='et';t.textContent=d.txt;
    const h=document.createElement('span');h.className='eh';h.textContent=fmtClock(e.ts)+' · '+fmtAgo(Date.now()-e.ts);
    row.append(i,t,h);box.appendChild(row);
  });
}
/* Events from the previous run of the module (RTC memory: survives a restart and a crash, not a power loss) */
function renderPrev(p){
  const card=$('#prevCard'),box=$('#prevList');if(!card||!box)return;
  if(!p||!Array.isArray(p.ev)||!p.ev.length){card.hidden=true;return;}
  card.hidden=false;box.textContent='';
  const hasUtc=isNum(p.utc)&&p.utc>0;   // time from GPS: only when the receiver knew the date and time before the restart
  $('#prevTitle').textContent=TL('before_the_last_restart_run_no_n_ran_for_t',{n:p.sess,t:fmtMs(p.up),e:hasUtc?TL('ended_at_about_c',{c:fmtClock(p.utc*1000)}):''});
  p.ev.slice().reverse().forEach(e=>{
    const d=evDesc(e),row=document.createElement('div');row.className='ev-row';
    const i=document.createElement('i');i.style.setProperty('--c','var('+d.c+')');
    const t=document.createElement('span');t.className='et';t.textContent=d.txt;
    const h=document.createElement('span');h.className='eh';const rel=e.b<1500?TL('just_before_the_restart'):TL('t_before_the_restart',{t:fmtAgo(e.b).replace(/^−/,'')});
    h.textContent=hasUtc?fmtClock(p.utc*1000-e.b)+' · '+rel:rel;
    row.append(i,t,h);box.appendChild(row);
  });
  $('#prevNote').textContent=TL('last_n_events_newest_on_top_times_come',{n:p.ev.length});
}
function evInRange(t0,t1){return evShow?EVT.filter(e=>e.ts>=t0&&e.ts<=t1):[];}
let chRange=300000,chPaused=false;const CH=[];
function lowerBound(a,x){let l=0,h=a.length;while(l<h){const m=(l+h)>>1;if(a[m]<x)l=m+1;else h=m;}return l;}
function niceTicks(lo,hi,n){
  const span=(hi-lo)||1,raw=span/n,mag=Math.pow(10,Math.floor(Math.log10(raw))),nr=raw/mag,step=(nr<1.5?1:nr<3?2:nr<7?5:10)*mag,out=[];
  for(let v=Math.ceil(lo/step)*step;v<=hi+step*1e-6;v+=step)out.push(+v.toFixed(10));return out;
}
const fmtRange=ms=>{if(ms>=60000){const m=ms/60000;return (Number.isInteger(m)?m:m.toFixed(1).replace('.',','))+' min';}return Math.round(ms/1000)+' s';};
const fmtClock=t=>{const d=new Date(t);return String(d.getHours()).padStart(2,'0')+':'+String(d.getMinutes()).padStart(2,'0')+':'+String(d.getSeconds()).padStart(2,'0');};
/* ---- chart helpers: a wall-clock time axis, colors with transparency, window statistics ---- */
const PL=46,PR=8;   // chart margins (left: Y-axis labels) - shared by drawing and mouse/touch
const p2=n=>String(n).padStart(2,'0');
const fmtHM=t=>{const d=new Date(t);return p2(d.getHours())+':'+p2(d.getMinutes());};
const fmtAgo=ms=>{const s=Math.max(0,Math.round(ms/1000));
  if(s<60)return '−'+s+' s';if(s<3600)return '−'+Math.floor(s/60)+' min '+p2(s%60)+' s';return '−'+Math.floor(s/3600)+' h '+p2(Math.floor(s%3600/60))+' min';};
const TSTEPS=[1e3,2e3,5e3,10e3,15e3,30e3,60e3,120e3,300e3,600e3,900e3,1800e3,3600e3,7200e3,10800e3,21600e3];
// Ticks at whole steps of LOCAL time (e.g. every 2 min: 14:02, 14:04), not counting from the moment the page was opened.
// The step is chosen so that the labels (HH:MM:SS for steps < 1 min, otherwise HH:MM) fit with a margin in the width pw.
function timeTicks(t0,t1,pw){
  const off=new Date().getTimezoneOffset()*60000,span=t1-t0;
  const step=TSTEPS.find(s=>(span/s)*((s<60e3?56:36)+16)<=pw)||TSTEPS[TSTEPS.length-1],out=[];
  for(let t=Math.ceil((t0-off)/step)*step+off;t<=t1;t+=step)out.push(t);
  return {out,step};
}
function rgba(c,a){
  let m=/^#([0-9a-f]{6})$/i.exec(c);
  if(!m){const s=/^#([0-9a-f]{3})$/i.exec(c);if(!s)return null;m=[0,s[1][0]+s[1][0]+s[1][1]+s[1][1]+s[1][2]+s[1][2]];}
  const n=parseInt(m[1],16);return 'rgba('+(n>>16)+','+((n>>8)&255)+','+(n&255)+','+a+')';
}
const fmtVal=(def,v)=>v==null?'—':(def.fixLbl?def.ticks[v]:v.toFixed(def.dec===undefined?1:def.dec)+(def.u||''));
function winStats(a,from,to){let n=0,sum=0,mn=Infinity,mx=-Infinity;for(let i=from;i<to;i++){const v=a[i];if(v==null)continue;n++;sum+=v;if(v<mn)mn=v;if(v>mx)mx=v;}return n?{n,min:mn,avg:sum/n,max:mx}:null;}
function lastIdx(a,from){for(let i=a.length-1;i>=from;i--)if(a[i]!=null)return i;return -1;}
// A "state" series (e.g. fix quality): every segment has the color of its state, so RTK stability is visible at a glance.
function drawState(ctx,sr,H,T,a,i0,X,Y,pl,pt,ph,col){
  let prev=null,run=null;const gap=H.gap||GAP;ctx.lineWidth=2;ctx.lineJoin='round';
  // fill with one rectangle per run of samples of the same state (a separate rectangle per sample would give streaks at the seams)
  const flush=xEnd=>{
    if(!run)return;
    const cc=col(sr.colorBy(run.v)),fc=rgba(cc,.18);
    if(fc)ctx.fillStyle=fc;else{ctx.globalAlpha=.18;ctx.fillStyle=cc;}
    ctx.fillRect(run.x0,run.y,Math.max(0,xEnd-run.x0),pt+ph-run.y);ctx.globalAlpha=1;run=null;
  };
  for(let i=i0;i<T.length;i++){
    const v=a[i];
    if(v==null){if(prev)flush(prev.x);prev=null;continue;}
    const x=Math.max(pl,X(T[i])),y=Y(v);
    if(prev&&T[i]-prev.t<=gap){
      if(run&&run.v!==prev.v)flush(prev.x);
      if(!run)run={v:prev.v,x0:prev.x,y:prev.y};
      ctx.strokeStyle=col(sr.colorBy(prev.v));ctx.beginPath();ctx.moveTo(prev.x,prev.y);ctx.lineTo(x,prev.y);ctx.stroke();
      if(y!==prev.y){ctx.strokeStyle=col(sr.colorBy(v));ctx.beginPath();ctx.moveTo(x,prev.y);ctx.lineTo(x,y);ctx.stroke();}
    }else if(prev)flush(prev.x);   // a gap in the data ends the run
    prev={x,y,t:T[i],v};
  }
  if(prev)flush(prev.x);
}
function drawChart(c,range,mini){
  const cv=c.cv,def=c.def,ctx=cv.getContext('2d'),dpr=window.devicePixelRatio||1,w=cv.clientWidth,h=cv.clientHeight;
  if(!w||!h)return;
  const W=Math.round(w*dpr),Hh=Math.round(h*dpr);if(cv.width!==W||cv.height!==Hh){cv.width=W;cv.height=Hh;}
  ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);
  const cs=getComputedStyle(document.documentElement),col=n=>cs.getPropertyValue(n).trim()||'#888';
  const H=(range>3600000)?LONG:HIST,now=Date.now(),t0=now-range,T=H.t,i0=Math.max(0,lowerBound(T,t0)-1);
  const pl=mini?2:46,pr=mini?2:8,pt=mini?3:8,pb=mini?3:22,pw=w-pl-pr,ph=h-pt-pb,X=t=>pl+(t-t0)/range*pw;
  let lo=Infinity,hi=-Infinity;
  for(const sr of def.series){const a=H.d[sr.k]||[];for(let i=i0;i<T.length;i++){const v=a[i];if(v!=null){if(v<lo)lo=v;if(v>hi)hi=v;}}}
  if(!isFinite(lo)){lo=0;hi=1;}
  const fLo=def.yMin!==undefined,fHi=def.yMax!==undefined;
  if(fLo)lo=def.yMin;else if(def.min0&&lo>0)lo=0;if(fHi)hi=def.yMax;
  const minSpan=def.minSpan||(def.dec===0?4:1);
  if(hi-lo<minSpan){
    // non-negative quantities (min0): the axis starts at 0 and grows upward, it does not go into negative values (corrections age, current)
    if(def.min0&&!fLo){lo=0;if(!fHi)hi=Math.max(hi,minSpan);}
    else{const m=(hi+lo)/2;if(!fLo)lo=m-minSpan/2;if(!fHi)hi=m+minSpan/2;}
    if(hi-lo<minSpan)hi=lo+minSpan;}
  const pad=(hi-lo)*0.08;if(!fLo&&!(def.min0&&lo===0))lo-=pad;if(!fHi)hi+=pad;
  const Y=v=>pt+(1-(v-lo)/(hi-lo))*ph;
  if(!mini){
    ctx.font='11px '+(cs.getPropertyValue('--mono')||'monospace');ctx.textBaseline='middle';
    for(const b of (def.bands||[])){const y1=Y(Math.min(b[1],hi)),y2=Y(Math.max(b[0],lo));if(y2>y1){ctx.globalAlpha=.13;ctx.fillStyle=col(b[2]);ctx.fillRect(pl,y1,pw,y2-y1);ctx.globalAlpha=1;}}
    const ticks=def.ticks?Object.keys(def.ticks).map(Number):niceTicks(lo,hi,4);
    ctx.lineWidth=1;
    for(const v of ticks){const y=Y(v);if(y<pt-1||y>pt+ph+1)continue;ctx.strokeStyle=col('--border');ctx.beginPath();ctx.moveTo(pl,y);ctx.lineTo(pl+pw,y);ctx.stroke();
      ctx.fillStyle=col('--label');ctx.textAlign='right';ctx.fillText(def.ticks?def.ticks[v]:(Math.abs(v)>=1000?(v/1000)+'k':String(+v.toFixed(2))),pl-6,y);}
    // time axis: wall-clock hours at equal intervals + subtle vertical lines
    const tt=timeTicks(t0,now,pw);
    ctx.fillStyle=col('--label');ctx.textBaseline='alphabetic';ctx.textAlign='center';
    for(const t of tt.out){
      const x=X(t);if(x<pl-1||x>pl+pw+1)continue;
      ctx.strokeStyle=rgba(col('--border'),.55)||col('--border');ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(x,pt);ctx.lineTo(x,pt+ph);ctx.stroke();
      const lbl=tt.step<60e3?fmtClock(t):fmtHM(t),tw=ctx.measureText(lbl).width;
      if(x-tw/2<pl-4||x+tw/2>w-1)continue;   // the label would go outside the chart: only the grid line stays
      ctx.fillText(lbl,x,h-6);
    }
    // zero line (roll, pitch, angle): clearly stronger than the grid
    if(def.zero&&lo<0&&hi>0){const y0=Y(0);ctx.strokeStyle=col('--label');ctx.globalAlpha=.65;ctx.lineWidth=1.2;ctx.beginPath();ctx.moveTo(pl,y0);ctx.lineTo(pl+pw,y0);ctx.stroke();ctx.globalAlpha=1;ctx.lineWidth=1;}
  }
  for(const sr of def.series){
    const a=H.d[sr.k]||[],color=col(sr.c);ctx.strokeStyle=color;ctx.lineWidth=mini?1.6:2;ctx.lineJoin='round';
    if(sr.colorBy){drawState(ctx,sr,H,T,a,i0,X,Y,pl,pt,ph,col);continue;}
    const seg=pts=>{
      if(!pts.length)return;
      if(pts.length===1){ctx.fillStyle=color;ctx.beginPath();ctx.arc(pts[0][0],pts[0][1],1.8,0,6.3);ctx.fill();return;}
      ctx.beginPath();pts.forEach((p,i)=>{if(!i)ctx.moveTo(p[0],p[1]);else{if(sr.step)ctx.lineTo(p[0],pts[i-1][1]);ctx.lineTo(p[0],p[1]);}});ctx.stroke();
      if(sr.fill){ctx.lineTo(pts[pts.length-1][0],pt+ph);ctx.lineTo(pts[0][0],pt+ph);ctx.closePath();
        const c0=rgba(color,mini?.22:.30),c1=rgba(color,0);   // a fill fading downward (gradient); without rgba - flat 16 %
        if(c0){const g=ctx.createLinearGradient(0,pt,0,pt+ph);g.addColorStop(0,c0);g.addColorStop(1,c1);ctx.fillStyle=g;ctx.fill();}
        else{ctx.globalAlpha=.16;ctx.fillStyle=color;ctx.fill();ctx.globalAlpha=1;}}
    };
    let pts=[],lastT=0;
    for(let i=i0;i<T.length;i++){
      const v=a[i];
      if(v==null||(pts.length&&T[i]-lastT>(H.gap||GAP))){seg(pts);pts=[];if(v==null){lastT=0;continue;}}
      pts.push([Math.max(pl,X(T[i])),Y(v)]);lastT=T[i];
    }
    seg(pts);
  }
  if(mini)return;
  const dotColor=(sr,v)=>col(sr.colorBy?sr.colorBy(v):sr.c);
  // "now" marker: a dot at the end of the line (only without a cursor and when the data is fresh)
  if(c.cur==null&&T.length){
    for(const sr of def.series){
      const a=H.d[sr.k]||[],i=lastIdx(a,i0);if(i<0||now-T[i]>(H.gap||GAP))continue;
      const x=Math.min(pl+pw,Math.max(pl,X(T[i])));
      ctx.lineWidth=3;ctx.strokeStyle=col('--panel');ctx.fillStyle=dotColor(sr,a[i]);
      ctx.beginPath();ctx.arc(x,Y(a[i]),3.5,0,6.3);ctx.stroke();ctx.fill();
    }
  }
  // events from the module: a dashed vertical line in the event color + a triangle at the top (without thumbnails)
  let evNear=null;
  if(!mini&&evShow){
    const evs=evInRange(t0,now);
    ctx.save();ctx.lineWidth=1;ctx.setLineDash([4,3]);
    for(const e of evs.slice(-80)){
      const x=X(e.ts),d=evDesc(e),cl=col(d.c);
      ctx.strokeStyle=rgba(cl,.8)||cl;ctx.beginPath();ctx.moveTo(x,pt);ctx.lineTo(x,pt+ph);ctx.stroke();
    }
    ctx.setLineDash([]);
    for(const e of evs.slice(-80)){const x=X(e.ts),d=evDesc(e);ctx.fillStyle=col(d.c);ctx.beginPath();ctx.moveTo(x-4,pt);ctx.lineTo(x+4,pt);ctx.lineTo(x,pt+6);ctx.closePath();ctx.fill();}
    ctx.restore();
    if(c.cur!=null){let best=null,bd=1e18;for(const e of evs){const dd=Math.abs(e.ts-c.cur);if(dd<bd){bd=dd;best=e;}}
      if(best&&bd<=7*range/pw)evNear=best;}
  }
  // cursor, values in the legend and the tooltip
  let j=T.length-1,cur=c.cur,cx=null;
  if(cur!=null&&T.length){j=Math.min(T.length-1,Math.max(0,lowerBound(T,cur)));if(j>0&&Math.abs(T[j-1]-cur)<Math.abs(T[j]-cur))j--;
    cx=Math.max(pl,Math.min(pl+pw,X(T[j])));ctx.strokeStyle=col('--label');ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(cx,pt);ctx.lineTo(cx,pt+ph);ctx.stroke();
    for(const sr of def.series){const v=(H.d[sr.k]||[])[j];if(v!=null){ctx.lineWidth=3;ctx.strokeStyle=col('--panel');ctx.fillStyle=dotColor(sr,v);ctx.beginPath();ctx.arc(cx,Y(v),3.5,0,6.3);ctx.stroke();ctx.fill();}}}
  c.tm.textContent='';   // the cursor time is now in the tooltip
  def.series.forEach((sr,i)=>{const v=(H.d[sr.k]||[])[j];
    c.lv[i].textContent=fmtVal(def,v);
    if(sr.colorBy)c.lk[i].style.background='var('+(v==null?sr.c:sr.colorBy(v))+')';});
  const tip=c.tip;
  if(cx!==null&&c.active){   // shared cursor: the tooltip only on the chart under the finger/mouse, the others show values in the legend
    c.tt.textContent=fmtClock(T[j])+' · '+fmtAgo(now-T[j]);
    if(evNear){const d=evDesc(evNear);c.te.hidden=false;c.te.textContent='▸ '+fmtClock(evNear.ts)+' '+d.txt;c.te.style.color='var('+d.c+')';}else c.te.hidden=true;
    def.series.forEach((sr,i)=>{const v=(H.d[sr.k]||[])[j];c.tv[i].textContent=fmtVal(def,v);c.ti[i].style.background='var('+(sr.colorBy&&v!=null?sr.colorBy(v):sr.c)+')';});
    tip.hidden=false;
    const tw=tip.offsetWidth,card=cv.parentNode;
    let left=cv.offsetLeft+cx+12;if(cx>w*0.55)left=cv.offsetLeft+cx-12-tw;
    tip.style.left=Math.max(4,Math.min(left,card.clientWidth-tw-4))+'px';tip.style.top=(cv.offsetTop+pt+2)+'px';
  }else tip.hidden=true;
  // window statistics: min / average / max (or time shares in states for fix quality)
  const ia=lowerBound(T,t0);
  if(def.fixLbl){
    const sr=def.series[0],a=H.d[sr.k]||[],cnt={};let n=0;
    for(let i=ia;i<T.length;i++){const v=a[i];if(v==null)continue;cnt[v]=(cnt[v]||0)+1;n++;}
    c.st.textContent='';
    if(!n)c.st.textContent=TL('no_data_in_this_window');
    for(const k of [4,3,2,1,0]){
      if(!cnt[k])continue;const pc=100*cnt[k]/n;
      const s=document.createElement('span'),dot=document.createElement('i');s.className='sh';dot.style.background='var('+sr.colorBy(k)+')';
      s.append(dot,document.createTextNode(def.ticks[k]+' '+(pc<1?'<1':Math.round(pc))+' %'));c.st.appendChild(s);
    }
  }else{
    def.series.forEach((sr,i)=>{
      const s=winStats(H.d[sr.k]||[],ia,T.length),fv=v=>v.toFixed(def.dec===undefined?1:def.dec);
      c.sv[i].textContent=s?(TL('min_a_avg_b_max_c',{a:fv(s.min),b:fv(s.avg),c:fv(s.max)})+(def.u||'')):TL('no_data_in_this_window');
    });
  }
}
/* ---- RTK summary above the charts: computed from the same window as the charts ---- */
const fmtSpan=ms=>{const s=Math.round(ms/1000);if(s<60)return s+' s';if(s<3600)return Math.floor(s/60)+' min '+p2(s%60)+' s';return Math.floor(s/3600)+' h '+p2(Math.floor(s%3600/60))+' min';};
function rtkSummary(range){
  const H=(range>3600000)?LONG:HIST,T=H.t,t0=Date.now()-range,ia=lowerBound(T,t0),fx=H.d.fixLvl||[],ag=H.d.ageDgps||[],gap=H.gap||GAP;
  const step=T.length>1?(T[T.length-1]-T[0])/(T.length-1):1000;
  let n=0,nf=0,best=0,runStart=0,losses=0,prevL=null,prevT=0,worst=-1;
  for(let i=ia;i<T.length;i++){
    const v=fx[i];
    if(v!=null){
      n++;const same=prevL!==null&&T[i]-prevT<=gap;
      if(v===4){nf++;if(!(same&&prevL===4))runStart=T[i];const d=T[i]-runStart+step;if(d>best)best=d;}
      else if(same&&prevL===4)losses++;      // loss of FIX = a transition from FIX to a worse state (without a gap in the data)
      prevL=v;prevT=T[i];
    }else prevL=null;
    const a=ag[i];if(a!=null&&a>worst)worst=a;
  }
  return {n,pct:n?100*nf/n:0,best,losses,worst,span:n*step};
}
const SUMT=[];
function updSummary(){
  if(!SUMT.length)return;
  const r=rtkSummary(chRange),set=(i,name,val,cls,sub)=>{const e=SUMT[i];e.style.setProperty('--c',TC[cls]);e.children[0].textContent=name;e.children[1].textContent=val;e.children[2].textContent=sub;};
  if(!r.n){set(0,TL('fix_in_window'),'—','muted',TL('no_data_2'));set(1,TL('longest_fix'),'—','muted','');set(2,TL('fix_losses'),'—','muted','');}
  else{
    set(0,TL('fix_in_window'),f(r.pct,0,' %'),r.pct>=90?'ok':r.pct>=50?'warn':'bad',TL('window_r',{r:fmtRange(chRange)}));
    set(1,TL('longest_fix'),r.best>0?fmtSpan(r.best):TL('none_2'),r.best<=0?'bad':r.best>=0.8*r.span?'ok':r.best>=0.4*r.span?'warn':'bad',TL('of_s_of_data',{s:fmtSpan(r.span)}));
    set(2,TL('fix_losses'),String(r.losses),r.losses===0?'ok':r.losses<=2?'warn':'bad',TL('fix_worse_state'));
  }
  set(3,TL('max_correction_age'),r.worst>=0?f(r.worst,1,' s'):'—',r.worst<0?'muted':r.worst>AGE_BAD_S?'bad':r.worst>AGE_WARN_S?'warn':'ok',TL('normal_about_1_s'));
}
function drawAll(){if(chPaused)return;CH.forEach(c=>drawChart(c,chRange));updSummary();}
/* ---- shared cursor: moving over one chart moves the cursor on all visible ones (one frame = one redraw) ---- */
let drawRaf=0;
const inView=el=>{const r=el.getBoundingClientRect();return r.bottom>0&&r.top<(window.innerHeight||800);};
function drawVisible(){
  if(drawRaf)return;
  drawRaf=requestAnimationFrame(()=>{drawRaf=0;
    const big=CH.find(x=>x.el.classList.contains('big'));
    (big?[big]:CH).forEach(c=>{if(big||inView(c.cv))drawChart(c,chRange);});});
}
function setCursor(t,active){CH.forEach(x=>{x.cur=t;x.active=(x===active);});drawVisible();}
const CH_RANGES=[['1 min',60e3],['5 min',300e3],['15 min',900e3],['60 min',3600e3],['6 h',21600e3],['24 h',86400e3]];
// Chart groups (the order within a group per the key list of the first series; the rest goes to "System").
const CH_GROUPS=[[TL('gps_and_rtk'),['hAccCm','fixLvl','ageDgps','sats','speed']],[TL('motion_and_steering'),['angle','curA','roll']],[TL('system_and_connectivity'),null]];
function buildCharts(){
  const bar=$('#chBar');
  try{const sv=+localStorage.getItem('aog.chRange');if(CH_RANGES.some(r=>r[1]===sv))chRange=sv;}catch(e){}   // remembered range (without pause - it would be surprising on return)
  CH_RANGES.forEach(([l,ms])=>{
    const b=document.createElement('button');b.className='pill'+(ms===chRange?' active':'');b.textContent=l;b.dataset.r=ms;
    b.onclick=()=>{chRange=ms;try{localStorage.setItem('aog.chRange',String(ms));}catch(e){}
      $$('.pill[data-r]',bar).forEach(x=>x.classList.toggle('active',+x.dataset.r===ms));chPaused=false;pb.textContent=TL('pause');pb.classList.remove('active');
      if(ms>3600000){LONG.t.length=0;for(const k in LONG.d)LONG.d[k].length=0;longRange=0;refreshLong(true);}
      drawAll();};bar.appendChild(b);});
  const pb=document.createElement('button');pb.className='pill';pb.textContent=TL('pause');pb.onclick=()=>{chPaused=!chPaused;pb.classList.toggle('active',chPaused);pb.textContent=chPaused?TL('resume'):TL('pause');};bar.appendChild(pb);
  const eb=document.createElement('button');eb.className='pill'+(evShow?' active':'');eb.textContent=TL('events');eb.title=TL('vertical_lines_restart_fix_ntrip_network');
  eb.onclick=()=>{evShow=!evShow;eb.classList.toggle('active',evShow);try{localStorage.setItem('aog.evShow',evShow?'1':'0');}catch(e){}drawAll();};bar.appendChild(eb);
  const cb=document.createElement('button');cb.className='pill';cb.textContent=TL('download_csv');cb.onclick=exportCsv;bar.appendChild(cb);
  // RTK summary bar (4 tiles)
  const sum=$('#chSum');
  for(let i=0;i<4;i++){const e=document.createElement('div');e.className='tile';e.innerHTML='<div class="tn"></div><div class="ts"></div><div class="tsub"></div>';sum.appendChild(e);SUMT.push(e);}
  const root=$('#chGrid'),listed=new Set(CH_GROUPS.flatMap(g=>g[1]||[]));
  CH_GROUPS.forEach(([title,keys])=>{
    const defs=keys?keys.map(k=>CHARTS.find(d=>d.series[0].k===k)).filter(Boolean):CHARTS.filter(d=>!listed.has(d.series[0].k));
    if(!defs.length)return;
    const h2=document.createElement('h2');h2.textContent=title;h2.className='fh2';h2.tabIndex=0;h2.setAttribute('role','button');root.appendChild(h2);
    const g=document.createElement('div');g.className='grid charts';root.appendChild(g);
    const sk='chg:'+title,fold=(shut,save)=>{g.hidden=shut;h2.classList.toggle('shut',shut);h2.setAttribute('aria-expanded',shut?'false':'true');
      if(save){const o=shutLoad();if(shut)o[sk]=1;else delete o[sk];shutSave(o);}if(!shut&&save)drawAll();};
    h2.onclick=()=>fold(!g.hidden,true);h2.onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();h2.onclick();}};
    fold(!!shutLoad()[sk],false);
    defs.forEach(def=>buildChart(def,g));
  });
  document.addEventListener('keydown',e=>{if(e.key==='Escape'){const b=CH.find(x=>x.el.classList.contains('big'));if(b)b.xb.click();}});
  if(chRange>3600000)refreshLong(true);
}
function buildChart(def,g){
  const el=document.createElement('article');el.className='card chart';
  const head=document.createElement('div');head.className='card-h';const h=document.createElement('h3');h.textContent=def.t;
  const tm=document.createElement('span');tm.className='tm';   // the cursor time is in the tooltip; the element stays only for compatibility with drawChart
  const xb=document.createElement('button');xb.className='xb';xb.type='button';xb.textContent='⤢';xb.title=TL('enlarge_to_full_screen');xb.setAttribute('aria-label',TL('enlarge_chart'));
  head.append(h,xb);
  const lg=document.createElement('div');lg.className='lg';const lv=[],lk=[];
  def.series.forEach(sr=>{const s=document.createElement('span');const i=document.createElement('i');i.style.background='var('+sr.c+')';s.appendChild(i);s.appendChild(document.createTextNode(sr.l));const b=document.createElement('b');b.textContent='—';s.appendChild(b);lg.appendChild(s);lv.push(b);lk.push(i);});
  const cv=document.createElement('canvas');
  // statistics row under the chart (min/avg/max or state shares) and the tooltip at the cursor - everything through textContent
  const st=document.createElement('div');st.className='st'+(def.fixLbl?' shares':'');const sv=[];
  if(!def.fixLbl)def.series.forEach(sr=>{
    const row=document.createElement('div');row.className='sr';const i=document.createElement('i');i.style.background='var('+sr.c+')';row.appendChild(i);
    if(def.series.length>1){const n=document.createElement('span');n.className='sn';n.textContent=sr.sn||sr.l;row.appendChild(n);}
    const v=document.createElement('span');row.appendChild(v);st.appendChild(row);sv.push(v);});
  const tip=document.createElement('div');tip.className='tip';tip.hidden=true;
  const tt=document.createElement('div');tt.className='tt';tip.appendChild(tt);const te=document.createElement('div');te.className='te';te.hidden=true;tip.appendChild(te);const tv=[],ti=[];
  def.series.forEach(sr=>{const r=document.createElement('div');r.className='tr';const i=document.createElement('i');i.style.background='var('+sr.c+')';
    const b=document.createElement('b');b.textContent='—';const n=document.createElement('span');n.textContent=sr.l;r.append(i,b,n);tip.appendChild(r);tv.push(b);ti.push(i);});
  el.append(head,lg,cv,st,tip);g.appendChild(el);
  const c={def,el,cv,tm,xb,lv,lk,st,sv,tip,tt,te,tv,ti,cur:null,active:false};CH.push(c);
  const move=e=>{const r=cv.getBoundingClientRect(),x=e.clientX-r.left,pw=r.width-PL-PR;setCursor(Date.now()-chRange+(x-PL)/pw*chRange,c);};
  cv.addEventListener('pointermove',move);cv.addEventListener('pointerdown',move);
  const out=()=>setCursor(null,null);cv.addEventListener('pointerleave',out);cv.addEventListener('pointercancel',out);
  xb.onclick=()=>{
    const on=!el.classList.contains('big');
    CH.forEach(x=>{x.el.classList.remove('big');x.xb.textContent='⤢';x.xb.title=TL('enlarge_to_full_screen');});
    if(on){el.classList.add('big');xb.textContent='✕';xb.title=TL('close_esc');}
    document.body.classList.toggle('noscroll',on);
    setCursor(null,null);requestAnimationFrame(()=>{drawChart(c,chRange);if(!on)drawAll();});
  };
}
function exportCsv(){
  const H=(chRange>3600000)?LONG:HIST,keys=Object.keys(H.d),T=H.t,rows=['czas,'+keys.join(',')];
  for(let i=0;i<T.length;i++)rows.push(new Date(T[i]).toISOString()+','+keys.map(k=>{const v=H.d[k][i];return v==null?'':(+v).toFixed(3);}).join(','));
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([rows.join('\n')],{type:'text/csv'}));a.download='aog-charts.csv';document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(a.href),1500);
  toast(TL('saved_n_samples_to_csv',{n:T.length}),'ok');
}
const HERO={cv:null,def:{series:[{k:'hAccCm',c:'--ok'}],min0:true,minSpan:2,dec:1},cur:null};

)HTML"
