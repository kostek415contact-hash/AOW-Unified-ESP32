// ======================================================================
// web/ui/js_core.h
// EN: Script part 1: translations (TL), utilities, panel protection (PIN / password). Part of the panel page (web/web_ui.h joins the parts in order; they are one string in flash).
// PL: Skrypt cz. 1: tłumaczenia (TL), narzędzia, ochrona panelu (PIN / hasło). Część strony panelu (web/web_ui.h skleja części po kolei; w pamięci flash to jeden napis).
// ======================================================================
R"HTML(<script>
'use strict';
/* ================= translations (i18n) ================= */
// TL(key, {param}) returns the text from the catalog of the selected language, and when it is missing there - from English, and when missing there too - the key itself.
// The keys are not any language (e.g. 'log_in_to_the_panel'); the texts are in web/lang/lang_<code>.h. Variables in the text: {name}.
// Rules for translators and developers: web/lang/README.md.
const CAT=(window.AOG_CAT&&typeof window.AOG_CAT==='object')?window.AOG_CAT:{};
const LCODE=document.documentElement.lang||'en',LANG=CAT[LCODE]||{},FB=CAT.en||{};
const TL=(k,p)=>{let r=LANG[k];if(typeof r!=='string')r=FB[k];if(typeof r!=='string')r=k;return p?r.replace(/\{(\w+)\}/g,(m,x)=>(x in p)?String(p[x]):m):r;};
// plural: Polish has 3 forms (1 / 2-4 / 5+), the other languages 2 (1 / the rest); a language with different forms = add a rule here
const PLURAL=n=>LCODE==='pl'?(n===1?0:(n%10>=2&&n%10<=4&&(n%100<10||n%100>=20))?1:2):(n===1?0:1);
/* --- translating firmware messages (the log, errors sent back by the module and marked in code as MSG, the restart reason) ---
   The firmware writes in English. AOG_LOGSRC holds the printf templates from the code (log_* keys) and the words inserted as %s
   (logarg_*); the translations (including Polish) live in the language catalog under the same keys ({1} {2}... = successive arguments).
   A line with no match stays unchanged (e.g. when someone changed the text in the code without adding a translation). */
const LOGARG=Object.create(null),LOGM=Object.create(null),LOGCACHE=new Map();
(function(){
  const S=window.AOG_LOGSRC;if(!S)return;
  const SPEC=/%%|%[-+ #0]*\d*(?:\.\d+)?(?:ll|l|z|h)?([diuxXcsfeEgG])/g,esc=t=>t.replace(/[.*+?^${}()|[\]\\\/]/g,'\\$&');
  const RX={d:'([-+]?\\d+)',i:'([-+]?\\d+)',u:'(\\d+)',x:'([0-9A-Fa-f]+)',X:'([0-9A-Fa-f]+)',c:'(.)',s:'(.*?)',f:'([-+]?\\d+(?:\\.\\d+)?|-?nan|-?inf)'};
  for(const k in S){
    if(k.indexOf('logarg_')===0){if(typeof (LANG[k]||FB[k])==='string')LOGARG[S[k]]=LANG[k]||FB[k];continue;}
    if(k.indexOf('log_')!==0&&k.indexOf('msg_')!==0)continue;
    const t=S[k];let re='',last=0,m;SPEC.lastIndex=0;
    while((m=SPEC.exec(t))){re+=esc(t.slice(last,m.index))+(m[0]==='%%'?'%':(RX[m[1]]||RX.s));last=SPEC.lastIndex;}
    re+=esc(t.slice(last));
    const tag=(t.match(/^\[[^\]]+\]/)||[''])[0];
    (LOGM[tag]||(LOGM[tag]=[])).push({re:new RegExp('^'+re+'$'),k:k,n:t.length});
  }
  for(const tag in LOGM)LOGM[tag].sort((a,b)=>b.n-a.n);
})();
const argTr=v=>(v in LOGARG)?LOGARG[v]:v;
function logTr(line){
  let r=LOGCACHE.get(line);if(r!==undefined)return r;
  r=line;const arr=LOGM[(line.match(/^\[[^\]]+\]/)||[''])[0]];
  if(arr)for(const m of arr){const x=m.re.exec(line);if(!x)continue;const tpl=LANG[m.k]||FB[m.k];
    if(typeof tpl==='string')r=tpl.replace(/\{(\d+)\}/g,(q,n)=>(+n<x.length)?argTr(x[+n]):q);break;}
  if(LOGCACHE.size>4000)LOGCACHE.clear();
  LOGCACHE.set(line,r);return r;
}
/* ================= utilities ================= */
const $=(q,r=document)=>r.querySelector(q), $$=(q,r=document)=>Array.from(r.querySelectorAll(q));
function i18nStatic(){
  $$('[data-i18n]').forEach(el=>{el.textContent=TL(el.dataset.i18n);});
  $$('[data-i18n-html]').forEach(el=>{el.innerHTML=TL(el.dataset.i18nHtml);});
  $$('[data-i18n-attr]').forEach(el=>{const p=el.dataset.i18nAttr.split(':');el.setAttribute(p[0],TL(p[1]));});
}
i18nStatic();
(function(){const sel=$('#langSelect');if(!sel)return;const cur=document.documentElement.lang||'pl';
  (window.AOG_LANGS||[['pl','Polski']]).forEach(x=>{const o=document.createElement('option');o.value=x[0];o.textContent=x[1];sel.appendChild(o);});
  sel.value=cur;sel.onchange=()=>{try{localStorage.setItem('lang',sel.value);}catch(e){}location.reload();};})();
const isNum=v=>typeof v==='number'&&isFinite(v);
const f=(v,d=1,u='')=>isNum(v)?v.toFixed(d)+u:'—';
const fmtMs=ms=>!isNum(ms)||ms<0?'—':ms<1000?Math.round(ms)+' ms':ms<60000?(ms/1000).toFixed(1)+' s':Math.round(ms/60000)+' min';
const fmtUp=s=>{if(!isNum(s))return '—';const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h>0?h+' h '+String(m).padStart(2,'0')+' min':m>0?m+' min '+String(s%60).padStart(2,'0')+' s':s+' s';};
const fmtKB=b=>!isNum(b)?'—':b<1024?b+' B':b<1048576?(b/1024).toFixed(1)+' KB':(b/1048576).toFixed(2)+' MB';
const yn=(v,a=TL('yes'),b=TL('no'))=>v?a:b;
const bitsCount=n=>{let c=0;n=n>>>0;while(n){c+=n&1;n>>>=1;}return c;};
const FIXN={0:TL('none'),1:'GPS',2:'DGPS',4:'RTK FIX',5:'RTK FLOAT'};
const fixName=q=>FIXN[q]!==undefined?FIXN[q]:String(q);
const fixCls=q=>q===4?'ok':q>=1?'warn':'bad';   // 4=RTK FIX (ok), 5=FLOAT/2=DGPS/1=GPS (warning), 0=none
let S=null;                       // last status
const H={'Content-Type':'application/x-www-form-urlencoded'};

function toast(msg,cls){
  const t=document.createElement('div');t.className='toast';
  t.style.setProperty('--c',cls==='bad'?'var(--bad)':cls==='warn'?'var(--warn)':'var(--ok)');t.textContent=msg;
  $('#toast').appendChild(t);setTimeout(()=>t.remove(),(cls==='bad'||cls==='warn')?6500:2600);
}
/* ===== panel protection: PIN / password (firmware: web/panel_auth.h) ===== */
let authTok='';try{authTok=sessionStorage.getItem('authTok')||'';}catch(e){}
function setTok(t){authTok=t||'';try{if(t)sessionStorage.setItem('authTok',t);else sessionStorage.removeItem('authTok');}catch(e){}if(typeof updAuthBtn==='function')updAuthBtn(S);}
/* Header button: Log in / Log out, shown only while the panel is protected by a PIN or password. */
function updAuthBtn(s){
  const b=document.getElementById('authBtn');if(!b)return;
  const prot=!!s&&(s.authMode==='pin'||s.authMode==='password');
  b.hidden=!prot;if(!prot)return;
  b.textContent=authTok?TL('log_out'):TL('log_in');
  b.classList.toggle('in',!!authTok);
  b.title=(authTok?TL('logged_in'):TL('not_logged_in')).replace(/^[\s•]+/,'');
}
(function(){const b=document.getElementById('authBtn');if(!b)return;
  b.onclick=async()=>{
    if(authTok){setTok('');toast(TL('logged_out'),'ok');if(typeof updAuthCard==='function')updAuthCard(S);}
    else if(await askLogin()&&typeof updAuthCard==='function')updAuthCard(S);
  };})();
function el(tag,cls,txt){const e=document.createElement(tag);if(cls)e.className=cls;if(txt!==undefined)e.textContent=txt;return e;}
let dlgN=0;
function authModal(title,build){
  const prev=document.activeElement;   // focus goes back there when the dialog closes
  const o=el('div','overlay'),b=el('div','box auth'),h=el('h3',null,title);
  h.id='dlgT'+(++dlgN);b.setAttribute('role','dialog');b.setAttribute('aria-modal','true');b.setAttribute('aria-labelledby',h.id);
  b.appendChild(h);o.appendChild(b);
  const close=()=>{o.remove();try{if(prev&&prev.focus&&document.contains(prev))prev.focus();}catch(e){}};
  // Tab / Shift+Tab stay inside the dialog (the page behind it is not reachable from the keyboard)
  o.addEventListener('keydown',e=>{if(e.key!=='Tab')return;
    const f=[...b.querySelectorAll('button,input,select,textarea,[tabindex]:not([tabindex="-1"])')].filter(x=>!x.disabled&&x.offsetParent!==null);if(!f.length)return;
    const a=f[0],z=f[f.length-1];
    if(!b.contains(document.activeElement)||(e.shiftKey&&document.activeElement===a)){e.preventDefault();(e.shiftKey?z:a).focus();}
    else if(!e.shiftKey&&document.activeElement===z){e.preventDefault();a.focus();}});
  build(b,close);document.body.appendChild(o);
  setTimeout(()=>{if(!b.contains(document.activeElement)){const x=b.querySelector('input,button');if(x)x.focus();}},80);   // a dialog that did not take focus itself
  return o;
}
/* Own confirmation dialog instead of window.confirm(): the browser adds "Don't allow this page to prompt you again" to confirm(), and once it is ticked
   every confirm() silently answers "no" - Restart, Reset and the calibration buttons would stop working without any message. */
function askConfirm(text,danger){
  return new Promise(res=>{
    const o=authModal(TL('confirm_action'),(b,close)=>{
      b.appendChild(el('p','note',text));
      const ok=el('button','btn '+(danger?'danger':'primary'),TL('confirm_action')),no=el('button','btn',TL('cancel')),row=el('div','btns');row.append(no,ok);b.appendChild(row);
      const done=v=>{close();document.removeEventListener('keydown',key,true);res(v);};
      const key=e=>{if(e.key==='Escape'){e.stopPropagation();done(false);}};
      document.addEventListener('keydown',key,true);
      ok.onclick=()=>done(true);no.onclick=()=>done(false);
      setTimeout(()=>no.focus(),50);   // the safe answer is the default one
    });
    o.addEventListener('click',e=>{if(e.target===o)o.querySelector('.btns button').click();});   // a tap outside = cancel
  });
}
let loginP=null;
function askLogin(){
  if(loginP)return loginP;
  const pin=!!(S&&S.authMode==='pin');
  loginP=new Promise(res=>{
    authModal(TL('log_in_to_the_panel'),(b,close)=>{
      b.appendChild(el('p','note',(pin?TL('enter_the_panel_pin_to_change_settings'):TL('enter_the_panel_password_to_change'))));
      const inp=el('input');inp.type='password';inp.autocomplete='current-password';if(pin)inp.inputMode='numeric';inp.placeholder=pin?'PIN':TL('password');b.appendChild(inp);
      const err=el('p','auth-err');b.appendChild(err);
      const ok=el('button','btn primary',TL('log_in')),no=el('button','btn',TL('cancel')),row=el('div','btns');row.append(ok,no);b.appendChild(row);
      const done=v=>{close();loginP=null;res(v);};
      const go=async()=>{
        ok.disabled=true;err.textContent='';
        try{
          const r=await fetch('/auth_login',{method:'POST',headers:H,body:new URLSearchParams({secret:inp.value})}),t=(await r.text()).trim();
          if(r.ok){setTok(t);done(true);return;}
          err.textContent=logTr(t)||TL('error_n',{n:r.status});inp.select();
        }catch(e){err.textContent=TL('no_connection_to_the_module');}
        ok.disabled=false;
      };
      ok.onclick=go;no.onclick=()=>done(false);
      inp.onkeydown=e=>{if(e.key==='Enter')go();else if(e.key==='Escape')done(false);};
      setTimeout(()=>inp.focus(),50);
    });
  });
  return loginP;
}
// fetch with the login token; after a 401 it asks for the PIN/password and retries once
async function authFetch(path,opts){
  opts=opts||{};
  for(let i=0;;i++){
    const h=Object.assign({},opts.headers||{});if(authTok)h['X-Auth']=authTok;
    const r=await fetch(path,Object.assign({},opts,{headers:h}));
    if(r.status!==401||i>=1)return r;
    setTok('');
    if(!(await askLogin()))return r;
  }
}
let authLater=false;try{authLater=sessionStorage.getItem('authLater')==='1';}catch(e){}
let authOpen=false;
function openAuthSetup(first){
  if(authOpen)return;authOpen=true;
  let mode='pin';
  authModal(first?TL('secure_the_panel'):TL('panel_security'),(b,close)=>{
    b.appendChild(el('p','note',first
      ?TL('this_is_the_first_time_the_panel_is')
      :TL('choose_how_to_protect_settings_changes_the')));
    const opts=[['pin','PIN'],['password',TL('password')],['off',TL('no_protection')]],bs={},seg=el('div','seg');
    opts.forEach(([k,t])=>{const x=el('button','btn',t);x.onclick=()=>{mode=k;sync();};bs[k]=x;seg.appendChild(x);});
    b.appendChild(seg);
    const hint=el('p','note');b.appendChild(hint);
    const prot=!!(S&&(S.authMode==='pin'||S.authMode==='password'));
    const cur=el('input');cur.type='password';cur.autocomplete='current-password';cur.placeholder=S&&S.authMode==='pin'?TL('current_pin'):TL('current_password');if(S&&S.authMode==='pin')cur.inputMode='numeric';
    if(prot){b.appendChild(el('p','note',TL('first_confirm_it_is_you_enter_the_current')));b.appendChild(cur);}
    const a=el('input'),c2=el('input'),wrap=el('div');[a,c2].forEach(i=>{i.type='password';i.autocomplete='new-password';});
    wrap.append(a,c2);b.appendChild(wrap);
    const err=el('p','auth-err');b.appendChild(err);
    const sv=el('button','btn primary',TL('save')),cl=el('button','btn',first?TL('later'):TL('cancel')),row=el('div','btns');row.append(sv,cl);b.appendChild(row);
    function sync(){
      opts.forEach(([k])=>bs[k].classList.toggle('on',k===mode));
      wrap.hidden=mode==='off';
      a.placeholder=mode==='pin'?TL('pin_4_8_digits'):TL('password_8_63_characters');c2.placeholder=TL('repeat');
      a.inputMode=c2.inputMode=mode==='pin'?'numeric':'text';
      hint.textContent=mode==='pin'?TL('pin_4_8_digits_quick_to_type_on_a_phone'):mode==='password'?TL('password_8_63_characters_stronger_than_a'):TL('the_panel_will_be_open_to_anyone_on_the');
      err.textContent='';
    }
    sync();
    const end=()=>{close();authOpen=false;};
    cl.onclick=()=>{if(first){authLater=true;try{sessionStorage.setItem('authLater','1');}catch(e){}}end();};
    sv.onclick=async()=>{
      err.textContent='';
      if(prot&&!cur.value){err.textContent=TL('enter_the_current_code');cur.focus();return;}
      if(mode!=='off'){
        if(a.value!==c2.value){err.textContent=TL('the_codes_do_not_match');return;}
        const v=mode==='pin'?(/^\d{4,8}$/.test(a.value)?null:TL('pin_4_8_digits_2')):(a.value.length>=8&&a.value.length<=63?null:TL('password_8_63_characters_2'));
        if(v){err.textContent=v;return;}
      }
      sv.disabled=true;
      try{
        const r=await authFetch('/auth_setup',{method:'POST',headers:H,body:new URLSearchParams({mode:mode,secret:mode==='off'?'':a.value,current:prot?cur.value:''})}),t=(await r.text()).trim();
        if(!r.ok){err.textContent=logTr(t)||TL('error_n',{n:r.status});sv.disabled=false;return;}
        setTok(t.length===32?t:'');if(S)S.authMode=mode;
        toast(mode==='off'?TL('panel_not_protected'):TL('panel_protected'),'ok');end();updAuthCard(S);
      }catch(e){err.textContent=TL('no_connection_to_the_module');sv.disabled=false;}
    };
  });
}
function maybeAuthPrompt(){if(S&&S.authMode==='none'&&!authLater&&!authOpen&&!rebooting)openAuthSetup(true);}
const AUTHC={};
function buildAuthCard(g){
  const c=card(TL('panel_security'));
  AUTHC.st=el('p','snd-head');c.appendChild(AUTHC.st);
  const b=btns(c,[[TL('set_change'),'primary',()=>openAuthSetup(false)],[TL('log_out'),'',()=>{setTok('');toast(TL('logged_out'),'ok');updAuthCard(S);}]]);AUTHC.out=b.children[1];
  c.appendChild(el('p','note',TL('a_pin_or_password_protects_changes_saving')));
  g.appendChild(c);
}
function updAuthCard(s){
  if(!AUTHC.st||!s)return;
  const m=s.authMode,prot=(m==='pin'||m==='password');
  AUTHC.st.textContent=m==='pin'?TL('protected_by_pin'):m==='password'?TL('protected_by_password'):m==='off'?TL('no_protection_disabled'):TL('not_set_panel_open');
  if(prot)AUTHC.st.textContent+=authTok?TL('logged_in'):TL('not_logged_in');
  AUTHC.out.hidden=!(prot&&authTok);
}
const postBusy=new Set();   // paths with a request in flight: a second tap on the same action is ignored
async function post(path,params,okMsg){
  if(postBusy.has(path))return false;
  postBusy.add(path);
  try{
    const r=await authFetch(path,{method:'POST',headers:H,body:params?new URLSearchParams(params):undefined});
    const txt=(await r.text()).trim();
    if(!r.ok)throw new Error(logTr(txt)||('HTTP '+r.status));          // the message from the firmware instead of a bare "HTTP 400"
    if(txt&&txt!=='OK')toast((okMsg?okMsg+' — ':'')+logTr(txt),'warn');   // the firmware saved it, but warns
    else if(okMsg)toast(okMsg,'ok');
    return true;
  }catch(e){toast((e&&(e.name==='TypeError'||e.name==='AbortError'))?TL('no_connection_to_the_module'):TL('error_m',{m:e.message}),'bad');return false;}
  finally{postBusy.delete(path);}
}

)HTML"
