// Options/Settings page specific functions

const localTZjson = 'timezones.json';
const localLocalesJson = 'wwwlocale.json';
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
    loadThemes();
    loadLayouts();
    loadTimezones();
    loadLocales();
    loadDisplayLocales();
    setupWeatherProviderToggle();
    setupDimmingControls();
  });
} else {
  // DOM already loaded (script loaded dynamically)
  loadThemes();
  loadLayouts();
  loadTimezones();
  loadLocales();
  loadDisplayLocales();
  setupWeatherProviderToggle();
  setupDimmingControls();
}

/** THEME & LAYOUT dropdowns **/
let themeData = null, layoutData = null;
let pendingThemeId = null, pendingLayoutId = null;

async function loadThemes() {
  try {
    const r = await fetch('themes.json');
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    themeData = await r.json();
    populateNamedDropdown('themeId', themeData);
    if (pendingThemeId !== null) {
      const sel = getId('themeId');
      if (sel) sel.value = pendingThemeId;
      pendingThemeId = null;
    }
  } catch(e) { console.error('Failed to load themes:', e); }
}

async function loadLayouts() {
  try {
    const r = await fetch('layouts.json');
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    layoutData = await r.json();
    populateNamedDropdown('layoutId', layoutData);
    if (pendingLayoutId !== null) {
      const sel = getId('layoutId');
      if (sel) sel.value = pendingLayoutId;
      pendingLayoutId = null;
    }
  } catch(e) { console.error('Failed to load layouts:', e); }
}

function populateNamedDropdown(elemId, data) {
  const sel = getId(elemId);
  if (!sel) return;
  sel.innerHTML = '';
  Object.entries(data).forEach(([id, name]) => {
    const opt = document.createElement('option');
    opt.value = id;
    opt.textContent = name.length > 40 ? name.substring(0,37)+'...' : name;
    sel.appendChild(opt);
  });
  sel.addEventListener('change', () => {
    if (sel.dataset.prev !== sel.value) {
      const cmd = elemId === 'themeId' ? 'theme' : 'layout';
      websocket.send(`${cmd}=${sel.value}`);
      sel.dataset.prev = sel.value;
    }
  });
}

// Hook for script.js websocket handler to restore current values
window.afterSetupElement = (function(orig) {
  return function(id, value, element) {
    if (typeof orig === 'function') orig(id, value, element);
    if (id === 'themeId') {
      if (themeData) getId('themeId').value = value;
      else pendingThemeId = value;
    }
    if (id === 'layoutId') {
      if (layoutData) getId('layoutId').value = value;
      else pendingLayoutId = value;
    }
  };
})(window.afterSetupElement);

/** SCREEN DIMMING **/
function syncDimmingUi(sendClampCommand = false) {
  const brightness = getId('br');
  const dimmingToggle = getId('dim');
  const dimmingTimeout = getId('dimto');
  const dimmingBrightness = getId('dimbr');
  if (!brightness || !dimmingToggle || !dimmingTimeout || !dimmingBrightness) {
    return false;
  }

  dimmingBrightness.min = '0';
  dimmingBrightness.max = '100';

  const brightnessValue = Number.isFinite(brightness.valueAsNumber) ? brightness.valueAsNumber : 0;
  const currentDimmingValue = Number.isFinite(dimmingBrightness.valueAsNumber) ? dimmingBrightness.valueAsNumber : 0;
  const nextDimmingValue = Math.max(0, Math.min(brightnessValue, currentDimmingValue));
  const changed = nextDimmingValue !== currentDimmingValue;

  if (changed) {
    dimmingBrightness.value = String(nextDimmingValue);
  }
  fillSlider(dimmingBrightness);

  if (changed && sendClampCommand) {
    websocket.send(`dimmingbrightness=${nextDimmingValue}`);
  }

  return changed;
}

function setupDimmingControls() {
  const brightness = getId('br');
  const dimmingToggle = getId('dim');
  const dimmingBrightness = getId('dimbr');
  if (!brightness || !dimmingToggle || !dimmingBrightness) {
    return;
  }

  const previousAfterSetupElement = window.afterSetupElement;
  window.afterSetupElement = (id, value, element) => {
    if (typeof previousAfterSetupElement === 'function') {
      previousAfterSetupElement(id, value, element);
    }
    if (id === 'br' || id === 'dimbr' || id === 'dim' || id === 'dimto') {
      syncDimmingUi();
    }
  };

  brightness.addEventListener('input', () => {
    syncDimmingUi(true);
  });

  dimmingBrightness.addEventListener('input', () => {
    syncDimmingUi();
  });

  dimmingToggle.addEventListener('click', () => {
    setTimeout(() => {
      syncDimmingUi();
    }, 0);
  });

  syncDimmingUi();
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

let displayLocalesData = null;

async function loadDisplayLocales() {
  try {
    const response = await fetch('dsplocale.json');
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    displayLocalesData = await response.json();
    populateDisplayLocaleDropdown(displayLocalesData);
    // If WebSocket data arrived before locales loaded, apply it now
    if (pendingLocaleData) {
      applyPendingLocaleData();
    }
  } catch (err) {
    console.error("Failed to load display locales:", err);
  }
}

function populateDisplayLocaleDropdown(locales) {
  const select = getId('locale_disp');
  if (!select) return;
  select.innerHTML = '';
  Object.entries(locales).sort().forEach(([code, name]) => {
    const option = document.createElement('option');
    option.value = code;
    option.textContent = `${code}: ${name}`;
    select.appendChild(option);
  });
}

function populateLocaleDropdown(locales) {
  const select = getId('locale_webui');
  if (!select) return;
  select.innerHTML = '';
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
  const newLocaleCode = pendingLocaleData.locale_webui;
  const oldLocaleCode = window.originalLocaleWebui;
  const localeChangedByReset = (oldLocaleCode !== null && newLocaleCode !== oldLocaleCode);
  
  let webuiDone = false, dispDone = false;
  // Set WebUI locale dropdown
  if (select && localesData) {
    const code = newLocaleCode;
    const i = [...select.options].findIndex(opt => opt.value === code);
    if (i !== -1) {
      select.selectedIndex = i;
    }
    webuiDone = true;
  }
  // Set display locale dropdown
  if (display && displayLocalesData) {
    const dispCode = pendingLocaleData.locale_disp;
    const match = dispCode.match(/([a-z]{2}_[A-Z]{2})/);
    const code = match ? match[1] : dispCode;
    const i = [...display.options].findIndex(opt => opt.value === code);
    if (i !== -1) {
      display.selectedIndex = i;
    }
    window.originalLocaleDisp = code;
    dispDone = true;
  }
  if (webuiDone && dispDone) pendingLocaleData = null;
  
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
  // Always send display locale change
  const dispSelect = getId('locale_disp');
  if (dispSelect && dispSelect.value) {
    websocket.send("locale_disp=" + dispSelect.value);
  }
  if (localeChanged) {
    console.log(`[Locale] Applying WebUI locale change to ${selectedCode}`);
    websocket.send("locale_webui=" + selectedCode);
    // Reload page after short delay to apply new locale
    setTimeout(function(){ window.location.reload(); }, 1000);
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
