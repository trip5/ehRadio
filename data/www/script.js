// ===== PLAYER AND PLAYLIST EDITOR FUNCTIONALITY =====

var hostname = window.location.hostname;
var modesd = false;
const query = window.location.search;
const params = new URLSearchParams(query);
const Title = 'ehRadio';
if(params.size>0){
  if(params.has('host')) hostname=params.get('host');
}
var websocket;
var wserrcnt = 0;  // failed attempts; cleared by a good session
var wstimeout;
var wsopenedAt = 0;  // open time; tells if a close was a failure
var wsLastMsg = 0;  // last frame received, the watchdog clock
var wsProbeAt = 0;  // probe sent at; 0 = none outstanding
var wsWatch = null;
var wsPageBurst = null;  // this page request set; re-issued on open
var loaded = false;
var currentItem = 0;

// retry gaps: after a reboot, then slow
const WS_RETRY_FAST_MS  = 2000;
const WS_RETRY_FAST_MAX = 4;
const WS_RETRY_MID_MS   = 10000;
const WS_RETRY_MID_MAX  = 12;
const WS_RETRY_SLOW_MS  = 30000;
const WS_JITTER         = 0.25;
// a short session keeps climbing the ladder
const WS_SESSION_OK_MS  = 10000;
// 15 missed NRSSI heartbeats at 2 s each
const WS_SILENCE_MS     = 30000;
// SOFT_AP sends no NRSSI, so probe first
const WS_PROBE_GRACE_MS = 5000;

window.addEventListener('load', onLoad);

// bitrate/codec display state
var currentCodec = '';
var currentBitrate = 0;
function updateBitinfo(){
  var txt = '';
  if(currentCodec) txt += currentCodec + ' ';
  if(currentBitrate) txt += currentBitrate + t('unit_kbits', 'kBits');
  if(!txt) txt = t('lbl_no_codec', 'No Codec');
  var bi = getId('bitinfo'); if(bi) bi.textContent = txt;
}

// rssi bars: counts are thresholded on device
function setRssiBars(level){
  var svg = getId('rssibars');
  if(!svg) return;
  var bars = svg.querySelectorAll('path');
  var lit = parseInt(level, 10);
  if(isNaN(lit) || lit < 0) lit = 0;
  if(lit > bars.length) lit = bars.length;
  for(var i = 0; i < bars.length; i++) bars[i].classList.toggle('off', i >= lit);
}


function loadCSS(href){ const link = document.createElement("link"); link.rel = "stylesheet"; link.href = href; document.head.appendChild(link); }
function loadJS(src, callback){ const script = document.createElement("script"); script.src = src; script.type = "text/javascript"; script.async = true; script.onload = callback; document.head.appendChild(script); }
function ensureFunctionLoaded(functionName, src, callback){
  if(typeof window[functionName] === 'function') { callback(); return; }
  loadJS(src, callback);
}

// the only socket opener; tears the old one down
function initWebSocket() {
  clearTimeout(wstimeout);
  wsStopWatch();
  if (websocket) {
    websocket.onopen = null;
    websocket.onclose = null;
    websocket.onmessage = null;
    try { websocket.close(); } catch (e) {}
    websocket = null;
  }
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(`ws://${hostname}/ws`);
  websocket.onopen    = onOpen;
  websocket.onclose   = onClose;
  websocket.onmessage = onMessage;
}
function onLoad(event) { initWebSocket(); }

// page state, re-asked on open; never actions
function wsSetPageBurst(list) { wsPageBurst = list; }
function wsRequestPageState() {
  if (!wsPageBurst || !websocket || websocket.readyState !== 1) return;
  for (let i = 0; i < wsPageBurst.length; i++) {
    try { websocket.send(wsPageBurst[i]); } catch (e) {}
  }
}

// retry delay ladder by failure count
function wsRetryDelay(n) {
  const base = (n <= WS_RETRY_FAST_MAX) ? WS_RETRY_FAST_MS
             : (n <= WS_RETRY_MID_MAX)  ? WS_RETRY_MID_MS
                                        : WS_RETRY_SLOW_MS;
  return Math.round(base * (1 + WS_JITTER * (Math.random() * 2 - 1)));
}

// no bars = no signal or no socket
function wsMarkDown() {
  setRssiBars(0);
}

// silence: probe, then reconnect if unanswered
function wsStartWatch() {
  wsStopWatch();
  wsLastMsg = Date.now();
  wsProbeAt = 0;
  wsWatch = setInterval(function(){
    if (!websocket || websocket.readyState !== 1) return;
    if (Date.now() - wsLastMsg <= WS_SILENCE_MS) { wsProbeAt = 0; return; }
    if (!wsProbeAt) {
      // probe: one small reply, changes nothing
      console.log('WebSocket silent, asking the device if it is there...');
      wsProbeAt = Date.now();
      try { websocket.send('getmute=1'); } catch (e) {}
      return;
    }
    if (Date.now() - wsProbeAt < WS_PROBE_GRACE_MS) return;
    console.log('WebSocket silent and unanswered, reconnecting...');
    initWebSocket();  // opener closes this socket on entry
    wsMarkDown();
  }, 1000);
}
function wsStopWatch() {
  if (wsWatch !== null) { clearInterval(wsWatch); wsWatch = null; }
}

// wake events reconnect only after load, only CLOSED
function wsWakeReconnect() {
  if (!loaded) return;
  if (websocket && websocket.readyState !== 3) return;
  wserrcnt = 0;
  initWebSocket();
}
window.addEventListener('online', wsWakeReconnect);
window.addEventListener('focus', wsWakeReconnect);
document.addEventListener('visibilitychange', function(){ if (document.visibilityState === 'visible') wsWakeReconnect(); });

function onOpen(event) {
  if (event.target !== websocket) return;   // a socket that has already been replaced
  console.log('Connection opened');
  wsopenedAt = Date.now();
  wsStartWatch();
  continueLoading(playMode);  // first load only: builds page, sets burst
  loaded = true;
  wsRequestPageState();  // state request; also repairs after reconnect
}
function onClose(event) {
  if (event.target !== websocket) return;   // a socket that has already been replaced
  wsStopWatch();
  // a session that lived clears the ladder
  if (wsopenedAt && Date.now() - wsopenedAt > WS_SESSION_OK_MS) wserrcnt = 0;
  wsopenedAt = 0;
  wserrcnt++;
  // arm the timer before touching the page
  wstimeout=setTimeout(initWebSocket, wsRetryDelay(wserrcnt));
  wsMarkDown();
}
function secondToTime(seconds){
  if(seconds>=3600){
    return new Date(seconds * 1000).toISOString().substring(11, 19);
  }else{
    return new Date(seconds * 1000).toISOString().substring(14, 19);
  }
}
function showById(show,hide){
  show.forEach(item=>{ getId(item).classList.remove('hidden'); });
  hide.forEach(item=>{ getId(item).classList.add('hidden'); });
}
function onMessage(event) {
  if (event.target !== websocket) return;   // a socket that has already been replaced
  wsLastMsg = Date.now();  // any frame proves the socket is alive
  wsProbeAt = 0;  // and answers the probe
  try{
    const data = JSON.parse(escapeData(event.data));
    // ir
    if(typeof data.ircode !== 'undefined'){
      getId('protocol').innerText=data.protocol;
      classEach('irrecordvalue', function(el){ if(el.hasClass("active")) el.innerText='0x'+data.ircode.toString(16).toUpperCase(); });
      return;
    }
    if(typeof data.irvals !== 'undefined'){
      classEach('irrecordvalue', function(el,i){ var val = data.irvals[i]; if(val>0) el.innerText='0x'+val.toString(16).toUpperCase(); else el.innerText=""; });
      return;
    }
    // end ir
    
    // online update
    if(typeof data.onlineupdateavailable !== 'undefined') {
      const btn = getId('check_online_update');
      if(btn) {
        if(data.onlineupdateavailable) {
          btn.value = t('msg_update_to', 'Update to {0}', data.remoteVersion);
          btn.disabled = false;
        } else {
          btn.value = t('msg_no_update', 'No Update Available');
          btn.disabled = true;
        }
      }
    }
    if(typeof data.onlineupdateerror !== 'undefined') {
      const btn = getId('check_online_update');
      if(btn) {
        btn.value = t('msg_update_error', 'Error: {0}', data.onlineupdateerror);
        btn.disabled = true;
      }
    }
    if(typeof data.onlineupdateprogress !== 'undefined') {
      const bar = getId('updateprogress');
      if(bar) {
        bar.hidden = false;
        bar.value = data.onlineupdateprogress;
        const status = getId('uploadstatus');
        if(status) status.textContent = t('msg_ota_progress', 'OTA Update: {0}% downloaded | please wait...', data.onlineupdateprogress);
        if (data.onlineupdateprogress >= 100) {
          getId("uploadstatus").textContent = t('msg_ota_complete_wait', 'OTA Update Complete. Radio will reboot, update files, and reboot again. This may take up to 3 minutes.');
          rebootingProgress(180);
        }
      }
    }
    if(typeof data.onlineupdatestatus !== 'undefined') {
      const btn = getId('check_online_update');
      if(btn) {
        btn.value = data.onlineupdatestatus;
        btn.disabled = true;
      }
    }
    // end online update
    
    if(typeof data.redirect !== 'undefined'){
      getId("mdnsnamerow").innerHTML=`<h3 style="line-height: 37px;color: #aaa; margin: 0 auto;">${t('msg_redirecting', 'Redirecting to {0}', data.redirect)}</h3>`;
      setTimeout(function(){ window.location.href=data.redirect; }, 4000);
      return;
    }
    // playlist validator progress
    if(typeof data.plvalidate !== 'undefined'){
      handleValidationMessage(data.plvalidate);
      return;
    }
    if(typeof data.playermode !== 'undefined') { //Web, SD
      const wasSd = modesd;
      modesd = data.playermode=='modesd';
      const modeChanged = _modeKnown && (wasSd !== modesd);
      _modeKnown = true;
      classEach('modeitem', function(el){ el.classList.add('hidden') });
      if(modesd) { getId('toggleplaylist').classList.add('sd-mode'); showById(['modesd', 'sdmanbtn', 'shuffle'],['plsvg']); } else { getId('toggleplaylist').classList.remove('sd-mode'); showById(['modeweb','plsvg','bitinfo'],['sdmanbtn','shuffle']); }
      // sdslider needs this line if fixed
      // if(modesd) { showById(['volslider', 'sdslider'],[]); } else { showById(['volslider'],['sdslider']); }
      showById(['volslider'],[]);
      getId('toggleplaylist').classList.remove('active');
      // mode changed: a switch started, lock the page
      if (modeChanged) { setModeSwitching(true); return; }
      // refused switch: drop the lock, keep the list
      if (_switching) {
        setModeSwitching(false);
        generatePlaylist(`http://${hostname}/data/playlist.csv`+"?"+new Date().getTime());
      }
      return;
    }
    if(typeof data.playlistready !== 'undefined') {
      // playlistready: false = rebuilding, true = unlock
      if (data.playlistready === false) { setModeSwitching(true); return; }
      setModeSwitching(false);
      generatePlaylist(`http://${hostname}/data/playlist.csv`+"?"+new Date().getTime());
      return;
    }
    if(typeof data.sdinit !== 'undefined') {
      if(data.sdinit==1) {
        getId('playernav').classList.add("sd");
        getId('voldownbutton').classList.add("hidden");
      }else{
        getId('playernav').classList.remove("sd");
        getId('voldownbutton').classList.remove("hidden");
      }
    }
    if(typeof data.sdpos !== 'undefined' && getId("sdpos")){
      if(data.sdtpos==0 && data.sdtend==0){
        getId("sdposvalscurrent").innerHTML="00:00";
        getId("sdposvalsend").innerHTML="00:00";
        getId("sdpos").value = data.sdpos;
        fillSlider(getId("sdpos"));
      }else{
        getId("sdposvalscurrent").innerHTML=secondToTime(data.sdtpos);
        getId("sdposvalsend").innerHTML=secondToTime(data.sdtend);
        getId("sdpos").value = data.sdpos;
        fillSlider(getId("sdpos"));
      }
      return;
    }
    if(typeof data.sdmin !== 'undefined' && getId("sdpos")){
      getId("sdpos").attr('min',data.sdmin); 
      getId("sdpos").attr('max',data.sdmax); 
      return;
    }
    if(typeof data.shuffle!== 'undefined'){
      if(data.shuffle==1){
        getId("shuffle").classList.add("active");
      }else{
        getId("shuffle").classList.remove("active");
      }
      return;
    }
    if(typeof data.tz_name !== 'undefined'){
      const select = document.getElementById("tz_name");
      const input = document.getElementById("tzposix");
      
      // Ensure dropdown is populated before trying to set selection
      if (select && select.options.length === 0 && timezoneData) {
        populateTZDropdown(timezoneData);
      }
      
      // not loaded yet: apply later
      if (!timezoneData) {
        pendingTZData = { tz_name: data.tz_name, tzposix: data.tzposix };
      } else if (select && input) {
        const i = [...select.options].findIndex(opt => opt.text === data.tz_name);
        if (i !== -1) {
          select.selectedIndex = i;
          input.value = select.options[i].value;
        } else {
          // show raw POSIX, keep the label
          const fallbackOption = new Option(data.tz_name, data.tzposix, true, true);
          select.appendChild(fallbackOption);
          select.selectedIndex = select.options.length - 1;
          input.value = data.tzposix;
        }
      }
      if (document.getElementById("sntp2")) document.getElementById("sntp2").value=data.sntp2;
      if (document.getElementById("sntp1")) document.getElementById("sntp1").value=data.sntp1;
      
      // Handle time sync interval slider
      if (data.timeinterval !== undefined) {
        const timeintervalEl = document.getElementById("timeinterval");
        if (timeintervalEl) {
          timeintervalEl.value = data.timeinterval;
          fillSlider(timeintervalEl);
        }
      }
      
      // keep locale data for later
      if (data.locale_webui && data.locale_disp) {
        pendingLocaleData = { locale_webui: data.locale_webui, locale_disp: data.locale_disp };
        
        // apply now if locales are loaded
        if (localesData) {
          applyPendingLocaleData();
        }
      }
      return;
    }
    
    if(typeof data.payload !== 'undefined'){
      data.payload.forEach(item=> {
        // battery: voltage and percentage only
        if(item.id === 'battery' && typeof item.value === 'string'){
          var m = /volt: (\d+)mV, percentage: (\d+)%/.exec(item.value);
          if(m){
            var volt = m[1], perc = m[2];
            setupElement('battery_volt', volt);
            setupElement('battery_perc', perc);
            // player shows %, settings shows both
            const compact = getId('battery');
            if (compact) compact.innerText = perc + '%';
            const wrap = getId('batteryinfo');
            if (wrap) wrap.classList.remove('hidden');
          }else{
            const nowrap = getId('batteryinfo');
            if (nowrap) nowrap.classList.add('hidden');   // no battery reported
          }
          return; // Skip normal setupElement for 'battery' id
        }
        setupElement(item.id, item.value);
        // set slider max from device
        if (typeof item.max !== 'undefined') {
          var el = getId(item.id);
          if (el && el.type === 'range') { el.max = item.max; fillSlider(el); }
        }
      });
    }else{
      if(typeof data.current !== 'undefined') { setCurrentItem(data.current); return; }
        if(typeof data.file !== 'undefined') {
          generatePlaylist(data.file+"?"+new Date().getTime());
          websocket.send('submitplaylistdone=1');
          websocket.send('getindex=1');
          return;
        }
      // group_x shows, hide_group_x hides; never a class
      if(typeof data.act !== 'undefined'){
        data.act.forEach(showclass=> {
          if (showclass.startsWith('hide_')) classEach(showclass.slice(5), function(el) { el.classList.add("hidden"); });
          else classEach(showclass, function(el) { el.classList.remove("hidden"); });
        });
        return;
      }
      // apply maxVol to volume slider
      if (typeof data.maxVol !== 'undefined') {
        var volSlider = getId('volume');
        if (volSlider) { volSlider.max = data.maxVol; fillSlider(volSlider); }
      }
      Object.keys(data).forEach(key=>{
        // GETSCREEN also carries the theme id
        if (key === 'themeId') document.documentElement.setAttribute('data-theme', data[key]);
        setupElement(key, data[key]);
      });
    }
  }catch(e){
    console.log("ws.onMessage error:", event.data);
  }
}
function escapeData(data){
  let m=data.match(/{.+?:\s"(.+?)"}/);
  if(m!==null){
    let m1 = m[1];
    if(m1.indexOf('"') !== -1){
      let mq=m1.replace(/["]/g, '\\\"');
      return data.replace(m1,mq);
    }
  }
  return data;
}
function getId(id,patent=document){
  return patent.getElementById(id);
}
function classEach(classname, callback) {
  document.querySelectorAll(`.${classname}`).forEach((item, index) => callback(item, index));
}
function quoteattr(s) {
  return ('' + s)
    .replace(/&/g, '&amp;')
    .replace(/'/g, '&apos;')
    .replace(/"/g, '&quot;')
    .replace(/</g, '&lt;');
}
HTMLElement.prototype.attr = function(name, value=null){
  if(value!==null){
    return this.setAttribute(name, value);
  }else{
    return this.getAttribute(name);
  }
}
HTMLElement.prototype.hasClass = function(className){
  return this.classList.contains(className);
}
function fillSlider(sl){
  const slaveid = sl.dataset.slaveid;
  const value = (sl.value-sl.min)/(sl.max-sl.min)*100;
  if(slaveid) getId(slaveid).innerText=sl.value;
  sl.style.background = 'linear-gradient(to right, var(--meter-fill) 0%,  var(--meter-fill) ' + value + '%, var(--meter-track) ' + value + '%, var(--meter-track) 100%)';
}
function setupElement(id,value){
  // handle bitrate/codec specially
  if(id === 'fmt'){
    currentCodec = value || '';
    updateBitinfo();
    return;
  }
  if(id === 'bitrate'){
    currentBitrate = parseInt(value) || 0;
    updateBitinfo();
    return;
  }
  if(id === 'rssi'){  // dBm for the tooltip only
    var rtt = getId('rssititle');
    if(rtt && value !== '' && value !== null && typeof value !== 'undefined')
      rtt.textContent = t('lbl_rssi', 'RSSI') + ': ' + value + t('unit_dbm', 'dBm');
    return;
  }
  if(id === 'rssibars'){  // bar count from the device
    setRssiBars(value);
    return;
  }
  if(id === 'mutebutton'){  // mute state comes from device only
    const btn = getId(id);
    if(btn) btn.classList.toggle('active', !!value);
    return;
  }
  const element = getId(id);
  if(element){
    if(element.classList.contains("checkbox")){
      element.classList.remove("checked");
      if(value) element.classList.add("checked");
    }
    if(element.classList.contains("classchange")){
      element.attr("class", "classchange");
      element.classList.add(value);
    }
    if(element.classList.contains("text")){
      element.innerText=value;
    }
    if(element.type==='text' || element.type==='number' || element.type==='password'){
      // battref is calibration, not ADC reference
      if (element.id !== 'battref') {
        element.value=value;
      }
    }
    if(element.type==='range'){
      element.value=value;
      fillSlider(element);
    }
    if(element.tagName==='SELECT'){
      element.value=String(value);
      // Trigger weather provider toggle if wapi changes
      if(id === 'wapi' && typeof setupWeatherProviderToggle === 'function') {
        const event = new Event('change');
        element.dispatchEvent(event);
      }
    }
    if(typeof window.afterSetupElement === 'function') {
      window.afterSetupElement(id, value, element);
    }
  }
}
// --- playlist ---

// playlist grammar mirrors Utility::parsePlaylistLine()
function cutFromToken(tok){
  const m = /^[dD][bB](\d{1,2})$/.exec((tok || '').trim());
  if(!m) return -1;
  return Math.max(0, Math.min(24, parseInt(m[1], 10)));
}
// dBcut: 2, "2" and "dB2" all mean the same
function readCut(v){
  if(v === null || v === undefined) return -1;
  const s = String(v).trim();
  if(s === '') return -1;
  const tagged = cutFromToken(s);
  if(tagged >= 0) return tagged;
  const n = parseInt(s, 10);
  if(isNaN(n)) return -1;
  return Math.max(0, Math.min(24, n));
}
// A token that can be a stream address - for the loose import path only, never for the grammar
function isUrlToken(t){
  if(!t) return false;
  if(t.includes('://') || t.includes('/')) return true;
  return /^[^\s@]+\.[a-z]{2,}(:\d+)?$/i.test(t);
}
// The value column: dBn is ours, a bare number is some other build's unit
function isValueToken(t){
  if(!t) return false;
  if(/^[dD][bB]\d{1,2}$/.test(t)) return true;
  const n = parseInt(t, 10);
  return !isNaN(n) && String(n) === t && n >= -64 && n <= 64;
}
function parsePlaylistLine(line){
  if(!line) return null;
  const s = line.replace(/^\s+|\s+$/g, '');
  if(!s) return null;
  let name, url, value;
  if(s.indexOf('\t') >= 0){
    const f = s.split('\t');
    if(f.length !== 3) return null;
    name = f[0]; url = f[1]; value = f[2];
  } else {
    const m = s.match(/^(.*)\s+(\S+)\s+(\S+)$/);
    if(!m) return null;
    name = m[1].replace(/\s+$/, ''); url = m[2]; value = m[3];
  }
  name = (name || '').replace(/^\s+|\s+$/g, '');
  url  = (url  || '').replace(/^\s+|\s+$/g, '');
  if(!name || !url) return null;
  return { name: name, url: url, dBcut: cutFromToken(value) };
}
function formatPlaylistLine(name, url, dBcut){
  return name + ' ' + url + ' ' + ((dBcut < 0) ? '0' : ('dB' + dBcut));
}
// row's cut, empty = not measured
function editorCutValue(inputs){
  const raw = (inputs[3].value || '').replace(/^\s+|\s+$/g, '');
  if(raw === '') return -1;
  const v = parseInt(raw, 10);
  if(isNaN(v)) return -1;
  return Math.max(0, Math.min(24, v));
}
function setCurrentItem(item){
  currentItem=item;
  const playlist = getId("playlist");
  let topPos = 0, lih = 0;
  playlist.querySelectorAll('li').forEach((item, index)=>{ item.attr('class','play'); if(index+1==currentItem){ item.classList.add("active"); topPos = item.offsetTop; lih = item.offsetHeight; } });
  playlist.scrollTo({ top: (topPos-playlist.offsetHeight/2+lih/2), left: 0, behavior: 'smooth' });
}
function initPLEditor(){
  ple= getId('pleditorcontent');
  if(!ple) return;
  let html='';
  ple.innerHTML="";
  pllines = getId('playlist').querySelectorAll('li');
  pllines.forEach((item,index)=>{
    html+=`<li class="pleitem" id="${'plitem'+index}"><span class="grabbable">${("00"+(index+1)).slice(-3)}</span>
      <span class="pleinput plecheck"><input type="checkbox" class="plcb" /></span>
      <input class="pleinput plename" type="text" value="${quoteattr(item.dataset.name)}" maxlength="169" />
      <input class="pleinput pleurl" type="text" value="${item.dataset.url}" maxlength="169" />
      <span class="pleinput pleplay" data-command="preview">&#9658;</span>
      <input class="pleinput pledbcut" type="number" min="0" max="24" step="1" placeholder="?" value="${item.dataset.dbcut}" />
      <span class="pleinput plestatus"></span>
      </li>`;
  });
  // one delegated listener survives rebuilds
  if(!ple._dbcutBound){
    ple._dbcutBound = true;
    ple.addEventListener('input', (ev)=>{
      const el = ev.target;
      if(!el.classList || !el.classList.contains('pledbcut')) return;
      const li = el.closest('.pleitem');
      if(!li) return;
      const row = parseInt(li.id.replace('plitem','')) + 1;
      if(row !== currentItem) return;
      if((el.value || '').trim() === '') return;         // empty means "not measured": nothing to preview
      const v = Math.max(0, Math.min(24, parseInt(el.value) || 0));
      if(String(v) !== el.value) el.value = v;
      window.clearTimeout(ple._dbcutTimer);
      ple._dbcutTimer = window.setTimeout(()=>sendDbcut(row, v), 250);
    });
  }
  ple.innerHTML=html;
}
function handlePlaylistData(fileData) {
  const ul = getId('playlist');
  ul.innerHTML='';
  if (!fileData) { alignPlaylistStripes(); return; }
  const lines = fileData.split(/\r?\n/);
  let li='', html='', row=0;
  for(var i = 0;i < lines.length;i++){
    const rec = parsePlaylistLine(lines[i]);
    if(!rec) continue;
    row++;
    const active=(row==currentItem)?' class="active"':'';
    const cut=(rec.dBcut>=0)?rec.dBcut:'';
    li=`<li${active} attr-id="${row}" class="play" data-name="${rec.name}" data-url="${rec.url}" data-dbcut="${cut}"><span class="text">${rec.name}</span><span class="count">${row}</span></li>`;
    html += li;
  }
  ul.innerHTML=html;
  alignPlaylistStripes();
  setCurrentItem(currentItem);
  if(!modesd){ initPLEditor(); syncDbcutToDevice(); }
}

// cut is live but temporary; re-push the file value
function sendDbcut(row, dBcut){
  if(modesd || !websocket || websocket.readyState !== 1) return;
  websocket.send(`dBcut=${row},${dBcut}`);
}
function syncDbcutToDevice(){
  if(modesd || !currentItem) return;
  const item = getId('plitem'+(currentItem-1));
  if(!item) return;
  const el = item.querySelector('.pledbcut');
  if(!el) return;
  // not measured: device keeps learning it
  sendDbcut(currentItem, Math.max(0, Math.min(24, parseInt(el.value) || 0)));
}

// stripe bands are 39px; re-run on resize
function alignPlaylistStripes() {
  const ul = getId('playlist');
  if (!ul) return;
  const rows = ul.children, band = 39, cycle = band * 2;
  if (!rows.length) { ul.style.backgroundPositionY = '0px'; return; }
  const last = rows[rows.length - 1];
  const end = last.offsetTop + last.offsetHeight;
  const shift = (rows.length % 2) ? band : 0;  // odd row count needs a shift
  ul.style.backgroundPositionY = (((end - shift) % cycle + cycle) % cycle) + 'px';
}
window.addEventListener('resize', alignPlaylistStripes);

// --- mode switch: icon, blank, lock ---

// SD verify/rebuild is slow: lock with a backstop
var _switching = false;
var _switchTimer = null;
var _modeKnown = false;  // first report is not a switch
var SWITCH_BACKSTOP_MS = 180000;   // 3 minutes

function setModeSwitching(on){
  _switching = !!on;
  document.body.classList.toggle('switching', _switching);
  if (_switchTimer) { window.clearTimeout(_switchTimer); _switchTimer = null; }
  if (!_switching) return;
  const pl = getId('playlist');
  if (pl) pl.innerHTML = '<div class="plloader"><span class="loader"></span></div>';
  const ple = getId('pleditorcontent');  // editor must not offer the old mode
  if (ple) ple.innerHTML = '';
  _switchTimer = window.setTimeout(function(){
    _switchTimer = null;
    setModeSwitching(false);
    generatePlaylist(`http://${hostname}/data/playlist.csv`+"?"+new Date().getTime());
  }, SWITCH_BACKSTOP_MS);
}

function generatePlaylist(path){
  getId('playlist').innerHTML='<div class="plloader"><span class="loader"></span></div>';
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      if (xhr.status == 200) {
        handlePlaylistData(xhr.responseText);
      } else {
        handlePlaylistData(null);
      }
    }
  };
  xhr.open("GET", path);
  xhr.send(null);
}
function uncheckSelectAll(){
  const checkbox = getId('selectAllCheckbox');
  if(checkbox) checkbox.checked = false;
}
function plAdd(){
  let ple=getId('pleditorcontent');
  let plitem = document.createElement('li');
  let cnt=ple.getElementsByTagName('li');
  plitem.attr('class', 'pleitem');
  plitem.attr('id', 'plitem'+(cnt.length));
  plitem.innerHTML = '<span class="grabbable">'+("00"+(cnt.length+1)).slice(-3)+'</span>\
      <span class="pleinput plecheck"><input type="checkbox" /></span>\
      <input class="pleinput plename" type="text" value="" maxlength="169" />\
      <input class="pleinput pleurl" type="text" value="" maxlength="169" />\
      <span class="pleinput pleplay" data-command="preview">&#9658;</span>\
      <input class="pleinput pledbcut" type="number" min="0" max="24" step="1" placeholder="?" value="0" />\
      <span class="pleinput plestatus"></span>';
  ple.appendChild(plitem);
  ple.scrollTo({
    top: ple.scrollHeight,
    left: 0,
    behavior: 'smooth'
  });
  uncheckSelectAll();
}
function selectAll(checkbox){
  let items=getId('pleditorcontent').getElementsByTagName('li');
  for (let i = 0; i < items.length; i++) {
    let cb = items[i].getElementsByTagName('span')[1].getElementsByTagName('input')[0];
    if (checkbox.checked) {
      // Invert selection
      cb.checked = !cb.checked;
    } else {
      // Uncheck all
      cb.checked = false;
    }
  }
}
function clearImportReviewFlags() {
  if (sessionStorage.getItem('pl_import_review') === 'true') {
    sessionStorage.removeItem('pl_import_review');
    sessionStorage.removeItem('pl_import_mode');
    if (getId('plImportWarning')) getId('plImportWarning').classList.add('hidden');
  }
}

function setImportReviewFlags(mode) {
  sessionStorage.setItem('pl_import_mode', mode);
  sessionStorage.setItem('pl_import_review', 'true');
  if (getId('plImportWarning')) getId('plImportWarning').classList.remove('hidden');
}

function undoPlaylistChanges(){
  clearImportReviewFlags();
  generatePlaylist(`http://${hostname}/data/playlist.csv`+"?"+new Date().getTime());
  uncheckSelectAll();
  if (validatorMode) setValidatorMode(false);  // Validator: Undo returns to editor
}

function loadCuratedForReview(mode) {
  console.log('[Curated] Loading pl_import.json for review, mode:', mode);
  
  // Open the playlist editor
  if (getId('pleditorwrap').classList.contains('hidden')) {
    getId('toggleplaylist').click();
  }
  
  // For merge mode, load current playlist first
  if (mode === 'merge') {
    generatePlaylist(`http://${hostname}/data/playlist.csv?` + Date.now());
    // Wait for current playlist to load, then import
    setTimeout(() => {
      doPlImportCurated('pl_import.json', mode);
      // Show warning banner
      setImportReviewFlags(mode);
    }, 300);
  } else {
    // replace: wait, clear, then import
    setTimeout(() => {
      doPlImportCurated('pl_import.json', mode);
      // Show warning banner
      setImportReviewFlags(mode);
    }, 400);
  }
}
function plRemove(){
  let items=getId('pleditorcontent').getElementsByTagName('li');
  let pass=[];
  for (let i = 0; i <= items.length - 1; i++) {
    if(items[i].getElementsByTagName('span')[1].getElementsByTagName('input')[0].checked) {
      pass.push(items[i]);
    }
  }
  if(pass.length==0) {
    alert(t('msg_choose_first', 'Choose something first'));
    return;
  }
  for (var i = 0; i < pass.length; i++)
  {
    pass[i].remove();
  }
  items=getId('pleditorcontent').getElementsByTagName('li');
  for (let i = 0; i <= items.length-1; i++) {
    items[i].getElementsByTagName('span')[0].innerText=("00"+(i+1)).slice(-3);
  }
  uncheckSelectAll();
}
function submitPlaylist(){
  clearImportReviewFlags();
  var items=getId("pleditorcontent").getElementsByTagName("li");
  var output="";
  for (var i = 0; i <= items.length - 1; i++) {
    inputs=items[i].getElementsByTagName("input");
    if(inputs[1].value == "" || inputs[2].value == "") continue;
    output += formatPlaylistLine(inputs[1].value, inputs[2].value, editorCutValue(inputs)) + "\r\n";
  }
  let file = new File([output], "playlist.csv",{type:"text/plain;charset=utf-8", lastModified:new Date().getTime()});
  let container = new DataTransfer();
  container.items.add(file);
  let fileuploadinput=getId("file-upload");
  fileuploadinput.files = container.files;
  doPlUpload(fileuploadinput);
  // Validator: Save returns to editor
  if (validatorMode) setValidatorMode(false);
  else toggleTarget(0, 'pleditorwrap');
}
function doPlUpload(finput) {
  if (!finput.files || !finput.files[0]) return;
  var formData = new FormData();
  formData.append("data", finput.files[0], "playlist.csv");
  var xhr = new XMLHttpRequest();
  xhr.onload = function() {
    if (xhr.status === 200) {
      // Server will send PLAYLISTSAVED event via WebSocket
      console.log('Playlist saved successfully');
    }
  };
  xhr.open("POST", `http://${hostname}/webboard`, true);
  xhr.send(formData);
  finput.value = '';
}

function doPlImportCurated(localPath, mode) {
  // load a local file (curated lists)
  const importMode = mode || 'replace';
  console.log('[doPlImportCurated] Loading local file:', localPath, 'mode:', importMode);
  fetch(localPath + '?' + Date.now())
    .then(response => {
      if (!response.ok) throw new Error('Failed to load ' + localPath);
      return response.text();
    })
    .then(content => {
      const isJSON = localPath.endsWith('.json');
      if (importMode === 'replace') {
        // Clear existing playlist
        getId('pleditorcontent').innerHTML = '';
      }
      if (isJSON) {
        parseAndAddJSON(content, addStationToEditor, importMode);
      } else {
        parseAndAddCSV(content, addStationToEditor, importMode);
      }
      uncheckSelectAll();
    })
    .catch(error => {
      console.error('[doPlImportCurated] Error loading file:', error);
      // no file: clear flags silently
      sessionStorage.removeItem('pl_import_review');
      sessionStorage.removeItem('pl_import_mode');
      if (getId('plImportWarning')) getId('plImportWarning').classList.add('hidden');
    });
}

function doPlImport(finput) {
  if (!finput.files || !finput.files[0]) return;
  const file = finput.files[0];
  const mode = window.importMode || 'replace';
  const reader = new FileReader();
  
  reader.onload = function(e) {
    const content = e.target.result;
    const isJSON = file.name.endsWith('.json');
    
    if (mode === 'replace') {
      // Clear existing playlist
      getId('pleditorcontent').innerHTML = '';
    }
    if (isJSON) {
      parseAndAddJSON(content, addStationToEditor, mode);
    } else {
      parseAndAddCSV(content, addStationToEditor, mode);
    }
    
    finput.value = '';
    delete window.importMode;
    uncheckSelectAll();
    setImportReviewFlags(mode);
  };
  reader.readAsText(file);
}

function parseAndAddJSON(content, targetCallback, mode = 'replace') {
  try {
    // JSONL/NDJSON: no brackets or commas
    content = content.trim();
    if (!content.startsWith('[')) {
      // one object per line: add commas
      const lines = content.split('\n').filter(line => line.trim());
      content = '[\n' + lines.join(',\n') + '\n]';
    }
    
    let data = JSON.parse(content);
    if (!Array.isArray(data)) data = [data];
    
    const existingURLs = mode === 'merge' ? getExistingURLs() : new Set();
    let addedCount = 0;
    let duplicateCount = 0;
    
    data.forEach(item => {
      // Extract name (try name first, then title)
      const name = item.name || item.title || '';
      
      // url: resolved, then url, then host
      let url = '';
      if (item.url_resolved) {
        url = item.url_resolved;
      } else if (item.url) {
        url = item.url;
      } else if (item.host) {
        // Build URL from host + file + port
        const host = item.host || '';
        const file = item.file || '';
        const port = item.port || 80;
        
        if (host) {
          const protocol = (port === 443) ? 'https://' : 'http://';
          url = protocol + host;
          // Only add port if non-default for the protocol
          if ((protocol === 'http://' && port !== 80) || (protocol === 'https://' && port !== 443)) {
            url += ':' + port;
          }
          if (file) {
            url += (file.startsWith('/') ? '' : '/') + file;
          }
        }
      }
      // only our dBcut counts: absent, null, empty and unparsable all mean unmeasured
      const dbcut = readCut(item.dBcut);
      // If no name, generate from URL
      const finalName = name || (url ? urlToName(url) : '');
      // Check for duplicates
      if (url) {
        if (existingURLs.has(url)) {
          duplicateCount++;
        } else {
          targetCallback(finalName, url, dbcut);
          existingURLs.add(url);
          addedCount++;
        }
      }
    });
    if (mode === 'replace') {
      alert(t('msg_import_replace', 'Import complete: {0} stations loaded.', addedCount));
    } else if (duplicateCount > 0) {
      alert(t('msg_import_merge', 'Import complete: {0} stations added, {1} duplicates skipped.', addedCount, duplicateCount));
    }
  } catch(e) {
    alert(t('msg_invalid_json', 'Invalid JSON format: {0}', e.message));
  }
}

function urlToName(url) {
  // derive a name from the URL
  let name = url.replace(/^https?:\/\//, ''); // Drop protocol
  name = name.replace(/:\d+/, ''); // Drop port number
  name = name.replace(/[=&?]/g, '-'); // Convert query chars to hyphens
  name = name.replace(/\/+/g, '-'); // Convert slashes to hyphens
  name = name.replace(/--+/g, '-'); // Collapse multiple hyphens
  name = name.replace(/^-|-$/g, ''); // Trim leading/trailing hyphens
  return name;
}

function parseAndAddCSV(content, targetCallback, mode = 'replace') {
  const lines = content.split(/\r?\n/);
  const existingURLs = mode === 'merge' ? getExistingURLs() : new Set();
  let addedCount = 0;
  let duplicateCount = 0;

  // Loose shapes: a URL token anywhere, a value token last, everything else is the name.  The grammar is tried first,
  // so this only runs for lines the device would reject - url first, two columns, no value column.
  const loose = line => {
    const tokens = line.split(/\s+/).filter(t => t);
    const valueIdx = (tokens.length > 1 && isValueToken(tokens[tokens.length - 1])) ? tokens.length - 1 : -1;
    const urlIdx = (valueIdx > 0 && isUrlToken(tokens[valueIdx - 1])) ? valueIdx - 1 : tokens.findIndex(isUrlToken);
    if (urlIdx === -1) return null;
    return {
      name: tokens.filter((t, i) => i !== urlIdx && i !== valueIdx).join(' '),
      url: tokens[urlIdx],
      dBcut: (valueIdx === -1) ? -1 : cutFromToken(tokens[valueIdx])
    };
  };

  lines.forEach(raw => {
    const line = raw.trim();
    if (!line) return;
    let rec = parsePlaylistLine(line);      // tab: three fields; spaces: value last, the URL before it
    if (!rec || !isUrlToken(rec.url)) rec = loose(line);
    if (!rec || !rec.url) return;
    let name = (rec.name || '').trim();
    let url = rec.url.trim();
    if (!url.startsWith('http://') && !url.startsWith('https://')) url = 'http://' + url;
    if (!name) name = urlToName(url);
    if (existingURLs.has(url)) duplicateCount++;
    else { targetCallback(name, url, rec.dBcut); existingURLs.add(url); addedCount++; }
  });

  if (mode === 'replace') {
    alert(t('msg_import_replace', 'Import complete: {0} stations loaded.', addedCount));
  } else if (duplicateCount > 0) {
    alert(t('msg_import_merge', 'Import complete: {0} stations added, {1} duplicates skipped.', addedCount, duplicateCount));
  }
}
function getExistingURLs() {
  const urls = new Set();
  const items = document.getElementById('pleditorcontent').getElementsByTagName('li');
  for (let i = 0; i < items.length; i++) {
    const inputs = items[i].getElementsByTagName('input');
    const url = inputs[2].value;  // URL is the third input
    if (url) urls.add(url);
  }
  return urls;
}

function addStationToEditor(name, url, dbcut) {
  const ple = getId('pleditorcontent');
  const cnt = ple.getElementsByTagName('li').length;
  const plitem = document.createElement('li');
  plitem.className = 'pleitem';
  plitem.id = 'plitem' + cnt;
  plitem.innerHTML = `<span class="grabbable">${("00"+(cnt+1)).slice(-3)}</span>
    <span class="pleinput plecheck"><input type="checkbox" class="plcb" /></span>
    <input class="pleinput plename" type="text" value="${name.replace(/"/g, '&quot;')}" maxlength="169" />
    <input class="pleinput pleurl" type="text" value="${url}" maxlength="169" />
    <span class="pleinput pleplay" data-command="preview">&#9658;</span>
    <input class="pleinput pledbcut" type="number" min="0" max="24" step="1" placeholder="?" value="${(dbcut >= 0) ? dbcut : ''}" />
    <span class="pleinput plestatus"></span>`;
  ple.appendChild(plitem);
}

function triggerImport(mode) {
  window.importMode = mode;
  getId('file-upload').click();
}
function exportCurrentPlaylist() {
  const items = getId('pleditorcontent').getElementsByTagName('li');
  if (items.length === 0) {
    alert(t('msg_pl_empty', 'Playlist is empty, nothing to export.'));
    return;
  }
  
  let csvContent = '';
  for (let i = 0; i < items.length; i++) {
    const inputs = items[i].getElementsByTagName('input');
    const name = inputs[1].value; // plename
    const url = inputs[2].value;   // pleurl
    const dBcut = editorCutValue(inputs);
    
    if (name === '' || url === '') {
      alert(t('msg_pl_missing', 'Station {0} is missing name or URL. Please fill in all fields before exporting.', i+1));
      return;
    }
    
    csvContent += formatPlaylistLine(name, url, dBcut) + '\r\n';
  }
  
  // Create blob and trigger download
  const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
  const link = document.createElement('a');
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
  link.href = URL.createObjectURL(blob);
  link.download = `playlist_${timestamp}.csv`;
  link.style.display = 'none';
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(link.href);
}

// === Webstations Share ===
const SHARE_BODY_CORE =
`This form will submit your playlist to webstations and will appear soon in ehRadio Curated Lists. Enter your playlist name below between the quotes. It will be processed using your Github user ID as a prefix.

PLAYLIST_NAME="Playlist"

To avoid overloading this repo, users will be restricted to 2 unique playlist submissions. Older playlists will be replaced by newer submissions. Same-name submissions will always be replaced.

You can enter comments but it's unlikely they will ever be read.

Thanks for participating!

- Trip5`;

const SHARE_BODY_FALLBACK_HEADER =
`Wow! That's a lot of radio stations! ehRadio can't pre-fill an issue with a list this large, so it has downloaded the playlist to your computer as a CSV file.

Attach that file to this message (drag it into the box below) and it can still be submitted.`;

// GitHub caps the request URL at 8192 characters
const MAX_SHARE_URL_LENGTH = 8192;

function shareIssueUrl(body){
  return 'https://github.com/trip5/webstations/issues/new' +
    '?template=' +
    '&title=' + encodeURIComponent('Playlist Submission') +
    '&body=' + encodeURIComponent(body) +
    '&labels=playlist';
}

function openShareUrl(url){
  // a click on an anchor is not a popup
  const link = document.createElement('a');
  link.href = url;
  link.target = '_blank';
  link.rel = 'noopener';
  link.style.display = 'none';
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
}

function sharePlaylist() {
  const items = getId('pleditorcontent').getElementsByTagName('li');
  if (items.length === 0) {
    alert(t('msg_pl_empty', 'Playlist is empty, nothing to share.'));
    return;
  }

  let csvData = '';
  for (let i = 0; i < items.length; i++) {
    const inputs = items[i].getElementsByTagName('input');
    const name = inputs[1].value;
    const url = inputs[2].value;
    const dBcut = editorCutValue(inputs);

    if (name === '' || url === '') {
      alert(t('msg_pl_missing', 'Station {0} is missing name or URL. Please fill in all fields before sharing.', i + 1));
      return;
    }

    csvData += formatPlaylistLine(name, url, dBcut) + '\r\n';
  }

  // file keeps CRLF, body does not
  const inlineBody = SHARE_BODY_CORE + '\n\nPLAYLIST_START/\n' + csvData.replace(/\r\n/g, '\n') + '\n/PLAYLIST_END';
  const inlineUrl = shareIssueUrl(inlineBody);

  if (inlineUrl.length <= MAX_SHARE_URL_LENGTH) {
    openShareUrl(inlineUrl);
    return;
  }

  // over the cap: open the tab first
  openShareUrl(shareIssueUrl(SHARE_BODY_FALLBACK_HEADER + '\n\n' + SHARE_BODY_CORE));
  downloadCSV(csvData);
}

function downloadCSV(csvContent) {
  const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
  const link = document.createElement('a');
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
  link.href = URL.createObjectURL(blob);
  link.download = 'playlist_' + timestamp + '.csv';
  link.style.display = 'none';
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(link.href);
}
// --- end playlist ---
let pleditorTimeout = null;
function toggleTarget(el, id){
  const target = getId(id);
  if(id=='pleditorwrap'){
    // SD mode: the SD card icon should do nothing
    if(modesd) return;
    // Clear any active preview states
    classEach('pleitem', function(el){ el.classList.remove('active') });
    // opening: refresh the list first
    if(target && target.classList.contains('hidden')) {
      // cancel a pending timeout
      if(pleditorTimeout) { clearTimeout(pleditorTimeout); pleditorTimeout = null; }
      // Don't toggle visibility yet, wait for fresh data
      generatePlaylist(`http://${hostname}/data/playlist.csv`+"?"+new Date().getTime());
      // this fetch re-enters initPLEditor; show after
      pleditorTimeout = setTimeout(() => {
        target.classList.remove("hidden");
        getId(target.dataset.target).classList.add("active");
        pleditorTimeout = null;
        resetValidatorMode();     // the editor always opens in editor mode
        updateGearIcon();
      }, 200);
      return;  // do not run the toggle below
    }
    // closing: cancel the timeout, close now
    if(pleditorTimeout) { clearTimeout(pleditorTimeout); pleditorTimeout = null; }
    // Clear import review flags when closing editor
    clearImportReviewFlags();
  }
  if(target){
    target.classList.toggle("hidden");
    getId(target.dataset.target).classList.toggle("active");
  }
  // after the hide: the gear reads editor state
  if(id=='pleditorwrap'){
    if(!pleditorOpen()) resetValidatorMode();
    updateGearIcon();
  }
}
// --- playlist validator ---
// editor mode is one class plus a morphing gear
var validatorMode = false;
var validatorRunning = false;
var GEAR_ICON = null;
var ICON_EDITOR =
  '<svg viewBox="0 0 24 24" class="stroke"><path stroke-width="2" d="M20,15 C19,16 21.25,18.75 20,20 C18.75,21.25 16,19 15,20 C14,21 13.5,23 12,23 C10.5,23 10,21 9,20 C8,19 5.25,21.25 4,20 C2.75,18.75 5,16 4,15 C3,14 1,13.5 1,12 C1,10.5 3,10 4,9 C5,8 2.75,5.25 4,4 C5.25,2.75 8,5 9,4 C10,3 10.5,1 12,1 C13.5,1 14,3 15,4 C16,5 18.75,2.75 20,4 C21.25,5.25 19,8 20,9 C21,10 23,10.5 23,12 C23,13.5 21,14 20,15 Z M7,12 L10,15 L17,8"/></svg>';
var ICON_VALIDATOR =
  '<svg viewBox="0 0 48 48" class="stroke"><path stroke-width="4" stroke-linecap="round" stroke-linejoin="round" d="M5 10L8 13L14 7 M5 24L8 27L14 21 M5 38L8 41L14 35 M21 24H43 M21 38H43 M21 10H43"/></svg>';

function pleditorOpen(){
  const w = getId('pleditorwrap');
  return !!w && !w.classList.contains('hidden');
}
function updateGearIcon(){
  const g = getId('settingsbtn'); if(!g) return;
  if(GEAR_ICON === null) GEAR_ICON = g.innerHTML;  // captured while still the gear
  if(!pleditorOpen()){ g.innerHTML = GEAR_ICON; return; }
  g.innerHTML = validatorMode ? ICON_VALIDATOR : ICON_EDITOR;
}
function setValidateButton(running){
  const b = getId('plvalidatebtn'); if(!b) return;
  b.textContent = running ? t('btn_cancel') : t('btn_pl_validate', 'Check All Streams');
}
function clearValidationStatus(){
  // rows only: the header has .plestatus too
  const box = getId('pleditorcontent');
  if(box) box.querySelectorAll('.plestatus').forEach(function(el){ el.textContent=''; el.classList.remove('pass','fail'); });
}
// back to a clean editor
function resetValidatorMode(){
  validatorMode = false;
  validatorRunning = false;
  document.body.classList.remove('plvalidating');
  const w = getId('pleditorwrap');
  if(w){ w.classList.remove('validator'); w.classList.remove('validating'); }
  const ttl = getId('pleditortitle');
  if(ttl) ttl.textContent = t('ttl_playlist_editor', 'Playlist Editor');
  setValidateButton(false);
  clearValidationStatus();
}
function setValidatorMode(on){
  validatorMode = !!on;
  const w = getId('pleditorwrap'); if(w) w.classList.toggle('validator', validatorMode);
  const ttl = getId('pleditortitle');
  if(ttl) ttl.textContent = validatorMode ? t('ttl_playlist_validator', 'Playlist Validator') : t('ttl_playlist_editor', 'Playlist Editor');
  clearValidationStatus();
  updateGearIcon();
}
function setValidationRunning(on){
  validatorRunning = !!on;
  const w = getId('pleditorwrap'); if(w) w.classList.toggle('validating', validatorRunning);
  document.body.classList.toggle('plvalidating', validatorRunning);   // reaches the settings gear, outside the wrap
  setValidateButton(validatorRunning);
}
function onValidateButton(){
  if(validatorRunning){ websocket.send('plvalidate=0'); return; }   // Cancel; the done message re-arms the button
  if(!validatorMode) return;
  clearValidationStatus();
  uncheckSelectAll();
  setValidationRunning(true);
  websocket.send('plvalidate=1');
}
function handleValidationMessage(pv){
  if(pv.start){ setValidationRunning(true); clearValidationStatus(); return; }
  if(typeof pv.i !== 'undefined'){
    const items = getId('pleditorcontent').getElementsByTagName('li');
    const li = items[pv.i - 1]; if(!li) return;
    const st = li.getElementsByClassName('plestatus')[0]; if(!st) return;
    const pass = (pv.r === 'pass');
    st.textContent = pass ? '\u2714' : '\u2716';
    st.classList.toggle('pass', pass);
    st.classList.toggle('fail', !pass);
    if(!pass){
      // tick the row and mark its name
      const cb = li.getElementsByClassName('plcb')[0]; if(cb) cb.checked = true;
      const nm = li.getElementsByClassName('plename')[0];
      if(nm){
        const note = '(' + t('msg_url_broken', 'URL broken?') + ')';
        if(nm.value.indexOf(note) === -1) nm.value = (nm.value ? nm.value + ' ' : '') + note;
      }
    }
    return;
  }
  if(pv.done){ setValidationRunning(false); return; }
}
function checkboxClick(cb, command){
  cb.classList.toggle("checked");
  websocket.send(`${command}=${cb.classList.contains("checked")?1:0}`);
}
function sliderInput(sl, command){
  websocket.send(`${command}=${sl.value}`);
  fillSlider(sl);
}
function submitWiFi(){
  var output="";
  var items=document.getElementsByClassName("credential");
  for (var i = 0; i <= items.length - 1; i++) {
    inputs=items[i].getElementsByTagName("input");
    if(inputs[0].value == "") continue;
    let ps=inputs[1].value==""?inputs[1].dataset.pass:inputs[1].value;
    output+=inputs[0].value+"\t"+ps+"\n";
  }
  if(output!=""){ // Well, let's say, quack.
    let file = new File([output], "tempwifi.csv",{type:"text/plain;charset=utf-8", lastModified:new Date().getTime()});
    let container = new DataTransfer();
    container.items.add(file);
    let fileuploadinput=getId("file-upload");
    fileuploadinput.files = container.files;
    var formData = new FormData();
    formData.append("wifile", fileuploadinput.files[0]);
    var xhr = new XMLHttpRequest();
    xhr.open("POST",`http://${hostname}/upload`,true);
    xhr.send(formData);
    fileuploadinput.value = '';
    // device restarts: poll /ready like every reboot
    rebootSystem(t('msg_settings_saved', 'Settings saved. Rebooting...'));
  }
}
function playItem(target){
  const item = target.attr('attr-id');
  setCurrentItem(item)
  websocket.send(`play=${item}`);
}
function hideSpinner(){
  getId("progress").classList.add("hidden");
  getId("content").classList.remove("hidden");
}
function loadLogoThen(callback){
  fetch(`logo.svg`).then(response => response.text()).then(svg => {
    getId('logo').innerHTML = svg;
    if (typeof callback === 'function') callback();
  });
}
function applyCommonPageMeta(){
  getId("version").innerText=`${radioVersion}`;
  localePromise.then(function(){ applyI18n(); });
}
function changeMode(el){
  const cmd = el.dataset.command;
  el.classList.add('hidden');
  // this is where a switch starts: flip the icons
  if(cmd=='sd') getId('modesd').classList.remove('hidden');
  else getId('modeweb').classList.remove('hidden');
  setModeSwitching(true);
  websocket.send("newmode="+(cmd=="web"?0:1));
}
function toggleShuffle(){
  let el = getId('shuffle');
  el.classList.toggle('active');
  websocket.send("shuffle="+(el.classList.contains('active')?1:0));
}
function playPreview(root) {
  const streamUrl = root.getElementsByClassName('pleurl')[0].value;
  const stationName = root.getElementsByClassName('plename')[0].value;
  
  // Send preview request to device
  if(streamUrl=='') { console.error("No stream URL available."); return; }
  
  sendStationAction(stationName, streamUrl, false);
}
// .hold keeps the hover colour briefly
const HOLD_MS = 2000;
function holdPressed(el){
  el.classList.add('pressed');
  clearTimeout(el.holdTimer);
  el.holdTimer = setTimeout(function(){ el.classList.remove('pressed'); }, HOLD_MS);
}
function continueLoading(mode){
  if(typeof mode === 'undefined' || loaded) return;
  if(mode=="player"){
    const pathname = window.location.pathname;
    if(['/','/index.html'].includes(pathname)){
      document.title = `${Title} - Player`;
      fetch(`player.html`).then(response => response.text()).then(player => {
        getId('content').classList.add('idx');
        getId('content').innerHTML = player; 
        loadLogoThen(function(){
          hideSpinner();
        });
        applyCommonPageMeta();
        if (newVerAvailable) getId('update_available').classList.remove('hidden');
        document.querySelectorAll('input[type="range"]').forEach(sl => { fillSlider(sl); });
        wsSetPageBurst(['getindex=1']);
        wsRequestPageState();
        // curated review, if pending
        if (sessionStorage.getItem('pl_import_review') === 'true') {
          const mode = sessionStorage.getItem('pl_import_mode') || 'replace';
          console.log('[Curated] Loading playlist for review, mode:', mode);
          setTimeout(() => loadCuratedForReview(mode), 200);
        }
        //generatePlaylist(`http://${hostname}/data/playlist.csv`+"?"+new Date().getTime());
      });
    }
    if(pathname=='/settings.html'){
      document.title = `${Title} - Settings`;
      fetch(`options.html`).then(response => response.text()).then(options => {
        getId('content').innerHTML = options;
        loadJS(`options.js`, () => {
          loadLogoThen(function(){
            hideSpinner();
            if (onlineUpdCapable) getId('webboard').classList.add('hidden');
          });
          applyCommonPageMeta();
          if (newVerAvailable) getId('update_available').classList.remove('hidden');
          document.querySelectorAll('input[type="range"]').forEach(sl => { fillSlider(sl); });
          if (timezoneData) {
            populateTZDropdown(timezoneData);
          }
          // this page request set, re-asked on open
          wsSetPageBurst(['getsystem=1','getscreen=1','getscreensaver=1','getlocale=1','getweather=1','getmqtt=1',
                          'getcontrols=1','getactive=1','getbattery=1','getmute=1']);
          getWiFi(`http://${hostname}/data/wifi.csv`);
          wsRequestPageState();
          classEach("reset", function(el){ el.innerHTML='<svg viewBox="0 0 16 16" class="fill"><path d="M8 3v5a36.973 36.973 0 0 1-2.324-1.166A44.09 44.09 0 0 1 3.417 5.5a52.149 52.149 0 0 1 2.26-1.32A43.18 43.18 0 0 1 8 3z"/><path d="M7 5v1h4.5C12.894 6 14 7.106 14 8.5S12.894 11 11.5 11H1v1h10.5c1.93 0 3.5-1.57 3.5-3.5S13.43 5 11.5 5h-4z"/></svg>'; });
          initDangerZone();
        });
      });
    }
    if(pathname=='/update.html'){
      document.title = `${Title} - Update`;
      fetch(`updform.html`).then(response => response.text()).then(updform => {
        getId('content').classList.add('upd');
        getId('content').innerHTML = updform;
        loadLogoThen(function(){
          hideSpinner();
          ensureFunctionLoaded('initOnlineUpdateChecker', `script2.js`, function(){ initOnlineUpdateChecker(); });
        });
        applyCommonPageMeta();
      });
    }
    if(pathname=='/ir.html'){
      document.title = `${Title} - IR Recorder`;
      fetch(`irrecord.html`).then(response => response.text()).then(ircontent => {
        getId('content').innerHTML = ircontent;
        loadLogoThen(function(){
          ensureFunctionLoaded('initControls', `script2.js`, function(){ initControls(); });
          hideSpinner();
        });
        applyCommonPageMeta();
      });
    }
  }else{ // AP mode
    fetch(`options.html`).then(response => response.text()).then(options => {
      getId('content').innerHTML = options;
      loadJS(`options.js`, () => {
        loadLogoThen(function(){
          hideSpinner();
        });
        applyCommonPageMeta();
        getWiFi(`http://${hostname}/data/wifi.csv`);
        wsSetPageBurst(['getactive=1']);
        wsRequestPageState();
      });
    });
  }
  document.body.addEventListener('click', (event) => {
    let target = event.target.closest('div, span, li');
    if (!target) return; // Exit early if no matching element found
    if(target.classList.contains("knob")) target = target.parentElement;
    //if(target.classList.contains("snfknob")) target = target.parentElement;
    if(target.parentElement.classList.contains("play")){ playItem(target.parentElement); return; }
    if(target.classList.contains("navitem")) { getId(target.dataset.target).scrollIntoView({ behavior: 'smooth' }); return; }
    if(target.classList.contains("reset")) { websocket.send("reset="+target.dataset.name); return; }
    if(target.classList.contains("done")) { window.location.href=`http://${hostname}/`; return; }
    if(target.id === 'dangerzone_sw1') { toggleDangerZoneSwitch('dangerzone_sw1'); event.preventDefault(); event.stopPropagation(); return; }
    if(target.id === 'dangerzone_sw2') { toggleDangerZoneSwitch('dangerzone_sw2'); event.preventDefault(); event.stopPropagation(); return; }
    if(target.id === 'dangerzone_sw3') { toggleDangerZoneSwitch('dangerzone_sw3'); event.preventDefault(); event.stopPropagation(); return; }
    let command = target.dataset.command;
    // resolve to the nearest [data-command] holder
    if (!command) {
      const holder = target.closest('[data-command]');
      if (holder) { target = holder; command = holder.dataset.command; }
    }
    if (command){
      if (target.classList.contains('hold')) holdPressed(target);
      if(target.classList.contains("local")){
        switch(command){
          case "toggle": toggleTarget(target, target.dataset.target); break;
          case "settings":
            // gear toggles Validator while editing
            if(pleditorOpen()){ setValidatorMode(!validatorMode); }
            else window.location.href=`http://${hostname}/settings.html`;
            break;
          case "plimport": break;
          case "plexport": exportCurrentPlaylist(); uncheckSelectAll(); break;
          case "pladd": plAdd(); break;
          case "pldel": plRemove(); break;
          case "plsubmit": submitPlaylist(); break;
          case "plvalidate": onValidateButton(); break;
          case "plundo": undoPlaylistChanges(); break;
          case "plshare": sharePlaylist(); break;
          case "fwupdate": window.location.href=`http://${hostname}/update.html`; break;
          // new tab: used beside settings
          case "editor": window.open(`http://${hostname}/editor.html`, '_blank'); break;
          case "webboard": window.location.href=`http://${hostname}/webboard`; break;
          case "setupir": window.location.href=`http://${hostname}/ir.html`; break;
          case "search": window.location.href=`http://${hostname}/search.html`; break;
          // page redirects itself to / when ready
          case "sdfilemanager": window.location.href=`http://${hostname}/sdmanager.html`; break;
          case "applyweather": applyWeather(); break;
          case "applytz": applyTZ(); break;
          case "applylocale": applyLocale(); break;
          case "applymqtt": applyMQTT(); break;
          case "wifiexport": window.open(`http://${hostname}/data/wifi.csv`+"?"+new Date().getTime()); break;
          case "wifiupload": submitWiFi(); break;
          case "confirm-reboot": showDangerConfirm('dz_reboot'); break;
          case "confirm-format": showDangerConfirm('dz_format'); break;
          case "confirm-reset": showDangerConfirm('dz_reset'); break;
          case "reboot": websocket.send("reboot=1"); rebootSystem(t('msg_rebooting', 'Rebooting...')); break;
          case "format": websocket.send("format=1"); rebootSystem(t('msg_format_reboot', 'Format LittleFS. Rebooting...'), 0, false); break;
          case "reset":  websocket.send("reset=1"); getId("settingscontent").innerHTML=`<h2>${t('msg_reset', 'Settings reset.')}</h2>`; setTimeout(() => location.reload(), 2000); break;
          case "shuffle": toggleShuffle(); break;
          case "ehdpsave": websocket.send(`ehdpname=${getId('ehdpname').value}`); break;
          case "restartmdns": {
            const mdnsValue = (getId('mdns').value || '').trim();
            websocket.send(`mdnsname=${mdnsValue}`);
            if (mdnsValue && typeof redirectWhenReady === 'function') {
              const mdnsHost = `${mdnsValue}.local`;
              getId("mdnsnamerow").innerHTML=`<h3 style="line-height: 37px;color: #aaa; margin: 0 auto;">${t('msg_mdns_restarting', 'mDNS restarting...')}</h3>`;
              redirectWhenReady({
                waitSeconds: 8,
                readyHost: mdnsHost,
                redirectUrl: `http://${mdnsHost}/settings.html`,
                afterReadyDelayMs: 500
              });
            }
            break;
          }
          case "savebattref": websocket.send(`battref=${getId('battref').value}`); break;
          default: break;
        }
      }else{
        if(target.classList.contains("checkbox")) checkboxClick(target, command);
        if(target.classList.contains("cmdbutton")) { websocket.send(`${command}=1`); }
        if(target.classList.contains("modeitem")) changeMode(target);
        if(target.hasClass("pleplay")) playPreview(target.parentElement);
        if(target.classList.contains("play")){
          const item = target.attr('attr-id');
          setCurrentItem(item)
          websocket.send(`${command}=${item}`);
        }
      }
      event.preventDefault(); event.stopPropagation();
    }
  });
  document.body.addEventListener('input', (event) => {
    let target = event.target;
    let command = target.dataset.command;
    if (!command) { command = target.parentElement.dataset.command; target = target.parentElement; }
    if (command) {
      if(target.type==='range') sliderInput(target, command);  //<-- range
      else websocket.send(`${command}=${target.value}`);       //<-- other
      event.preventDefault(); event.stopPropagation();
    }
  });
  document.body.addEventListener('change', (event) => {
    const target = event.target;
    if (target.tagName === 'SELECT') {
      const command = target.dataset.command;
      if (command) {
        websocket.send(`${command}=${target.value}`);
        event.preventDefault(); event.stopPropagation();
      }
    }
  });
  document.body.addEventListener('mousewheel', (event) => {
    const target = event.target;
    if(target.type==='range'){
      const command = target.dataset.command;
      target.valueAsNumber += event.deltaY>0?-1:1;
      if (command) {
        sliderInput(target, command);
      }
    }
  });
}

// ===== UPDATE =====
var uploadWithError = false;
var rebootTimer;
var readyPollTimer;
var readyRedirectDelayTimer;
var readyRedirectToken = 0;

function clearReadyRedirectTimers(){
  clearTimeout(rebootTimer);
  clearTimeout(readyPollTimer);
  clearTimeout(readyRedirectDelayTimer);
}

function redirectWhenReady(options){
  const settings = options || {};
  const waitSeconds = Math.max(1, settings.waitSeconds || 5);
  const pollMs = Math.max(250, settings.pollMs || 1000);
  const afterReadyDelayMs = Math.max(0, settings.afterReadyDelayMs || 1000);
  const fetchTimeoutMs = Math.max(250, settings.fetchTimeoutMs || 750);
  const redirectUrl = settings.redirectUrl || `http://${hostname}/`;
  const readyHost = settings.readyHost || hostname;
  const onTick = typeof settings.onTick === 'function' ? settings.onTick : null;
  const onReady = typeof settings.onReady === 'function' ? settings.onReady : null;
  const start = Date.now();
  let sawNotReady = false;
  const token = ++readyRedirectToken;

  clearReadyRedirectTimers();

  const finishRedirect = () => {
    if (token !== readyRedirectToken) return;
    clearReadyRedirectTimers();
    window.location.replace(redirectUrl);
  };

  const scheduleNext = (delayMs) => {
    if (token !== readyRedirectToken) return;
    readyPollTimer = setTimeout(pollReady, delayMs);
  };

  const updateProgress = () => {
    if (!onTick) return;
    const elapsedMs = Date.now() - start;
    const progress = Math.min(100, Math.round((elapsedMs / (waitSeconds * 1000)) * 100));
    onTick({ elapsedMs, waitSeconds, progress, sawNotReady });
  };

  const pollReady = () => {
    if (token !== readyRedirectToken) return;

    updateProgress();

    if (Date.now() - start >= waitSeconds * 1000) {
      finishRedirect();
      return;
    }

    const controller = new AbortController();
    const fetchTimeout = setTimeout(() => controller.abort(), fetchTimeoutMs);

    fetch(`http://${readyHost}/ready?ts=${Date.now()}`, {
      cache: 'no-store',
      signal: controller.signal
    })
      .then((response) => response.ok ? response.json() : { ready: false })
      .then((data) => {
        clearTimeout(fetchTimeout);
        if (token !== readyRedirectToken) return;
        if (data.ready === true) {
          if (!sawNotReady) {
            scheduleNext(pollMs);
            return;
          }
          if (onReady) onReady();
          readyRedirectDelayTimer = setTimeout(finishRedirect, afterReadyDelayMs);
          return;
        }
        sawNotReady = true;
        scheduleNext(pollMs);
      })
      .catch(() => {
        clearTimeout(fetchTimeout);
        if (token !== readyRedirectToken) return;
        sawNotReady = true;
        scheduleNext(pollMs);
      });
  };

  pollReady();
}

function doUpdate(el) {
  let binfile = getId('binfile').files[0];
  if(binfile){
    getId('updateform').classList.add('hidden');
    getId("updateprogress").value = 0;
    getId('updateprogress').hidden=false;
    getId('update_cancel_button').hidden=true;
    getId('check_online_update').classList.add('hidden');
    var formData = new FormData();
    formData.append("updatetarget", getId('uploadtype1').checked?"firmware":"littlefs");
    formData.append("update", binfile);
    var xhr = new XMLHttpRequest();
    uploadWithError = false;
    xhr.onreadystatechange = function() {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        if(xhr.responseText!="OK"){
          getId("uploadstatus").innerHTML = xhr.responseText;
          uploadWithError=true;
        }
      }
    }
    xhr.upload.addEventListener("progress", progressHandler, false);
    xhr.addEventListener("load", completeHandler, false);
    xhr.addEventListener("error", errorHandler, false);
    xhr.addEventListener("abort", abortHandler, false);
    xhr.open("POST",`http://${hostname}/update`,true);
    xhr.send(formData);
  }else{
    alert(t('msg_choose_first', 'Choose something first'));
  }
}
function progressHandler(event) {
  var percent = (event.loaded / event.total) * 100;
  getId("uploadstatus").textContent = t('msg_upload_pct', '{0}% uploaded | please wait...', Math.round(percent));
  getId("updateprogress").value = Math.round(percent);
  if (percent >= 100) {
    getId("uploadstatus").textContent = t('msg_upload_writing', 'Please wait, writing file to filesystem...');
  }
}
function rebootingProgress(waitSeconds) {
  redirectWhenReady({
    waitSeconds: waitSeconds,
    afterReadyDelayMs: 1000,
    onTick: function(state){
      const progress = getId("updateprogress");
      if(progress) progress.value = state.progress;
    },
    onReady: function(){
      const progress = getId("updateprogress");
      const status = getId("uploadstatus");
      if(progress) progress.value = 100;
      if(status) status.textContent = t('msg_system_ready', 'System ready, reloading...');
    }
  });
}
function completeHandler(event) {
  if(uploadWithError) return;
  getId("uploadstatus").textContent = t('msg_upload_complete', 'Upload complete, rebooting...');
  rebootingProgress(60);
}
function errorHandler(event) {
  getId('updateform').classList.remove('hidden');
  getId('updateprogress').hidden=true;
  getId("updateprogress").value = 0;
  getId("status").textContent = t('msg_upload_failed', 'Upload Failed');
  getId('check_online_update').classList.remove('hidden');
}
function abortHandler(event) {
  getId('updateform').classList.remove('hidden');
  getId('updateprogress').hidden=true;
  getId("updateprogress").value = 0;
  getId("status").textContent = t('msg_upload_aborted', 'Upload Aborted');
  getId('check_online_update').classList.remove('hidden');
}

// ===== Playlist Editor Dragging Functionality =====

let dragged, id, index, indexDrop, list;
let scrollInterval = null;
let lastX = 0, lastY = 0;
let startX = 0, startY = 0;
let pressHandle = null;    // handle held; the drag starts on movement
const DRAG_START_PX = 6;

function getLi(el) {
	while (el && el !== document.body && el !== null) {
		if (el.classList && el.classList.contains('pleitem')) return el;
		el = el.parentNode;
	}
	return null;
}

function clearIndicators() {
	const items = document.querySelectorAll('.pleitem');
	items.forEach(item => {
		item.classList.remove('drag-over-above', 'drag-over-below');
	});
}

function stopAutoScroll() {
	if (scrollInterval) {
		clearInterval(scrollInterval);
		scrollInterval = null;
	}
}

function handleAutoScroll(x, y) {
	if (!dragged) return;
	const container = document.getElementById('pleditorcontent');
	if (!container) return;
	const rect = container.getBoundingClientRect();
	const threshold = 50;
	let direction = 0;

	if (y < rect.top + threshold) direction = -1;
	else if (y > rect.bottom - threshold) direction = 1;

	lastX = x;
	lastY = y;

	if (direction !== 0) {
		if (!scrollInterval) {
			scrollInterval = setInterval(() => {
				container.scrollTop += direction * 8;
				const target = document.elementFromPoint(lastX, lastY);
				const targetLi = getLi(target);
				setIndicator({ clientX: lastX, clientY: lastY }, targetLi);
			}, 25);
		}
	} else {
		stopAutoScroll();
	}
}

function updateIndices() {
	let items = document.getElementById('pleditorcontent').getElementsByTagName('li');
	for (let i = 0; i < items.length; i++) {
		items[i].getElementsByTagName('span')[0].innerText = ("00" + (i + 1)).slice(-3);
	}
}

function startDrag(target) {
	if (validatorMode) return;  // no reordering in the Validator
	const li = getLi(target);
	if (li) {
		dragged = li;
		id = li.id;
		list = li.parentNode.children;
		for (let i = 0; i < list.length; i += 1) {
			if (list[i] === dragged) {
				index = i;
				break;
			}
		}
		dragged.classList.add('dragging');
	}
}

function setIndicator(e, targetLi) {
	if (!dragged || !targetLi || targetLi === dragged) {
		clearIndicators();
		return;
	}
	const rect = targetLi.getBoundingClientRect();
	const mouseY = (e.touches && e.touches.length > 0) ? e.touches[0].clientY : (e.clientY || e.pageY || 0);
	const threshold = rect.top + rect.height / 2;
	
	clearIndicators();
	if (mouseY < threshold) {
		targetLi.classList.add('drag-over-above');
	} else {
		targetLi.classList.add('drag-over-below');
	}
}

function doDrop(target) {
	stopAutoScroll();
	const targetLi = getLi(target);
	if (dragged && targetLi && targetLi !== dragged) {
		const isAbove = targetLi.classList.contains('drag-over-above');
		dragged.remove();
		for (let i = 0; i < list.length; i += 1) {
			if (list[i] === targetLi) {
				indexDrop = i;
				break;
			}
		}
		if (isAbove) {
			targetLi.before(dragged);
		} else {
			targetLi.after(dragged);
		}
		updateIndices();
	}
	if (dragged) dragged.classList.remove('dragging');
	clearIndicators();
	dragged = null;
}

// one pointer path for mouse and touch
function endDrag() {
	pressHandle = null;
	stopAutoScroll();
	if (dragged) dragged.classList.remove('dragging');
	clearIndicators();
	dragged = null;
}

document.addEventListener("pointerdown", (e) => {
	const handle = e.target.closest ? e.target.closest('.grabbable') : null;
	if (!handle || !getLi(handle)) return;
	if (e.pointerType === 'mouse' && e.button !== 0) return;
	if (e.cancelable) e.preventDefault();   // keeps the browser's long press out
	pressHandle = handle;
	startX = e.clientX;
	startY = e.clientY;
	try { handle.setPointerCapture(e.pointerId); } catch (err) {}
});

document.addEventListener("pointermove", (e) => {
	if (dragged) {
		handleAutoScroll(e.clientX, e.clientY);
		setIndicator(e, getLi(document.elementFromPoint(e.clientX, e.clientY)));
		return;
	}
	if (!pressHandle) return;
	if (Math.abs(e.clientX - startX) <= DRAG_START_PX && Math.abs(e.clientY - startY) <= DRAG_START_PX) return;
	const handle = pressHandle;
	pressHandle = null;
	startDrag(handle);
	setIndicator(e, getLi(document.elementFromPoint(e.clientX, e.clientY)));   // mark at once
});

document.addEventListener("pointerup", (e) => {
	const target = document.elementFromPoint(e.clientX, e.clientY);
	pressHandle = null;
	if (dragged) doDrop(target);
});

document.addEventListener("pointercancel", () => {
	if (!dragged) { pressHandle = null; return; }   // nothing started, nothing to undo
	endDrag();
});

// long press must not open the browser menu
document.addEventListener("contextmenu", (e) => {
	const onHandle = e.target.closest && e.target.closest('.grabbable');
	if (onHandle || getLi(e.target)) {
		e.preventDefault();
		return false;
	}
});
