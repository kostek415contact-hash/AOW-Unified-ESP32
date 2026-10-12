// ======================================================================
// web/ui/js_defs.h
// EN: Script part 2: declarative cards and the data definitions (rows of Readings / Diagnostics). Part of the panel page (web/web_ui.h joins the parts in order; they are one string in flash).
// PL: Skrypt cz. 2: karty deklaratywne i definicje danych (wiersze Odczytu / Diagnostyki). Część strony panelu (web/web_ui.h skleja części po kolei; w pamięci flash to jeden napis).
// ======================================================================
R"HTML(/* ================= declarative cards ================= */
// row: [label, fn(s)->text, fn(s)->class, fn(s)->whether_to_show]
function mkRows(box,rows){
  return rows.map(r=>{
    const row=document.createElement('div');row.className='row';
    const l=document.createElement('span');l.className='rl';l.textContent=r[0];
    const v=document.createElement('span');v.className='rv';
    row.append(l,v);box.appendChild(row);
    return {row,v,fn:r[1],cls:r[2],show:r[3]};
  });
}
function updRows(list,s){
  for(const r of list){
    if(r.show&&!r.show(s)){r.row.hidden=true;continue;}
    r.row.hidden=false;let val,cls='';
    try{val=r.fn(s);}catch(e){val='—';(window.rowErrors=window.rowErrors||[]).push(String(e&&e.message));}   // kept for the browser smoke test
    try{cls=(r.cls&&r.cls(s))||'';}catch(e){}
    r.v.textContent=(val===undefined||val===null)?'—':val;
    r.v.className='rv '+cls;
  }
}
function mkCard(title,rows){
  const c=document.createElement('article');c.className='card';
  const h=document.createElement('h3');h.textContent=title;c.appendChild(h);
  const box=document.createElement('div');c.appendChild(box);
  c._list=mkRows(box,rows);return c;
}

/* ================= data definitions ================= */
const agioOk=s=>isNum(s.agioAgeMs)?(s.agioAgeMs>=0&&s.agioAgeMs<2000):!!(s.cmdFresh||s.machineFresh);
const gapTxt=(mx,n1,n2)=>fmtMs(mx)+' • >1 s: '+n1+' • >2 s: '+n2;
const gapCls=(n1,n2)=>n2>0?'bad':n1>0?'warn':'ok';
const netCls=s=>s.apMode?'warn':s.netConnected?'ok':'bad';
/* RTK corrections age: ageDgps = -1 when none (empty GGA field / plain GPS / GGA stopped flowing), ageLvl from the firmware: 0 ok, 1 late, 2 stalled, 3 no data */
const ageTxt=s=>s.ageDgps>=0?f(s.ageDgps,1,' s'):TL('none_2');
const ageCls=s=>({0:'ok',1:'warn',2:'bad'})[s.ageLvl]||'';
const READ=[
 [TL('gps_position'),[
  ['Fix',s=>fixName(s.fixQuality),s=>fixCls(s.fixQuality)],
  [TL('satellites'),s=>s.satellites],
  [TL('frequency'),s=>f(s.gpsRateHz,1,' Hz')],
  ['HDOP',s=>f(s.hdop,1)],
  [TL('horizontal_accuracy_hacc'),s=>s.hAcc>=0?f(s.hAcc,3,' m'):'—'],
  [TL('vertical_accuracy_vacc'),s=>s.vAcc>=0?f(s.vAcc,3,' m'):'—'],
  [TL('altitude'),s=>f(s.altitude,1,' m')],
  [TL('latitude'),s=>s.lat],[TL('longitude'),s=>s.lon]]],
 [TL('gps_motion'),[
  [TL('speed'),s=>f(s.gpsSpeedKmh,1,' km/h')],[TL('course'),s=>f(s.gpsHeading,1,'°')],[TL('rtk_correction_age'),ageTxt,ageCls]]],
 ['IMU (BNO085)',[
  [TL('connection'),s=>yn(s.imuConnected,'OK',TL('none')),s=>s.imuConnected?'ok':'bad'],
  [TL('heading_in_panda'),s=>f(s.imuHeading,1,'°')],
  [TL('pitch_after_correction'),s=>f(s.imuPitch,1,'°')],[TL('roll_after_correction'),s=>f(s.imuRoll,1,'°')],
  [TL('acceleration_x_y_z'),s=>f(s.imuAccelX,2)+' / '+f(s.imuAccelY,2)+' / '+f(s.imuAccelZ,2)+' m/s²']]],
 [TL('steering_agopenweb'),[
  [TL('angle_sensor'),s=>s.steerSensorOk?s.steerRaw+' / '+f(s.steerAngleDeg,1,'°'):TL('none'),s=>s.steerSensorOk?'':'bad'],
  [TL('command'),s=>s.cmdFresh?(s.cmdEngaged?TL('engaged'):TL('ready')):TL('none'),s=>s.cmdFresh?(s.cmdEngaged?'ok':'warn'):'bad'],
  [TL('target_angle'),s=>f(s.cmdTargetAngle,1,'°'),null,s=>s.cmdFresh],
  [TL('speed_aog'),s=>f(s.cmdSpeedKmh,1,' km/h'),null,s=>s.cmdFresh],
  [TL('active_sections'),s=>bitsCount(s.cmdSections)+' / 16',null,s=>s.cmdFresh]]],
 [TL('inputs_outputs'),[
  [TL('steering_button'),s=>s.steerBtnPressed?TL('pressed'):TL('released'),s=>s.steerBtnPressed?'ok':''],
  [TL('work_switch'),s=>s.workSwitchActive?TL('working'):TL('transport'),s=>s.workSwitchActive?'ok':''],
  [TL('motor_driver'),s=>s.motorDriverEn?TL('active'):TL('inactive'),s=>s.motorDriverEn?'ok':'warn'],
  [TL('section_expander'),s=>s.sectionExpEn?'MCP23017 OK':(s.sectionExpCfg?TL('not_responding'):TL('inactive')),s=>s.sectionExpEn?'ok':(s.sectionExpCfg?'bad':'warn')],
  [TL('hydraulics_hydlift'),s=>hydText(s)+(s.hydLiftManual?TL('manual'):' [AUTO]'),s=>(s.hydLiftActive||s.hydDownActive)?'ok':'',s=>s.hydLiftEn],
  [TL('lift_times_from_agopenweb_raise_lower'),s=>s.machCfgRx?(s.liftRaiseSec+' / '+s.liftLowerSec+' s'):TL('not_received_yet'),null,s=>s.hydLiftEn],
  [TL('sections_from_aog'),s=>s.machineFresh?'0x'+(s.sectionsMask>>>0).toString(16).toUpperCase().padStart(4,'0'):'—',s=>s.machineFresh?'ok':''],
  [TL('current_sensor'),s=>s.currentSensEn?(isNum(s.currState)?(s.currState===2?f(s.currAmps,1,' A'):CUR_STATE[s.currState]):TL('active')):TL('inactive'),s=>s.currentSensEn?((!isNum(s.currState)||s.currState===2)?'ok':(s.currState>=3?'bad':'warn')):'warn'],
  ['Ethernet',s=>s.useEthernet?TL('active'):TL('disabled_wifi'),s=>s.useEthernet?'ok':'']]],
 [TL('network_and_rtcm_corrections'),[
  [TL('mode'),s=>s.apMode?TL('own_access_point_ap'):s.netType,s=>netCls(s)],
  [TL('signal_connection'),s=>s.netConnected?(s.netType==='WiFi'?s.rssi+' dBm':TL('connected')):TL('none'),s=>netCls(s)],
  [TL('ip_address'),s=>s.ip],
  [TL('rtcm_from_agopenweb'),s=>TL('kb_n_pkts',{kb:fmtKB(s.rtcmBytes),n:s.rtcmPackets})],
  [TL('own_ntrip'),s=>s.ntripStreaming?TL('streaming'):TL('connecting'),s=>s.ntripStreaming?'ok':'warn',s=>s.ntripEnabled],
  [TL('ntrip_received'),s=>fmtKB(s.ntripBytes),null,s=>s.ntripEnabled]]],
 [TL('system'),[
  [TL('firmware_version_build'),s=>(s.firmwareVersion||'?')+' • '+s.firmwareBuild],
  [TL('uptime'),s=>fmtUp(s.uptimeSec)],
  [TL('core_1_loop_load'),s=>f(s.loopLoadPct,1,'%'),s=>s.loopLoadPct>85?'warn':''],
  [TL('core_0_web_ota_load'),s=>f(s.coreZeroLoadPct,1,'%'),s=>s.coreZeroLoadPct>80?'warn':''],
  [TL('loop_cycle_time'),s=>f(s.loopAvgMs,2,' ms')],
  [TL('free_ram'),s=>isNum(s.freeHeap)?Math.round(s.freeHeap/1024)+' KB':'—',s=>s.freeHeap<60000?'warn':''],
  ['PSRAM',s=>s.psramFound?f(s.freePsram/1048576,1)+' / '+f(s.totalPsram/1048576,1)+' MB':TL('none'),s=>s.psramFound?'':'warn'],
  [TL('chip_temperature'),s=>f(s.chipTempC,0,' °C'),s=>s.chipTempC>75?'warn':'',s=>isNum(s.chipTempC)],
  [TL('last_restart'),s=>argTr(s.resetReason),s=>/PANIC|watchdog|brownout/i.test(s.resetReason||'')?'warn':'',s=>!!s.resetReason],
  [TL('lifetime_starts'),s=>s.lifeBoots+' • '+TL('lifetime_abnormal_n',{n:s.lifeAbnormal}),s=>s.lifeAbnormal>0?'warn':'',s=>isNum(s.lifeBoots)],
  [TL('lifetime_motor_time'),s=>fmtUp(s.lifeMotorSec),null,s=>isNum(s.lifeMotorSec)],
  [TL('lifetime_trips'),s=>TL('lifetime_trips_n',{a:s.lifeRunaway,b:s.lifeKickout,c:s.lifeDeadman}),s=>(s.lifeRunaway+s.lifeKickout+s.lifeDeadman)>0?'warn':'',s=>isNum(s.lifeRunaway)]]]
];

const FLAGS=[ // POST key, label, JSON field
 ['dStepTiming',TL('step_timing_cpu_by_module'),'debugStepTiming'],['dWifi','WiFi','debugWifi'],['dEth','Ethernet','debugEthernet'],
 ['dNet',TL('network_pgn'),'debugNet'],['dPgn',TL('pgn_general'),'debugPgn'],['dGps','GPS','debugGps'],['dRtcm','RTCM / NTRIP','debugRtcm'],
 ['dSteerBtn',TL('steer_button'),'debugSteerButton'],['dWorkSw',TL('work_switch'),'debugWorkSwitch'],['dSteerSens',TL('angle_sensor'),'debugSteeringSensor'],['dCurrent',TL('current_sensor'),'debugCurrent'],['dMotor',TL('motor_runaway_detector'),'debugMotor'],
 ['dImu','IMU','debugImu'],['dSteerCmd',TL('steer_command'),'debugSteerCommand'],['dSteerCmdRaw',TL('steer_command_raw'),'debugSteerCommandRaw'],
 ['dMachCmd',TL('machine_command'),'debugMachineCommand'],['dSys',TL('system'),'debugSys'],['dWeb','WWW / OTA','debugWeb']];
const flagState={};let flagPending=false;

// Verdict of the manual RST test from three independent indications: whether the sensor was silent, how long the
// stream took to return, and whether yaw returned to ~0 (after a real reset it does).
function angDiff(a,b){let d=a-b;while(d>180)d-=360;while(d<-180)d+=360;return d;}
function rstVerdict(s){
  const st=s.imuRstState;
  if(st===1)return TL('sensor_reset_in_progress');
  if(st===3)return s.imuRstWasSilent
    ?TL('the_sensor_was_silent_and_did_not_come')
    :TL('after_the_rst_reset_the_sensor_did_not');
  if(st!==2)return '';
  const ms=s.imuRstMs,yb=s.imuRstYawBefore,ya=s.imuRstYawAfter;
  if(s.imuRstWasSilent)return TL('the_sensor_was_silent_and_came_back_after',{ms:ms});
  const moved=Math.abs(yb)>5;
  if(moved&&Math.abs(ya)<3)return TL('rst_works_yaw_went_from_a_deg_to_b_deg_the',{a:f(yb,1),b:f(ya,1),ms:ms});
  if(moved&&Math.abs(angDiff(ya,yb))<3)return TL('rst_did_not_reset_the_sensor_yaw_stayed_at',{a:f(ya,1),b:f(yb,1)});
  return (ms>=60?TL('the_stream_broke_for_ms_ms_looks_like_the',{ms:ms}):TL('the_stream_came_back_after_ms_ms_i_e',{ms:ms}))+' '+TL('to_be_sure_rotate_the_enclosure_by_about');
}
// What the pair (time since the last frame, bytes/s) says about the cause - without guessing beyond the data.
function imuDiagnosis(s){
  if(s.imuSilentMs<1000)return TL('stream_ok');
  if(s.imuBps===0)return TL('rx_line_is_silent_0_b_s_the_sensor')+(s.imuRstEn===false?TL('recommended_connect_the_sensor_rst_wire_to'):'');
  return TL('there_are_bytes_on_the_line_n_b_s_but_they',{n:s.imuBps});
}
// WiFi disconnect reason codes (wifi_err_reason_t from ESP-IDF) -> a short description.
const WIFI_REASON={1:TL('unspecified'),2:TL('authentication_expired'),3:TL('station_left_the_network'),4:TL('association_expired_no_traffic'),5:TL('ap_too_many_stations'),
  8:TL('disconnected_on_request'),15:TL('key_handshake_timeout_wrong_password'),16:TL('group_key_update_timeout'),23:TL('802_1x_authentication_error'),
  200:TL('signal_lost_beacon_timeout'),201:TL('network_not_found_out_of_range_or_switched'),202:TL('authentication_error_password'),203:TL('association_error'),
  204:TL('handshake_timeout_password'),205:TL('connection_failed'),206:TL('access_point_tsf_reset'),207:TL('roaming')};
const wifiReasonText=c=>c+' · '+(WIFI_REASON[c]||TL('unknown_code'));
const PGN_NAME={200:'Hello',202:'Scan',254:TL('steer_command_2'),239:'machine',252:TL('steer_settings'),215:'RTCM'};
function pgnList(s){
  const a=s.pgnRx||[];
  if(!a.length)return TL('nothing_received');
  return a.map(x=>x.p+(PGN_NAME[x.p]?' '+PGN_NAME[x.p]:'')+': '+x.n+'×').join(', ')+(s.pgnOther>0?TL('other_n',{n:s.pgnOther}):'');
}
const OTA_STATE=[TL('none_since_start'),TL('in_progress'),TL('finished_ok'),TL('error')];
const GPS_BAUD_SRC=[TL('from_config_h'),TL('detected_by_autobaud'),TL('fallback_9600'),TL('detected_on_retry')];
const GPS_CHIPSET=(s)=>s.gpsChipset===1?TL('quectel_v',{v:s.gpsChipsetInfo||TL('no_version')}):s.gpsChipset===2?TL('no_reply_to_pqtmver_not_a_quectel_or_not'):TL('not_checked_yet');
const DIAG={ // flag key -> {rows | build}
 dStepTiming:{title:TL('step_timing_cpu_by_module_core_1'),build:(box,s)=>{
   const steps=s.cpuSteps||[];let sum=0;
   if(!steps.length){box.innerHTML='<p class="off-hint">'+TL('waiting_for_the_first_full_measurement')+'</p>';return;}
   box.innerHTML='';const list=mkRows(box,steps.map(x=>{sum+=x.p;return [x.n,()=>f(x.p,1,' %')+'  (max '+f(x.m/1000,1)+' ms)',()=>x.p>40?'bad':x.p>15?'warn':''];}));
   list.push(...mkRows(box,[[TL('rest_not_measured_idle'),()=>f(Math.max(0,100-sum),1,' %')]]));updRows(list,s);
   const n=document.createElement('p');n.className='note';n.textContent=TL('pct_of_time_spent_in_the_module_including');box.appendChild(n);}},
 dWifi:{title:'WiFi',rows:[
   [TL('mode'),s=>s.apMode?TL('ap_own_access_point'):s.netType,s=>netCls(s)],
   [TL('connection'),s=>yn(s.netConnected,TL('yes'),TL('none')),s=>s.netConnected?'ok':'bad'],
   [TL('address_with_name_mdns'),s=>'http://'+s.mdnsName+'.local',null,s=>typeof s.mdnsName==='string'&&s.mdnsName.length>0],
   [TL('network_ssid'),s=>s.wifiSsid],[TL('signal'),s=>s.netType==='WiFi'?s.rssi+' dBm':'—'],[TL('ip_address'),s=>s.ip],
   [TL('address_mask_gateway_actual'),s=>s.ipNow+' / '+s.maskNow+' / '+s.gwNow,null,s=>typeof s.ipNow==='string'&&s.ipNow!=='0.0.0.0'],
   [TL('address_assigned_by'),s=>s.wifiDhcpNow?TL('dhcp_router_phone'):TL('module_settings_fixed_address'),null,s=>typeof s.wifiDhcpNow==='boolean'&&s.netConnected&&s.netType==='WiFi'],
   [TL('where_i_send_to_agopenweb'),s=>s.agioDest+(s.agioKnown?TL('known_address_unicast'):TL('broadcast_waiting_for_the_first_packet')),s=>s.agioKnown?'ok':'warn',s=>typeof s.agioDest==='string'],
   [TL('connection_losses_since_start'),s=>s.wifiLost,s=>s.wifiLost>0?'warn':'ok',s=>isNum(s.wifiLost)],
   [TL('last_disconnect'),s=>s.wifiReason<0?TL('none_yet'):TL('r_t_ago',{r:wifiReasonText(s.wifiReason),t:fmtMs(s.wifiReasonAgeMs)}),null,s=>isNum(s.wifiReason)],
   [TL('wifi_outage_time'),s=>s.wifiOutN>0?TL('wifi_outage_val',{last:fmtMs(s.wifiOutLastMs),max:fmtMs(s.wifiOutMaxMs),avg:fmtMs(s.wifiOutAvgMs),n:s.wifiOutN}):TL('none_yet'),s=>s.wifiOutMaxMs>5000?'warn':'',s=>isNum(s.wifiOutN)],
   [TL('wifi_outage_now'),s=>fmtMs(s.wifiOutNowMs),s=>'bad',s=>isNum(s.wifiOutNowMs)&&s.wifiOutNowMs>0],
   [TL('failover_scans_switches_to_another_network'),s=>s.wifiScans+' / '+s.wifiSwitches,null,s=>isNum(s.wifiScans)],
   [TL('restarts_caused_by_wifi_loss_in_a_row'),s=>TL('n_of_2',{n:s.wifiFbRestarts}),s=>s.wifiFbRestarts>0?'warn':'ok',s=>isNum(s.wifiFbRestarts)]]},
 dEth:{title:TL('ethernet_w5500'),rows:[
   [TL('state'),s=>s.useEthernet?TL('active'):s.enEthernet?TL('enabled_no_link_wifi_in_use'):TL('disabled_in_settings'),s=>s.useEthernet?'ok':s.enEthernet?'warn':'muted'],
   [TL('ip_address'),s=>s.ip,null,s=>s.useEthernet]],
   note:TL('to_use_ethernet_enable_it_in_settings_and')},
 dNet:{title:TL('network_and_communication_with_agopenweb'),rows:[
   [TL('last_packet_from_agopenweb'),s=>isNum(s.agioAgeMs)?(s.agioAgeMs<0?TL('never'):TL('t_ago',{t:fmtMs(s.agioAgeMs)})):'—',s=>agioOk(s)?'ok':'bad'],
   [TL('module_address'),s=>s.localIp],[TL('gateway'),s=>s.gateway],[TL('mask'),s=>s.subnet],[TL('agopenweb_broadcast'),s=>s.agioIp],
   [TL('rtcm_from_agopenweb'),s=>TL('kb_n_pkts',{kb:fmtKB(s.rtcmBytes),n:s.rtcmPackets})],
   [TL('packets_sent_succeeded_errors_tx_fail'),s=>s.txOk+' / '+s.txFail,s=>s.txFail>0?'warn':'ok',s=>isNum(s.txOk)],
   [TL('last_send_error'),s=>s.txFailAgeMs<0?TL('never'):TL('t_ago',{t:fmtMs(s.txFailAgeMs)}),null,s=>isNum(s.txFailAgeMs)],
   [TL('udp_socket_rebuilds_after_the_network'),s=>s.udpRebuilds,null,s=>isNum(s.udpRebuilds)],
   [TL('rejected_frames_bad_header_or_length'),s=>s.rxBad,s=>s.rxBad>0?'warn':'ok',s=>isNum(s.rxBad)],
   [TL('gga_from_the_receiver_longest_gap'),s=>gapTxt(s.ggaGapMaxMs,s.ggaGapN1,s.ggaGapN2),s=>gapCls(s.ggaGapN1,s.ggaGapN2),s=>isNum(s.ggaGapMaxMs)],
   [TL('panda_send_attempts_longest_gap'),s=>gapTxt(s.pandaTryMaxMs,s.pandaTryN1,s.pandaTryN2),s=>gapCls(s.pandaTryN1,s.pandaTryN2),s=>isNum(s.pandaTryMaxMs)],
   [TL('panda_successful_sends_longest_gap'),s=>gapTxt(s.pandaOkMaxMs,s.pandaOkN1,s.pandaOkN2),s=>gapCls(s.pandaOkN1,s.pandaOkN2),s=>isNum(s.pandaOkMaxMs)],
   [TL('last_gap_1_s_in_panda_sending'),s=>s.pandaBigGapAgeMs<0?TL('none_3'):TL('t_ago',{t:fmtMs(s.pandaBigGapAgeMs)}),null,s=>isNum(s.pandaBigGapAgeMs)]],
   note:TL('agopenweb_considers_the_gps_module')},
 dPgn:{title:TL('pgn_commands_and_settings_from_agopenweb'),rows:[
   [TL('steer_command_pgn254'),s=>s.cmdFresh?TL('fresh'):TL('none_2'),s=>s.cmdFresh?'ok':'bad'],
   [TL('machine_command_pgn239'),s=>s.machineFresh?TL('fresh'):TL('none_2'),s=>s.machineFresh?'ok':'warn'],
   [TL('steer_settings_pgn252'),s=>s.steerSettingsRx?(s.steerSettingsStored?TL('from_module_memory'):TL('received')):TL('not_received'),s=>s.steerSettingsRx?'ok':'warn'],
   ['countsPerDeg',s=>s.steerCountsPerDeg,null,s=>s.steerSettingsRx],['steerOffset',s=>s.steerOffset,null,s=>s.steerSettingsRx],
   ['ackermanFix',s=>s.steerAckermanFix,null,s=>s.steerSettingsRx],
   [TL('frames_received_by_pgn'),s=>pgnList(s),null,s=>Array.isArray(s.pgnRx)],
   [TL('steer_command_last_largest_interval'),s=>s.cmdDeltaMs+' / '+s.cmdMaxDeltaMs+' ms',s=>s.cmdMaxDeltaMs>500?'warn':'',s=>isNum(s.cmdDeltaMs)],
   [TL('crc_checksums_correct_wrong'),s=>s.crcOk+' / '+s.crcBad,s=>s.crcBad>0?'warn':'ok',s=>isNum(s.crcOk)],
   [TL('rejected_because_of_a_wrong_checksum_254'),s=>s.crcRejected,s=>s.crcRejected>0?'warn':'ok',s=>isNum(s.crcRejected)]],
   note:TL('control_commands_pgn_254_239_252_with_a')},
 dGps:{title:'GPS',rows:[
   ['Fix',s=>fixName(s.fixQuality),s=>fixCls(s.fixQuality)],[TL('satellites'),s=>s.satellites],[TL('frequency'),s=>f(s.gpsRateHz,1,' Hz')],
   ['HDOP',s=>f(s.hdop,1)],['hAcc / vAcc',s=>(s.hAcc>=0?f(s.hAcc,3):'—')+' / '+(s.vAcc>=0?f(s.vAcc,3):'—')+' m'],
   [TL('rtk_correction_age'),ageTxt,ageCls],[TL('speed_course'),s=>f(s.gpsSpeedKmh,1)+' km/h / '+f(s.gpsHeading,1)+'°'],
   [TL('uart_bytes'),s=>s.gpsDiagBytes],[TL('lines_starting_with'),s=>s.gpsDiagDollar],
   [TL('correct_checksum'),s=>s.gpsDiagChecksum+(s.gpsDiagDollar>0?' ('+f(100*s.gpsDiagChecksum/s.gpsDiagDollar,1)+' %)':''),s=>(s.gpsDiagDollar-s.gpsDiagChecksum)>Math.max(3,0.005*s.gpsDiagDollar)?'warn':'ok'],
   [TL('recognised_gga_vtg_rmc_gst'),s=>s.gpsDiagRecognized],
   [TL('data_throughput'),s=>gpsBps===null?'—':f(gpsBps/1024,1,' KB/s')],
   [TL('uart_speed'),s=>s.gpsBaud+' baud ('+(GPS_BAUD_SRC[s.gpsBaudSrc]||'?')+')',s=>s.gpsBaudSrc===2?'warn':'',s=>isNum(s.gpsBaud)],
   [TL('chipset'),s=>GPS_CHIPSET(s),s=>s.gpsChipset===1?'ok':s.gpsChipset===2?'warn':'',s=>isNum(s.gpsChipset)],
   [TL('gps_config'),s=>s.gpsCfg===1?TL('gps_cfg_written'):s.gpsCfg===2?TL('gps_cfg_matched'):s.gpsCfg===3?TL('gps_cfg_other'):TL('gps_cfg_running'),s=>s.gpsCfg>=1?'ok':'',s=>isNum(s.gpsCfg)],
   [TL('speed_detection_retries'),s=>s.gpsValidNmea?TL('not_needed_nmea_is_flowing'):(s.gpsRetryActive?TL('in_progress_attempt_n',{n:s.gpsRetryAttempts}):TL('every_15_s_attempts_n',{n:s.gpsRetryAttempts})),s=>s.gpsValidNmea?'ok':'warn',s=>typeof s.gpsValidNmea==='boolean']],keepRows:true,build:(box,s)=>renderSats(box)},
 dRtcm:{title:'RTCM / NTRIP',rows:[
   [TL('own_ntrip_client'),s=>s.ntripEnabled?(s.ntripStreaming?TL('streaming'):TL('connecting')):TL('disabled'),s=>s.ntripEnabled?(s.ntripStreaming?'ok':'warn'):'muted'],
   [TL('caster'),s=>s.ntripHost+':'+s.ntripPort+' / '+s.ntripMount,null,s=>s.ntripEnabled],
   [TL('received_2'),s=>fmtKB(s.ntripBytes),null,s=>s.ntripEnabled],
   [TL('consecutive_failures_time_to_next_attempt'),s=>s.ntripFails+' / '+fmtMs(s.ntripRetryMs)+(s.ntripRedirected?TL('caster_redirect'):''),s=>s.ntripFails>0?(s.ntripFailKind>=3?'bad':'warn'):'ok',s=>s.ntripEnabled&&isNum(s.ntripFails)],
   [TL('last_cause'),s=>NTRIP_FAIL[s.ntripFailKind]||'—',s=>s.ntripFailKind>=3?'bad':null,s=>s.ntripEnabled&&isNum(s.ntripFails)&&s.ntripFails>0],
   [TL('disconnects_since_start'),s=>s.ntripReconnects,s=>s.ntripReconnects>0?'warn':'',s=>s.ntripEnabled],
   [TL('gga_real_fallback'),s=>s.ntripGgaReal+' / '+s.ntripGgaFallback,null,s=>s.ntripEnabled],
   [TL('time_to_float_fix'),s=>s.ntripMsToFloat>0?f(s.ntripMsToFloat/1000,1,' s'):'—',null,s=>s.ntripEnabled],
   [TL('time_to_full_fix'),s=>s.ntripMsToFix>0?f(s.ntripMsToFix/1000,1,' s'):'—',null,s=>s.ntripEnabled],
   [TL('rtcm_frames_correct_crc_errors'),s=>s.ntripRtcmFrames+' / '+s.ntripRtcmCrcErr+(s.ntripRtcmCrcErr>0?' '+TL('p_pct_corrupted',{p:(100*s.ntripRtcmCrcErr/(s.ntripRtcmFrames+s.ntripRtcmCrcErr)).toFixed(1)}):''),s=>s.ntripRtcmCrcErr>0?(s.ntripRtcmCrcErr*20>s.ntripRtcmFrames?'bad':'warn'):'ok',s=>s.ntripEnabled&&isNum(s.ntripRtcmFrames)],
   [TL('rtcm_message_types'),s=>(s.ntripRtcmTypes&&s.ntripRtcmTypes.length)?s.ntripRtcmTypes.map(t=>t.type+': '+t.count+'×').join(', '):TL('no_data'),null,s=>s.ntripEnabled],
   [TL('correction_source_now'),s=>[TL('none_nothing_flowing'),TL('own_ntrip_client_2'),TL('agopenweb_relay_pgn_215')][s.rtcmSource]||'?',s=>s.rtcmSource===0?'warn':'ok',s=>isNum(s.rtcmSource)],
   [TL('rtcm_from_agopenweb_relay'),s=>TL('kb_n_pkts',{kb:fmtKB(s.rtcmBytes),n:s.rtcmPackets})],
   [TL('dropped_from_agopenweb_own_ntrip_is'),s=>TL('kb_n_pkts',{kb:fmtKB(s.rtcmDropBytes),n:s.rtcmDropPackets}),null,s=>isNum(s.rtcmDropPackets)]],
   note:TL('the_receiver_always_gets_a_single_source')},
 dSteerBtn:{title:TL('steering_button'),rows:[[TL('state'),s=>s.steerBtnPressed?TL('pressed'):TL('released'),s=>s.steerBtnPressed?'ok':'']]},
 dWorkSw:{title:TL('work_switch'),rows:[[TL('state'),s=>s.workSwitchActive?TL('working'):TL('transport'),s=>s.workSwitchActive?'ok':'']]},
 dSteerSens:{title:TL('steering_angle_sensor'),rows:[
   [TL('sensor'),s=>yn(s.steerSensorOk,'OK',TL('none')),s=>s.steerSensorOk?'ok':'bad'],
   [TL('raw_adc_reading'),s=>s.steerRaw],[TL('angle_converted'),s=>f(s.steerAngleDeg,1,'°')],
   [TL('saved_left_right_adc'),s=>s.steerAdcLeft+' / '+s.steerAdcRight],
   [TL('calibration_span_adc'),s=>Math.abs(s.steerAdcRight-s.steerAdcLeft),s=>Math.abs(s.steerAdcRight-s.steerAdcLeft)<200?'bad':''],
   [TL('left_right_angle_range'),s=>f(s.steerLeftDeg,1)+'° / '+f(s.steerRightDeg,1)+'°'],
   [TL('adc_input_voltage'),s=>f(s.steerVolts,3,' V'),null,s=>isNum(s.steerVolts)],
   [TL('i2c_errors_since_start'),s=>s.steerI2cErr,s=>s.steerI2cErr>0?'warn':'ok',s=>isNum(s.steerI2cErr)],
   [TL('reading_within_calibrated_range'),s=>s.steerRawInRange?TL('yes'):TL('no_wire_broken_or_loose'),s=>s.steerRawInRange?'ok':'bad',s=>typeof s.steerRawInRange==='boolean'],
   [TL('age_of_last_reading'),s=>s.steerReadAgeMs<0?TL('never'):fmtMs(s.steerReadAgeMs),s=>(s.steerReadAgeMs<0||s.steerReadAgeMs>250)?'bad':'ok',s=>isNum(s.steerReadAgeMs)]],
   note:TL('a_reading_older_than_250_ms_stops_the')},
 dMotor:{title:TL('motor_and_runaway_detector'),rows:[
   [TL('motor_driver'),s=>s.motorDriverEn?TL('active'):TL('inactive'),s=>s.motorDriverEn?'ok':'warn',s=>typeof s.motorDriverEn==='boolean'],
   [TL('runaway_detector'),s=>s.runEn?(RUN_STATE[s.runState]||'?'):TL('disabled_2'),s=>!s.runEn?'warn':s.runState===1?'ok':'bad',s=>typeof s.runEn==='boolean'],
   [TL('steering_armed'),s=>s.motorArmed?TL('armed'):TL('not_armed_switch_steering_off_and_on'),s=>s.motorArmed?'ok':(s.cmdEngaged?'bad':'warn'),s=>typeof s.motorArmed==='boolean'&&s.motorDriverEn],
   [TL('current_protection'),s=>s.currentSensEn?TL('active'):TL('off_only_end_stops_and_runaway_detector_guard'),s=>s.currentSensEn?'ok':'warn',s=>typeof s.currentSensEn==='boolean'&&s.motorDriverEn],
   [TL('loop_stalls_cut_by_safety_timer'),s=>s.motorDeadmanTrips,s=>s.motorDeadmanTrips>0?'bad':'ok',s=>isNum(s.motorDeadmanTrips)&&s.motorDriverEn],
   [TL('direction_dir_pin'),s=>s.motorInvert?TL('inverted'):TL('normal'),null,s=>typeof s.motorInvert==='boolean'],
   [TL('pwm_angle_convention_towards_the_positive'),s=>s.motorPwm,null,s=>isNum(s.motorPwm)&&s.motorDriverEn],
   [TL('angle_error_target_measured'),s=>f(s.motorErrDeg,2,'°'),null,s=>isNum(s.motorErrDeg)&&s.motorDriverEn],
   [TL('movement_in_the_window_towards_the_drive'),s=>f(s.runMoveDeg,2,'°')+' '+TL('required_a_deg_over_ms_ms',{a:f(s.runMinMoveDeg,1),ms:s.runWindowMs}),null,s=>isNum(s.runMoveDeg)&&s.motorDriverEn&&s.runEn],
   [TL('trips_since_start'),s=>s.runTrips+(s.runTrips>0?' '+TL('last_r_movement_m',{r:RUN_REASON[s.runLastReason],m:f(s.runLastMoveDeg,1,'°')}):''),s=>s.runTrips>0?'warn':'ok',s=>isNum(s.runTrips)],
   [TL('signal_to_agopenweb_pgn_250'),s=>s.kickoutActive?TL('sent_255_kickout'):TL('normal_load'),s=>s.kickoutActive?'bad':'ok',s=>typeof s.kickoutActive==='boolean'],
   [TL('kickout_signals_since_start'),s=>s.kickoutCount,s=>s.kickoutCount>0?'warn':'ok',s=>isNum(s.kickoutCount)],
   [TL('cause_of_last_kickout'),s=>KICK_TXT[s.kickoutCause]||'—',s=>s.kickoutCause>0?'warn':'ok',s=>isNum(s.kickoutCause)],
   [TL('settings_from_agopenweb_gainp_highpwm'),s=>s.steerSettingsRx?(s.steerGainP+' / '+s.steerHighPwm+' / '+s.steerMinPwm+(s.steerSettingsStored?' ('+TL('from_module_memory')+')':'')):TL('not_received'),s=>motorSettingsMissing(s)?'warn':'ok',s=>typeof s.steerSettingsRx==='boolean'],
   [TL('evaluation_since_the_error'),s=>f(s.runMinErrDeg,1,'°'),null,s=>isNum(s.runMinErrDeg)]],
   note:TL('reversed_direction_the_wheel_moves_the')},
 dCurrent:{title:TL('motor_current_sensor_acs71240'),rows:[
   [TL('state'),s=>CUR_STATE[s.currState]||'?',s=>s.currState===2?'ok':s.currState===0?'':'bad',s=>isNum(s.currState)],
   [TL('instantaneous_average_current'),s=>f(s.currAmps,2)+' / '+f(s.currAvgAmps,2)+' A',null,s=>isNum(s.currAmps)&&s.currentSensEn],
   [TL('peak_over_the_last_second'),s=>f(s.currRecentPeakA,2,' A'),null,s=>isNum(s.currRecentPeakA)&&s.currentSensEn],
   [TL('peak_since_reset'),s=>f(s.currPeakA,2,' A'),null,s=>isNum(s.currPeakA)&&s.currentSensEn],
   [TL('output_voltage_zero'),s=>f(s.currVolts,3)+' / '+f(s.currZeroV,3)+' V',null,s=>isNum(s.currVolts)&&s.currentSensEn],
   [TL('age_of_last_reading'),s=>s.currAgeMs<0?TL('never'):fmtMs(s.currAgeMs),s=>(s.currAgeMs<0||s.currAgeMs>250)?'bad':'ok',s=>isNum(s.currAgeMs)&&s.currentSensEn],
   [TL('sampling_rate'),s=>curSps===null?'—':f(curSps,0,TL('samples_s')),s=>curSps===null?'':curSps<15?'warn':'ok',s=>isNum(s.currSamples)&&s.currentSensEn],
   [TL('protection_trips_since_start'),s=>s.currTrips+(s.currTrips>0?' '+TL('last_at_a',{a:f(s.currLastTripA,1,' A')}):''),s=>s.currTrips>0?'warn':'ok',s=>isNum(s.currTrips)&&s.currentSensEn],
   [TL('thresholds_kickout_time_hard'),s=>f(s.currKickoutA,1)+' A / '+s.currKickoutMs+' ms / '+f(s.currHardA,1)+' A',null,s=>isNum(s.currKickoutA)],
   [TL('zero_calibration'),s=>curCalText(s),s=>s.currCalState===3?'bad':s.currCalState===2?'ok':'',s=>isNum(s.currCalState)&&s.currentSensEn&&s.currCalState>0]],
   note:TL('the_motor_stops_on_overcurrent_a_sensor')},
 dImu:{title:'IMU (BNO085)',rows:[
   [TL('connection'),s=>yn(s.imuConnected,'OK',TL('none')),s=>s.imuConnected?'ok':'bad'],
   [TL('heading'),s=>f(s.imuHeading,1,'°')],[TL('pitch_roll_after_correction'),s=>f(s.imuPitch,1)+' / '+f(s.imuRoll,1)+'°'],
   [TL('pitch_roll_raw'),s=>f(s.imuPitchRaw,1)+' / '+f(s.imuRollRaw,1)+'°'],[TL('board_rotation'),s=>f(s.imuRotDeg,1,'°')],[TL('roll_sign'),s=>s.imuRollInvert?TL('inverted'):TL('normal')],
   [TL('acceleration_x_y_z'),s=>f(s.imuAccelX,2)+' / '+f(s.imuAccelY,2)+' / '+f(s.imuAccelZ,2)+' m/s²'],
   [TL('time_since_last_packet'),s=>s.imuSilentMs+' ms',s=>s.imuSilentMs>500?'bad':'ok'],
   [TL('packets_s_expected_100'),s=>s.imuPps,s=>s.imuPps<80?'warn':'ok'],
   [TL('longest_gap_since_start'),s=>s.imuMaxGapMs+' ms',s=>s.imuMaxGapMs>500?'warn':'ok'],
   [TL('uart_restarts_self_healing'),s=>s.imuRestarts,s=>s.imuRestarts>0?'warn':'ok'],
   [TL('raw_uart_bytes_healthy_1900'),s=>s.imuBps+' B/s',s=>s.imuBps>=1500?'ok':s.imuBps>0?'warn':'bad',s=>isNum(s.imuBps)],
   [TL('last_uart_byte'),s=>s.imuLastByteMs<0?TL('never'):TL('t_ago',{t:fmtMs(s.imuLastByteMs)}),s=>s.imuLastByteMs<0||s.imuLastByteMs>1000?'bad':'ok',s=>isNum(s.imuLastByteMs)],
   [TL('sensor_hardware_reset_rst'),s=>s.imuRstEn?TL('enabled_done_n',{n:s.imuRstCount}):TL('disabled_no_pin_imu_reset_pin_in_config_h'),s=>s.imuRstEn?(s.imuRstCount>1?'warn':'ok'):'muted',s=>typeof s.imuRstEn==='boolean'],
   [TL('diagnosis'),s=>imuDiagnosis(s),s=>s.imuSilentMs<1000?'ok':'bad',s=>isNum(s.imuBps)]],
   keepRows:true,
   build:(box,s)=>{
     let w=box.querySelector('.rsttest');
     if(!w){
       w=document.createElement('div');w.className='rsttest';
       w.innerHTML='<div class="btns"><button class="btn">'+TL('reset_the_imu_sensor_rst_test')+'</button></div><div class="result" hidden></div>'+
         '<p class="note">'+TL('rst_wire_test_rotate_the_enclosure_by')+'</p>';
       box.appendChild(w);
       w.querySelector('button').onclick=async()=>{
         if(!(await askConfirm(TL('resetting_the_imu_sensor_changes_its'))))return;
         try{const r=await authFetch('/imu_reset',{method:'POST'});
           if(r.ok)toast(TL('rst_pulse_sent_result_in_a_moment'),'ok');
           else toast(TL('reset_unavailable_m',{m:logTr((await r.text()).trim())}),'bad');}
         catch(e){toast(TL('error_m',{m:e.message}),'bad');}
       };
     }
     w.hidden=(s.imuRstEn!==true);                    // only when the RST pin is configured
     const res=w.querySelector('.result'),v=rstVerdict(s);
     res.hidden=!v;res.textContent=v;
   }},
 dSteerCmd:{title:TL('steer_command_from_agopenweb'),rows:[
   [TL('command'),s=>s.cmdFresh?(s.cmdEngaged?TL('engaged'):TL('ready')):TL('none'),s=>s.cmdFresh?(s.cmdEngaged?'ok':'warn'):'bad'],
   [TL('target_angle'),s=>f(s.cmdTargetAngle,1,'°'),null,s=>s.cmdFresh],[TL('speed'),s=>f(s.cmdSpeedKmh,1,' km/h'),null,s=>s.cmdFresh],
   [TL('sections'),s=>bitsCount(s.cmdSections)+' / 16',null,s=>s.cmdFresh]]},
 dSteerCmdRaw:{title:TL('steer_command_raw_bytes'),rows:[
   [TL('last_frame_hex'),s=>s.cmdRaw||TL('nothing_received'),null,s=>typeof s.cmdRaw==='string'],
   [TL('commands_received_since_start'),s=>s.cmdRxCount,null,s=>isNum(s.cmdRxCount)],
   [TL('last_largest_interval'),s=>s.cmdDeltaMs+' / '+s.cmdMaxDeltaMs+' ms',s=>s.cmdMaxDeltaMs>500?'warn':'',s=>isNum(s.cmdDeltaMs)]],
   note:TL('byte_layout_80_81_header_3rd_sender_4th')},
 dMachCmd:{title:TL('machine_command_and_outputs'),rows:[
   [TL('command'),s=>s.machineFresh?TL('fresh'):TL('none_2'),s=>s.machineFresh?'ok':'warn'],
   [TL('section_mask'),s=>'0x'+(s.sectionsMask>>>0).toString(16).toUpperCase().padStart(4,'0')+'  ('+bitsCount(s.sectionsMask)+' / 16)'],
   [TL('expander_mcp23017'),s=>s.sectionExpEn?TL('active'):(s.sectionExpCfg?TL('not_responding'):TL('inactive')),s=>s.sectionExpEn?'ok':(s.sectionExpCfg?'bad':'warn')],
   ['hydLift',s=>s.hydLiftEn?(hydText(s)+(s.hydLiftManual?TL('manual'):' [AUTO]')):TL('no_pin_assigned')],
   [TL('outputs_on_now'),s=>outText(s),s=>s.machOut?'ok':''],
   [TL('pin_map_agopenweb'),s=>pinMapText(s)+'  ('+(s.machPinsRx?TL('pins_from_agopenweb'):TL('pins_stored_or_default'))+')',null,s=>Array.isArray(s.machPins)],
   [TL('invert_relay_agopenweb'),s=>s.machInvert?TL('relay_invert_on'):TL('relay_invert_off'),s=>s.machInvert?'warn':'',s=>typeof s.machInvert==='boolean'],
   [TL('user_values_agopenweb'),s=>Array.isArray(s.machUser)?s.machUser.join(' / '):'—',null,s=>s.machCfgRx],
   [TL('steer_config_pgn251'),s=>s.steerCfgRx?(TL('received')+(s.motorInvertAog?', '+TL('invert_motor_agopenweb')+': '+TL('inverted'):'')):TL('not_received'),s=>s.steerCfgRx?'ok':'warn',s=>typeof s.steerCfgRx==='boolean']],
   // 16 output pins of the expander: pin number, the function assigned in AgOpenWeb, lit while the output is on
   build:(box,s)=>{let b=box.querySelector('.bits');if(!b){b=document.createElement('div');b.className='bits pins';for(let i=0;i<16;i++){const e=document.createElement('i');e.appendChild(document.createElement('b')).textContent=String(i+1);e.appendChild(document.createElement('span'));b.appendChild(e);}box.appendChild(b);}
     const m=Array.isArray(s.machPins)?s.machPins:null;
     Array.from(b.children).forEach((el,i)=>{const f=m?m[i]:(i+1);el.classList.toggle('on',((m?s.machOut:s.sectionsMask)>>>i&1)===1);el.classList.toggle('idle',!f);
       el.lastChild.textContent=f?(FN_CHIP[f]||f):'';el.title=(i+1)+': '+(f?(FN_SHORT[f]||f):TL('none'));});},keepRows:true},
 dSys:{title:TL('system'),rows:[
   [TL('firmware_version_build'),s=>(s.firmwareVersion||'?')+' • '+s.firmwareBuild],[TL('uptime'),s=>fmtUp(s.uptimeSec)],
   [TL('last_restart'),s=>argTr(s.resetReason),null,s=>!!s.resetReason],
   [TL('core_1_load'),s=>f(s.loopLoadPct,1,'%'),s=>s.loopLoadPct>85?'warn':''],[TL('core_0_load'),s=>f(s.coreZeroLoadPct,1,'%')],
   [TL('loop_cycle_time'),s=>f(s.loopAvgMs,2,' ms')],
   [TL('free_ram_now'),s=>Math.round(s.freeHeap/1024)+' KB',s=>s.freeHeap<60000?'warn':''],
   [TL('minimum_free_ram_since_start'),s=>isNum(s.minFreeHeap)?Math.round(s.minFreeHeap/1024)+' KB':'—',s=>s.minFreeHeap<40000?'warn':'',s=>isNum(s.minFreeHeap)],
   [TL('free_ram_trend'),s=>!s.heapTrendValid?TL('collecting_data_n_of_m_samples',{n:s.heapTrendBuckets,m:s.heapTrendNeed}):(Math.abs(s.heapTrendBph)<512?TL('stable'):TL('v_kb_h',{v:f(s.heapTrendBph/1024,1)})),s=>s.heapLeak?'warn':'',s=>isNum(s.heapTrendBuckets)],
   [TL('largest_free_block'),s=>isNum(s.maxAllocHeap)?Math.round(s.maxAllocHeap/1024)+' KB':'—',null,s=>isNum(s.maxAllocHeap)],
   ['PSRAM',s=>s.psramFound?f(s.freePsram/1048576,1)+' / '+f(s.totalPsram/1048576,1)+' MB':TL('none')],
   [TL('chip_temperature'),s=>f(s.chipTempC,0,' °C'),s=>s.chipTempC>75?'warn':'',s=>isNum(s.chipTempC)],
   [TL('watchdog'),s=>s.watchdogS+' s']]},
 dWeb:{title:TL('www_ota_update'),rows:[
   [TL('update_via_web_page'),s=>'/update'],
   [TL('last_update_attempt'),s=>OTA_STATE[s.otaState]||'?',s=>s.otaState===3?'bad':s.otaState===2?'ok':s.otaState===1?'warn':'muted',s=>isNum(s.otaState)],
   [TL('source'),s=>['—',TL('arduino_ide'),TL('update_page')][s.otaSource]||'—',null,s=>isNum(s.otaState)&&s.otaSource>0],
   [TL('uploaded'),s=>fmtKB(s.otaBytes)+(s.otaTotal>0?' z '+fmtKB(s.otaTotal):''),null,s=>isNum(s.otaState)&&s.otaSource>0],
   [TL('error_2'),s=>logTr(s.otaError),'bad',s=>isNum(s.otaState)&&s.otaState===3],
   [TL('finished'),s=>TL('t_ago',{t:fmtMs(s.otaAgeMs)}),null,s=>isNum(s.otaAgeMs)&&s.otaAgeMs>=0]],
   note:TL('while_the_file_is_being_uploaded_the_panel')}
};
let gpsPrev=null,gpsBps=null;
let curPrev=null,curSps=null;   // current sampling rate (samples/s) computed from the currSamples counter
function curCalText(s){return s.currCalState===1?TL('in_progress_p_pct',{p:s.currCalPct}):s.currCalState===3?TL('failed_r',{r:logTr(s.currCalReason)}):(['—',TL('in_progress_2'),TL('ready'),TL('failed')][s.currCalState]||'—');}

)HTML"
