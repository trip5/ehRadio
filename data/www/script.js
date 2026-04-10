var hostname = window.location.hostname;
var modesd = false;
const query = window.location.search;
const params = new URLSearchParams(query);
const Title = 'ehRadio';
if(params.size>0){
  if(params.has('host')) hostname=params.get('host');
}
var websocket;
var wserrcnt = 0;
var wstimeout;
var loaded = false;
var currentItem = 0;
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


function loadCSS(href){ const link = document.createElement("link"); link.rel = "stylesheet"; link.href = href; document.head.appendChild(link); }
function loadJS(src, callback){ const script = document.createElement("script"); script.src = src; script.type = "text/javascript"; script.async = true; script.onload = callback; document.head.appendChild(script); }
function ensureFunctionLoaded(functionName, src, callback){
  if(typeof window[functionName] === 'function') { callback(); return; }
  loadJS(src, callback);
}

function initWebSocket() {
  clearTimeout(wstimeout);
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(`ws://${hostname}/ws`);
  websocket.onopen    = onOpen;
  websocket.onclose   = onClose;
  websocket.onmessage = onMessage;
}
function onLoad(event) { initWebSocket(); }

function onOpen(event) {
  console.log('Connection opened');
  continueLoading(playMode); //playMode in variables.js
  loaded = true;
  wserrcnt=0;
}
function onClose(event) {
  wserrcnt++;
  wstimeout=setTimeout(initWebSocket, wserrcnt<10?2000:120000);
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
  try{
    const data = JSON.parse(escapeData(event.data));
    /*ir*/
    if(typeof data.ircode !== 'undefined'){
      getId('protocol').innerText=data.protocol;
      classEach('irrecordvalue', function(el){ if(el.hasClass("active")) el.innerText='0x'+data.ircode.toString(16).toUpperCase(); });
      return;
    }
    if(typeof data.irvals !== 'undefined'){
      classEach('irrecordvalue', function(el,i){ var val = data.irvals[i]; if(val>0) el.innerText='0x'+val.toString(16).toUpperCase(); else el.innerText=""; });
      return;
    }
    /*end ir*/
    
    /*online update*/
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
    /*end online update*/
    
    if(typeof data.redirect !== 'undefined'){
      getId("mdnsnamerow").innerHTML=`<h3 style="line-height: 37px;color: #aaa; margin: 0 auto;">${t('msg_redirecting', 'Redirecting to {0}', data.redirect)}</h3>`;
      setTimeout(function(){ window.location.href=data.redirect; }, 4000);
      return;
    }
    if(typeof data.playermode !== 'undefined') { //Web, SD
      modesd = data.playermode=='modesd';
      classEach('modeitem', function(el){ el.classList.add('hidden') });
      if(modesd) showById(['modesd', 'sdsvg'],['plsvg']); else showById(['modeweb','plsvg','bitinfo'],['sdsvg','shuffle']);
      showById(['volslider'],['sdslider']);
      getId('toggleplaylist').classList.remove('active');
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
      
      // If timezoneData not loaded yet, store for later application
      if (!timezoneData) {
        pendingTZData = { tz_name: data.tz_name, tzposix: data.tzposix };
      } else if (select && input) {
        const i = [...select.options].findIndex(opt => opt.text === data.tz_name);
        if (i !== -1) {
          select.selectedIndex = i;
          input.value = select.options[i].value;
        } else {
          // Just show the raw POSIX and keep the label in memory
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
      
      // Store locale data for pending application after locales.json loads
      if (data.locale_webui && data.locale_disp) {
        pendingLocaleData = { locale_webui: data.locale_webui, locale_disp: data.locale_disp };
        
        // If localesData already loaded, apply immediately
        if (localesData) {
          applyPendingLocaleData();
        }
      }
      return;
    }
    
    /* Locale update completed - reload page to load new translations */
    if(typeof data.locale_updated !== 'undefined'){
      console.log('[Locale] Update successful, reloading page...');
      setTimeout(function(){ window.location.reload(); }, 1000);
      return;
    }
    
    /* Locale update failed - show error message */
    if(typeof data.locale_update_failed !== 'undefined'){
      console.error('[Locale] Update failed');
      const select = getId('locale_webui');
      const display = getId('locale_disp');
      
      if (display) {
        display.value = 'Download failed';
      }
      
      setTimeout(function(){ 
        // Restore both dropdown and display field after error message
        // Restore dropdown selection to original
        if (select && window.originalLocaleWebui) {
          const i = [...select.options].findIndex(opt => opt.value === window.originalLocaleWebui);
          if (i !== -1) {
            select.selectedIndex = i;
          }
        }
        
        // Restore display field to original
        if (display && window.originalLocaleDisp) {
          display.value = window.originalLocaleDisp;
        }
      }, 3000);
      return;
    }
    if(typeof data.payload !== 'undefined'){
      data.payload.forEach(item=> {
        // battery string: parse and fill separate elements (labels in HTML with data-i18n)
        if(item.id === 'battery' && typeof item.value === 'string'){
          var m = /volt: (\d+)mV, percentage: (\d+)%?, status: (.+)/.exec(item.value);
          if(m){
            var volt = m[1], perc = m[2], stat = m[3].trim();
            
            setupElement('battery_volt', volt);
            setupElement('battery_perc', perc);
            
            // Hide all battery status spans, then show the active one
            ['battery_charging', 'battery_discharging', 'battery_idle'].forEach(function(id) {
              var el = getId(id);
              if(el) el.classList.add('hidden');
            });
            
            var statusId = 'battery_' + stat.toLowerCase();
            var statusEl = getId(statusId);
            if(statusEl) statusEl.classList.remove('hidden');
            
            // Show battery info wrapper
            const wrap = getId('batteryinfo');
            if (wrap) wrap.classList.remove('hidden');
          }
          return; // Skip normal setupElement for 'battery' id
        }
        setupElement(item.id, item.value);
      });
    }else{
      if(typeof data.current !== 'undefined') { setCurrentItem(data.current); return; }
      if(typeof data.file !== 'undefined') { generatePlaylist(data.file+"?"+new Date().getTime()); websocket.send('submitplaylistdone=1'); return; }
      if(typeof data.act !== 'undefined'){ data.act.forEach(showclass=> { classEach(showclass, function(el) { el.classList.remove("hidden"); }); }); return; }
      Object.keys(data).forEach(key=>{
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
  sl.style.background = 'linear-gradient(to right, var(--accent-dark) 0%,  var(--accent-dark) ' + value + '%, var(--odd-bg-color) ' + value + '%, var(--odd-bg-color) 100%)';
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
      // Skip battref - it's now a calibration input for measured voltage, not ADC reference
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
  }
}
/***--- playlist ---***/
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
    html+=`<li class="pleitem" id="${'plitem'+index}"><span class="grabbable" draggable="true">${("00"+(index+1)).slice(-3)}</span>
      <span class="pleinput plecheck"><input type="checkbox" class="plcb" /></span>
      <input class="pleinput plename" type="text" value="${quoteattr(item.dataset.name)}" maxlength="140" />
      <input class="pleinput pleurl" type="text" value="${item.dataset.url}" maxlength="140" />
      <span class="pleinput pleplay" data-command="preview">&#9658;</span>
      <input class="pleinput pleovol" type="number" min="-30" max="30" step="1" value="${item.dataset.ovol}" />
      </li>`;
  });
  ple.innerHTML=html;
}
function handlePlaylistData(fileData) {
  const ul = getId('playlist');
  ul.innerHTML='';
  if (!fileData) return;
  const lines = fileData.split('\n');
  let li='', html='';
  for(var i = 0;i < lines.length;i++){
    let line = lines[i].split('\t');
    if(line.length==3){
      const active=(i+1==currentItem)?' class="active"':'';
      li=`<li${active} attr-id="${i+1}" class="play" data-name="${line[0].trim()}" data-url="${line[1].trim()}" data-ovol="${line[2].trim()}"><span class="text">${line[0].trim()}</span><span class="count">${i+1}</span></li>`;
      html += li;
    }
  }
  ul.innerHTML=html;
  setCurrentItem(currentItem);
  if(!modesd) initPLEditor();
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
  plitem.innerHTML = '<span class="grabbable" draggable="true">'+("00"+(cnt.length+1)).slice(-3)+'</span>\
      <span class="pleinput plecheck"><input type="checkbox" /></span>\
      <input class="pleinput plename" type="text" value="" maxlength="140" />\
      <input class="pleinput pleurl" type="text" value="" maxlength="140" />\
      <span class="pleinput pleplay" data-command="preview">&#9658;</span>\
      <input class="pleinput pleovol" type="number" min="-30" max="30" step="1" value="0" />';
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
    // Replace mode: wait for editor to open and load current playlist first
    // then clear and import (prevents race condition)
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
    let ovol = inputs[3].value;
    if(ovol < -30) ovol = -30;
    if(ovol > 30) ovol = 30;
    output+=inputs[1].value+"\t"+inputs[2].value+"\t"+ovol+"\n";
  }
  let file = new File([output], "playlist.csv",{type:"text/plain;charset=utf-8", lastModified:new Date().getTime()});
  let container = new DataTransfer();
  container.items.add(file);
  let fileuploadinput=getId("file-upload");
  fileuploadinput.files = container.files;
  doPlUpload(fileuploadinput);
  toggleTarget(0, 'pleditorwrap');
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
  // Load local file from ESP32 filesystem (for curated playlists)
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
      // Silently clear flags if file doesn't exist (e.g., after fresh flash)
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
    // Handle JSONL/NDJSON (missing opening/closing brackets and commas)
    content = content.trim();
    if (!content.startsWith('[')) {
      // JSONL format: multiple objects on separate lines, need commas between them
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
      
      // Extract URL with fallback chain: url_resolved -> url -> host+file+port
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
      // Extract and clamp ovol
      const ovol = Math.max(-30, Math.min(30, parseInt(item.ovol || 0)));
      // If no name, generate from URL
      const finalName = name || (url ? urlToName(url) : '');
      // Check for duplicates
      if (url) {
        if (existingURLs.has(url)) {
          duplicateCount++;
        } else {
          targetCallback(finalName, url, ovol);
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
  // Convert URL to readable name: drop protocol, port, convert special chars to hyphens
  let name = url.replace(/^https?:\/\//, ''); // Drop protocol
  name = name.replace(/:\d+/, ''); // Drop port number
  name = name.replace(/[=&?]/g, '-'); // Convert query chars to hyphens
  name = name.replace(/\/+/g, '-'); // Convert slashes to hyphens
  name = name.replace(/--+/g, '-'); // Collapse multiple hyphens
  name = name.replace(/^-|-$/g, ''); // Trim leading/trailing hyphens
  return name;
}

function parseAndAddCSV(content, targetCallback, mode = 'replace') {
  const lines = content.split('\n');
  const existingURLs = mode === 'merge' ? getExistingURLs() : new Set();
  let addedCount = 0;
  let duplicateCount = 0;
  
  lines.forEach(line => {
    line = line.trim();
    if (!line) return;
    let name = '', url = '', ovol = 0;
    
    // Try tab-delimited first
    if (line.includes('\t')) {
      const parts = line.split('\t').map(p => p.trim());
      
      if (parts.length === 1) {
        // 1 field: URL only
        if (parts[0].includes('.') && (parts[0].includes('/') || parts[0].includes('://'))) {
          url = parts[0];
          name = ''; // Will be generated from URL later
          ovol = 0;
        }
      } else if (parts.length === 2) {
        // 2 fields: one is URL, one is name (order does not matter)
        let urlIdx = -1, nameIdx = -1;
        for (let i = 0; i < 2; i++) {
          if (parts[i].includes('.') && (parts[i].includes('/') || parts[i].includes('://'))) {
            urlIdx = i;
          } else {
            nameIdx = i;
          }
        }
        if (urlIdx !== -1 && nameIdx !== -1) {
          url = parts[urlIdx];
          name = parts[nameIdx];
          ovol = 0;
        }
      } else if (parts.length >= 3) {
        // 3+ fields: Find URL (required), ovol (optional), and name from remaining fields
        let urlIdx = -1;
        for (let i = 0; i < parts.length; i++) {
          if (parts[i].includes('.') && (parts[i].includes('/') || parts[i].includes('://'))) {
            urlIdx = i;
            break;
          }
        }
        
        if (urlIdx !== -1) {
          url = parts[urlIdx];
          
          // Find ovol (optional) - numeric value between -64 and 64
          let ovolIdx = -1;
          for (let i = 0; i < parts.length; i++) {
            if (i === urlIdx) continue; // Skip URL field
            const val = parseInt(parts[i]);
            if (!isNaN(val) && parts[i] === val.toString() && val >= -64 && val <= 64) {
              ovolIdx = i;
              ovol = Math.max(-30, Math.min(30, val));
              break; // Use the first matching ovol
            }
          }
          
          // Name is FIRST field that's not URL or ovol
          for (let i = 0; i < parts.length; i++) {
            if (i !== urlIdx && i !== ovolIdx && parts[i]) {
              name = parts[i];
              break;
            }
          }
          
          if (ovolIdx === -1) ovol = 0;
        }
      }
    } else if (/\s{2,}/.test(line)) {
      // Two-or-more-space-delimited: split by 2+ consecutive spaces
      const parts = line.split(/\s{2,}/).map(p => p.trim()).filter(p => p);
      
      if (parts.length === 1) {
        // 1 field: URL only
        if (parts[0].includes('.') && (parts[0].includes('/') || parts[0].includes('://'))) {
          url = parts[0];
          name = '';
          ovol = 0;
        }
      } else if (parts.length === 2) {
        // 2 fields: one is URL, one is name (order does not matter)
        let urlIdx = -1, nameIdx = -1;
        for (let i = 0; i < 2; i++) {
          if (parts[i].includes('.') && (parts[i].includes('/') || parts[i].includes('://'))) {
            urlIdx = i;
          } else {
            nameIdx = i;
          }
        }
        if (urlIdx !== -1 && nameIdx !== -1) {
          url = parts[urlIdx];
          name = parts[nameIdx];
          ovol = 0;
        }
      } else if (parts.length >= 3) {
        // 3+ fields: Find URL (required), name is first non-URL field, rest are ignored
        let urlIdx = -1;
        for (let i = 0; i < parts.length; i++) {
          if (parts[i].includes('.') && (parts[i].includes('/') || parts[i].includes('://'))) {
            urlIdx = i;
            break;
          }
        }
        
        if (urlIdx !== -1) {
          url = parts[urlIdx];
          // Name is first field that's not the URL
          for (let i = 0; i < parts.length; i++) {
            if (i !== urlIdx && parts[i]) {
              name = parts[i];
              break;
            }
          }
          ovol = 0;
        }
      }
    } else {
      // Single-space-delimited: find URL (contains . and / or ://)
      const tokens = line.split(/\s+/);
      let urlIdx = -1;
      
      for (let i = 0; i < tokens.length; i++) {
        if (tokens[i].includes('.') && (tokens[i].includes('/') || tokens[i].includes('://'))) {
          urlIdx = i;
          break;
        }
      }
      
      if (urlIdx !== -1) {
        url = tokens[urlIdx];
        
        // Name is FIRST field only
        if (urlIdx > 0) {
          // URL not first, so name is first token
          name = tokens[0];
          // Check if LAST token is ovol (must be at end)
          const lastToken = tokens[tokens.length - 1];
          const lastNum = parseInt(lastToken);
          if (!isNaN(lastNum) && lastNum >= -64 && lastNum <= 64 && tokens.length >= 2) {
            ovol = Math.max(-30, Math.min(30, lastNum));
          }
        } else {
          // URL is first: everything after URL is the name (no ovol)
          const nameTokens = [];
          for (let i = urlIdx + 1; i < tokens.length; i++) {
            nameTokens.push(tokens[i]);
          }
          name = nameTokens.join(' ');
          ovol = 0;
        }
      }
    }
    
    // Add http:// if missing
    if (url && !url.startsWith('http://') && !url.startsWith('https://')) {
      url = 'http://' + url;
    }
    
    // If no name, generate from URL
    if (!name && url) {
      name = urlToName(url);
    }
    
    // Check for duplicates
    if (url) {
      if (existingURLs.has(url)) {
        duplicateCount++;
      } else {
        targetCallback(name, url, ovol);
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
}

function getExistingURLs() {
  const urls = new Set();
  const items = document.getElementById('pleditorcontent').getElementsByTagName('li');
  for (let i = 0; i < items.length; i++) {
    const inputs = items[i].getElementsByTagName('input');
    const url = inputs[2].value; // URL is third input (after checkbox and name)
    if (url) urls.add(url);
  }
  return urls;
}

function addStationToEditor(name, url, ovol) {
  const ple = getId('pleditorcontent');
  const cnt = ple.getElementsByTagName('li').length;
  const plitem = document.createElement('li');
  plitem.className = 'pleitem';
  plitem.id = 'plitem' + cnt;
  plitem.innerHTML = `<span class="grabbable" draggable="true">${("00"+(cnt+1)).slice(-3)}</span>
    <span class="pleinput plecheck"><input type="checkbox" class="plcb" /></span>
    <input class="pleinput plename" type="text" value="${name.replace(/"/g, '&quot;')}" maxlength="140" />
    <input class="pleinput pleurl" type="text" value="${url}" maxlength="140" />
    <span class="pleinput pleplay" data-command="preview">&#9658;</span>
    <input class="pleinput pleovol" type="number" min="-30" max="30" step="1" value="${ovol}" />`;
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
    const ovol = inputs[3].value;  // pleovol
    
    if (name === '' || url === '') {
      alert(t('msg_pl_missing', 'Station {0} is missing name or URL. Please fill in all fields before exporting.', i+1));
      return;
    }
    
    csvContent += `${name}\t${url}\t${ovol}\n`;
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
/***--- eof playlist ---***/
let pleditorTimeout = null;
function toggleTarget(el, id){
  const target = getId(id);
  if(id=='pleditorwrap'){
    // Clear any active preview states
    classEach('pleitem', function(el){ el.classList.remove('active') });
    // If opening editor (not closing), refresh playlist first to ensure fresh data
    if(target && target.classList.contains('hidden')) {
      // Cancel any pending timeout to prevent race conditions
      if(pleditorTimeout) { clearTimeout(pleditorTimeout); pleditorTimeout = null; }
      // Don't toggle visibility yet, wait for fresh data
      generatePlaylist(`http://${hostname}/data/playlist.csv`+"?"+new Date().getTime());
      // generatePlaylist will call handlePlaylistData, which will call initPLEditor
      // Then we toggle visibility after a brief delay to let data load
      pleditorTimeout = setTimeout(() => {
        target.classList.remove("hidden");
        getId(target.dataset.target).classList.add("active");
        pleditorTimeout = null;
      }, 200);
      return; // Exit early, don't run the normal toggle below
    }
    // If closing editor, cancel any pending timeout and close immediately
    if(pleditorTimeout) { clearTimeout(pleditorTimeout); pleditorTimeout = null; }
    // Clear import review flags when closing editor
    clearImportReviewFlags();
  }
  if(target){
    if(id=='pleditorwrap' && modesd) {
      getId('sdslider').classList.toggle('hidden');
      getId('volslider').classList.toggle('hidden');
      getId('bitinfo').classList.toggle('hidden');
      getId('shuffle').classList.toggle('hidden');
    }else target.classList.toggle("hidden");
    getId(target.dataset.target).classList.toggle("active");
  }
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
    getId("settingscontent").innerHTML='<h2>'+t('msg_settings_saved', 'Settings saved. Rebooting...')+'</h2>';
    getId("settingsdone").classList.add("hidden");
    getId("navigation").classList.add("hidden");
    setTimeout(function(){ window.location.href=`http://${hostname}/`; }, 10000);
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
  fetch(`logo.svg?v=${radioVersion}`).then(response => response.text()).then(svg => {
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
  if(cmd=='web') getId('modesd').classList.remove('hidden');
  else getId('modeweb').classList.remove('hidden');
  websocket.send("newmode="+(cmd=="web"?0:1));
}
function toggleShuffle(){
  let el = getId('shuffle');
  el.classList.toggle('active');
  websocket.send("shuffle="+el.classList.contains('active'));
}
function playPreview(root) {
  const streamUrl = root.getElementsByClassName('pleurl')[0].value;
  const stationName = root.getElementsByClassName('plename')[0].value;
  
  // Send preview request to device
  if(streamUrl=='') { console.error("No stream URL available."); return; }
  
  sendStationAction(stationName, streamUrl, false);
}
function continueLoading(mode){
  if(typeof mode === 'undefined' || loaded) return;
  if(mode=="player"){
    const pathname = window.location.pathname;
    if(['/','/index.html'].includes(pathname)){
      document.title = `${Title} - Player`;
      fetch(`player.html?${radioVersion}`).then(response => response.text()).then(player => { 
        getId('content').classList.add('idx');
        getId('content').innerHTML = player; 
        loadLogoThen(function(){
          hideSpinner();
        });
        applyCommonPageMeta();
        if (newVerAvailable) getId('update_available').classList.remove('hidden');
        document.querySelectorAll('input[type="range"]').forEach(sl => { fillSlider(sl); });
        websocket.send('getindex=1');
        // Check if we need to load curated playlist for review
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
      fetch(`options.html?${radioVersion}`).then(response => response.text()).then(options => {
        getId('content').innerHTML = options;
        loadJS(`options.js?${radioVersion}`, () => {
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
          websocket.send('getsystem=1');
          websocket.send('getscreen=1');
          websocket.send('getlocale=1');
          websocket.send('getweather=1');
          websocket.send('getmqtt=1');
          websocket.send('getcontrols=1');
          getWiFi(`http://${hostname}/data/wifi.csv`+"?"+new Date().getTime());
          websocket.send('getactive=1');
          websocket.send('getbattery=1');
          classEach("reset", function(el){ el.innerHTML='<svg viewBox="0 0 16 16" class="fill"><path d="M8 3v5a36.973 36.973 0 0 1-2.324-1.166A44.09 44.09 0 0 1 3.417 5.5a52.149 52.149 0 0 1 2.26-1.32A43.18 43.18 0 0 1 8 3z"/><path d="M7 5v1h4.5C12.894 6 14 7.106 14 8.5S12.894 11 11.5 11H1v1h10.5c1.93 0 3.5-1.57 3.5-3.5S13.43 5 11.5 5h-4z"/></svg>'; });
          initDangerZone();
        });
      });
    }
    if(pathname=='/update.html'){
      document.title = `${Title} - Update`;
      fetch(`updform.html?${radioVersion}`).then(response => response.text()).then(updform => {
        getId('content').classList.add('upd');
        getId('content').innerHTML = updform;
        loadLogoThen(function(){
          hideSpinner();
          ensureFunctionLoaded('initOnlineUpdateChecker', `script2.js?${radioVersion}`, function(){ initOnlineUpdateChecker(); });
        });
        applyCommonPageMeta();
      });
    }
    if(pathname=='/ir.html'){
      document.title = `${Title} - IR Recorder`;
      fetch(`irrecord.html?${radioVersion}`).then(response => response.text()).then(ircontent => {
        getId('content').innerHTML = ircontent;
        loadLogoThen(function(){
          ensureFunctionLoaded('initControls', `script2.js?${radioVersion}`, function(){ initControls(); });
          hideSpinner();
        });
        applyCommonPageMeta();
      });
    }
  }else{ // AP mode
    fetch(`options.html?${radioVersion}`).then(response => response.text()).then(options => {
      getId('content').innerHTML = options;
      loadJS(`options.js?${radioVersion}`, () => {
        loadLogoThen(function(){
          hideSpinner();
        });
        applyCommonPageMeta();
        getWiFi(`http://${hostname}/data/wifi.csv`+"?"+new Date().getTime());
        websocket.send('getactive=1');
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
    if (command){
      if(target.classList.contains("local")){
        switch(command){
          case "toggle": toggleTarget(target, target.dataset.target); break;
          case "settings": window.location.href=`http://${hostname}/settings.html`; break;
          case "plimport": break;
          case "plexport": exportCurrentPlaylist(); uncheckSelectAll(); break;
          case "pladd": plAdd(); break;
          case "pldel": plRemove(); break;
          case "plsubmit": submitPlaylist(); break;
          case "plundo": undoPlaylistChanges(); break;
          case "fwupdate": window.location.href=`http://${hostname}/update.html`; break;
          case "webboard": window.location.href=`http://${hostname}/webboard`; break;
          case "setupir": window.location.href=`http://${hostname}/ir.html`; break;
          case "search": window.location.href=`http://${hostname}/search.html`; break;
          case "applyweather": applyWeather(); break;
          case "applytz": applyTZ(); break;
          case "applylocale": applyLocale(); break;
          case "applymqtt": applyMQTT(); break;
          case "wifiexport": window.open(`http://${hostname}/data/wifi.csv`+"?"+new Date().getTime()); break;
          case "wifiupload": submitWiFi(); break;
          case "confirm-reboot": showDangerConfirm('dz_reboot'); break;
          case "confirm-format": showDangerConfirm('dz_format'); break;
          case "confirm-reset": showDangerConfirm('dz_reset'); break;
          case "reboot": websocket.send("reboot=1"); rebootSystem(t('msg_rebooting', 'Rebooting...'), 15, true); break;
          case "format": websocket.send("format=1"); rebootSystem(t('msg_format_reboot', 'Format SPIFFS. Rebooting...'), 0, false); break;
          case "reset":  websocket.send("reset=1"); rebootSystem(t('msg_reset_reboot', 'Reset settings. Rebooting...'), 15, true); break;
          case "shuffle": toggleShuffle(); break;
          case "ehdpsave": websocket.send(`ehdpname=${getId('ehdpname').value}`); break;
          case "rebootmdns": {
            const mdnsValue = (getId('mdns').value || '').trim();
            websocket.send(`mdnsname=${mdnsValue}`);
            websocket.send("rebootmdns=1");
            rebootSystem(t('msg_rebooting', 'Rebooting...'), 0, false);
            if (typeof redirectWhenReady === 'function') {
              const mdnsHost = mdnsValue ? `${mdnsValue}.local` : hostname;
              redirectWhenReady({
                waitSeconds: 20,
                readyHost: mdnsHost,
                redirectUrl: `http://${mdnsHost}/settings.html`,
                afterReadyDelayMs: 1000
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

/** UPDATE **/
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
    formData.append("updatetarget", getId('uploadtype1').checked?"firmware":"spiffs");
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
    getId("uploadstatus").textContent = t('msg_upload_writing', 'Please wait, writing file to filesystem');
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
  getId("uploadstatus").textContent = t('msg_upload_complete', 'Upload Complete, rebooting...');
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


