// ======================================================================
// web/ui/js_backup.h
// EN: Script part 4: settings backup, AgOpenWeb senders, the rest of the Settings forms. Part of the panel page (web/web_ui.h joins the parts in order; they are one string in flash).
// PL: Skrypt cz. 4: kopia ustawień, nadawcy AgOpenWeb, reszta formularzy Ustawień. Część strony panelu (web/web_ui.h skleja części po kolei; w pamięci flash to jeden napis).
// ======================================================================
R"HTML(/* --- settings backup (Settings): export to a file and import. Without passwords and the NTRIP login (WiFi, AP, NTRIP, PIN) and without hardware calibration (angle ADC, current zero).
   The import has no write path of its own: every group goes through the existing /settings_save_* endpoint, so the same validation and authorization apply as with a manual save. --- */
const BK_FORMAT='aog-unified-settings',BK_VERSION=1;
const BK_GROUPS=[
  {id:'net',name:TL('network_and_addresses'),url:'/settings_save_network',restart:true,
   keys:{wifiSsid:'s',wifiSsid2:'s',wifiSsid3:'s',wifiSsid4:'s',apSsid:'s',localIp:'s',gateway:'s',subnet:'s',agioIp:'s',wifiFixed1:'b',wifiFixed2:'b',wifiFixed3:'b',wifiFixed4:'b'},
   params:o=>({wifiSsid:o.wifiSsid,wifiSsid2:o.wifiSsid2,wifiSsid3:o.wifiSsid3,wifiSsid4:o.wifiSsid4,apSsid:o.apSsid,localIp:o.localIp,gateway:o.gateway,subnet:o.subnet,agioIp:o.agioIp,
     fixed1:o.wifiFixed1?'1':'0',fixed2:o.wifiFixed2?'1':'0',fixed3:o.wifiFixed3?'1':'0',fixed4:o.wifiFixed4?'1':'0'}),
   optional:['wifiSsid2','wifiSsid3','wifiSsid4']},
  {id:'ntrip',name:TL('ntrip_without_login_and_password'),url:'/settings_save_ntrip',restart:true,
   keys:{ntripEnabled:'b',ntripHost:'s',ntripPort:'n',ntripMount:'s'},
   params:(o,st)=>({enNtrip:o.ntripEnabled?'1':'0',ntripHost:o.ntripHost,ntripPort:String(o.ntripPort),ntripMount:o.ntripMount,ntripUser:st.ntripUser||''})},   // the login stays current (it is not stored in the file); no password = unchanged
  {id:'features',name:TL('enabled_features'),url:'/settings_save_features',restart:true,
   keys:{enSection:'b',enMotor:'b',enCurrent:'b',enImu:'b',enLed:'b'},
   params:o=>({enSection:o.enSection?'1':'0',enMotor:o.enMotor?'1':'0',enCurrent:o.enCurrent?'1':'0',enImu:o.enImu?'1':'0',enLed:o.enLed?'1':'0'})},
  {id:'thr',name:TL('time_thresholds_and_led_brightness'),url:'/settings_save_thresholds',restart:true,
   keys:{ethGraceMs:'n',wifiTimeoutMs:'n',watchdogS:'n',ledBrightness:'n'},
   params:o=>({ethGraceMs:String(o.ethGraceMs),wifiTimeoutMs:String(o.wifiTimeoutMs),watchdogS:String(o.watchdogS),ledBrightness:String(o.ledBrightness)})},
  {id:'motor',name:TL('motor_and_runaway_detector'),url:'/settings_save_motor',
   keys:{runEn:'b',runWindowMs:'n',runMinMoveDeg:'n',runMinErrDeg:'n',motorInvert:'b'},
   params:o=>({runEn:o.runEn?'1':'0',runWindowMs:String(o.runWindowMs),runMinMoveDeg:String(o.runMinMoveDeg),runMinErrDeg:String(o.runMinErrDeg),motorInvert:o.motorInvert?'1':'0'})},
  {id:'cur',name:TL('motor_current_thresholds'),url:'/settings_save_current',
   keys:{currKickoutA:'n',currKickoutMs:'n',currHardA:'n'},
   params:o=>({currKickoutA:String(o.currKickoutA),currKickoutMs:String(o.currKickoutMs),currHardA:String(o.currHardA)})},
  {id:'range',name:TL('steering_angle_range'),url:'/settings_save_steering_range',
   keys:{steerLeftDeg:'n',steerRightDeg:'n'},params:o=>({leftDeg:String(o.steerLeftDeg),rightDeg:String(o.steerRightDeg)})},
  {id:'imu',name:TL('imu_board_rotation'),url:'/settings_save_imu_rotation',
   keys:{imuRotDeg:'n',imuRollInvert:'b'},optional:['imuRollInvert'],params:o=>({rotDeg:String(o.imuRotDeg),rollInvert:o.imuRollInvert?'1':'0'})},
  {id:'imu3d',name:TL('imu_3d_mounting'),url:'/settings_save_imu_mount3d',
   keys:{imuMount3d:'s'},optional:['imuMount3d'],params:o=>({m:o.imuMount3d})},
  {id:'eth',name:TL('ethernet_w5500'),url:'/settings_save_ethernet',
   keys:{enEthernet:'b'},optional:['enEthernet'],params:o=>({enEthernet:o.enEthernet?'1':'0'})},
  {id:'engage',name:TL('steering_engage_detection'),url:'/settings_save_agio_engage',
   keys:{cmdEngageMask:'n'},params:o=>({mask:String(o.cmdEngageMask)}),allowed:{cmdEngageMask:[1,4,5]}},
  {id:'sender',name:TL('agopenweb_sender'),url:'/settings_save_agio_sender',
   keys:{agioPinned:'s'},params:o=>({ip:o.agioPinned}),optional:['agioPinned']}
];
// From the last status: the settings object for a file (only the fields that actually came from the module).
function backupBuild(st){
  const out={};
  BK_GROUPS.forEach(g=>{for(const k in g.keys){
    if(k==='agioPinned'){out[k]=(st.agioMode==='manual'&&typeof st.agioSel==='string')?st.agioSel:'';continue;}
    if(st[k]!==undefined)out[k]=st[k];}});
  return out;
}
function backupFile(st){
  return {format:BK_FORMAT,version:BK_VERSION,firmware:st.firmwareVersion||'',exported:new Date().toISOString(),
    note:TL('a_backup_of_the_module_settings_it'),
    settings:backupBuild(st)};
}
// Validates the file and compares it with the current state. Returns {error} or {groups:[{g,changed,missing,params,diff}]} - only the SHAPE and types (the ranges are checked by the module).
function backupPlan(obj,st){
  if(!obj||typeof obj!=='object'||Array.isArray(obj)||obj.format!==BK_FORMAT)return{error:TL('this_is_not_a_settings_backup_file_of_this')};
  if(!Number.isInteger(obj.version)||obj.version<1||obj.version>BK_VERSION)return{error:TL('unsupported_file_version_v',{v:obj.version})};
  const set=obj.settings;if(!set||typeof set!=='object'||Array.isArray(set))return{error:TL('the_file_contains_no_settings')};
  const groups=BK_GROUPS.map(g=>{
    const cur=backupBuild(st),opt=g.optional||[],bad=[],diff=[];
    for(const k in g.keys){
      const t=g.keys[k],v=set[k];
      const okType=t==='s'?typeof v==='string'&&v.length<=128&&!/[\u0000-\u001f]/.test(v):t==='b'?typeof v==='boolean':typeof v==='number'&&isFinite(v);
      if(!okType){if(v===undefined&&opt.includes(k)&&cur[k]!==undefined){continue;}bad.push(k);continue;}
      if(g.allowed&&g.allowed[k]&&!g.allowed[k].includes(v)){bad.push(k);continue;}
      if(cur[k]!==undefined&&cur[k]!==v)diff.push(k);
    }
    const merged={};for(const k in g.keys)merged[k]=set[k]!==undefined?set[k]:cur[k];   // only known keys (the rest of the file is ignored)
    return{g,bad,diff,changed:!bad.length&&diff.length>0,params:bad.length?null:g.params(merged,st)};
  });
  return{groups};
}
let BKPLAN=null;
const plN=(n,k)=>{const f=String(typeof LANG[k]==='string'?LANG[k]:(typeof FB[k]==='string'?FB[k]:k)).split('|');return f[Math.min(PLURAL(n),f.length-1)];};   // plural forms in the catalog: 'single|plural' (pl: 'one|few|many')
function buildBackup(g){
  const c=card(TL('settings_backup'));
  const n=document.createElement('p');n.className='note';
  n.textContent=TL('save_the_module_settings_to_a_file_or');
  c.appendChild(n);
  btns(c,[[TL('download_settings_backup'),'primary',()=>{
    if(!S){toast(TL('no_data_from_the_module'),'bad');return;}
    const blob=new Blob([JSON.stringify(backupFile(S),null,2)],{type:'application/json'});
    const a=document.createElement('a');a.href=URL.createObjectURL(blob);
    const d=new Date(),p=x=>String(x).padStart(2,'0');
    a.download='aog-settings-'+d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+'.json';
    document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(a.href),2000);
    toast(TL('settings_backup_downloaded_without'),'ok');}],
   [TL('load_backup_from_file'),'',()=>BKUI.file.click()]]);
  const f=document.createElement('input');f.type='file';f.accept='.json,application/json';f.hidden=true;c.appendChild(f);
  const box=document.createElement('div');box.className='bk-box';box.hidden=true;c.appendChild(box);
  BKUI.file=f;BKUI.box=box;
  f.onchange=()=>{const file=f.files&&f.files[0];f.value='';if(!file)return;
    if(file.size>65536){toast(TL('the_file_is_too_large_for_a_settings'),'bad');return;}
    const r=new FileReader();r.onload=()=>{let o=null;try{o=JSON.parse(String(r.result));}catch(e){}
      if(!o){showBackupPlan({error:TL('could_not_read_the_file_it_is_not_valid')});return;}
      showBackupPlan(backupPlan(o,S||{}));};r.onerror=()=>toast(TL('could_not_read_the_file'),'bad');r.readAsText(file);};
  g.appendChild(c);
}
const BKUI={file:null,box:null};
function showBackupPlan(plan){
  const box=BKUI.box;box.textContent='';box.hidden=false;BKPLAN=null;
  if(plan.error){const e=document.createElement('div');e.className='ev-row';e.innerHTML='<i style="--c:var(--bad)"></i><span class="et"></span>';e.querySelector('.et').textContent=plan.error;box.appendChild(e);return;}
  BKPLAN=plan;
  const todo=plan.groups.filter(x=>x.changed);
  plan.groups.forEach(x=>{
    const e=document.createElement('div');e.className='ev-row';
    const i=document.createElement('i');i.style.setProperty('--c',x.bad.length?'var(--warn)':x.changed?'var(--accent)':'var(--ok)');
    const t=document.createElement('span');t.className='et';t.textContent=x.g.name;
    const h=document.createElement('span');h.className='eh';
    h.textContent=x.bad.length?TL('skipped_missing_or_invalid_fields_in_the'):x.changed?TL('will_change_n_w',{n:x.diff.length,w:plN(x.diff.length,'setting')}):TL('no_changes');
    e.append(i,t,h);box.appendChild(e);});
  const eng=S&&S.cmdEngaged;
  const msg=document.createElement('p');msg.className='note';msg.style.margin='8px 0 0';
  msg.textContent=!todo.length?TL('nothing_to_change_the_settings_in_the_file'):eng?TL('steering_is_engaged_in_agopenweb_disengage'):
    TL('changes_will_be_saved_the_same_way_as_a')+(todo.some(x=>x.g.restart)?TL('some_settings_take_effect_after_the_module'):'');
  box.appendChild(msg);
  if(todo.length&&!eng)btns(box,[[TL('restore_n_w',{n:todo.length,w:plN(todo.length,'group')}),'primary',applyBackup],[TL('cancel'),'',()=>{box.hidden=true;box.textContent='';BKPLAN=null;}]]);
}
async function applyBackup(){
  if(!BKPLAN)return;
  if(S&&S.cmdEngaged){toast(TL('disengage_steering_in_agopenweb'),'bad');return;}
  const todo=BKPLAN.groups.filter(x=>x.changed);let done=0,restart=false;
  for(const x of todo){
    const ok=await post(x.g.url,x.params,null);
    if(!ok){toast(TL('stopped_at_group_g_saved_d_of_n',{g:x.g.name,d:done,n:todo.length}),'bad');return;}
    done++;if(x.g.restart)restart=true;
  }
  BKPLAN=null;BKUI.box.hidden=true;BKUI.box.textContent='';
  toast(TL('restored_n_w',{n:done,w:plN(done,'settings_group')})+(restart?TL('restart_the_module_for_all_of_them_to_take'):''),'ok');
}
function buildSenders(g){
  const c=card(TL('agopenweb_senders_takes_effect_immediately'));
  SND.head=document.createElement('p');SND.head.className='snd-head';c.appendChild(SND.head);
  SND.list=document.createElement('div');c.appendChild(SND.list);
  SND.empty=document.createElement('p');SND.empty.className='note';SND.empty.textContent=TL('waiting_for_the_first_frame_from_agopenweb');c.appendChild(SND.empty);
  const bb=btns(c,[[TL('automatic_mode'),'',()=>sndPick('')]]);SND.autoBtn=bb.firstChild;
  const n=document.createElement('p');n.className='note';
  n.textContent=TL('the_module_talks_to_one_sender_at_a_time')+
    TL('automatic_mode_the_module_picks_the_first')+
    TL('pinned_only_the_chosen_address_even_when');
  c.appendChild(n);
  g.appendChild(c);
}
function updSenders(s){
  if(!SND.list)return;
  const arr=Array.isArray(s.agioSenders)?s.agioSenders:[];
  const manual=s.agioMode==='manual',has=typeof s.agioSel==='string'&&s.agioSel!=='0.0.0.0';
  SND.head.textContent='';
  const lead=document.createElement('span');lead.textContent=TL('the_module_talks_to');
  const who=document.createElement('b');who.textContent=has?s.agioSel:TL('nobody_waiting_for_the_first_frame');
  const how=document.createElement('span');how.textContent=has?(manual?TL('pinned_manually'):TL('chosen_automatically')):'';
  SND.head.append(lead,who,how);
  const seen={};
  arr.forEach(x=>{
    let r=SND.rows[x.ip];
    if(!r){r=mkSndRow(x.ip);SND.rows[x.ip]=r;SND.list.appendChild(r.el);}
    seen[x.ip]=1;
    const [t,cl]=sndStatus(x);
    r.el.className='snd-row'+(x.sel?' sel':'');
    r.st.textContent=t+(x.sel&&manual?TL('pinned'):'');r.st.className='snd-st '+cl;
    r.sub.textContent=TL('n_frames_last_t',{n:x.n,t:x.age<0?TL('has_not_arrived_yet'):TL('t_ago',{t:fmtMs(x.age)})})+(x.ign>0?' • '+TL('ignored_n',{n:x.ign}):'');
    if(x.sel&&manual){r.b.hidden=true;}
    else{r.b.hidden=false;r.b.textContent=x.sel?TL('pin_permanently'):TL('talk_to_this_one');r.b.className='btn'+(x.sel?'':' primary');}
  });
  for(const ip in SND.rows)if(!seen[ip]){SND.rows[ip].el.remove();delete SND.rows[ip];}
  SND.empty.hidden=arr.length>0;
  SND.autoBtn.hidden=!manual;
}
/* Settings page: titled sections (jump chips on top), cards in a plain grid in the order you read them, long explanations folded under "Details". */
const SETSEC=[];
// A settings section is a box with a title (tap = fold / unfold), a short description and a grid of cards. Only the first one starts
// open; the choice is remembered in this browser (aog.shut, key set:<id>: 1 = shut, 0 = open).
function setSecToggle(box,open,save){
  box.classList.toggle('shut',!open);
  const h=box.firstChild;h.setAttribute('aria-expanded',open?'true':'false');
  if(save&&box.dataset.sid){const o=shutLoad();o['set:'+box.dataset.sid]=open?0:1;shutSave(o);}
}
function setSection(root,id,title,desc){
  const box=el('section','secbox'),h=el('h2','sech',title);h.id='sec-'+id;box.dataset.sid=id;
  h.tabIndex=0;h.setAttribute('role','button');
  h.onclick=()=>setSecToggle(box,box.classList.contains('shut'),true);
  h.onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();h.onclick();}};
  box.appendChild(h);
  if(desc)box.appendChild(el('p','secd',desc));
  const grid=el('div','setsec');box.appendChild(grid);root.appendChild(box);
  const v=shutLoad()['set:'+id];
  setSecToggle(box,v===undefined?SETSEC.length===0:v===0,false);
  SETSEC.push([id,title]);
  return grid;
}
/* Search bar above the sections: filters the cards by their text (titles, labels, hints, placeholders); matching sections open,
   the rest hide. Clearing it restores the folded / open state from before the search. Two buttons fold / unfold all sections. */
/* "Recently edited": the last few settings cards the user changed an input in (this browser only); a button jumps to the card. */
const RECENT_KEY='aog.recent';
function recentLoad(){try{const a=JSON.parse(localStorage.getItem(RECENT_KEY)||'[]');return Array.isArray(a)?a:[];}catch(e){return [];}}
function setRecent(root,nav){
  const bar=el('div','recent');
  const draw=()=>{
    bar.textContent='';const list=recentLoad().filter(r=>r&&r.s&&r.t);bar.hidden=!list.length;if(!list.length)return;
    bar.appendChild(el('span','rl',TL('recent_settings')));
    list.forEach(r=>{const b=el('button','pill',r.t);b.type='button';b.onclick=()=>{
      const h=document.getElementById('sec-'+r.s);if(!h)return;setSecToggle(h.parentNode,true,true);
      const c=[...h.parentNode.querySelectorAll('.setsec > .card')].find(x=>{const t=x.querySelector('h3');return t&&t.textContent===r.t;});
      const tg=c||h;tg.scrollIntoView({behavior:'smooth',block:'start'});if(c){c.classList.remove('flash');void c.offsetWidth;c.classList.add('flash');}};bar.appendChild(b);});
  };
  root.addEventListener('change',e=>{
    const c=e.target.closest&&e.target.closest('.setsec > .card');if(!c)return;const h3=c.querySelector('h3'),box=c.closest('.secbox');
    if(!h3||!box||e.target.type==='search')return;
    const t=h3.textContent,s=box.dataset.sid;
    const a=recentLoad().filter(r=>!(r&&r.s===s&&r.t===t));a.unshift({s,t});
    try{localStorage.setItem(RECENT_KEY,JSON.stringify(a.slice(0,5)));}catch(x){}
    draw();
  });
  root.insertBefore(bar,nav);draw();
  return bar;
}
function setSearchBar(root,nav,rec){
  const bar=el('div','setbar'),inp=document.createElement('input');
  inp.type='search';inp.placeholder=TL('search_settings');inp.setAttribute('aria-label',TL('search_settings'));inp.autocomplete='off';
  const none=el('div','nomatch',TL('no_settings_found'));none.hidden=true;
  const boxes=()=>[...root.querySelectorAll('.secbox')];
  let before=null;
  const textOf=c=>(c.textContent+' '+[...c.querySelectorAll('input,select,textarea')].map(i=>(i.placeholder||'')+' '+(i.getAttribute('aria-label')||'')).join(' ')).toLowerCase();
  const run=()=>{
    const q=inp.value.trim().toLowerCase();
    if(!q){
      if(before){boxes().forEach((b,i)=>{b.hidden=false;setSecToggle(b,!before[i],false);});before=null;}
      root.querySelectorAll('.setsec > .card').forEach(c=>{c.hidden=false;});
      nav.hidden=false;if(rec)rec.hidden=!recentLoad().length;none.hidden=true;return;
    }
    if(!before)before=boxes().map(b=>b.classList.contains('shut'));
    nav.hidden=true;if(rec)rec.hidden=true;let any=false;
    boxes().forEach(b=>{
      let hit=0;
      b.querySelectorAll('.setsec > .card').forEach(c=>{const m=textOf(c).indexOf(q)>=0;c.hidden=!m;if(m)hit++;});
      b.hidden=!hit;if(hit){any=true;setSecToggle(b,true,false);}
    });
    none.hidden=any;
  };
  inp.oninput=run;
  inp.onkeydown=e=>{if(e.key==='Escape'){inp.value='';run();}};
  const mk=(txt,open)=>{const b=el('button','pill',txt);b.type='button';
    b.onclick=()=>{if(inp.value){inp.value='';run();}boxes().forEach(x=>setSecToggle(x,open,true));};return b;};
  bar.append(inp,mk(TL('collapse_all'),false),mk(TL('expand_all'),true));
  root.insertBefore(bar,rec||nav);root.appendChild(none);
  return run;
}
/* Long explanations fold under "Details" (a tap opens them). Status messages and warnings (bad / warn classes, hidden ones) stay visible. */
function foldLong(root){
  root.querySelectorAll('.note,.hint,.secd,ol.steps').forEach(p=>{
    if(p.hidden||p.id==='restartHint'||p.id==='featHint'||p.closest('details')||p.textContent.length<100||/\b(bad|warn|ok|active)\b/.test(p.className))return;
    const d=el('details','fold'),sm=el('summary','',TL('show_details'));
    p.replaceWith(d);d.append(sm,p);
  });
}
/* Cards fold by tapping their title (the chevron on the right); the choice is remembered in this browser. Two small buttons above the
   grid fold / unfold all of them. Diagnostics cards keep their on/off switch visible when folded. */
const SHUT_KEY='aog.shut';
function shutLoad(){try{return JSON.parse(localStorage.getItem(SHUT_KEY)||'{}')||{};}catch(e){return {};}}
function shutSave(o){try{localStorage.setItem(SHUT_KEY,JSON.stringify(o));}catch(e){}}
function cardFolding(grid){
  const sid=grid.id,cards=()=>[...grid.querySelectorAll('.card')].filter(c=>c.querySelector('h3'));
  const keyOf=c=>sid+':'+c.querySelector('h3').dataset.k;
  const apply=(c,shut,save)=>{
    c.classList.toggle('shut',shut);c.querySelector('h3').setAttribute('aria-expanded',shut?'false':'true');
    if(save){const o=shutLoad();if(shut)o[keyOf(c)]=1;else delete o[keyOf(c)];shutSave(o);}
  };
  const st=shutLoad();
  cards().forEach(c=>{
    const h=c.querySelector('h3');h.dataset.k=h.textContent;h.classList.add('fh');h.tabIndex=0;h.setAttribute('role','button');
    const t=()=>apply(c,!c.classList.contains('shut'),true);
    h.onclick=t;h.onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();t();}};
    apply(c,!!st[keyOf(c)],false);
  });
  const bar=el('div','foldbar'),mk=(txt,shut)=>{const b=el('button','pill',txt);b.type='button';
    b.onclick=()=>{const o=shutLoad();cards().forEach(c=>{c.classList.toggle('shut',shut);c.querySelector('h3').setAttribute('aria-expanded',shut?'false':'true');
      if(shut)o[keyOf(c)]=1;else delete o[keyOf(c)];});shutSave(o);};return b;};
  bar.append(mk(TL('collapse_all'),true),mk(TL('expand_all'),false));
  grid.parentNode.insertBefore(bar,grid);
}
function buildSet(){
  const root=$('#setGrid'),wifiCards=[];
  root.className='setpage';
  const nav=el('div','setnav');root.appendChild(nav);
  const g=setSection(root,'network',TL('sec_network'),TL('up_to_4_wifi_networks_the_module_connects'));
  let c=card(TL('network_save_requires_restart'));   // the last card of the section: one Save for everything above it
  const netSave=c;
  for(let i=1;i<=4;i++){
    const sfx=i===1?'':String(i), req=i===1;
    const w=card(req?TL('wifi_1_required'):TL('wifi_n_optional',{n:i}));
    w.append(fld('setWifiSsid'+sfx,'SSID','text',{max:32,check:v=>req&&v.trim()===''?TL('enter_the_network_name'):null}),
      fld('setWifiPass'+sfx,TL('password'),'password',{pw:true,max:63,hint:i>1?TL('empty_ssid_unused_slot'):undefined,
        check:v=>(!req&&INP['setWifiSsid'+sfx].value.trim()==='')||v.length===0||(v.length>=8&&v.length<=63)?null:TL('empty_open_network_or_8_63_characters')}),
      sw('setWifiFixed'+sfx,TL('force_a_fixed_ip_address_for_this_network')));
    wifiCards.push(w);
  }
  const apCard=card(TL('access_point_ap'));
  apCard.append(fld('setApSsid',TL('ap_ssid_own_access_point'),'text',{max:32,check:v=>v.trim()===''?TL('enter_a_name'):null}),
    fld('setApPass',TL('ap_password'),'password',{pw:true,noClear:true,max:63,hint:TL('8_63_characters_wpa2_requirement_empty'),check:v=>(v.length===0&&INP.setApPass.dataset.pwset==='1')||(v.length>=8&&v.length<=63)?null:TL('8_63_characters_required_otherwise_the_ap')}));
  const ipCard=card(TL('fixed_address_and_broadcast'));
  ipCard.append(fld('setLocalIp',TL('module_ip_address'),'text',{check:chkIp}),fld('setGateway',TL('gateway_2'),'text',{check:chkIp}),
    fld('setSubnet',TL('subnet_mask'),'text',{check:chkMask}),fld('setAgioIp',TL('agopenweb_broadcast_address'),'text',{check:chkIp}));
  const nb=btns(c,[[TL('save_network'),'primary',async()=>{
    if(!validate(['setWifiSsid','setWifiPass','setWifiSsid2','setWifiPass2','setWifiSsid3','setWifiPass3','setWifiSsid4','setWifiPass4','setApSsid','setApPass','setLocalIp','setGateway','setSubnet','setAgioIp']))return;
    if(!(await askConfirm(TL('confirm_save_network'))))return;
    const dh={};for(let i=1;i<=4;i++)dh['fixed'+i]=INP['setWifiFixed'+(i===1?'':i)].checked?'1':'0';
    const ok=await post('/settings_save_network',Object.assign(vals({wifiSsid:'setWifiSsid',wifiPass:'setWifiPass',wifiSsid2:'setWifiSsid2',wifiPass2:'setWifiPass2',
      wifiSsid3:'setWifiSsid3',wifiPass3:'setWifiPass3',wifiSsid4:'setWifiSsid4',wifiPass4:'setWifiPass4',
      apSsid:'setApSsid',apPass:'setApPass',localIp:'setLocalIp',gateway:'setGateway',subnet:'setSubnet',agioIp:'setAgioIp'}),dh),TL('saved_restart_the_module_for_it_to_take'));
    if(ok){$('#restartHint').hidden=false;refreshPw();}}],[TL('restart_now'),'',restart]]);
  const hint=document.createElement('p');hint.className='note';hint.id='restartHint';hint.hidden=true;hint.textContent=TL('network_changes_take_effect_after_a');c.appendChild(hint);
  wifiCards.forEach(w=>g.appendChild(w));g.appendChild(apCard);g.appendChild(ipCard);g.appendChild(netSave);
  const gA=setSection(root,'agopenweb',TL('sec_agopenweb'));
  buildSenders(gA);
  buildEngage(gA);
  const gS=setSection(root,'security',TL('sec_security'));
  buildAuthCard(gS);
  buildBackup(gS);
  // ntrip
  const gN=setSection(root,'ntrip',TL('sec_ntrip'));
  c=card(TL('own_ntrip_client_takes_effect_immediately'));
  c.append(sw('enNtrip',TL('enable_own_ntrip_client')),fld('setNtripHost',TL('host_caster'),'text',{max:80,check:v=>v.trim()===''?TL('enter_the_caster_address'):(/\s/.test(v)?TL('no_spaces'):null)}),
    fld('setNtripPort',TL('port'),'number',{check:chkNum(1,65535)}),fld('setNtripMount',TL('mountpoint'),'text',{max:60,check:v=>/\s/.test(v)?TL('no_spaces'):null}),
    fld('setNtripUser',TL('user'),'text',{max:60,check:v=>v.includes(':')?TL('no_colon_it_breaks_basic_login'):null}),fld('setNtripPass',TL('password'),'password',{pw:true,max:60}));
  btns(c,[[TL('save'),'primary',()=>{if(!validate(['setNtripHost','setNtripPort','setNtripMount','setNtripUser']))return;
    post('/settings_save_ntrip',{enNtrip:INP.enNtrip.checked?'1':'0',...vals({ntripHost:'setNtripHost',ntripPort:'setNtripPort',ntripMount:'setNtripMount',ntripUser:'setNtripUser',ntripPass:'setNtripPass'})},TL('ntrip_settings_saved')).then(ok=>{if(ok)refreshPw();});}]]);
  gN.appendChild(c);
  // features
  const gH=setSection(root,'hardware',TL('sec_hardware'));
  c=card(TL('features_take_effect_immediately'));
  [['enSection',TL('section_expander_mcp23017')],['enMotor',TL('motor_driver_cytron')],['enCurrent',TL('current_sensor')],['enImu','IMU (BNO085)'],['enLed',TL('rgb_led_status')]].forEach(([id,l])=>c.appendChild(sw(id,l)));
  const fh=document.createElement('div');fh.className='hint';fh.id='featHint';
  fh.textContent=TL('the_motor_driver_and_current_sensor_work');
  c.appendChild(fh);
  btns(c,[[TL('save'),'primary',async()=>{const p={};['enSection','enMotor','enCurrent','enImu','enLed'].forEach(k=>p[k]=INP[k].checked?'1':'0');
    // switching the current protection or the motor driver off is the risky part: say so
    const off=S&&((S.currentSensEn&&!INP.enCurrent.checked)||(S.motorDriverEn&&!INP.enMotor.checked));
    if(!(await askConfirm(off?TL('confirm_features_protection_off'):TL('confirm_save_features'),!!off)))return;
    await post('/settings_save_features',p,TL('features_saved'));}]]);
  gH.appendChild(c);
  // thresholds
  c=card(TL('ethernet_w5500_takes_effect_after_a_restart'));
  c.appendChild(sw('enEthernet',TL('use_ethernet_w5500')));
  const eh=document.createElement('div');eh.className='hint';eh.textContent=TL('needs_the_w5500_wired_if_ethernet_does_not');c.appendChild(eh);
  btns(c,[[TL('save'),'primary',async()=>{if(!(await askConfirm(TL('confirm_save_ethernet'))))return;
    await post('/settings_save_ethernet',{enEthernet:INP.enEthernet.checked?'1':'0'},TL('ethernet_setting_saved_restart_to_apply'));}]]);
  gH.appendChild(c);
  c=card(TL('thresholds_and_times'));
  c.append(fld('setEthGrace',TL('ethernet_how_many_ms_to_wait_before'),'number',{check:chkNum(1000,60000,'ms')}),
    fld('setWifiTimeout',TL('wifi_timeout_before_falling_back_to_ap_ms'),'number',{check:chkNum(3000,120000,'ms')}),
    fld('setWatchdogS',TL('watchdog_timeout_s_requires_restart'),'number',{hint:TL('recommended_3_10_s_with_a_motor_connected'),check:chkNum(3,300,'s')}),
    fld('setLedBrightness',TL('rgb_led_brightness_takes_effect'),'number',{check:chkNum(0,255)}));
  btns(c,[[TL('save'),'primary',async()=>{if(!validate(['setEthGrace','setWifiTimeout','setWatchdogS','setLedBrightness']))return;
    if(!(await askConfirm(TL('confirm_save_thresholds'))))return;
    await post('/settings_save_thresholds',vals({ethGraceMs:'setEthGrace',wifiTimeoutMs:'setWifiTimeout',watchdogS:'setWatchdogS',ledBrightness:'setLedBrightness'}),TL('saved_the_watchdog_needs_a_restart'));}]]);
  gH.appendChild(c);
  // logging
  const gL=setSection(root,'logging',TL('sec_logging'));
  c=card(TL('diagnostic_logging_debug'));
  const fl=document.createElement('div');fl.className='flags';c.appendChild(fl);
  FLAGS.forEach(([k,l])=>{const r=sw('dbg_'+k,l);r.querySelector('input').addEventListener('change',e=>{flagState[k]=e.target.checked;saveFlags();});fl.appendChild(r);});
  const nt=document.createElement('p');nt.className='note';nt.textContent=TL('a_change_is_saved_immediately_you_will_see');c.appendChild(nt);
  gL.appendChild(c);
  // module
  const gM=setSection(root,'module',TL('sec_module'));
  c=card(TL('module'));
  btns(c,[[TL('restart'),'',restart],[TL('ota_update'),'',()=>{location.href='/update';}],[TL('restore_everything_to_defaults'),'danger',factoryReset]]);
  gM.appendChild(c);
  // jump chips on top + folding of the long explanations
  SETSEC.forEach(([id,t])=>{const b=el('button','pill',t);b.onclick=()=>{const h=document.getElementById('sec-'+id);if(!h)return;setSecToggle(h.parentNode,true,true);h.scrollIntoView({behavior:'smooth',block:'start'});};nav.appendChild(b);});
  foldLong(root);
  const rec=setRecent(root,nav);
  setSearchBar(root,nav,rec);
}
let settingsFilled=false;
function fillSettings(s,force){
  if(settingsFilled&&!force)return;settingsFilled=true;
  const set=(id,v)=>{if(INP[id])INP[id].value=(v===undefined||v===null)?'':v;};
  set('setWifiSsid',s.wifiSsid);
  set('setWifiSsid2',s.wifiSsid2);
  set('setWifiSsid3',s.wifiSsid3);
  set('setWifiSsid4',s.wifiSsid4);
  for(let i=1;i<=4;i++){const el=INP['setWifiFixed'+(i===1?'':i)];if(el)el.checked=!!s['wifiFixed'+i];}
  set('setApSsid',s.apSsid);
  set('setCurKickA',s.currKickoutA);set('setCurKickMs',s.currKickoutMs);set('setCurHardA',s.currHardA);
  set('setRunWindowMs',s.runWindowMs);set('setRunMinMoveDeg',s.runMinMoveDeg);set('setRunMinErrDeg',s.runMinErrDeg);
  if(INP.setMotorInvert)INP.setMotorInvert.checked=!!s.motorInvert;if(INP.setRunEn)INP.setRunEn.checked=!!s.runEn;
  set('setLocalIp',s.localIp);set('setGateway',s.gateway);set('setSubnet',s.subnet);set('setAgioIp',s.agioIp);
  INP.enNtrip.checked=!!s.ntripEnabled;set('setNtripHost',s.ntripHost);set('setNtripPort',s.ntripPort);set('setNtripMount',s.ntripMount);
  set('setNtripUser',s.ntripUser);applyPwState(s);
  ['enSection','enMotor','enCurrent','enImu','enLed'].forEach(k=>INP[k].checked=!!s[k]);if(INP.enEthernet)INP.enEthernet.checked=!!s.enEthernet;
  set('setEthGrace',s.ethGraceMs);set('setWifiTimeout',s.wifiTimeoutMs);set('setWatchdogS',s.watchdogS);set('setLedBrightness',s.ledBrightness);
  set('setSteerLeftDeg',s.steerLeftDeg);set('setSteerRightDeg',s.steerRightDeg);if(INP.setImuRollInvert)INP.setImuRollInvert.checked=!!s.imuRollInvert;
}
function syncActiveWifiBadge(s){
  for(let i=1;i<=4;i++){
    const sfx=i===1?'':String(i),inp=INP['setWifiSsid'+sfx];if(!inp)continue;
    let badge=inp.parentElement.querySelector('.hint.active');
    const isActive=!s.apMode&&s.netType==='WiFi'&&s.netConnected&&s.wifiActiveSlot===i;
    if(isActive&&!badge){badge=document.createElement('span');badge.className='hint active';badge.textContent=TL('active_now');inp.parentElement.appendChild(badge);}
    if(!isActive&&badge)badge.remove();
  }
}
function syncFlagSwitches(){FLAGS.forEach(([k])=>{const i=INP['dbg_'+k];if(i)i.checked=!!flagState[k];});}
async function restart(){
  if(!(await askConfirm(TL('really_restart_the_device'))))return;
  const ov=()=>overlay(TL('restarting'),TL('the_page_will_refresh_by_itself_when_the'));
  authFetch('/restart',{method:'POST'}).then(r=>{if(r.ok)ov();else toast(TL('restart_not_performed')+(r.status===401?TL('login_required'):r.status===409?TL('steering_is_engaged_disengage_it_in'):' (HTTP '+r.status+')'),'bad');}).catch(ov);
}
async function factoryReset(){
  if(!(await askConfirm(TL('restore_all_settings_to_defaults_this'),true)))return;
  const ov=()=>overlay(TL('restoring_default_settings'),TL('the_module_will_restart_if_the_network'));
  authFetch('/factory_reset',{method:'POST'}).then(r=>{if(r.ok)ov();else toast(TL('restore_not_performed')+(r.status===401?TL('login_required'):r.status===409?TL('steering_is_engaged_disengage_it_in'):' (HTTP '+r.status+')'),'bad');}).catch(ov);
}
let rebooting=false;
function overlay(t,s){rebooting=true;const o=document.createElement('div');o.className='overlay';o.innerHTML='<div class="box"><h3 style="margin:0 0 8px;font-size:16px"></h3><p style="margin:0;color:var(--label)"></p></div>';
  o.querySelector('h3').textContent=t;o.querySelector('p').textContent=s;document.body.appendChild(o);
  // If the module never goes down (the request did not arrive) or does not come back, do not leave a black screen for ever.
  setTimeout(()=>{if(!o.isConnected)return;o.remove();rebooting=false;if(wasDown)setOffline(true);},60000);}

)HTML"
