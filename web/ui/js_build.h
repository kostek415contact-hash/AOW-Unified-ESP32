// ======================================================================
// web/ui/js_build.h
// EN: Script part 3: building the UI - status tiles, ready-to-drive, calibration, settings. Part of the panel page (web/web_ui.h joins the parts in order; they are one string in flash).
// PL: Skrypt cz. 3: budowa interfejsu - kafelki, gotowość do jazdy, kalibracja, ustawienia. Część strony panelu (web/web_ui.h skleja części po kolei; w pamięci flash to jeden napis).
// ======================================================================
R"HTML(/* ================= building the UI ================= */
const cardsRead=[];const cardsDiag={};
function buildRead(){
  const g=$('#readGrid');READ.forEach(([t,rows])=>{const c=mkCard(t,rows);g.appendChild(c);cardsRead.push(c);});
  // Hydraulics: a card like the others (status rows + manual buttons), not a separate block under the grid
  const hc=mkCard(TL('hydraulics'),[
    [TL('state'),s=>s.hydLiftEn?hydText(s):TL('no_pin_assigned'),s=>(s.hydLiftActive||s.hydDownActive)?'ok':''],
    [TL('lift_control'),s=>s.hydLiftEn?(s.hydLiftManual?TL('manual_2'):'AgOpenWeb'):'—',s=>s.hydLiftManual?'warn':'']]);
  const hb=el('div','btns');
  [['/hydlift_manual_on','raise'],['/hydlift_manual_off','lower'],['/hydlift_auto','auto_agopenweb']].forEach(([u,k])=>{
    const b=el('button','btn',k==='raise'?TL('raise'):k==='lower'?TL('lower'):TL('auto_agopenweb'));b.dataset.post=u;if(k!=='auto_agopenweb')b.dataset.confirm='1';hb.appendChild(b);});
  hc.appendChild(hb);g.appendChild(hc);cardsRead.push(hc);
  cardFolding(g);
}
function buildDiag(){
  const g=$('#diagGrid');
  FLAGS.forEach(([key,label,jf])=>{
    const d=DIAG[key];if(!d)return;
    const c=document.createElement('article');c.className='card';
    const head=document.createElement('div');head.className='card-h';
    const h=document.createElement('h3');h.textContent=d.title;
    const sw=document.createElement('label');sw.className='sw';
    sw.innerHTML='<input type="checkbox" aria-label="'+label+'"><span class="tr"></span>';
    head.append(h,sw);c.appendChild(head);
    const body=document.createElement('div');c.appendChild(body);
    const rowsBox=document.createElement('div');body.appendChild(rowsBox);
    const list=mkRows(rowsBox,d.rows||[]);
    const extra=document.createElement('div');body.appendChild(extra);
    if(d.note){const n=document.createElement('p');n.className='note';n.textContent=d.note;body.appendChild(n);}
    const off=document.createElement('p');off.hidden=true;
    const input=sw.querySelector('input');
    input.addEventListener('change',()=>{flagState[key]=input.checked;saveFlags();});
    cardsDiag[key]={c,body,off,input,list,extra,d,jf};
    g.appendChild(c);
  });
  foldLong($('#tab-diag'));
}
async function saveFlags(){
  flagPending=true;
  const p={};FLAGS.forEach(([k])=>p[k]=flagState[k]?'1':'0');
  const ok=await post('/settings_save_debug',p);
  if(!ok&&S){FLAGS.forEach(([k,,jf])=>flagState[k]=!!S[jf]);}
  setTimeout(()=>{flagPending=false;},1200);
}
/* Open cards go first, in the order of the list, and the closed ones follow: switching a card on moves it to the top, so everything that is
   being watched sits together instead of being scattered between switched-off cards. */
let diagOrderSig=null;
function orderDiag(){
  const g=document.getElementById('diagGrid');if(!g)return;
  const keys=FLAGS.map(f=>f[0]).filter(k=>cardsDiag[k]);
  const open=keys.filter(k=>flagState[k]),sig=open.join(',');
  if(sig===diagOrderSig)return;diagOrderSig=sig;
  const ordered=open.concat(keys.filter(k=>!flagState[k])).map(k=>cardsDiag[k].c);
  if(g._m){g._m=ordered;masonry('diagGrid',true,true);}
  else ordered.forEach(c=>g.appendChild(c));
}
function updDiag(s){
  if(!flagPending)FLAGS.forEach(([k,,jf])=>flagState[k]=!!s[jf]);
  for(const k in cardsDiag){
    const x=cardsDiag[k],on=!!flagState[k];
    x.input.checked=on;x.body.hidden=!on;x.off.hidden=on;
    if(!on)continue;
    if(x.d.build&&!x.d.keepRows){x.d.build(x.extra,s);}
    else{updRows(x.list,s);if(x.d.build)x.d.build(x.extra,s);}
  }
  orderDiag();
}

/* --- status tiles --- */
const hasAgioSel=s=>typeof s.agioSel==='string'&&s.agioSel!=='0.0.0.0';
const foreignSenders=s=>(Array.isArray(s.agioSenders)?s.agioSenders:[]).filter(x=>!x.sel&&x.age>=0&&x.age<5000);   // foreign senders that are talking NOW
function tiles(s){
  const agio=agioOk(s);
  const t=[];
  const foreign=foreignSenders(s);
  t.push({n:'AgOpenWeb',s:agio?TL('connected'):TL('none_2'),c:foreign.length?'warn':agio?'ok':'bad',sub:(hasAgioSel(s)?s.agioSel+' • ':'')+(isNum(s.agioAgeMs)&&s.agioAgeMs>=0?TL('last_packet_t_ago',{t:fmtMs(s.agioAgeMs)}):(agio?TL('commands_received'):TL('no_packets')))+(foreign.length?' • '+TL('foreign_sender_n',{n:foreign.length}):''),go:foreign.length?'settings':'diag'});
  t.push({n:TL('network'),s:s.apMode?TL('ap_mode'):s.netConnected?(s.netType==='WiFi'?s.rssi+' dBm':TL('connected')):TL('none_2'),c:netCls(s),sub:s.apMode?TL('own_access_point'):s.ip,go:'readings'});
  const viaAog=!s.ntripEnabled&&s.rtcmSource===2;   // own client off, but corrections arrive relayed from AgOpenWeb
  t.push({n:'NTRIP',s:viaAog?TL('relay_from_agopenweb'):!s.ntripEnabled?TL('disabled'):s.ntripStreaming?TL('streaming'):TL('connecting'),c:viaAog?'ok':!s.ntripEnabled?'muted':s.ntripStreaming?'ok':'warn',sub:s.ntripEnabled?fmtKB(s.ntripBytes)+' • '+TL('disconnects_n',{n:s.ntripReconnects}):viaAog?fmtKB(s.rtcmBytes)+' • '+TL('own_client_off'):TL('enable_in_settings'),go:s.ntripEnabled?'diag':'settings'});
  t.push({n:'IMU',s:s.enImu?(s.imuConnected?'OK':TL('none_2')):TL('disabled_4'),c:!s.enImu?'muted':s.imuConnected?'ok':'bad',sub:s.enImu?TL('n_pkts_s_roll_r_deg',{n:s.imuPps,r:f(s.imuRoll,1)}):'',go:'diag'});
  t.push({n:TL('angle_sensor'),s:s.steerSensorOk?f(s.steerAngleDeg,1,'°'):TL('none_2'),c:s.steerSensorOk?'ok':'bad',sub:s.steerSensorOk?'ADC '+s.steerRaw:TL('check_ads1115'),go:'calibration'});
  t.push({n:TL('steering'),s:(s.motorDriverEn&&s.runState>=2)?(s.runState===3?TL('stop_reversed_direction'):s.runState===4?TL('stop_angle_sensor_frozen'):TL('stop_no_movement')):s.cmdFresh?(s.cmdEngaged?TL('engaged'):TL('ready_2')):TL('no_commands'),c:(s.motorDriverEn&&s.runState>=2)?'bad':s.cmdFresh?(s.cmdEngaged?'ok':'warn'):'muted',sub:TL('motor_s',{s:s.motorDriverEn?TL('active'):TL('inactive')})+(s.cmdFresh?' • '+TL('target_a',{a:f(s.cmdTargetAngle,1,'°')}):''),go:'readings'});
  t.push({n:TL('motor_current'),s:!s.currentSensEn?TL('disabled'):!isNum(s.currState)?TL('active'):s.currState===2?f(s.currAmps,1,' A'):s.currState===3?TL('overcurrent'):s.currState===4?TL('sensor_error'):TL('no_data_2'),
    c:!s.currentSensEn?'muted':(!isNum(s.currState)||s.currState===2)?'ok':s.currState===1?'warn':'bad',
    sub:!s.currentSensEn?TL('enable_in_settings'):isNum(s.currTrips)?TL('peak_a',{a:f(s.currPeakA,1,' A')})+' • '+TL('trips_n',{n:s.currTrips}):'',go:'calibration'});
  t.push({n:TL('sections_and_hydraulics'),s:s.machineFresh?bitsCount(s.sectionsMask)+' / 16':'—',c:s.machineFresh?'ok':'muted',sub:s.hydLiftEn?'hydLift: '+(s.hydLiftActive?TL('lifting_2'):s.hydDownActive?TL('lowering_2'):TL('idle'))+(s.hydLiftManual?' ['+TL('manual_2')+']':''):'hydLift: '+TL('no_pin_assigned'),go:'readings'});
  t.push({n:TL('system'),s:'CPU '+f(s.loopLoadPct,0,'%'),c:s.loopLoadPct>85?'warn':'ok',sub:'RAM '+Math.round(s.freeHeap/1024)+' KB • '+fmtUp(s.uptimeSec),go:'diag'});
  return t;
}
const TC={ok:'var(--ok)',warn:'var(--warn)',bad:'var(--bad)',muted:'var(--label)'};
/* --- ready to drive: a single answer "can we go?" from a checklist (fixed order, inactive features skipped) --- */
function readiness(s){
  const k=[];const add=(n,c,t,go)=>k.push({n,c,t,go});
  add(TL('network'),!s.netConnected?'bad':s.apMode?'warn':'ok',!s.netConnected?TL('none_2'):s.apMode?TL('ap_mode'):TL('connected'),'readings');
  add('AgOpenWeb',agioOk(s)?'ok':'bad',agioOk(s)?TL('packets_flowing'):TL('no_packets'),'readings');
  add('GPS',fixCls(s.fixQuality),s.fixQuality<1?TL('no_fix'):fixName(s.fixQuality),'readings');
  if(s.ageLvl===2)add(TL('rtk_corrections'),'bad',TL('stopped'),'readings');
  else if(s.ageLvl===1)add(TL('rtk_corrections'),'warn',TL('delayed'),'readings');
  else if(s.ntripEnabled)add(TL('rtk_corrections'),s.ntripStreaming?'ok':'warn',s.ntripStreaming?TL('flowing'):TL('connecting'),'readings');
  else if(s.rtcmSource===2)add(TL('rtk_corrections'),'ok',TL('flowing_via_agopenweb'),'readings');
  else add(TL('rtk_corrections'),'muted',TL('ntrip_off'),'readings');
  add(TL('angle_sensor'),!s.steerSensorOk?'bad':s.steerCalOk===false?'bad':'ok',!s.steerSensorOk?TL('no_reading'):s.steerCalOk===false?TL('calibration_invalid'):f(s.steerAngleDeg,1,'°'),'calibration');
  if(s.enImu)add('IMU',s.imuConnected?'ok':'bad',s.imuConnected?'OK':TL('none_2'),'diag');
  if(!s.motorDriverEn)add(TL('motor'),'muted',TL('disabled'),'calibration');
  else{
    const rs=isNum(s.runState)?s.runState:0,cs=(s.currentSensEn&&isNum(s.currState))?s.currState:2;
    const unarmed=s.motorArmed===false;   // after a restart the motor stays still until steering was seen OFF; engaged + unarmed = it will not move
    const bad=rs>=2||cs===3||cs===4||(unarmed&&!!s.cmdEngaged),warn=!bad&&(cs===1||!!s.kickoutActive||unarmed||motorSettingsMissing(s));
    add(TL('motor'),bad?'bad':warn?'warn':'ok',rs>=2?TL('stop_protection'):cs===3?TL('overcurrent_2'):cs===4?TL('current_sensor_error'):unarmed?TL('not_armed_switch_steering_off_and_on'):cs===1?TL('no_current_data'):s.kickoutActive?TL('kickout_active'):motorSettingsMissing(s)?TL('no_settings_from_aow'):TL('healthy'),'calibration');
  }
  if(s.sectionExpCfg&&!s.sectionExpEn)add(TL('section_expander'),'warn',TL('not_responding'),'diag');
  const bads=k.filter(x=>x.c==='bad').length,warns=k.filter(x=>x.c==='warn').length;
  const v=bads?'bad':warns?'warn':'ok';
  return {v,bads,warns,k};
}
function updReady(s){
  const box=$('#ready');if(!box)return;
  const r=readiness(s);
  if(box.children.length!==2||box.children[1].children.length!==r.k.length){
    box.innerHTML='';
    const h=document.createElement('div');h.className='rh';h.innerHTML='<span class="rv"></span><span class="rs"></span>';
    const c=document.createElement('div');c.className='rc';
    r.k.forEach(()=>{const e=document.createElement('div');e.className='ck';e.innerHTML='<i></i><b></b><span></span>';c.appendChild(e);});
    box.append(h,c);
  }
  box.style.setProperty('--c',TC[r.v]);
  box.children[0].children[0].textContent=r.v==='ok'?TL('ready_to_drive'):r.v==='warn'?TL('ready_with_warnings_n',{n:r.warns}):TL('not_ready_n',{n:r.bads});
  box.children[0].children[1].textContent=s.cmdEngaged?TL('steering_engaged'):s.cmdFresh?TL('steering_off'):TL('no_commands_from_agopenweb');
  r.k.forEach((x,i)=>{const e=box.children[1].children[i];e.style.setProperty('--c',TC[x.c]);e.dataset.go=x.go;e.children[1].textContent=x.n;e.children[2].textContent=x.t;});
}
function updStatus(s){
  updReady(s);
  const hv=$('#heroV'),hero=$('#hero'),c=TC[fixCls(s.fixQuality)];
  hv.textContent=fixName(s.fixQuality);hero.style.setProperty('--c',c);
  if(HERO.cv){HERO.def.series[0].c={ok:'--ok',warn:'--warn',bad:'--bad'}[fixCls(s.fixQuality)];drawChart(HERO,120000,true);}
  const acc=s.hAcc>=0?' • ±'+f(s.hAcc*100,1,' cm'):'';
  $('#heroS').textContent=TL('n_satellites',{n:s.satellites})+' • '+f(s.gpsRateHz,1,' Hz')+acc+' • HDOP '+f(s.hdop,1)+(s.ageDgps>=0?' • '+TL('corrections_a',{a:f(s.ageDgps,1,' s')}):'');
  // notes
  const a=[];
  if(!s.netConnected)a.push(['bad',TL('no_network_connection')]);else if(s.apMode)a.push(['warn',TL('ap_mode_the_module_is_not_on_your_wifi')]);
  if(s.fixQuality<1)a.push(['bad',TL('gps_no_fix')]);else if(s.fixQuality!==4)a.push(['warn',TL('gps_f_not_a_full_rtk_fix',{f:fixName(s.fixQuality)})]);
  if(s.ageLvl===2)a.push(['bad',TL('rtk_corrections_age_a_the_rtcm_stream',{a:f(s.ageDgps,0,' s')})]);
  else if(s.ageLvl===1)a.push(['warn',TL('rtk_corrections_are_late_age_a_normal',{a:f(s.ageDgps,0,' s')})]);
  if(s.enImu&&!s.imuConnected)a.push(['bad',isNum(s.imuBps)?(s.imuBps===0?TL('imu_rx_line_is_silent_0_b_s'):TL('imu_bytes_are_arriving_n_b_s_but_without',{n:s.imuBps})):TL('imu_no_data')]);
  if(s.sectionExpCfg&&!s.sectionExpEn)a.push(['bad',TL('section_expander_mcp23017_not_responding_sections_do')]);
  if(!s.steerSensorOk)a.push(['bad',TL('angle_sensor_no_reading')]);
  else if(s.steerCalSaved===false)a.push(['bad',TL('angle_sensor_never_calibrated_save_left_and_right_motor')]);
  else if(s.steerCalOk===false)a.push(['warn',TL('angle_sensor_calibration_invalid_left')]);
  if(motorSettingsMissing(s))a.push(['warn',TL('motor_no_steering_settings_from_agopenweb')]);
  if(s.motorDriverEn&&isNum(s.runState)){
    if(s.runState===3)a.push(['bad',TL('motor_stopped_reversed_direction')]);
    else if(s.runState===4)a.push(['bad',TL('motor_stopped_angle_sensor_frozen_the')]);
    else if(s.runState===2)a.push(['bad',TL('motor_stopped_the_wheel_does_not_move')]);}
  if(s.kickoutActive)a.push(['warn',TL('motor_protection_tripped')+(s.kickoutCause>0?' ('+KICK_TXT[s.kickoutCause]+')':'')+' '+TL('agopenweb_receives_the_kickout_signal_pgn')]);
  if(s.ntripEnabled&&s.ntripFails>0&&s.ntripFailKind>=3)a.push(['bad',TL('ntrip_r_next_attempt_in_t',{r:NTRIP_FAIL[s.ntripFailKind]||TL('error_3'),t:fmtMs(s.ntripRetryMs)})]);
  if(s.cmdFresh&&!s.cmdEngaged&&s.cmdEngageMask===4&&s.cmdStatusByte===1&&Math.abs(s.cmdTargetAngle)>0.05)a.push(['warn',TL('agopenweb_sends_status_byte_01_with_the')]);
  if(s.currentSensEn&&isNum(s.currState)){
    if(s.currState===3)a.push(['bad',TL('motor_blocked_overcurrent_peak_a',{a:f(s.currPeakA,1,' A')})]);
    else if(s.currState===4)a.push(['bad',TL('current_sensor_voltage_out_of_range_not')]);
    else if(s.currState===1)a.push(['warn',TL('current_sensor_no_data_motor_blocked')]);}
  if(s.ntripEnabled&&!s.ntripStreaming)a.push(['warn',TL('ntrip_connecting_to_the_caster')]);
  if(!agioOk(s))a.push(['warn',TL('no_packets_from_agopenweb')]);
  const fo=foreignSenders(s);
  if(fo.length)a.push(['warn',TL('a_second_agopenweb_on_the_network_ips_the',{ips:fo.map(x=>x.ip).join(', '),sel:s.agioSel})]);
  if(isNum(s.freeHeap)&&s.freeHeap<60000)a.push(['warn',TL('low_free_ram')]);
  if(s.heapLeak)a.push(['warn',TL('possible_memory_leak_free_ram_falls_v_kb_h',{v:f(Math.abs(s.heapTrendBph)/1024,1)})]);
  if(s.resetReason&&/PANIC|watchdog|brownout/i.test(s.resetReason)&&s.uptimeSec<900)a.push(['warn',TL('last_restart_r',{r:argTr(s.resetReason)})]);
  const box=$('#attn');box.innerHTML='';
  if(!a.length){const d=document.createElement('div');d.className='ai';d.innerHTML='<i style="--c:var(--ok)"></i><span>'+TL('everything_is_fine')+'</span>';box.appendChild(d);}
  a.forEach(([k,t])=>{const d=document.createElement('div');d.className='ai';const i=document.createElement('i');i.style.setProperty('--c',TC[k]);const sp=document.createElement('span');sp.textContent=t;d.append(i,sp);box.appendChild(d);});
  // tiles
  const g=$('#tiles');const data=tiles(s);
  if(g.children.length!==data.length){g.innerHTML='';data.forEach(()=>{const e=document.createElement('div');e.className='tile';e.tabIndex=0;e.setAttribute('role','link');e.innerHTML='<div class="tn"></div><div class="ts"></div><div class="tsub"></div>';g.appendChild(e);});}
  data.forEach((d,i)=>{const e=g.children[i];e.style.setProperty('--c',TC[d.c]);e.dataset.go=d.go;e.children[0].textContent=d.n;e.children[1].textContent=d.s;e.children[2].textContent=d.sub;});
}
function updHeader(s){
  const c=netCls(s);$('#mainDot').className='dot '+c;
  $('#netChip').textContent=s.apMode?'AP':s.netConnected?(s.netType==='WiFi'?'WiFi '+s.rssi+' dBm':s.netType):TL('no_network');
  updStrip(s);
}
/* A slim "ready to drive" bar under the title on every tab except Status (which has the full card): verdict + the first problems; tap = Status. */
function fixHeaderH(){const h=document.querySelector('header.top');if(h)document.documentElement.style.setProperty('--hh',h.offsetHeight+'px');}
function updStrip(s){
  const b=$('#rstrip');if(!b)return;
  const was=b.hidden;
  if(curTab==='status'){b.hidden=true;}
  else{
    const r=readiness(s),bad=r.k.filter(x=>x.c==='bad'||x.c==='warn');
    b.hidden=false;b.style.setProperty('--c',TC[r.v]);
    b.children[0].textContent=r.v==='ok'?TL('ready_to_drive'):r.v==='warn'?TL('ready_with_warnings_n',{n:r.warns}):TL('not_ready_n',{n:r.bads});
    b.children[1].textContent=bad.slice(0,3).map(x=>x.n+': '+x.t).join(' • ');
  }
  if(was!==b.hidden||!b.hidden)fixHeaderH();
}

/* --- calibration --- */
const KICK_TXT=['—',TL('overcurrent_current_latch'),TL('current_sensor_error_voltage_out_of_range'),TL('current_sensor_gives_no_samples'),TL('wheel_does_not_move_despite_the_drive'),TL('motor_direction_reversed'),TL('angle_sensor_frozen')];
const NTRIP_FAIL={0:TL('connection_error_or_dropped'),3:TL('wrong_login_data_user_password'),4:TL('no_such_mountpoint'),5:TL('the_caster_returned_the_source_list_wrong'),6:TL('unsupported_caster_response')};
const RUN_STATE=[TL('disabled'),TL('ok_monitoring_active'),TL('no_progress_motor_stopped'),TL('reversed_direction_motor_stopped'),TL('angle_sensor_frozen_motor_stopped')];
const RUN_WARN={2:TL('the_motor_was_driven_but_the_angle_did_not'),
  4:TL('the_raw_angle_sensor_reading_was_identical'),
  3:TL('the_wheel_moved_in_the_opposite_direction')};
const RUN_REASON=['—','',TL('no_progress'),TL('reversed_direction'),TL('angle_sensor_frozen')];
// A detector message with a hint matched to the data: a disconnected angle sensor (voltage ~0 V) and a very low minPWM.
function runWarnText(s){
  const base=RUN_WARN[s.runState]||'';
  if(s.runState!==2)return base;
  const pre=[];
  if(isNum(s.steerVolts)&&s.steerVolts<0.05)pre.push(TL('the_angle_sensor_looks_disconnected_input',{v:f(s.steerVolts,3,' V')}));
  if(isNum(s.steerMinPwm)&&s.steerMinPwm<15)pre.push(TL('minpwm_n_is_very_low_the_motor_may_not',{n:s.steerMinPwm}));
  return pre.length?pre.join(' ')+' '+TL('other_possible_causes')+' '+TL('motor_runaway_check_list'):base;
}
// Result of the manual motor test (Left/Right): what was measured and what follows from it.
const TEST_ABORT=['',TL('steering_was_engaged_in_agopenweb_during'),TL('current_protection_stopped_the_motor'),TL('the_motor_driver_was_disabled_during_the'),TL('the_wheel_reached_the_end_of_its_range')];
function testResultText(s){
  if(!isNum(s.testState)||s.testState===0)return '';
  const side=s.testDir>0?TL('right'):TL('left');
  if(s.testState===1)return TL('test_in_progress_side_pwm_pwm_ms_ms',{side:side,pwm:s.testPwm,ms:s.testMs});
  const head=TL('last_test_side_pwm_pwm_ms_ms',{side:side.toUpperCase(),pwm:s.testPwm,ms:s.testMs});
  if(s.testState===3)return head+TL('aborted')+' '+(TEST_ABORT[s.testReason]||'');
  const hasAng=s.testDeltaDeg>-900,hasCur=s.testPeakA>=0;
  const parts=[hasAng?TL('angle_d_deg',{d:(s.testDeltaDeg>=0?'+':'')+f(s.testDeltaDeg,1)}):TL('no_angle_reading'),hasCur?TL('current_peak_a_a',{a:f(s.testPeakA,2)}):TL('current_sensor_disabled')];
  const v=[];
  if(hasCur)v.push(s.testPeakA>=0.3?TL('the_motor_draws_current_it_is_powered'):TL('no_current_draw_check_the_motor_supply_6'));
  if(hasAng){
    if(Math.abs(s.testDeltaDeg)<0.5)v.push(TL('the_angle_did_not_change_angle_sensor'));
    else if((s.testDeltaDeg>0)===(s.testDir>0))v.push(TL('the_wheel_moved_in_the_commanded_direction'));
    else v.push(TL('the_wheel_went_the_opposite_way_tick'));
  }
  return head+parts.join(', ')+(v.length?'. '+v.join('; ')+'.':'.');
}
// Until AgOpenWeb sends the steering settings (PGN 252), gainP/highPWM/minPWM = 0, and the controller output = error x gainP, so PWM = 0.
function motorSettingsMissing(s){return !!s.motorDriverEn&&typeof s.steerSettingsRx==='boolean'&&(!s.steerSettingsRx||s.steerGainP===0||s.steerHighPwm===0);}
const MOTOR_SET_WARN=TL('the_motor_will_get_no_pwm_agopenweb_has');
const CUR_STATE=[TL('disabled'),TL('no_data_2'),'OK',TL('overcurrent_motor_blocked'),TL('sensor_error_motor_blocked')];
const CUR_WARN={0:TL('the_current_sensor_is_disabled_enable_it'),
  1:TL('no_fresh_current_readings_ads1115_i2c'),
  3:TL('overcurrent_motor_stopped_the_lock_clears'),
  4:TL('sensor_voltage_outside_0_15_3_15_v_the')};
const CUR_CAL=['—',TL('in_progress_2'),TL('ready'),TL('failed')];
let calCards={};
function buildCal(){
  const g=$('#calGrid');
  // angle sensor
  const a=document.createElement('article');a.className='card';
  a.innerHTML='<h3>'+TL('steering_angle_sensor')+'</h3><div class="big" id="calAngle">—</div><div class="bigs" id="calRaw">—</div>'+
   '<div class="gauge"><b id="calZero"></b><i id="calMark"></i></div><div class="gl"><span>'+TL('left_2')+'</span><span>'+TL('right_2')+'</span></div>'+
   '<ol class="steps"><li>'+TL('turn_the_wheels_all_the_way_to_the_left')+'</li><li>'+TL('turn_the_wheels_all_the_way_to_the_right')+'</li><li>'+TL('enter_the_actual_end_stop_angles_of_the')+'</li></ol>'+
   '<div class="btns"><button class="btn" id="calL">'+TL('save_as_left')+'</button><button class="btn" id="calR">'+TL('save_as_right')+'</button></div>'+
   '<div class="result" id="calWarn" hidden></div>'+
   '<label class="field"><span>'+TL('left_angle_degrees')+'</span><input id="setSteerLeftDeg" type="number" step="0.1"></label>'+
   '<label class="field"><span>'+TL('right_angle_degrees')+'</span><input id="setSteerRightDeg" type="number" step="0.1"></label>'+
   '<div class="btns"><button class="btn primary" id="calRange">'+TL('save_range')+'</button></div>';
  g.appendChild(a);
  // IMU
  const b=document.createElement('article');b.className='card';
  b.innerHTML='<h3>'+TL('imu_board_rotation_roll_pitch_correction')+'</h3><div id="imuCalRows"></div>'+
   '<ol class="steps"><li>'+TL('after_mounting_place_the_enclosure_level')+'</li><li>'+TL('tilt_it_sideways_around_the_axis_along_the')+'</li><li>'+TL('the_angle_fills_in_by_itself_click_save')+'</li></ol>'+
   '<p class="note">'+TL('imu_3d_hint')+'</p>'+
   '<div class="btns"><button class="btn" id="imuA">'+TL('point_a_level')+'</button><button class="btn" id="imuB">'+TL('point_b_tilted')+'</button></div>'+
   '<div class="result" id="imuRes">'+TL('no_point_stored_yet')+'</div>'+
   '<label class="field"><span>'+TL('imu_tilt_type')+'</span><select id="imuTilt"><option value="0">'+TL('imu_tilt_side_right_down')+'</option><option value="1">'+TL('imu_tilt_nose_up')+'</option></select></label>'+
   '<div class="btns"><button class="btn primary" id="imu3d">'+TL('save_3d_mounting')+'</button><button class="btn" id="imu3dOff">'+TL('switch_3d_mounting_off')+'</button></div>'+
   '<p class="note">'+TL('imu_manual_angles_hint')+'</p>'+
   '<label class="field"><span>'+TL('imu_board_yaw_deg')+'</span><input id="setImuBy" type="number" step="0.1" min="-180" max="180"></label>'+
   '<label class="field"><span>'+TL('imu_board_pitch_deg')+'</span><input id="setImuBp" type="number" step="0.1" min="-90" max="90"></label>'+
   '<label class="field"><span>'+TL('imu_board_roll_deg')+'</span><input id="setImuBr" type="number" step="0.1" min="-180" max="180"></label>'+
   '<div class="btns"><button class="btn" id="imuAng">'+TL('save_board_angles')+'</button></div>'+
   '<label class="field"><span>'+TL('board_rotation_degrees')+'</span><input id="setImuRot" type="number" step="0.1"></label>'+
   '<div class="btns"><button class="btn primary" id="imuSave">'+TL('save')+'</button></div>';
  b.insertBefore(sw('setImuRollInvert',TL('invert_roll_sign')),b.lastElementChild);
  g.appendChild(b);
  INP.setSteerLeftDeg=$('#setSteerLeftDeg');INP.setSteerRightDeg=$('#setSteerRightDeg');INP.setImuRot=$('#setImuRot');INP.setImuBy=$('#setImuBy');INP.setImuBp=$('#setImuBp');INP.setImuBr=$('#setImuBr');
  // motor current sensor (ACS71240)
  const cc=document.createElement('article');cc.className='card';cc.id='curCard';
  cc.innerHTML='<h3>'+TL('motor_current_sensor_acs71240_30_a')+'</h3><div class="big" id="curAmps">—</div><div class="bigs" id="curState">—</div>'+
   '<div id="curRows"></div><div class="result" id="curWarn" hidden></div>'+
   '<ol class="steps"><li>'+TL('connect_the_sensor_to_input_a1_of_the')+'</li><li>'+TL('with_the_motor_and_steering_off_press_zero')+'</li><li>'+TL('set_the_thresholds_with_a_margin_above')+'</li></ol>'+
   '<div class="btns"><button class="btn" id="curZero">'+TL('zero_motor_off')+'</button><button class="btn" id="curReset">'+TL('clear_lock')+'</button></div>'+
   '<div id="curFields"></div><div class="btns"><button class="btn primary" id="curSave">'+TL('save_thresholds')+'</button></div>';
  g.appendChild(cc);
  $('#curFields').append(
    fld('setCurKickA',TL('current_threshold_kickout_a_after_the_time'),'number',{step:'0.1',check:chkNum(0.5,28,'A')}),
    fld('setCurKickMs',TL('time_above_threshold_ms'),'number',{check:v=>chkNum(20,5000,'ms')(v)||(/^\d+$/.test(v.trim())?null:TL('integer'))}),
    fld('setCurHardA',TL('hard_threshold_a_immediate_2_samples'),'number',{step:'0.1',check:v=>chkNum(1,30,'A')(v)||(parseFloat(v)<parseFloat(INP.setCurKickA.value)+0.5?TL('at_least_0_5_a_above_the_kickout_threshold'):null)}));
  calCards.cur=mkRows($('#curRows'),[
   [TL('instantaneous_average_current'),s=>f(s.currAmps,2)+' / '+f(s.currAvgAmps,2)+' A',s=>s.currState===2?'ok':'',s=>isNum(s.currAmps)],
   [TL('peak_since_last_reset'),s=>f(s.currPeakA,2,' A'),null,s=>isNum(s.currPeakA)],
   [TL('output_voltage_zero'),s=>f(s.currVolts,3)+' / '+f(s.currZeroV,3)+' V',null,s=>isNum(s.currVolts)],
   [TL('reading_age'),s=>s.currAgeMs<0?TL('never'):fmtMs(s.currAgeMs),s=>(s.currAgeMs<0||s.currAgeMs>250)?'bad':'ok',s=>isNum(s.currAgeMs)&&s.currentSensEn],
   [TL('zero_calibration'),s=>curCalText(s),s=>s.currCalState===3?'bad':s.currCalState===2?'ok':'',s=>isNum(s.currCalState)&&s.currCalState>0]]);
  act('#curZero',async()=>{if(await askConfirm(TL('confirm_zero_current_sensor')))await post('/current_zero',null,TL('zeroing_started_about_1_2_s'));});
  act('#curReset',async()=>{if(await askConfirm(TL('confirm_clear_current_lock'),true))await post('/current_reset',null,TL('lock_cleared'));});
  // motor: direction and runaway detector
  const mc=document.createElement('article');mc.className='card';mc.id='motCard';
  mc.innerHTML='<h3>'+TL('motor_direction_and_runaway_detector')+'</h3><div class="bigs" id="runState">—</div><div id="motRows"></div><div class="result" id="runWarn" hidden></div>'+
   '<ol class="steps"><li>'+TL('for_the_first_test_disconnect_the_motor')+'</li><li>'+TL('engage_steering_in_agopenweb_with_a_target')+'</li><li>'+TL('tick_reverse_motor_direction_save_and')+'</li></ol>';
  g.appendChild(mc);
  mc.append(sw('setMotorInvert',TL('reverse_motor_direction_dir_pin')),sw('setRunEn',TL('runaway_detector_enabled')));
  mc.append(
    fld('setRunWindowMs',TL('detector_window_ms_time_for_the_wheel_to'),'number',{check:v=>chkNum(300,5000,'ms')(v)||(/^\d+$/.test(v.trim())?null:TL('integer'))}),
    fld('setRunMinMoveDeg',TL('minimum_movement_in_the_window_deg'),'number',{step:'0.1',check:chkNum(0.2,10,'°')}),
    fld('setRunMinErrDeg',TL('evaluate_only_from_an_error_of_deg'),'number',{step:'0.1',check:chkNum(0.5,20,'°')}));
  const mb=document.createElement('div');mb.className='btns';mb.innerHTML='<button class="btn primary" id="motSave">'+TL('save_motor_settings')+'</button><button class="btn" id="runReset">'+TL('clear_detector_lock')+'</button>';mc.appendChild(mb);
  // manual motor test: Left / Right
  const tc=document.createElement('div');tc.id='motTest';
  tc.innerHTML='<h3 style="margin-top:18px">'+TL('motor_test_without_agopenweb')+'</h3><div class="hint">'+TL('disconnect_the_motor_from_the_steering')+'</div>';
  mc.appendChild(tc);
  tc.append(
    fld('setTestPwm',TL('test_pwm_10_150_of_255'),'number',{check:v=>chkNum(10,150,'')(v)||(/^\d+$/.test(v.trim())?null:TL('integer'))}),
    fld('setTestMs',TL('pulse_time_ms_100_1000'),'number',{check:v=>chkNum(100,1000,'ms')(v)||(/^\d+$/.test(v.trim())?null:TL('integer'))}));
  INP.setTestPwm.value='60';INP.setTestMs.value='300';
  const tb=document.createElement('div');tb.className='btns';tb.innerHTML='<button class="btn" id="testL">◀ '+TL('left_3')+'</button><button class="btn" id="testR">'+TL('right_3')+' ▶</button>';tc.appendChild(tb);
  tc.insertAdjacentHTML('beforeend','<div class="hint" id="testWhy" hidden></div><div class="result" id="testRes" hidden></div>');
  const runTest=async dir=>{if(!validate(['setTestPwm','setTestMs']))return;
    if(!(await askConfirm(TL('confirm_motor_test_n',{n:dir==='L'?TL('left_3'):TL('right_3')}),true)))return;
    await post('/motor_test',{dir:dir,pwm:INP.setTestPwm.value,ms:INP.setTestMs.value},dir==='L'?TL('test_left'):TL('test_right'));};
  act('#testL',()=>runTest('L'));act('#testR',()=>runTest('R'));
  calCards.mot=mkRows($('#motRows'),[
   [TL('motor_driver'),s=>s.motorDriverEn?TL('active'):TL('inactive_enable_in_settings'),s=>s.motorDriverEn?'ok':'warn',s=>typeof s.motorDriverEn==='boolean'],
   [TL('direction'),s=>((!!s.motorInvert)!==(!!s.motorInvertAog)?TL('inverted'):TL('normal'))+(s.motorInvertAog?'  ('+TL('direction_panel_p_agopenweb_a',{p:s.motorInvert?TL('inverted'):TL('normal'),a:TL('inverted')})+')':''),null,s=>typeof s.motorInvert==='boolean'],
   [TL('pwm_angle_error_now'),s=>s.motorPwm+' / '+f(s.motorErrDeg,1,'°'),null,s=>isNum(s.motorPwm)&&s.motorDriverEn],
   [TL('movement_in_the_window_towards_the_drive'),s=>f(s.runMoveDeg,2,'°'),null,s=>isNum(s.runMoveDeg)&&s.motorDriverEn&&s.runEn],
   [TL('settings_from_agopenweb_gainp_highpwm'),s=>s.steerSettingsRx?(s.steerGainP+' / '+s.steerHighPwm+' / '+s.steerMinPwm+(s.steerSettingsStored?' ('+TL('from_module_memory')+')':'')):TL('not_received'),s=>motorSettingsMissing(s)?'warn':'ok',s=>typeof s.steerSettingsRx==='boolean'&&s.motorDriverEn],
   [TL('trips_since_start'),s=>s.runTrips+(s.runTrips>0?' '+TL('last_r_movement_m',{r:RUN_REASON[s.runLastReason],m:f(s.runLastMoveDeg,1,'°')}):''),s=>s.runTrips>0?'warn':'ok',s=>isNum(s.runTrips)]]);
  act('#motSave',async()=>{if(!validate(['setRunWindowMs','setRunMinMoveDeg','setRunMinErrDeg']))return;
    // the wording follows the riskiest change: detector off > direction flipped > ordinary save
    const flipped=S&&typeof S.motorInvert==='boolean'&&INP.setMotorInvert.checked!==S.motorInvert;
    const risky=!INP.setRunEn.checked||flipped;
    const msg=!INP.setRunEn.checked?TL('confirm_runaway_detector_off'):flipped?TL('confirm_motor_direction_change'):TL('confirm_save_motor_settings');
    if(!(await askConfirm(msg,risky)))return;
    await post('/settings_save_motor',{runEn:INP.setRunEn.checked?'1':'0',runWindowMs:INP.setRunWindowMs.value,runMinMoveDeg:INP.setRunMinMoveDeg.value,runMinErrDeg:INP.setRunMinErrDeg.value,motorInvert:INP.setMotorInvert.checked?'1':'0'},TL('motor_settings_saved'));});
  act('#runReset',async()=>{if(await askConfirm(TL('confirm_clear_runaway_lock'),true))await post('/runaway_reset',null,TL('detector_lock_cleared'));});
  act('#curSave',async()=>{if(!validate(['setCurKickA','setCurKickMs','setCurHardA']))return;
    if(!(await askConfirm(TL('confirm_save_current_thresholds'))))return;
    await post('/settings_save_current',vals({currKickoutA:'setCurKickA',currKickoutMs:'setCurKickMs',currHardA:'setCurHardA'}),TL('current_thresholds_saved'));});
  calCards.imuList=mkRows($('#imuCalRows'),[
   [TL('imu_angles_raw_sensor'),s=>f(s.imuRollRaw,1)+' / '+f(s.imuPitchRaw,1)+' / '+f(s.imuYawRaw,1)+'°'],
   [TL('imu_angles_after_correction'),s=>f(s.imuRoll,1)+' / '+f(s.imuPitch,1)+' / '+f(s.imuHeading,1)+'°'],
   [TL('imu_3d_mounting'),s=>s.imuMount3d?TL('imu_mount_on'):TL('imu_mount_off'),()=>'ok'],
   [TL('imu_board_angles'),s=>isNum(s.imuMountYaw)?f(s.imuMountYaw,1)+' / '+f(s.imuMountPitch,1)+' / '+f(s.imuMountRoll,1)+'°':'—',()=>'ok'],
   [TL('saved_board_rotation'),s=>f(s.imuRotDeg,1,'°'),()=>'ok'],
   [TL('roll_sign'),s=>s.imuRollInvert?TL('inverted'):TL('normal'),()=>'ok']]);
  act('#calL',async()=>{if(await askConfirm(TL('save_this_position_as_an_angle_end_point')))await post('/steering_cal_left',null,TL('left_saved_adc_n',{n:S?S.steerRaw:'?'}));});
  act('#calR',async()=>{if(await askConfirm(TL('save_this_position_as_an_angle_end_point')))await post('/steering_cal_right',null,TL('right_saved_adc_n',{n:S?S.steerRaw:'?'}));});
  act('#calRange',async()=>{
    const l=parseFloat($('#setSteerLeftDeg').value),r=parseFloat($('#setSteerRightDeg').value);
    if(!isFinite(l)||!isFinite(r)||Math.abs(l)>90||Math.abs(r)>90){toast(TL('angles_must_be_numbers_in_the_range_90'),'bad');return;}
    if(Math.abs(r-l)<5){toast(TL('left_and_right_must_differ_by_at_least'),'bad');return;}
    if(!(await askConfirm(TL('confirm_save_angle_range'))))return;
    await post('/settings_save_steering_range',{leftDeg:l,rightDeg:r},TL('angle_range_saved'));});
  let A=null,B=null;
  const mark=async w=>{if(!S)return;
    if(!isNum(S.imuRollRaw)||!isNum(S.imuPitchRaw)){toast(TL('imu_no_data'),'bad');return;}   // no sensor reading = nothing to store
    if(!(await askConfirm(TL('confirm_imu_point_n',{n:w}))))return;
    // the 3D mounting needs a steady reading: average ~2 s of status samples (roll as a circular mean, it wraps at +-180)
    const acc={p:0,sr:0,cr:0,x:0,y:0,z:0,n:0};
    for(let i=0;i<10;i++){
      const q=S;if(q&&isNum(q.imuRollRaw)&&isNum(q.imuPitchRaw)){acc.p+=q.imuPitchRaw;acc.sr+=Math.sin(q.imuRollRaw*Math.PI/180);acc.cr+=Math.cos(q.imuRollRaw*Math.PI/180);
        acc.x+=+q.imuAccelX||0;acc.y+=+q.imuAccelY||0;acc.z+=+q.imuAccelZ||0;acc.n++;}
      if(i<9)await new Promise(r=>setTimeout(r,200));}
    if(!acc.n){toast(TL('imu_no_data'),'bad');return;}
    const p={r:Math.atan2(acc.sr,acc.cr)*180/Math.PI,p:acc.p/acc.n,ax:acc.x/acc.n,ay:acc.y/acc.n,az:acc.z/acc.n};if(w==='A')A=p;else B=p;
    toast(TL('imu_point_n_stored',{n:w}),'ok');
    const out=$('#imuRes');out.style.whiteSpace='pre-line';
    // what is stored so far: the values of each point (so the person sees exactly what the 3D mounting will be computed from)
    const pv=(n,P)=>TL('imu_point_values',{n:n,r:P.r.toFixed(1),p:P.p.toFixed(1),a:Math.hypot(P.ax,P.ay,P.az).toFixed(2)});
    const lines=[];if(A)lines.push(pv('A',A));if(B)lines.push(pv('B',B));
    if(!A||!B){lines.push(TL('stored_p_now_store_the_second_point',{p:(A?'A ':'')+(B?'B':'')}));out.textContent=lines.join('\n');return;}
    // the tilt between the poses from the accelerometer (needs no convention): 3 deg is the minimum, 10-15 deg is reliable
    const na=Math.hypot(A.ax,A.ay,A.az),nb=Math.hypot(B.ax,B.ay,B.az);
    const tilt=(na>0&&nb>0)?Math.acos(Math.max(-1,Math.min(1,(A.ax*B.ax+A.ay*B.ay+A.az*B.az)/(na*nb))))*180/Math.PI:0;
    lines.push(TL('imu_points_tilt',{t:tilt.toFixed(1)}));
    const dr=B.r-A.r,dp=B.p-A.p;
    if(Math.abs(dr)>=3){const psi=Math.atan(dp/dr)*180/Math.PI;$('#setImuRot').value=psi.toFixed(1);   // only for the plain board rotation below
      lines.push(TL('roll_change_r_deg_pitch_change_p_deg',{r:dr.toFixed(1),p:dp.toFixed(1),o:psi.toFixed(1)}));}
    out.textContent=lines.join('\n');};
  act('#imuA',()=>mark('A'));act('#imuB',()=>mark('B'));
  act('#imu3d',async()=>{
    if(!A||!B){toast(TL('store_points_a_and_b_first'),'bad');return;}
    if(!(await askConfirm(TL('confirm_save_imu_mount3d'))))return;
    const q=(P,k)=>String(P[k]);
    await post('/imu_mount_solve',{aPitch:q(A,'p'),aRoll:q(A,'r'),aAx:q(A,'ax'),aAy:q(A,'ay'),aAz:q(A,'az'),
      bPitch:q(B,'p'),bRoll:q(B,'r'),bAx:q(B,'ax'),bAy:q(B,'ay'),bAz:q(B,'az'),kind:$('#imuTilt').value},TL('imu_3d_mounting_saved'));});
  act('#imuAng',async()=>{
    const y=parseFloat(INP.setImuBy.value),p=parseFloat(INP.setImuBp.value),r=parseFloat(INP.setImuBr.value);
    if(![y,p,r].every(isFinite)||Math.abs(y)>180||Math.abs(p)>90||Math.abs(r)>180){toast(TL('imu_board_angles_range'),'bad');return;}
    if(!(await askConfirm(TL('confirm_save_imu_mount3d'))))return;
    await post('/settings_save_imu_mount3d_angles',{yaw:y,pitch:p,roll:r},TL('imu_3d_mounting_saved'));});
  act('#imu3dOff',async()=>{if(!(await askConfirm(TL('confirm_imu_mount3d_off'))))return;
    await post('/settings_save_imu_mount3d',{m:''},TL('imu_3d_mounting_switched_off'));});
  act('#imuSave',async()=>{const v=parseFloat($('#setImuRot').value);if(!isFinite(v)||Math.abs(v)>180){toast(TL('enter_a_number_in_the_range_180_180'),'bad');return;}
    if(!(await askConfirm(TL('confirm_save_imu_rotation'))))return;
    await post('/settings_save_imu_rotation',{rotDeg:v,rollInvert:INP.setImuRollInvert.checked?'1':'0'},TL('board_rotation_saved'));});
  foldLong(g);   // the step-by-step instructions fold under "Details" (the cards stay short and line up)
  cardFolding(g);
}
function updCal(s){
  $('#calAngle').textContent=s.steerSensorOk?f(s.steerAngleDeg,1,'°'):TL('none');
  $('#calRaw').textContent=TL('raw_adc_r_left_l_right_p',{r:s.steerRaw,l:s.steerAdcLeft,p:s.steerAdcRight});
  const span=s.steerRightDeg-s.steerLeftDeg;let p=span!==0?(s.steerAngleDeg-s.steerLeftDeg)/span:0.5;p=Math.max(0,Math.min(1,p));
  $('#calMark').style.left=(p*100)+'%';
  const zero=span!==0?(0-s.steerLeftDeg)/span:0.5;const z=$('#calZero');z.style.left=(Math.max(0,Math.min(1,zero))*100)+'%';
  const w=$('#calWarn');const unsaved=s.steerCalSaved===false;const bad=unsaved||(s.steerCalOk===false)||Math.abs(s.steerAdcRight-s.steerAdcLeft)<200;
  w.hidden=!bad;if(bad)w.textContent=unsaved?TL('warning_the_angle_sensor_was_never_calibrated'):TL('warning_the_left_and_right_points_are');
  updRows(calCards.imuList,s);
  // current sensor
  const has=isNum(s.currState);
  $('#curAmps').textContent=has&&s.currState>=2?f(s.currAmps,1,' A'):'—';
  $('#curState').textContent=has?CUR_STATE[s.currState]:TL('no_data_from_the_firmware_older_version');
  const cw=$('#curWarn');const wt=has?CUR_WARN[s.currState]:'';cw.hidden=!wt;cw.textContent=wt||'';
  cw.classList.toggle('bad',has&&(s.currState===3||s.currState===4));
  $('#curZero').disabled=!(has&&s.currentSensEn);$('#curReset').disabled=!(has&&s.currentSensEn);
  updRows(calCards.cur,s);
  // motor / runaway detector
  const hm=isNum(s.runState);
  $('#runState').textContent=!hm?TL('no_data_from_the_firmware_older_version'):(s.motorDriverEn?RUN_STATE[s.runState]:TL('motor_driver_disabled'));
  const rw=$('#runWarn');const tripTxt=hm&&s.motorDriverEn?runWarnText(s):'';const rt=tripTxt||(motorSettingsMissing(s)?MOTOR_SET_WARN:'');rw.hidden=!rt;rw.textContent=rt||'';rw.classList.toggle('bad',!!tripTxt);
  $('#runReset').disabled=!(hm&&s.motorDriverEn);
  updRows(calCards.mot,s);
  // manual test: buttons active only when the driver is enabled, steering is disabled and nothing is in progress
  const tOk=hm&&!!s.motorDriverEn&&isNum(s.testState);
  const tblock=!tOk||!!s.cmdEngaged||s.testState===1;
  $('#testL').disabled=tblock;$('#testR').disabled=tblock;
  // why the buttons are inactive (so as not to guess)
  const why=!hm?TL('no_data_from_the_module_older_firmware'):!s.motorDriverEn?TL('the_motor_driver_is_disabled_settings'):!isNum(s.testState)?TL('no_test_data_from_the_module_older'):s.cmdEngaged?TL('steering_in_agopenweb_is_engaged_disengage'):s.testState===1?TL('test_in_progress'):'';
  const tw=$('#testWhy');tw.hidden=!tblock||!why;tw.textContent=why?TL('buttons_inactive_w',{w:why}):'';
  const tt=tOk?testResultText(s):'';const tr=$('#testRes');tr.hidden=!tt;tr.textContent=tt;tr.classList.toggle('bad',tOk&&s.testState===3);
}

/* --- settings --- */
const INP={};const VAL={};
// ---- Passwords: the module does not send them back in /status (only the "set" flag). An empty field = unchanged; "delete password" sends <name>Clear=1. ----
function pwBtn(i){return i.parentElement&&i.parentElement.querySelector('.pwclear');}
function setPwClear(i,on){i.dataset.clear=on?'1':'0';i.value='';i.disabled=on;
  i.placeholder=on?TL('the_password_will_be_removed_on_save'):TL('set_empty_unchanged');const b=pwBtn(i);if(b)b.textContent=on?TL('undo_removal'):TL('remove_password');}
function setPwState(id,isSet){const i=INP[id];if(!i)return;i.dataset.pwset=isSet?'1':'0';i.dataset.clear='0';i.value='';i.disabled=false;
  i.placeholder=isSet?TL('set_empty_unchanged'):TL('no_password');const b=pwBtn(i);if(b){b.textContent=TL('remove_password');b.hidden=!isSet;}}
const PW_FIELDS=[['setWifiPass','wifiPass'],['setWifiPass2','wifiPass2'],['setWifiPass3','wifiPass3'],['setWifiPass4','wifiPass4'],['setApPass','apPass'],['setNtripPass','ntripPass']];
function applyPwState(s){PW_FIELDS.forEach(([id,k])=>{if(!INP[id])return;
  if(typeof s[k+'Set']==='boolean')setPwState(id,s[k+'Set']);            // new firmware: only the flag
  else if(s[k]!==undefined){INP[id].dataset.pwset='0';INP[id].value=s[k];}});}   // old firmware returned the password as is
async function refreshPw(){try{const r=await fetch('/status',{cache:'no-store'});applyPwState(await r.json());}catch(e){}}
function fld(id,label,type,o){
  o=o||{};const w=document.createElement('label');w.className='field';
  const sp=document.createElement('span');sp.textContent=label;const i=document.createElement('input');i.id=id;i.type=type||'text';
  if(o.step)i.step=o.step;if(o.min!==undefined)i.min=o.min;if(o.max!==undefined)i.max=o.max;if(o.max!==undefined&&type==='text')i.maxLength=o.max;
  if(o.ph)i.placeholder=o.ph;i.autocomplete='off';
  const em=document.createElement('span');em.className='em';
  w.append(sp,i);if(o.hint){const h=document.createElement('span');h.className='hint';h.textContent=o.hint;w.appendChild(h);}
  if(o.pw){i.dataset.pw='1';i.dataset.pwset='0';i.dataset.clear='0';   // passwords are NOT sent back from the module: an empty field = unchanged
    if(!o.noClear){const b=document.createElement('button');b.type='button';b.className='pwclear';b.textContent=TL('remove_password');b.hidden=true;
      b.onclick=e=>{e.preventDefault();setPwClear(i,i.dataset.clear!=='1');VAL[id]&&VAL[id]();};w.appendChild(b);}}
  w.appendChild(em);
  INP[id]=i;VAL[id]=()=>{const e=o.check?o.check(i.value):null;w.classList.toggle('err',!!e);em.textContent=e||'';return e;};
  i.addEventListener('input',VAL[id]);return w;
}
const chkMask=v=>{const m=v.trim().split('.');if(m.length!==4||!m.every(x=>/^\d{1,3}$/.test(x)&&+x<=255))return TL('invalid_subnet_mask');let n=0;m.forEach(x=>{n=((n<<8)|+x)>>>0;});const inv=(~n)>>>0;return n!==0&&((inv&((inv+1)>>>0))>>>0)===0?null:TL('invalid_mask_e_g_255_255_255_0');};
const chkIp=v=>/^(\d{1,3}\.){3}\d{1,3}$/.test(v.trim())&&v.trim().split('.').every(x=>+x<=255)?null:TL('invalid_ip_address');
const chkNum=(a,b,u)=>v=>{const n=Number(v);return v!==''&&isFinite(n)&&n>=a&&n<=b?null:TL('range_a_b',{a:a,b:b})+(u?' '+u:'');};
function sw(id,label){const l=document.createElement('label');l.className='swrow';l.innerHTML='<span class="st"></span><span class="sw"><input type="checkbox"><span class="tr"></span></span>';l.firstChild.textContent=label;const i=l.querySelector('input');i.id=id;INP[id]=i;return l;}
function card(title){const c=document.createElement('article');c.className='card';const h=document.createElement('h3');h.textContent=title;c.appendChild(h);return c;}
/* Machine outputs: what the lift is doing and how the AgOpenWeb pin map reads (Pin 1..16 -> function). */
const FN_CHIP=['','S1','S2','S3','S4','S5','S6','S7','S8','S9','S10','S11','S12','S13','S14','S15','S16','Up','Dn','TL','TR','GS'];   // short labels on the pin chips
const FN_SHORT=['','S1','S2','S3','S4','S5','S6','S7','S8','S9','S10','S11','S12','S13','S14','S15','S16','HydUp','HydDown','TramL','TramR','GeoStop'];
function hydText(s){return s.hydLiftActive?TL('lifting'):s.hydDownActive?TL('lowering'):TL('idle');}
function outText(s){if(!Array.isArray(s.machPins))return '—';const l=s.machPins.map((f,i)=>(f&&((s.machOut>>>i)&1))?(i+1)+':'+(FN_SHORT[f]||f):'').filter(Boolean);return l.length?l.join('  '):TL('none');}
function pinMapText(s){if(!Array.isArray(s.machPins))return '—';const l=s.machPins.map((f,i)=>f?(i+1)+':'+(FN_SHORT[f]||f):'').filter(Boolean);return l.length?l.join('  '):TL('none');}
/* Binds an action to a button that is looked up by selector: ignores taps while the previous run is still going
   (a "busy" flag, not [disabled], because the status poll owns the disabled state of some of these buttons). */
function act(sel,fn){const b=$(sel);b.onclick=async ev=>{if(b.dataset.busy)return;b.dataset.busy='1';try{await fn(ev);}finally{delete b.dataset.busy;}};}
function btns(c,list){const b=document.createElement('div');b.className='btns';list.forEach(([t,cls,fn])=>{const x=document.createElement('button');x.className='btn '+(cls||'');x.textContent=t;x.onclick=async ev=>{if(x.disabled)return;x.disabled=true;try{await fn(ev);}finally{x.disabled=false;}};b.appendChild(x);});c.appendChild(b);return b;}
function validate(ids){let bad=false;ids.forEach(id=>{if(VAL[id]&&VAL[id]())bad=true;});if(bad)toast(TL('correct_the_marked_fields'),'bad');return !bad;}
function vals(map){const o={};for(const k in map){const i=INP[map[k]];
  if(i.dataset.pw==='1'){if(i.dataset.clear==='1'){o[k+'Clear']='1';continue;}if(i.value===''&&i.dataset.pwset==='1')continue;}   // unchanged: we do not send
  o[k]=i.value;}return o;}
/* --- AgOpenWeb senders (Settings): list, status, choosing who the module talks to --- */
const SND={rows:{},head:null,list:null,empty:null,autoBtn:null};
function sndStatus(x){   // [tekst, klasa]
  const live=x.age>=0&&x.age<5000;
  if(x.sel)return live?[TL('connected_2'),'ok']:[TL('selected_but_silent'),'warn'];
  return live?[TL('ignored'),'warn']:[TL('inactive'),'muted'];
}
async function sndPick(ip){
  if(!(await askConfirm(ip?TL('confirm_pin_sender_n',{n:ip}):TL('confirm_sender_automatic'))))return;
  await post('/settings_save_agio_sender',{ip:ip},ip?TL('the_module_now_talks_only_to_ip',{ip:ip}):TL('automatic_mode_first_active_sender'));
}
function mkSndRow(ip){
  const el=document.createElement('div');el.className='snd-row';
  const a=document.createElement('b');a.className='snd-ip';a.textContent=ip;
  const st=document.createElement('span');st.className='snd-st';
  const sub=document.createElement('span');sub.className='snd-sub';
  const b=document.createElement('button');b.className='btn';b.onclick=()=>sndPick(ip);
  el.append(a,st,sub,b);
  return {el,sub,st,b};
}
/* --- how AgOpenWeb marks "steering enabled" in the PGN 254 status byte --- */
const ENG={btns:{},head:null};
function buildEngage(g){
  const c=card(TL('steering_engage_detection_takes_effect'));
  ENG.head=document.createElement('p');ENG.head.className='snd-head';c.appendChild(ENG.head);
  const seg=document.createElement('div');seg.className='seg';
  [['1',TL('bit_0x01_agopenweb_default')],['4',TL('bit_0x04_other_code')],['5',TL('both_bits_0x01_or_0x04')]].forEach(([m,t])=>{
    const b=document.createElement('button');b.className='btn';b.textContent=t;
    b.onclick=async()=>{if(b.dataset.busy)return;b.dataset.busy='1';try{
      if(!(await askConfirm(TL('confirm_engage_mode_n',{n:t}))))return;
      await post('/settings_save_agio_engage',{mask:m},TL('steering_per_agopenweb_t',{t:t}));}finally{delete b.dataset.busy;}};
    ENG.btns[m]=b;seg.appendChild(b);
  });
  c.appendChild(seg);
  const n=document.createElement('p');n.className='note';
  n.textContent=TL('agopenweb_sends_the_steering_state_in_the')+
    TL('bit_0x04_and_both_bits_are_kept_for_other');
  c.appendChild(n);
  g.appendChild(c);
}
function updEngage(s){
  if(!ENG.head||!isNum(s.cmdEngageMask))return;
  const m=String(s.cmdEngageMask);
  for(const k in ENG.btns)ENG.btns[k].className='btn'+(k===m?' on':'');
  const sb=isNum(s.cmdStatusByte)?s.cmdStatusByte.toString(16).toUpperCase().padStart(2,'0'):'—';
  ENG.head.textContent=TL('now_status_byte_from_agopenweb_b_steering',{b:sb,p:s.cmdEngaged?TL('engaged'):TL('not_engaged'),m:s.cmdEngageMask.toString(16).toUpperCase().padStart(2,'0')});
}
)HTML"
