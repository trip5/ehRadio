// Options/Settings page specific functions

const localTZjson = 'timezones.json';
const localLocalesJson = 'locales.json';
let timezoneData = null;
let localesData = null;
let pendingTZData = null; // Store WebSocket data if it arrives before timezones load
let pendingLocaleData = null; // Store WebSocket data if it arrives before locales load

// Store original locale globally so error handler in script.js can access it
if (typeof window.originalLocaleWebui === 'undefined') {
  window.originalLocaleWebui = null;
}
if (typeof window.originalLocaleDisp === 'undefined') {
  window.originalLocaleDisp = null;
}

// Load timezones and locales - handle both immediate and deferred execution
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => {
    loadTimezones();
    loadLocales();
    setupWeatherProviderToggle();
  });
} else {
  // DOM already loaded (script loaded dynamically)
  loadTimezones();
  loadLocales();
  setupWeatherProviderToggle();
}

/** WEATHER **/
function setupWeatherProviderToggle() {
  const wapiSelect = document.getElementById('wapi');
  if (!wapiSelect) return;
  
  function toggleWeatherFields() {
    const provider = wapiSelect.value;
    const isOpenWeather = (provider === 'OW25' || provider === 'OW30');
    const langRow = document.getElementById('weatherlangrow');
    const keyRow = document.getElementById('weatherkeyrow');
    
    if (langRow) {
      if (isOpenWeather) {
        langRow.classList.remove('hidden');
      } else {
        langRow.classList.add('hidden');
      }
    }
    if (keyRow) {
      if (isOpenWeather) {
        keyRow.classList.remove('hidden');
      } else {
        keyRow.classList.add('hidden');
      }
    }
  }
  
  // Add change listener
  wapiSelect.addEventListener('change', toggleWeatherFields);
  
  // Trigger once on load to set initial state
  toggleWeatherFields();
}

/** LOCALE **/
async function loadTimezones() {
  try {
    const response = await fetch(localTZjson);
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    timezoneData = await response.json();
    populateTZDropdown(timezoneData);
    // If WebSocket data arrived before timezones loaded, apply it now
    if (pendingTZData) {
      applyPendingTZData();
    }
  } catch (err) {
    console.error("Failed to load local timezones:", err);
  }
}

function populateTZDropdown(zones) {
  const select = getId('tz_name');
  const input = getId('tzposix');
  if (!select || !input) {
    return;
  }
  select.innerHTML = '';
  input.readOnly = true;
  Object.entries(zones).forEach(([zone, posix]) => {
    const option = document.createElement('option');
    option.value = posix;
    option.textContent = zone.length > 60 ? zone.substring(0, 57) + '...' : zone;
    select.appendChild(option);
  });
  // Handle dropdown changes
  select.addEventListener('change', () => {
    input.value = select.value;
  });
}

function applyPendingTZData() {
  if (!pendingTZData) return;
  const select = document.getElementById("tz_name");
  const input = document.getElementById("tzposix");
  if (select && input) {
    const i = [...select.options].findIndex(opt => opt.text === pendingTZData.tz_name);
    if (i !== -1) {
      select.selectedIndex = i;
      input.value = select.options[i].value;
    } else {
      const fallbackOption = new Option(pendingTZData.tz_name, pendingTZData.tzposix, true, true);
      select.appendChild(fallbackOption);
      select.selectedIndex = select.options.length - 1;
      input.value = pendingTZData.tzposix;
    }
  }
  pendingTZData = null;
}

async function loadLocales() {
  try {
    const response = await fetch(localLocalesJson);
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    localesData = await response.json();
    populateLocaleDropdown(localesData);
    // If WebSocket data arrived before locales loaded, apply it now
    if (pendingLocaleData) {
      applyPendingLocaleData();
    }
  } catch (err) {
    console.error("Failed to load locales:", err);
  }
}

function populateLocaleDropdown(locales) {
  const select = getId('locale_webui');
  if (!select) return;
  select.innerHTML = '';
  // Check if online update capable (defined in variables.js)
  const canUpdate = typeof onlineUpdCapable !== 'undefined' && onlineUpdCapable;
  if (!canUpdate) {
    // Can't update - show htmlLocale and optionally device locale if locale.json exists
    // Get htmlLocale from variables.js and add as first option
    const htmlLocaleCode = (typeof htmlLocale !== 'undefined') ? htmlLocale : 'en_US';
    const htmlLocaleName = locales[htmlLocaleCode] || htmlLocaleCode;
    const htmlOption = document.createElement('option');
    htmlOption.value = htmlLocaleCode;
    htmlOption.textContent = `${htmlLocaleCode}: ${htmlLocaleName}`;
    select.appendChild(htmlOption);
    // Try to fetch locale.json to see if there's a second locale available
    fetch('locale.json')
      .then(response => response.ok ? response.json() : Promise.reject('Not found'))
      .then(data => {
        if (data.locale_code && data.locale_code !== htmlLocaleCode) {
          // Device has a different locale file - add it as second option
          const code = data.locale_code;
          const name = data.locale || locales[code] || code;
          const option = document.createElement('option');
          option.value = code;
          option.textContent = `${code}: ${name}`;
          select.appendChild(option);
          select.disabled = false;
          // Select currently active locale (check uiLocale variable or pendingLocaleData)
          let currentLocale = htmlLocaleCode;
          if (typeof uiLocale !== 'undefined') {
            currentLocale = uiLocale;
          } else if (pendingLocaleData && pendingLocaleData.locale_webui) {
            currentLocale = pendingLocaleData.locale_webui;
          }
          const i = [...select.options].findIndex(opt => opt.value === currentLocale);
          if (i !== -1) {
            select.selectedIndex = i;
          }
        } else {
          // locale.json exists but same as htmlLocale, only 1 option
          select.disabled = true;
        }
      })
      .catch(() => {
        // locale.json doesn't exist, only htmlLocale available
        console.log('locale.json not available, using htmlLocale only');
        select.disabled = true;
      });
    return;
  }
  // Populate all locales
  Object.entries(locales).sort().forEach(([code, name]) => {
    const option = document.createElement('option');
    option.value = code;
    option.textContent = `${code}: ${name}`;
    select.appendChild(option);
  });
}

function applyPendingLocaleData() {
  if (!pendingLocaleData) return;
  const select = getId('locale_webui');
  const display = getId('locale_disp');
  // Check if online update capable (defined in variables.js)
  const canUpdate = typeof onlineUpdCapable !== 'undefined' && onlineUpdCapable;
  
  const newLocaleCode = pendingLocaleData.locale_webui;
  const oldLocaleCode = window.originalLocaleWebui;
  const localeChangedByReset = (oldLocaleCode !== null && newLocaleCode !== oldLocaleCode);
  
  // Set WebUI locale dropdown (only when canUpdate is true)
  if (select && localesData && canUpdate) {
    const code = newLocaleCode;
    const i = [...select.options].findIndex(opt => opt.value === code);
    if (i !== -1) {
      select.selectedIndex = i;
    } else {
      // Locale not in list - add it as fallback
      const name = localesData[code] || code;
      const option = new Option(`${code}: ${name}`, code, true, true);
      select.appendChild(option);
      select.selectedIndex = select.options.length - 1;
    }
  } else if (select && !canUpdate) {
    // When canUpdate is false, dropdown is already populated by populateLocaleDropdown
    // Update the selected option
    const code = newLocaleCode;
    const i = [...select.options].findIndex(opt => opt.value === code);
    if (i !== -1) {
      select.selectedIndex = i;
    }
  }
  // Format display locale field nicely
  if (display && localesData) {
    const dispCode = pendingLocaleData.locale_disp;
    // Try to extract just the locale code (e.g., "lt_LT" from longer string)
    const match = dispCode.match(/([a-z]{2}_[A-Z]{2})/);
    const code = match ? match[1] : dispCode;
    const name = localesData[code] || dispCode;
    const displayValue = `${code}: ${name}`;
    display.value = displayValue;
    window.originalLocaleDisp = displayValue; // Store original display value globally
  }
  pendingLocaleData = null;
  
  // If locale was changed by reset, trigger apply automatically to show message and reload
  if (localeChangedByReset) {
    applyLocale(); // This checks: select.value !== window.originalLocaleWebui (still old value)
  }
  
  // Update original value after potential applyLocale call
  window.originalLocaleWebui = newLocaleCode;
}

function applyLocale(){
  const select = getId('locale_webui');
  if (!select || !select.value) return;
  const selectedCode = select.value;
  const selectedName = select.selectedOptions[0].textContent;
  // Check if locale actually changed
  const localeChanged = (selectedCode !== window.originalLocaleWebui);
  if (localeChanged) {
    // Show downloading message
    const display = getId('locale_disp');
    if (display) {
      display.value = t('msg_please_wait', 'Please wait...');
    }
    console.log(`[Locale] Requesting locale change to ${selectedCode}`);
    websocket.send("locale_webui=" + selectedCode);
  } else {
    console.log(`[Locale] Locale unchanged (${selectedCode}), skipping download`);
  }
  websocket.send("tz_name="+getId("tz_name").selectedOptions[0].text);
  websocket.send("tzposix="+getId("tzposix").value);
  websocket.send("sntp2="+getId("sntp2").value);
  websocket.send("sntp1="+getId("sntp1").value);
}

/** MQTT **/
function checkboxState(id){
  const el = getId(id);
  return (el && el.classList.contains('checked')) ? 1 : 0;
}

function applyMQTT(){
  websocket.send("mqttenable="+checkboxState("mqttenable"));
  websocket.send("mqtthost="+getId("mqtthost").value);
  websocket.send("mqttport="+getId("mqttport").value);
  websocket.send("mqttuser="+getId("mqttuser").value);
  websocket.send("mqttpass="+getId("mqttpass").value);
  websocket.send("mqtttopic="+getId("mqtttopic").value);
}

/** WEATHER **/
function applyWeather(){
  // Re-send all weather toggles on apply to keep final persisted state aligned with the UI.
  websocket.send("wenable="+checkboxState("wen"));
  websocket.send("wen_feelslike="+checkboxState("wen_feelslike"));
  websocket.send("wen_humidity="+checkboxState("wen_humidity"));
  websocket.send("wen_pressure="+checkboxState("wen_pressure"));
  websocket.send("wen_wind="+checkboxState("wen_wind"));
  websocket.send("wlat="+getId("wlat").value);
  websocket.send("wlon="+getId("wlon").value);
  websocket.send("wapi="+getId("wapi").value);
  websocket.send("wlang="+getId("wlang").value);
  websocket.send("wkey="+getId("wkey").value);
}

/** WIFI **/
function handleWiFiData(fileData) {
  if (!fileData) return;
  var lines = fileData.split('\n');
  for(var i = 0;i < lines.length;i++){
    let line = lines[i].split('\t');
    if(line.length==2){
      getId("ssid"+i).value=line[0].trim();
      getId("pass"+i).attr('data-pass', line[1].trim());
    }
  }
}

function getWiFi(path){
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      if (xhr.status == 200) {
        handleWiFiData(xhr.responseText);
      } else {
        handleWiFiData(null);
      }
    }
  };
  xhr.open("GET", path);
  xhr.send(null);
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

/** SYSTEM **/
function rebootSystem(info, waitSeconds = 15, autoReload = true){
  getId("settingscontent").innerHTML=`<h2>${info}</h2>`;
  getId("settingsdone").classList.add("hidden");
  getId("navigation").classList.add("hidden");
  if (!autoReload) return;
  if (typeof redirectWhenReady === 'function') {
    redirectWhenReady({
      waitSeconds: waitSeconds,
      afterReadyDelayMs: 1000
    });
  } else {
    setTimeout(function(){ window.location.href=`http://${hostname}/`; }, Math.max(1, waitSeconds) * 1000);
  }
}

/** TOOLS AKA DANGER ZONE **/
function scrollToBottom() {
  setTimeout(() => {
    const anchor = document.getElementById('page-bottom');
    if (anchor) {
      anchor.scrollIntoView();
    }
  }, 100);
}

function showDangerConfirm(buttonId) {
  hideDangerConfirm();
  const btn = getId(buttonId);
  if(btn) {
    btn.classList.remove('hidden');
    scrollToBottom();
  }
}

function hideDangerConfirm() {
  const btns = ['dz_reboot', 'dz_format', 'dz_reset'];
  btns.forEach(id => {
    const btn = getId(id);
    if(btn) btn.classList.add('hidden');
  });
}

function checkDangerZone() {
  const sw1 = getId('dangerzone_sw1');
  const sw2 = getId('dangerzone_sw2');
  const sw3 = getId('dangerzone_sw3');
  if(!sw1 || !sw2 || !sw3) return;
  
  const allChecked = sw1.classList.contains('checked') && 
                     sw2.classList.contains('checked') && 
                     sw3.classList.contains('checked');
  
  const dangerzone = getId('dangerzone');
  const txt = getId('dangerzone_txt');
  if(allChecked) {
    if(dangerzone) {
      dangerzone.classList.remove('hidden');
      scrollToBottom();
    }
    if(txt) txt.textContent = t('lbl_dz_careful', 'Be Careful!');
  } else {
    if(dangerzone) dangerzone.classList.add('hidden');
    if(txt) txt.textContent = t('lbl_dz_unlock', 'Turn on all switches to unlock');
  }
}

function toggleDangerZoneSwitch(switchId) {
  const sw = getId(switchId);
  if(!sw) return;
  sw.classList.toggle('checked');
  hideDangerConfirm();
  checkDangerZone();
}

function initDangerZone() {
  const switches = ['dangerzone_sw1', 'dangerzone_sw2', 'dangerzone_sw3'];
  switches.forEach(id => {
    const sw = getId(id);
    if(sw) sw.classList.remove('checked');
  });
  const txt = getId('dangerzone_txt');
  if(txt) txt.textContent = t('lbl_dz_unlock', 'Turn on all switches to unlock');
  const dangerzone = getId('dangerzone');
  if(dangerzone) dangerzone.classList.add('hidden');
  hideDangerConfirm();
}
