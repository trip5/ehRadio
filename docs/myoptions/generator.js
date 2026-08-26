// @ts-nocheck
/*  ehRadio myoptions Generator - generator.js
 *  Reads JSON config files and builds a dynamic hardware configuration page.
 *  State is serialized via LZ-string into the URL hash for shareable links.
 * 
 * Sorry if it looks vibe-coded... it pretty much is.
 * 
 * The .json architecture is more important than how well-coded this javascript / html is
 * - Trip5
 */

'use strict';

// ============================================================
// State encoding helpers (base64 + URI encoding for URL-safe storage)
// URL hash can hold thousands of characters; typical config ~1500-2000 chars.
// ============================================================
var LZString = {
  compressToBase64: function(str) {
    try {
      return btoa(unescape(encodeURIComponent(str)));
    } catch(e) {
      return btoa(str);
    }
  },
  decompressFromBase64: function(b64) {
    try {
      return decodeURIComponent(escape(atob(b64)));
    } catch(e) {
      return atob(b64);
    }
  }
};

// ============================================================
// Global state
// ============================================================
var gData = {
  boards: [],
  boardData: null,       // loaded board .json
  spi: [],
  display: [],
  audio: [],
  input: [],
  peripherals: [],
  locale: [],
  timezones: {},
  defaults: [],
  name: []
};

var gPioIni = '';  // platformio.ini template text
var gVersion = '2026.05';

// ============================================================
// Bootstrap: fetch all JSON files in sequence
// ============================================================
function fetchJSON(url) {
  return fetch(url).then(function(r) {
    if (!r.ok) throw new Error('Failed to load: ' + url);
    return r.json();
  });
}

function fetchText(url) {
  return fetch(url).then(function(r) {
    if (!r.ok) throw new Error('Failed to load: ' + url);
    return r.text();
  });
}

Promise.all([
  fetchJSON('boards.json'),
  fetchJSON('spi.json'),
  fetchJSON('display.json'),
  fetchJSON('audio.json'),
  fetchJSON('input.json'),
  fetchJSON('peripherals.json'),
  fetchJSON('locale.json'),
  fetchJSON('timezones.json'),
  fetchJSON('defaults.json'),
  fetchJSON('name.json'),
  fetchText('platformio.ini')
]).then(function(results) {
  gData.boards     = results[0];
  gData.spi        = results[1];
  gData.display    = results[2];
  gData.audio      = results[3];
  gData.input      = results[4];
  gData.peripherals= results[5];
  gData.locale     = results[6];
  var tzRaw        = results[7];
  gData.defaults   = results[8];
  gData.name       = results[9];
  gPioIni          = results[10];

  // timezones.json: ["Time Zone", {"Africa/Abidjan":"GMT0"}, {"Africa/Accra":"GMT0"}, ...]
  // Each timezone is its own object in the array. Merge them all.
  gData.timezones = {};
  if (Array.isArray(tzRaw)) {
    for (var ti = 1; ti < tzRaw.length; ti++) {
      if (tzRaw[ti] && typeof tzRaw[ti] === 'object') {
        Object.assign(gData.timezones, tzRaw[ti]);
      }
    }
  } else if (typeof tzRaw === 'object') {
    gData.timezones = tzRaw;
  }

  buildPage();
  loadStateFromHash();
}).catch(function(err) {
  document.getElementById('gen-root').innerHTML =
    '<div style="color:#C80C02;padding:20px;text-align:center;">Error loading configuration files: ' +
    err.message + '<br>Please serve this page via a local HTTP server.</div>';
});

// ============================================================
// Unique ID counter for inputs
// ============================================================
var _idCounter = 0;
function uid(prefix) { return (prefix || 'f') + (++_idCounter); }

// ============================================================
// Pad/format helpers
// ============================================================
function padDefine(name, val, width) {
  width = width || 21;
  var s = '#define ' + name;
  while (s.length < 8 + width) s += ' ';
  return s + val;
}

function commentText(comment) {
  if (!comment) return '';
  // linkify http URLs
  return comment.replace(/(https?:\/\/[^\s)]+)/g, function(url) { return url; });
}

// Helper: extract title, info, items from section JSON arrays
// First element can be a string (title only) or object {title, info}
function getSectionMeta(dataArr) {
  var first = dataArr[0];
  if (typeof first === 'string') return { title: first, info: null, items: dataArr.slice(1) };
  if (typeof first === 'object' && first !== null && !Array.isArray(first)) {
    var title = first.title || first.name || '';
    var info = first.info || null;
    return { title: title, info: info, items: dataArr.slice(1) };
  }
  return { title: '', info: null, items: dataArr.slice(1) };
}

// Helper: linkify URLs in info text for innerHTML rendering
function sectionInfoHTML(info) {
  if (!info) return '';
  return info.replace(/(https?:\/\/[^\s)]+)/g, function(url) {
    return '<a href="' + url + '" target="_blank" rel="noopener">' + url + '</a>';
  });
}

// ============================================================
// Build entire page
// ============================================================
function buildPage() {
  var root = document.getElementById('gen-root');
  root.innerHTML = '';

  // 1. Name section (from name.json)
  root.appendChild(buildNameSection());

  // 2. Board section
  root.appendChild(buildBoardSection());

  // 3. Pin color legend
  root.appendChild(buildPinLegend());

  // 4. SPI Bus sections (rendered inside board section update)
  var spiDiv = document.createElement('div');
  spiDiv.id = 'spi-sections';
  root.appendChild(spiDiv);

  // 5. Display section
  root.appendChild(buildSingleSelectSection(gData.display, 'display-section', 'dsp-sel', onDisplayChange));

  // 6. Audio section
  root.appendChild(buildSingleSelectSection(gData.audio, 'audio-section', 'aud-sel', onAudioChange));

  // 7. Input section
  root.appendChild(buildCheckboxGroupSection(gData.input, 'input-section'));

  // 8. Peripherals section
  root.appendChild(buildCheckboxGroupSection(gData.peripherals, 'peripherals-section'));

  // 9. User Defaults section (includes Locale and Time Zone)
  root.appendChild(buildDefaultsSection());

  // 10. Extra custom defines section
  root.appendChild(buildExtraDefinesSection());

  // Show buttons
  document.getElementById('buttons-area2').style.display = 'flex';

  // Wire up preview debounce (document-level, one-time setup)
  document.addEventListener('input', schedulePreviewUpdate);
  document.addEventListener('change', schedulePreviewUpdate);
  // Reveal preview section
  document.getElementById('preview-section').classList.remove('hidden');
}

// ============================================================
// Name section
// ============================================================
function buildNameSection() {
  var meta = getSectionMeta(gData.name);
  var item = meta.items[0];

  var sec = makeSection(meta.title || 'Firmware Name', meta.info);
  var row = document.createElement('div');
  row.className = 'ctrl-row';

  if (item && item.defines) {
    item.defines.forEach(function(def) {
      var cell = makeDefineCell(def, 'name', null, null);
      row.appendChild(cell);
    });
  }
  sec.appendChild(row);
  return sec;
}

// ============================================================
// Board section
// ============================================================
function buildBoardSection() {
  var sec = makeSection('Board');
  sec.id = 'board-section';

  // Dropdown row
  var selRow = document.createElement('div');
  selRow.className = 'board-select-row';
  var lbl = document.createElement('label');
  lbl.htmlFor = 'board-sel';
  lbl.textContent = 'Select Board:';
  var sel = document.createElement('select');
  sel.id = 'board-sel';
  sel.className = 'wide';
  gData.boards.forEach(function(b, idx) {
    var opt = document.createElement('option');
    opt.value = idx;
    opt.textContent = b.name;
    sel.appendChild(opt);
  });
  sel.addEventListener('change', function() { onBoardChange(false); });
  selRow.appendChild(lbl);
  selRow.appendChild(sel);
  sec.appendChild(selRow);

  // Image area
  var imgArea = document.createElement('div');
  imgArea.id = 'board-image-area';
  sec.appendChild(imgArea);

  return sec;
}

// ============================================================
// Pin color legend — non-bordered help area between Board and SPI
// ============================================================
function buildPinLegend() {
  var wrap = document.createElement('div');
  wrap.className = 'pin-legend';
  wrap.id = 'pin-legend';

  var title = document.createElement('div');
  title.className = 'pin-legend-title';
  title.textContent = 'PIN COLOR LEGEND';
  wrap.appendChild(title);

  var items = [
    { label: 'Safe',   cls: 'pin-safe' },
    { label: 'Valid',  cls: 'pin-ok' },
    { label: 'EN/RST', cls: 'pin-en' },
    { label: 'I2C Reuse', cls: 'pin-i2c' },
    { label: 'Duplicate', cls: 'pin-dup' },
    { label: 'Invalid', cls: 'pin-error' }
  ];

  // Two rows of three
  for (var ri = 0; ri < 2; ri++) {
    var row = document.createElement('div');
    row.className = 'pin-legend-row';
    for (var ci = 0; ci < 3; ci++) {
      var idx = ri * 3 + ci;
      if (idx >= items.length) break;
      var it = items[idx];
      var box = document.createElement('span');
      box.className = 'pin-legend-box ' + it.cls;
      box.textContent = it.label;
      row.appendChild(box);
    }
    wrap.appendChild(row);
  }

  return wrap;
}

function onBoardChange() {
  var sel = document.getElementById('board-sel');
  var boardEntry = gData.boards[parseInt(sel.value)];
  if (!boardEntry) return;

  fetchJSON(boardEntry.file).then(function(data) {
    gData.boardData = data[0]; // boards json files are arrays with one item
    updateBoardImageArea();
    updateSPISections();
    validateAllPins();
    updateAllResetButtons();
    updatePreview();
  }).catch(function(err) {
    console.error('Board file error:', err);
  });
}

function updateBoardImageArea() {
  var bd = gData.boardData;
  if (!bd) return;
  var area = document.getElementById('board-image-area');
  area.innerHTML = '';

  if (bd.info) {
    var info = document.createElement('div');
    info.className = 'section-info';
    info.textContent = bd.info;
    area.appendChild(info);
  }

  if (bd.image) {
    var img = document.createElement('img');
    img.id = 'board-thumb';
    img.src = bd.image;
    img.alt = bd.name || 'Board';
    img.title = 'Click to zoom';
    img.onclick = function() { openZoom(bd.image); };
    area.appendChild(img);

    var hint = document.createElement('div');
    hint.className = 'board-click-hint';
    hint.textContent = 'click to zoom';
    area.appendChild(hint);
  }

  if (bd.url) {
    var link = document.createElement('a');
    link.href = bd.url;
    link.target = '_blank';
    link.rel = 'noopener';
    link.className = 'board-url-link';
    link.textContent = 'click here for more information';
    area.appendChild(link);
  }

  // Reset-all-pins ↶ link (only if board has default_pins)
  if (bd.default_pins && bd.default_pins[0]) {
    var rstDiv = document.createElement('div');
    rstDiv.style.marginTop = '8px';
    rstDiv.style.textAlign = 'center';

    var rstBtn = document.createElement('span');
    rstBtn.className = 'bulk-reset-btn';
    rstBtn.textContent = '↶';
    rstBtn.title = 'Reset all to board defaults';
    rstBtn.onclick = function(e) {
      e.preventDefault();
      e.stopPropagation();
      resetPins();
    };

    var rstText = document.createElement('div');
    rstText.style.color = '#cc6666';
    rstText.style.fontSize = '1.1rem';
    rstText.style.marginTop = '4px';
    rstText.textContent = 'reset all to board defaults';

    rstDiv.appendChild(rstBtn);
    rstDiv.appendChild(rstText);
    area.appendChild(rstDiv);
  }
}

function updateSPISections() {
  var bd = gData.boardData;
  var container = document.getElementById('spi-sections');

  // Snapshot current SPI state before clearing (preserve across board changes)
  var spiState = {};
  container.querySelectorAll('[id^="spi-chk-"]').forEach(function(chk) {
    var bus = chk.dataset.spiBus;
    spiState[bus] = { checked: chk.checked, pins: {} };
  });
  container.querySelectorAll('[id^="spi-pins-"] input[data-type="pin"]').forEach(function(inp) {
    var bus = inp.closest('[id^="spi-pins-"]');
    if (!bus) return;
    var busKey = bus.id.replace('spi-pins-', '');
    if (spiState[busKey]) {
      spiState[busKey].pins[inp.dataset.pin || inp.dataset.define] = inp.value;
    }
  });

  container.innerHTML = '';
  if (!bd || !bd.spi) return;

  var spiItems = gData.spi.slice(1); // skip header string
  spiItems.forEach(function(busObj) {
    var busKey = busObj.spi; // "A", "B", etc.
    if (!bd.spi.includes(busKey)) return; // skip if board doesn't have this bus

    var sec = makeSection(busObj.name);
    sec.id = 'spi-section-' + busKey;

    // Checkbox header — SPI buses are always treated as optional
    var header = document.createElement('div');
    header.className = 'check-item-header';
    header.style.marginBottom = '8px';
    var chk = document.createElement('input');
    chk.type = 'checkbox';
    chk.id = 'spi-chk-' + busKey;
    chk.dataset.spiBus = busKey;
    var nameLbl = document.createElement('span');
    nameLbl.className = 'item-name';
    nameLbl.textContent = busObj.name;
    header.appendChild(chk);
    header.appendChild(nameLbl);
    sec.appendChild(header);

    // Pins wrapper — hidden until checkbox is checked
    var pinsWrap = document.createElement('div');
    pinsWrap.id = 'spi-pins-' + busKey;
    pinsWrap.className = 'hidden';

    if (busObj.info) {
      var infoDiv = document.createElement('div');
      infoDiv.className = 'section-info';
      infoDiv.textContent = busObj.info;
      pinsWrap.appendChild(infoDiv);
    }

    var row = document.createElement('div');
    row.className = 'ctrl-row';
    busObj.pins.forEach(function(pinName) {
      var cell = makePinCell(pinName, null);
      row.appendChild(cell);
    });
    pinsWrap.appendChild(row);
    sec.appendChild(pinsWrap);
    container.appendChild(sec);

    // Toggle pin visibility on checkbox change
    chk.addEventListener('change', function() {
      if (chk.checked) {
        pinsWrap.classList.remove('hidden');
        nameLbl.style.color = '#fff';
      } else {
        pinsWrap.classList.add('hidden');
        nameLbl.style.color = '';
      }
      validateAllPins();
      updateAllResetButtons();
    });
    header.addEventListener('click', function(e) {
      if (e.target === chk) return;
      chk.checked = !chk.checked;
      chk.dispatchEvent(new Event('change'));
    });

    // Restore saved state for this bus (preserves across board changes)
    var saved = spiState[busKey];
    if (saved) {
      if (saved.checked) {
        chk.checked = true;
        pinsWrap.classList.remove('hidden');
        nameLbl.style.color = '#fff';
      }
      Object.keys(saved.pins).forEach(function(pn) {
        var pinEl = pinsWrap.querySelector('[data-pin="' + pn + '"]');
        if (pinEl) pinEl.value = saved.pins[pn];
      });
    }
  });

  // Update SPI selectors in display/audio/input/peripherals sections
  updateAllSpiSelectors();
}

function applyDefaultPins() {
  var bd = gData.boardData;
  if (!bd || !bd.default_pins || !bd.default_pins[0]) return;
  var defaults = bd.default_pins[0];
  Object.keys(defaults).forEach(function(pinName) {
    var els = document.querySelectorAll('[data-pin="' + pinName + '"]');
    els.forEach(function(el) {
      el.value = defaults[pinName];
    });
  });
}

// ============================================================
// Apply board default_selects (non-pin settings: sections, dropdowns, define values)
// Called before applyDefaultPins so section rebuilds happen before pin fill
// ============================================================
function applyDefaultSelects() {
  var bd = gData.boardData;
  if (!bd) return;

  // Process explicit default_selects if present
  if (bd.default_selects && bd.default_selects.length) {
    var selects = bd.default_selects;

    // First pass: strings — enable sections, select dropdown items
    selects.forEach(function(item) {
      if (typeof item === 'string') applyDefaultSelectString(item);
    });

    // Second pass: objects — set individual define values (after sections are built)
    selects.forEach(function(item) {
      if (typeof item === 'object' && item !== null && !Array.isArray(item)) {
        applyDefaultSelectObject(item);
      }
    });
  }

  // Auto-enable input/peripheral items whose pins appear in default_pins
  if (bd.default_pins && bd.default_pins[0]) {
    autoEnablePinnedItems();
  }
}

function applyDefaultSelectString(str) {
  // SPI bus checkbox (single uppercase letter, e.g. "A", "B")
  if (/^[A-Z]$/.test(str)) {
    var spiChk = document.getElementById('spi-chk-' + str);
    if (spiChk) {
      spiChk.checked = true;
      var pinsWrap = document.getElementById('spi-pins-' + str);
      if (pinsWrap) pinsWrap.classList.remove('hidden');
      var nl = spiChk.nextElementSibling;
      if (nl && nl.tagName === 'SPAN') nl.style.color = '#fff';
    }
    return;
  }

  // Display item (matches by item.define)
  var dspItems = gData.display.slice(1);
  var dspIdx = dspItems.findIndex(function(d) { return d.define === str; });
  if (dspIdx >= 0) {
    var dspSel = document.getElementById('dsp-sel');
    if (dspSel && parseInt(dspSel.value) !== dspIdx) {
      dspSel.value = dspIdx;
      onDisplayChange();
    }
    return;
  }

  // Audio item (matches by item.define)
  var audItems = gData.audio.slice(1);
  var audIdx = audItems.findIndex(function(a) { return a.define === str; });
  if (audIdx >= 0) {
    var audSel = document.getElementById('aud-sel');
    if (audSel && parseInt(audSel.value) !== audIdx) {
      audSel.value = audIdx;
      onAudioChange();
    }
    return;
  }

  // Input item (matches by item.define or item.name)
  var inpItems = gData.input.slice(1);
  var inpIdx = inpItems.findIndex(function(item) { return item.define === str || item.name === str; });
  if (inpIdx >= 0) {
    revealCheckItem('input-section-item-' + inpIdx);
    return;
  }

  // Peripheral item (matches by item.define or item.name)
  var perItems = gData.peripherals.slice(1);
  var perIdx = perItems.findIndex(function(item) { return item.define === str || item.name === str; });
  if (perIdx >= 0) {
    revealCheckItem('peripherals-section-item-' + perIdx);
    return;
  }
}

function revealCheckItem(itemId) {
  var chk = document.getElementById(itemId + '-chk');
  if (!chk || chk.checked) return;
  chk.checked = true;
  var body = document.getElementById(itemId + '-body');
  if (body && body.children.length > 0) {
    body.classList.remove('hidden');
    var nl = chk.nextElementSibling;
    if (nl && nl.tagName === 'SPAN') nl.style.color = '#fff';
  }
}

// Auto-enable input/peripheral items whose pins appear in board default_pins
function autoEnablePinnedItems() {
  var bd = gData.boardData;
  if (!bd || !bd.default_pins || !bd.default_pins[0]) return;
  var defaults = bd.default_pins[0];

  Object.keys(defaults).forEach(function(pinName) {
    // Check input items
    gData.input.slice(1).forEach(function(item, idx) {
      var pins = item.pins;
      if (!pins) return;
      if (typeof pins === 'string') pins = [pins];
      if (pins.indexOf(pinName) >= 0) {
        revealCheckItem('input-section-item-' + idx);
      }
    });

    // Check peripheral items
    gData.peripherals.slice(1).forEach(function(item, idx) {
      var pins = item.pins;
      if (!pins) return;
      if (typeof pins === 'string') pins = [pins];
      if (pins.indexOf(pinName) >= 0) {
        revealCheckItem('peripherals-section-item-' + idx);
      }
    });
  });
}

function applyDefaultSelectObject(obj) {
  Object.keys(obj).forEach(function(defName) {
    var val = obj[defName];

    // Optional sub-define checkbox (data-opt-for)
    var optChk = document.querySelector('input[type="checkbox"][data-opt-for="' + defName + '"]');
    if (optChk) {
      var enable = (val !== false && val !== 'false');
      if (enable !== optChk.checked) {
        optChk.checked = enable;
        var ctrlCell = optChk.closest('.ctrl-cell');
        if (ctrlCell) {
          var innerDiv = ctrlCell.querySelector('div[id]');
          var lbl = ctrlCell.querySelector('.opt-label');
          if (innerDiv) innerDiv.classList[enable ? 'remove' : 'add']('hidden');
          if (lbl) lbl.classList[enable ? 'add' : 'remove']('checked');
        }
      }
      if (!enable) return; // explicitly disabled — nothing more to do
      // Fall through to set the control value below
    }

    // Default item checkbox (data-def-item)
    var defChk = document.querySelector('input[type="checkbox"][data-def-item="' + defName + '"]');
    if (defChk) {
      var enable2 = (val !== false && val !== 'false');
      if (enable2 !== defChk.checked) {
        defChk.checked = enable2;
        var defItem = defChk.closest('.default-item');
        if (defItem) {
          var defCtrl = defItem.querySelector('.default-item-ctrl');
          if (defCtrl) defCtrl.classList[enable2 ? 'remove' : 'add']('hidden');
          var defLbl = defChk.nextElementSibling;
          if (defLbl) defLbl.style.color = enable2 ? '#fff' : '';
        }
      }
      if (!enable2) return; // explicitly disabled — nothing more to do
      // Fall through to set the control value below
    }

    // Generic control: find all elements with matching data-define
    var controls = document.querySelectorAll('[data-define="' + defName + '"]');
    controls.forEach(function(ctrl) {
      if (ctrl.tagName === 'SELECT') {
        ctrl.value = String(val);
      } else if (ctrl.type === 'radio') {
        ctrl.checked = (ctrl.value === String(val));
      } else if (ctrl.type === 'checkbox') {
        ctrl.checked = (val === true || val === 'true');
      } else {
        ctrl.value = val;
      }
    });
  });
}

// ============================================================
// Reset buttons (↶) — show for pins+selects that have board defaults
// ============================================================
function updateAllResetButtons() {
  var bd = gData.boardData;
  var pinDefaults = (bd && bd.default_pins && bd.default_pins[0]) ? bd.default_pins[0] : null;
  var selectDefaults = getSelectDefaults();

  // 1. Pin inputs — show ↶ if pin name is in default_pins
  var allPinInputs = document.querySelectorAll('input[data-type="pin"]');
  allPinInputs.forEach(function(inp) {
    var pinName = inp.dataset.pin || inp.dataset.define;
    var rstBtn = findRstBtn(inp);
    if (!pinName || !rstBtn) return;

    if (pinDefaults && pinDefaults.hasOwnProperty(pinName)) {
      rstBtn.classList.remove('hidden');
      rstBtn.onclick = function(e) {
        e.preventDefault(); e.stopPropagation();
        inp.value = pinDefaults[pinName];
        validateAllPins();
        updatePreview();
      };
    } else {
      rstBtn.classList.add('hidden');
      rstBtn.onclick = null;
    }
  });

  // 2. All controls from default_selects object (pins + non-pins)
  if (selectDefaults) {
    Object.keys(selectDefaults).forEach(function(defName) {
      var val = selectDefaults[defName];
      var controls = document.querySelectorAll('[data-define="' + defName + '"]');
      controls.forEach(function(ctrl) {
        var rstBtn = findRstBtn(ctrl);
        if (!rstBtn) return;
        rstBtn.classList.remove('hidden');
        rstBtn.onclick = function(e) {
          e.preventDefault(); e.stopPropagation();
          setSingleControlValue(defName, val);
          validateAllPins();
          updatePreview();
        };
      });
    });
  }
}

// Extract the merged object from default_selects array
function getSelectDefaults() {
  var bd = gData.boardData;
  if (!bd || !bd.default_selects || !bd.default_selects.length) return null;
  var merged = {};
  bd.default_selects.forEach(function(item) {
    if (typeof item === 'object' && item !== null && !Array.isArray(item)) {
      Object.assign(merged, item);
    }
  });
  return Object.keys(merged).length ? merged : null;
}

// Find the ↶ button associated with a control element
function findRstBtn(ctrl) {
  // Try parent node first (flex row wrapper for default items & define cells)
  var parent = ctrl.parentNode;
  if (parent) {
    var btn = parent.querySelector('.pin-reset-btn');
    if (btn) return btn;
  }
  // Fall back: search .ctrl-cell ancestor
  var cell = ctrl.closest('.ctrl-cell');
  if (cell) {
    var btn2 = cell.querySelector('.pin-reset-btn');
    if (btn2) return btn2;
  }
  return null;
}

// Set a single control's value (used by individual ↶ buttons)
function setSingleControlValue(defName, val) {
  // Optional sub-define checkbox
  var optChk = document.querySelector('input[type="checkbox"][data-opt-for="' + defName + '"]');
  if (optChk && !optChk.checked) {
    optChk.checked = true;
    var ctrlCell = optChk.closest('.ctrl-cell');
    if (ctrlCell) {
      var innerDiv = ctrlCell.querySelector('div[id]');
      var lbl = ctrlCell.querySelector('.opt-label');
      if (innerDiv) innerDiv.classList.remove('hidden');
      if (lbl) lbl.classList.add('checked');
    }
  }
  // Default item checkbox
  var defChk = document.querySelector('input[type="checkbox"][data-def-item="' + defName + '"]');
  if (defChk && !defChk.checked) {
    defChk.checked = true;
    var defItem = defChk.closest('.default-item');
    if (defItem) {
      var defCtrl = defItem.querySelector('.default-item-ctrl');
      if (defCtrl) defCtrl.classList.remove('hidden');
      var defLbl = defChk.nextElementSibling;
      if (defLbl) defLbl.style.color = '#fff';
    }
  }
  // Set the control value
  var controls = document.querySelectorAll('[data-define="' + defName + '"]');
  controls.forEach(function(ctrl) {
    if (ctrl.tagName === 'SELECT') {
      ctrl.value = String(val);
    } else if (ctrl.type === 'radio') {
      ctrl.checked = (ctrl.value === String(val));
    } else if (ctrl.type === 'checkbox') {
      ctrl.checked = (val === true || val === 'true');
    } else {
      ctrl.value = val;
    }
  });
}

// ============================================================
// SPI selector helpers
// ============================================================
function getAvailableBuses() {
  var bd = gData.boardData;
  if (!bd || !bd.spi) return ['A'];
  return bd.spi;
}

function makeSpiSelect(defName, defObj, scopeId) {
  var id = uid('spi');
  var buses = getAvailableBuses();
  // Default: last listed bus (usually 'B' if available, else 'A')
  var defaultBus = buses[buses.length - 1];
  var sel = document.createElement('select');
  sel.id = id;
  sel.className = 'narrow';
  sel.dataset.define = defName;
  sel.dataset.type = 'spi';
  sel.dataset.scope = scopeId || '';
  buses.forEach(function(b) {
    var opt = document.createElement('option');
    opt.value = b;
    opt.textContent = b + (b === defaultBus ? ' (default)' : '');
    if (b === defaultBus) opt.selected = true;
    sel.appendChild(opt);
  });
  return sel;
}

function updateAllSpiSelectors() {
  var buses = getAvailableBuses();
  var defaultBus = buses[buses.length - 1];
  var sels = document.querySelectorAll('select[data-type="spi"]');
  sels.forEach(function(sel) {
    var current = sel.value;
    sel.innerHTML = '';
    buses.forEach(function(b) {
      var opt = document.createElement('option');
      opt.value = b;
      opt.textContent = b;
      sel.appendChild(opt);
    });
    // Try to preserve current value; fall back to default
    if (buses.includes(current)) {
      sel.value = current;
    } else {
      sel.value = defaultBus;
    }
  });
}

// ============================================================
// Build a single-select section (Display / Audio)
// ============================================================
function buildSingleSelectSection(dataArr, secId, selId, onChangeFn) {
  var meta = getSectionMeta(dataArr);
  var items = meta.items;

  var sec = makeSection(meta.title || 'Section', meta.info);
  sec.id = secId;

  var selRow = document.createElement('div');
  selRow.className = 'select-row';
  var sel = document.createElement('select');
  sel.id = selId;
  sel.className = 'wide';
  items.forEach(function(item, idx) {
    var opt = document.createElement('option');
    opt.value = idx;
    opt.textContent = item.name;
    sel.appendChild(opt);
  });
  sel.addEventListener('change', onChangeFn);
  selRow.appendChild(sel);
  sec.appendChild(selRow);

  var detailDiv = document.createElement('div');
  detailDiv.id = secId + '-detail';
  sec.appendChild(detailDiv);

  // Render initial selection
  setTimeout(onChangeFn, 0);

  return sec;
}

function onDisplayChange() {
  var sel = document.getElementById('dsp-sel');
  if (!sel) return;
  var items = gData.display.slice(1);
  var item = items[parseInt(sel.value)];
  if (!item) return;

  var detail = document.getElementById('display-section-detail');
  detail.innerHTML = '';

  if (item.info) {
    var infoDiv = document.createElement('div');
    infoDiv.className = 'section-info';
    infoDiv.textContent = item.info;
    detail.appendChild(infoDiv);
  }
  renderItemDetail(detail, item, 'display');
  updateAllResetButtons();
}

function onAudioChange() {
  var sel = document.getElementById('aud-sel');
  if (!sel) return;
  var items = gData.audio.slice(1);
  var item = items[parseInt(sel.value)];
  if (!item) return;

  var detail = document.getElementById('audio-section-detail');
  detail.innerHTML = '';

  if (item.info) {
    var infoDiv = document.createElement('div');
    infoDiv.className = 'section-info';
    infoDiv.textContent = item.info;
    detail.appendChild(infoDiv);
  }
  renderItemDetail(detail, item, 'audio');
  updateAllResetButtons();
}

// ============================================================
// Render item detail (pins + defines) - used by display, audio, check-items
// ============================================================
function renderItemDetail(container, item, scopeId) {
  if (!item) return;

  // Pins row (mandatory)
  var pins = item.pins;
  if (pins) {
    if (typeof pins === 'string') pins = [pins];
    var pinRow = document.createElement('div');
    pinRow.className = 'ctrl-row';
    pins.forEach(function(pinName) {
      pinRow.appendChild(makePinCell(pinName, scopeId));
    });
    container.appendChild(pinRow);
  }

  // Defines (optional or mandatory based on "optional" flag)
  if (item.defines) {
    renderDefines(container, item.defines, scopeId);
  }
}

function renderDefines(container, defines, scopeId) {
  // Group: mandatory (no checkbox) in one flex row, optional (checkbox) mixed in
  // We build rows: each define is a cell, up to 3 per row
  var row = document.createElement('div');
  row.className = 'ctrl-row';
  var cellCount = 0;

  defines.forEach(function(def) {
    // flag with optional:false -> no UI, always output
    if (def.type === 'flag' && def.optional === false) {
      // no UI cell needed
      return;
    }

    var cell = makeDefineCell(def, scopeId, null, null);
    row.appendChild(cell);
    cellCount++;

    if (cellCount % 3 === 0) {
      container.appendChild(row);
      row = document.createElement('div');
      row.className = 'ctrl-row';
    }
  });
  if (row.children.length > 0) container.appendChild(row);
}

// ============================================================
// Make a define cell (with or without optional checkbox)
// ============================================================
function makeDefineCell(def, scopeId, extraClass, parentCheckId) {
  var cell = document.createElement('div');
  cell.className = 'ctrl-cell' + (extraClass ? ' ' + extraClass : '');

  var isOptional = (def.optional !== false); // default: optional
  if (def.type === 'spi') isOptional = false; // spi selectors are always mandatory
  var defName = def.define;

  if (isOptional) {
    // Checkbox label row
    var lbl = document.createElement('label');
    lbl.className = 'opt-label';
    var chk = document.createElement('input');
    chk.type = 'checkbox';
    chk.dataset.optFor = defName;
    var nameSpan = document.createElement('span');
    nameSpan.textContent = defName;
    lbl.appendChild(chk);
    lbl.appendChild(nameSpan);
    if (def.comment) {
      var helpBtn = document.createElement('span');
      helpBtn.className = 'help-btn';
      helpBtn.textContent = '?';
      helpBtn.title = 'Help';
      helpBtn.onclick = function(e) { e.preventDefault(); e.stopPropagation(); showHelp(defName, def.comment); };
      lbl.appendChild(helpBtn);
    }
    cell.appendChild(lbl);

    // Control div (hidden until checkbox checked)
    var ctrlDiv = document.createElement('div');
    ctrlDiv.id = uid('ctrl');
    ctrlDiv.className = 'hidden';
    // Flex row for control + ↶
    var ctrlRow = document.createElement('div');
    ctrlRow.style.display = 'flex';
    ctrlRow.style.alignItems = 'center';
    ctrlRow.style.gap = '2px';
    var ctrlWidget = makeControl(def, scopeId);
    ctrlRow.appendChild(ctrlWidget);
    var rstBtn3 = document.createElement('span');
    rstBtn3.className = 'pin-reset-btn hidden';
    rstBtn3.textContent = '↶';
    rstBtn3.title = 'Reset to default';
    ctrlRow.appendChild(rstBtn3);
    ctrlDiv.appendChild(ctrlRow);
    cell.appendChild(ctrlDiv);

    chk.addEventListener('change', function() {
      if (chk.checked) {
        ctrlDiv.classList.remove('hidden');
        lbl.classList.add('checked');
      } else {
        ctrlDiv.classList.add('hidden');
        lbl.classList.remove('checked');
      }
      validateAllPins();
    });
  } else {
    // Mandatory - just label + control
    var lblDiv = document.createElement('div');
    lblDiv.className = 'ctrl-label mandatory';
    lblDiv.textContent = defName;
    if (def.comment) {
      var helpBtn2 = document.createElement('span');
      helpBtn2.className = 'help-btn';
      helpBtn2.textContent = '?';
      helpBtn2.onclick = function(e) { e.stopPropagation(); showHelp(defName, def.comment); };
      lblDiv.appendChild(helpBtn2);
    }
    cell.appendChild(lblDiv);
    // Flex row for control + ↶
    var ctrlRow2 = document.createElement('div');
    ctrlRow2.style.display = 'flex';
    ctrlRow2.style.alignItems = 'center';
    ctrlRow2.style.gap = '2px';
    var ctrlWidget2 = makeControl(def, scopeId);
    ctrlRow2.appendChild(ctrlWidget2);
    var rstBtn4 = document.createElement('span');
    rstBtn4.className = 'pin-reset-btn hidden';
    rstBtn4.textContent = '↶';
    rstBtn4.title = 'Reset to default';
    ctrlRow2.appendChild(rstBtn4);
    cell.appendChild(ctrlRow2);
  }

  return cell;
}

// ============================================================
// Make a pin cell (always mandatory)
// ============================================================
function makePinCell(pinName, scopeId) {
  var cell = document.createElement('div');
  cell.className = 'ctrl-cell';

  var lbl = document.createElement('div');
  lbl.className = 'ctrl-label mandatory';
  lbl.textContent = pinName;
  cell.appendChild(lbl);

  // Row for input + ↶ reset button
  var row = document.createElement('div');
  row.style.display = 'flex';
  row.style.alignItems = 'center';
  row.style.gap = '2px';

  var inp = document.createElement('input');
  inp.type = 'number';
  inp.value = '';
  inp.dataset.pin = pinName;
  inp.dataset.scope = scopeId || '';
  inp.dataset.define = pinName;
  inp.dataset.type = 'pin';
  inp.dataset.defaultVal = ''; // top-level pins have no JSON default
  inp.addEventListener('input', function() { validateAllPins(); });
  row.appendChild(inp);

  var rstBtn = document.createElement('span');
  rstBtn.className = 'pin-reset-btn hidden';
  rstBtn.textContent = '↶';
  rstBtn.title = 'Reset to board default';
  row.appendChild(rstBtn);

  cell.appendChild(row);
  return cell;
}

// ============================================================
// Make the actual control widget for a define
// ============================================================
function makeControl(def, scopeId) {
  var type = def.type;
  var defVal = (def.default !== undefined) ? def.default : '';

  if (type === 'pin') {
    var inp = document.createElement('input');
    inp.type = 'number';
    inp.value = defVal !== '' ? String(defVal) : '';
    inp.dataset.define = def.define;
    inp.dataset.type = 'pin';
    inp.dataset.scope = scopeId || '';
    inp.dataset.pin = def.define;
    inp.dataset.defaultVal = defVal !== '' ? String(defVal) : ''; // JSON default for reset
    inp.addEventListener('input', function() { validateAllPins(); });
    return inp;

  } else if (type === 'numeric') {
    var inp2 = document.createElement('input');
    inp2.type = 'number';
    inp2.value = defVal !== '' ? defVal : 0;
    if (def.min !== undefined) inp2.min = def.min;
    if (def.max !== undefined) inp2.max = def.max;
    inp2.dataset.define = def.define;
    inp2.dataset.type = 'numeric';
    inp2.dataset.scope = scopeId || '';
    return inp2;

  } else if (type === 'text') {
    var inp3 = document.createElement('input');
    inp3.type = 'text';
    inp3.value = defVal !== '' ? defVal : '';
    inp3.className = 'wide-text';
    inp3.dataset.define = def.define;
    inp3.dataset.type = 'text';
    inp3.dataset.scope = scopeId || '';
    return inp3;

  } else if (type === 'boolean') {
    return makeBooleanControl(def, scopeId);

  } else if (type === 'enum' || type === 'char') {
    var sel = document.createElement('select');
    var opts = Array.isArray(def.options) ? def.options : [def.options];
    opts.forEach(function(o) {
      var opt = document.createElement('option');
      opt.value = o;
      opt.textContent = o + (o === defVal ? ' (default)' : '');
      if (o === defVal) opt.selected = true;
      sel.appendChild(opt);
    });
    sel.dataset.define = def.define;
    sel.dataset.type = type;
    sel.dataset.scope = scopeId || '';
    return sel;

  } else if (type === 'spi') {
    return makeSpiSelect(def.define, def, scopeId);

  } else if (type === 'flag') {
    // For optional flags: show a simple checkbox labelled "include"
    var chkDiv = document.createElement('div');
    chkDiv.style.textAlign = 'center';
    var flagChk = document.createElement('input');
    flagChk.type = 'checkbox';
    flagChk.dataset.define = def.define;
    flagChk.dataset.type = 'flag';
    flagChk.dataset.scope = scopeId || '';
    chkDiv.appendChild(flagChk);
    return chkDiv;

  } else if (type === 'define') {
    // bare define - show as read-only
    var span = document.createElement('span');
    span.textContent = def.define;
    span.style.color = '#888';
    span.style.fontSize = '12px';
    span.style.fontFamily = 'Courier New, monospace';
    return span;
  }

  // Fallback: text input
  var fallback = document.createElement('input');
  fallback.type = 'text';
  fallback.value = String(defVal);
  fallback.dataset.define = def.define;
  fallback.dataset.scope = scopeId || '';
  return fallback;
}

function makeBooleanControl(def, scopeId) {
  var wrap = document.createElement('div');
  wrap.className = 'radio-pair';
  var name = uid('bool');
  var defVal = (def.default !== undefined) ? def.default : true;
  var defStr = String(defVal);

  ['true', 'false'].forEach(function(v) {
    var lbl = document.createElement('label');
    var rad = document.createElement('input');
    rad.type = 'radio';
    rad.name = name;
    rad.value = v;
    rad.dataset.define = def.define;
    rad.dataset.type = 'boolean';
    rad.dataset.scope = scopeId || '';
    if (v === defStr) rad.checked = true;
    lbl.appendChild(rad);
    lbl.appendChild(document.createTextNode(' ' + v + (v === defStr ? ' (default)' : '')));
    wrap.appendChild(lbl);
  });
  return wrap;
}

// ============================================================
// Checkbox group sections (Input, Peripherals)
// ============================================================
function buildCheckboxGroupSection(dataArr, secId) {
  var meta = getSectionMeta(dataArr);
  var items = meta.items;

  var sec = makeSection(meta.title || 'Section', meta.info);
  sec.id = secId;

  items.forEach(function(item, idx) {
    var itemId = secId + '-item-' + idx;
    var div = document.createElement('div');
    div.className = 'check-item';
    div.id = itemId;

    // Header with checkbox
    var header = document.createElement('div');
    header.className = 'check-item-header';
    var chk = document.createElement('input');
    chk.type = 'checkbox';
    chk.id = itemId + '-chk';
    chk.dataset.itemId = itemId;

    var nameLbl = document.createElement('span');
    nameLbl.className = 'item-name';
    nameLbl.textContent = item.name;

    header.appendChild(chk);
    header.appendChild(nameLbl);

    // Help button if item has a top-level comment
    if (item.comment) {
      (function(def, cmt) {
        var hBtn = document.createElement('span');
        hBtn.className = 'help-btn';
        hBtn.textContent = '?';
        hBtn.style.marginLeft = '6px';
        hBtn.onclick = function(e) { e.stopPropagation(); showHelp(def, cmt); };
        header.appendChild(hBtn);
      })(item.define || item.name, item.comment);
    }

    div.appendChild(header);

    // Body (hidden by default)
    var body = document.createElement('div');
    body.className = 'check-item-body hidden';
    body.id = itemId + '-body';

    // Optional info note at top of body
    if (item.info) {
      var infoEl = document.createElement('div');
      infoEl.className = 'section-info';
      infoEl.textContent = item.info;
      body.appendChild(infoEl);
    }

    renderItemDetail(body, item, itemId);

    div.appendChild(body);
    sec.appendChild(div);

    // If body has no content, don't try to show/hide it — just update name color
    var hasBody = body.children.length > 0;
    header.addEventListener('click', function(e) {
      if (e.target === chk) return; // let checkbox handle itself
      chk.checked = !chk.checked;
      if (hasBody) {
        toggleCheckItemBody(chk, body);
      } else {
        var nl = chk.nextElementSibling;
        if (nl && nl.tagName === 'SPAN') nl.style.color = chk.checked ? '#fff' : '';
        validateAllPins();
      }
    });
    chk.addEventListener('change', function() {
      if (hasBody) {
        toggleCheckItemBody(chk, body);
      } else {
        var nl = chk.nextElementSibling;
        if (nl && nl.tagName === 'SPAN') nl.style.color = chk.checked ? '#fff' : '';
        validateAllPins();
      }
    });
  });

  return sec;
}

function toggleCheckItemBody(chk, body) {
  if (chk.checked) {
    body.classList.remove('hidden');
    var nameEl = chk.nextElementSibling;
    if (nameEl) nameEl.style.color = '#fff';
  } else {
    body.classList.add('hidden');
    var nameEl2 = chk.nextElementSibling;
    if (nameEl2) nameEl2.style.color = '';
  }
  validateAllPins();
  updateAllResetButtons();
}
// ============================================================

// User Defaults section
// ============================================================
function buildDefaultsSection() {
  var meta = getSectionMeta(gData.defaults);
  var items = meta.items;

  var sec = makeSection(meta.title || 'User Defaults', meta.info);
  sec.id = 'defaults-section';

  items.forEach(function(item) {
    // Locale select — render complex dropdown from gData.locale
    if (item.type === 'locale_select') {
      var locales = gData.locale.slice(1);
      var lDiv = document.createElement('div');
      lDiv.className = 'default-item';
      var lHdr = document.createElement('div');
      lHdr.className = 'default-item-header';
      var lChk = document.createElement('input');
      lChk.type = 'checkbox'; lChk.id = 'locale-chk'; lChk.dataset.defItem = item.define;
      var lLbl = document.createElement('span');
      lLbl.className = 'item-name'; lLbl.textContent = item.name;
      lHdr.appendChild(lChk); lHdr.appendChild(lLbl);
      lDiv.appendChild(lHdr);
      var lCtrl = document.createElement('div');
      lCtrl.className = 'default-item-ctrl hidden'; lCtrl.id = 'locale-ctrl';
      lCtrl.style.marginTop = '10px'; lCtrl.style.textAlign = 'center';
      var lSel = document.createElement('select');
      lSel.id = 'locale-sel'; lSel.className = 'wide';
      lSel.dataset.define = item.define;
      locales.forEach(function(loc) {
        var opt = document.createElement('option');
        opt.value = loc.locale_code;
        var parts = [loc.locale_code];
        if (loc.locale) parts.push(loc.locale);
        if (loc.locale_en) parts.push(loc.locale_en);
        opt.textContent = parts.join(' - ');
        lSel.appendChild(opt);
      });
      lCtrl.appendChild(lSel); lDiv.appendChild(lCtrl);
      lChk.addEventListener('change', function() {
        if (lChk.checked) { lCtrl.classList.remove('hidden'); lLbl.style.color = '#fff'; }
        else { lCtrl.classList.add('hidden'); lLbl.style.color = ''; }
      });
      lHdr.addEventListener('click', function(e) { if (e.target === lChk) return; lChk.checked = !lChk.checked; lChk.dispatchEvent(new Event('change')); });
      sec.appendChild(lDiv);
      return;
    }

    // Timezone select — render complex dropdown from gData.timezones
    if (item.type === 'timezone_select') {
      var tzKeys = Object.keys(gData.timezones);
      var tDiv = document.createElement('div');
      tDiv.className = 'default-item';
      var tHdr = document.createElement('div');
      tHdr.className = 'default-item-header';
      var tChk = document.createElement('input');
      tChk.type = 'checkbox'; tChk.id = 'tz-chk'; tChk.dataset.defItem = item.define;
      var tLbl = document.createElement('span');
      tLbl.className = 'item-name'; tLbl.textContent = item.name;
      tHdr.appendChild(tChk); tHdr.appendChild(tLbl);
      tDiv.appendChild(tHdr);
      var tCtrl = document.createElement('div');
      tCtrl.className = 'default-item-ctrl hidden'; tCtrl.id = 'tz-ctrl';
      tCtrl.style.marginTop = '10px'; tCtrl.style.textAlign = 'center';
      var tSel = document.createElement('select');
      tSel.id = 'tz-sel'; tSel.className = 'wide';
      tSel.dataset.define = item.define;
      tzKeys.forEach(function(tz) {
        var opt = document.createElement('option');
        opt.value = tz; opt.textContent = tz;
        tSel.appendChild(opt);
      });
      tCtrl.appendChild(tSel); tDiv.appendChild(tCtrl);
      tChk.addEventListener('change', function() {
        if (tChk.checked) { tCtrl.classList.remove('hidden'); tLbl.style.color = '#fff'; }
        else { tCtrl.classList.add('hidden'); tLbl.style.color = ''; }
      });
      tHdr.addEventListener('click', function(e) { if (e.target === tChk) return; tChk.checked = !tChk.checked; tChk.dispatchEvent(new Event('change')); });
      sec.appendChild(tDiv);
      return;
    }

    var div = document.createElement('div');
    div.className = 'default-item';

    var header = document.createElement('div');
    header.className = 'default-item-header';
    var chk = document.createElement('input');
    chk.type = 'checkbox';
    chk.id = uid('def');
    chk.dataset.defItem = item.define;

    var lbl = document.createElement('span');
    lbl.className = 'item-name';
    lbl.textContent = item.name;
    header.appendChild(chk);
    header.appendChild(lbl);

    // Help button if comment
    if (item.comment) {
      var helpBtn = document.createElement('span');
      helpBtn.className = 'help-btn';
      helpBtn.textContent = '?';
      helpBtn.onclick = function(e) { e.stopPropagation(); showHelp(item.define, item.comment); };
      helpBtn.style.marginLeft = '6px';
      header.appendChild(helpBtn);
    }

    div.appendChild(header);

    if (item.type !== 'flag') {
      // Non-flag items: create a collapsible control div with ↶
      var ctrlDiv = document.createElement('div');
      ctrlDiv.className = 'default-item-ctrl hidden';
      ctrlDiv.id = uid('defctrl');

      // Info hint text (revealed when checkbox is checked, like peripherals check-items)
      if (item.info) {
        var infoDiv = document.createElement('div');
        infoDiv.className = 'section-info';
        infoDiv.textContent = item.info;
        ctrlDiv.appendChild(infoDiv);
      }

      var defCtrlRow = document.createElement('div');
      defCtrlRow.style.display = 'flex';
      defCtrlRow.style.alignItems = 'center';
      defCtrlRow.style.justifyContent = 'center';
      defCtrlRow.style.gap = '2px';
      defCtrlRow.appendChild(makeControl(item, 'defaults'));
      var defRstBtn = document.createElement('span');
      defRstBtn.className = 'pin-reset-btn hidden';
      defRstBtn.textContent = '↶';
      defRstBtn.title = 'Reset to default';
      defCtrlRow.appendChild(defRstBtn);
      ctrlDiv.appendChild(defCtrlRow);
      div.appendChild(ctrlDiv);

      chk.addEventListener('change', function() {
        ctrlDiv.classList[chk.checked ? 'remove' : 'add']('hidden');
        lbl.style.color = chk.checked ? '#fff' : '';
      });
    } else {
      // flag type: the top-level checkbox IS the toggle, no child control needed
      chk.addEventListener('change', function() {
        lbl.style.color = chk.checked ? '#fff' : '';
      });
    }

    sec.appendChild(div);

    header.addEventListener('click', function(e) {
      if (e.target === chk) return;
      chk.checked = !chk.checked;
      chk.dispatchEvent(new Event('change'));
    });
  });

  return sec;
}
// ============================================================

// Extra custom text section
// ============================================================
function buildExtraDefinesSection() {
  var sec = makeSection('Custom Extras for myoptions.h');
  sec.id = 'extra-defines-section';

  var header = document.createElement('div');
  header.className = 'default-item-header';
  var chk = document.createElement('input');
  chk.type = 'checkbox';
  chk.id = 'extra-defines-chk';
  var lbl = document.createElement('span');
  lbl.className = 'item-name';
  lbl.textContent = 'Extra';
  header.appendChild(chk);
  header.appendChild(lbl);

  var ctrlDiv = document.createElement('div');
  ctrlDiv.id = 'extra-defines-ctrl';
  ctrlDiv.className = 'hidden';
  ctrlDiv.style.marginTop = '10px';
  ctrlDiv.style.textAlign = 'center';

  var ta = document.createElement('textarea');
  ta.id = 'extra-defines-text';
  ta.className = 'extra-defines-text';
  ta.placeholder = '#define SOME_OPTION true';
  ctrlDiv.appendChild(ta);

  sec.appendChild(header);
  sec.appendChild(ctrlDiv);

  chk.addEventListener('change', function() {
    if (chk.checked) {
      ctrlDiv.classList.remove('hidden');
      lbl.style.color = '#fff';
    } else {
      ctrlDiv.classList.add('hidden');
      lbl.style.color = '';
    }
  });
  header.addEventListener('click', function(e) {
    if (e.target === chk) return;
    chk.checked = !chk.checked;
    chk.dispatchEvent(new Event('change'));
  });

  return sec;
}

// ============================================================
// Section factory
// ============================================================
function makeSection(title, info) {
  var sec = document.createElement('div');
  sec.className = 'gen-section';
  var titleDiv = document.createElement('div');
  titleDiv.className = 'gen-section-title';
  var span = document.createElement('span');
  span.textContent = title;
  titleDiv.appendChild(span);
  sec.appendChild(titleDiv);
  // Optional section-level info text (supports http/https links)
  if (info) {
    var infoDiv = document.createElement('div');
    infoDiv.className = 'section-info';
    var html = sectionInfoHTML(info);
    if (html !== info) {
      infoDiv.innerHTML = html;
    } else {
      infoDiv.textContent = info;
    }
    sec.appendChild(infoDiv);
  }
  return sec;
}

// ============================================================
// Pin validation
// ============================================================
function validateAllPins() {
  // Gather all visible pin inputs
  var allPinInputs = Array.from(document.querySelectorAll('input[data-type="pin"]'));
  var visiblePins = allPinInputs.filter(function(el) {
    return isElementVisible(el);
  });

  // Clear ALL pin classes
  allPinInputs.forEach(function(el) {
      el.classList.remove('pin-error', 'pin-ok', 'pin-dup', 'pin-i2c', 'pin-en', 'pin-safe');
  });

  var bd = gData.boardData;
  var validPins = bd ? bd.valid_pins : null;
  var safePins = bd ? bd.safe_pins : null;

  // Map of value -> [{el, define}] for duplicate detection
  var pinValueMap = {};
  visiblePins.forEach(function(el) {
    var val = parseInt(el.value);
    if (isNaN(val)) return;
    if (!pinValueMap[val]) pinValueMap[val] = [];
    pinValueMap[val].push({
      el: el,
      define: (el.dataset.define || el.dataset.pin || '').toUpperCase()
    });
  });

  visiblePins.forEach(function(el) {
    var val = parseInt(el.value);
    var define = (el.dataset.define || el.dataset.pin || '').toUpperCase();

    // Blank / empty — no highlight (stays dark)
    if (el.value.trim() === '') return;

    // NaN — red (invalid number)
    if (isNaN(val)) { el.classList.add('pin-error'); return; }

    // 255 — no highlight (unused sentinel, stays dark)
    if (val === 255) return;

    // -1 — purple (EN pin)
    if (val === -1) { el.classList.add('pin-en'); return; }

    // Check valid_pins — red if not in list
    if (validPins && !validPins.includes(val)) {
      el.classList.add('pin-error');
      return;
    }

    // Check duplicates
    var group = pinValueMap[val];
    if (group && group.length > 1) {
      var isI2c = (define.indexOf('SDA') >= 0 || define.indexOf('SCL') >= 0 || define.indexOf('I2C') >= 0);
      if (isI2c) {
        el.classList.add('pin-i2c'); // orange — allowed but warn
      } else {
        el.classList.add('pin-dup'); // yellow — error
      }
      return;
    }

    // All good — blue if safe, green if valid
    if (safePins && safePins.includes(val)) {
      el.classList.add('pin-safe'); // recommended safe pin
    } else {
      el.classList.add('pin-ok');   // valid but requires caution
    }
  });
}

function isElementVisible(el) {
  var node = el;
  while (node && node !== document.body) {
    if (node.classList && node.classList.contains('hidden')) return false;
    node = node.parentNode;
  }
  return true;
}

// ============================================================
// Clear all sections — uncheck/deselect everything before reset
// ============================================================
function clearAllSections() {
  // SPI bus checkboxes
  document.querySelectorAll('input[type="checkbox"][data-spi-bus]').forEach(function(chk) {
    if (chk.checked) {
      chk.checked = false;
      var bus = chk.dataset.spiBus;
      var pinsWrap = document.getElementById('spi-pins-' + bus);
      if (pinsWrap) pinsWrap.classList.add('hidden');
      var nl = chk.nextElementSibling;
      if (nl && nl.tagName === 'SPAN') nl.style.color = '';
    }
  });

  // Display dropdown — reset to first item
  var dspSel = document.getElementById('dsp-sel');
  if (dspSel && parseInt(dspSel.value) !== 0) {
    dspSel.value = 0;
    onDisplayChange();
  }

  // Audio dropdown — reset to first item
  var audSel = document.getElementById('aud-sel');
  if (audSel && parseInt(audSel.value) !== 0) {
    audSel.value = 0;
    onAudioChange();
  }

  // Input item checkboxes
  document.querySelectorAll('#input-section input[type="checkbox"]').forEach(function(chk) {
    if (chk.checked) {
      chk.checked = false;
      var itemId = chk.dataset.itemId;
      var body = document.getElementById(itemId + '-body');
      if (body) body.classList.add('hidden');
      var nl = chk.nextElementSibling;
      if (nl && nl.tagName === 'SPAN') nl.style.color = '';
    }
  });

  // Peripheral item checkboxes
  document.querySelectorAll('#peripherals-section input[type="checkbox"]').forEach(function(chk) {
    if (chk.checked) {
      chk.checked = false;
      var itemId = chk.dataset.itemId;
      var body = document.getElementById(itemId + '-body');
      if (body) body.classList.add('hidden');
      var nl = chk.nextElementSibling;
      if (nl && nl.tagName === 'SPAN') nl.style.color = '';
    }
  });

  // Optional sub-define checkboxes
  document.querySelectorAll('input[type="checkbox"][data-opt-for]').forEach(function(chk) {
    if (chk.checked) {
      chk.checked = false;
      var ctrlCell = chk.closest('.ctrl-cell');
      if (ctrlCell) {
        var innerDiv = ctrlCell.querySelector('div[id]');
        var lbl = ctrlCell.querySelector('.opt-label');
        if (innerDiv) innerDiv.classList.add('hidden');
        if (lbl) lbl.classList.remove('checked');
      }
    }
  });

  // Default item checkboxes
  document.querySelectorAll('input[type="checkbox"][data-def-item]').forEach(function(chk) {
    if (chk.checked) {
      chk.checked = false;
      var defItem = chk.closest('.default-item');
      if (defItem) {
        var defCtrl = defItem.querySelector('.default-item-ctrl');
        if (defCtrl) defCtrl.classList.add('hidden');
        var defLbl = chk.nextElementSibling;
        if (defLbl) defLbl.style.color = '';
      }
    }
  });

  // Extra defines
  ['extra-defines-chk'].forEach(function(id) {
    var chk = document.getElementById(id);
    if (chk && chk.checked) chk.checked = false;
    var ctrl = document.getElementById(id.replace('-chk', '-ctrl'));
    if (ctrl) ctrl.classList.add('hidden');
    if (chk) {
      var nl = chk.nextElementSibling;
      if (nl && nl.tagName === 'SPAN') nl.style.color = '';
    }
  });
}

// ============================================================
// Reset pins button
// ============================================================
function resetPins() {
  // Step 0: Clear all sections (uncheck/deselect everything)
  clearAllSections();
  // Step 1: Reset all pin inputs to their stored JSON defaults (or blank)
  document.querySelectorAll('input[data-type="pin"]').forEach(function(el) {
    el.value = (el.dataset.defaultVal !== undefined) ? el.dataset.defaultVal : '';
  });
  // Step 2: Apply board default_selects (enables sections, sets dropdowns/values)
  applyDefaultSelects();
  // Step 3: Apply board-specific default_pins (overrides matching pins)
  applyDefaultPins();
  validateAllPins();
  updateAllResetButtons();
  updatePreview(); // immediate update on reset (no debounce)
  showAlert('info', 'Reset to board defaults.', 2000);
}

// ============================================================
// Pin Configuration Preview (live, debounced 2 s)
// ============================================================
var _previewTimer = null;

function buildPreviewText() {
  var bd = gData.boardData;
  if (!bd) return '';
  var boardName = bd.name || 'Unknown Board';

  var dspSel = document.getElementById('dsp-sel');
  var dspItems = gData.display.slice(1);
  var dspItem = dspSel ? dspItems[parseInt(dspSel.value)] : null;
  var dspName = dspItem ? dspItem.name : '';

  var audSel = document.getElementById('aud-sel');
  var audItems = gData.audio.slice(1);
  var audItem = audSel ? audItems[parseInt(audSel.value)] : null;
  var audName = audItem ? audItem.name : '';

  var spiAssignments = collectSpiAssignments();
  var pinDiagram = buildPinDiagram();

  var out = boardName + '\n';
  out += 'Display: ' + dspName + '\n';
  out += 'Audio Decoder: ' + audName + '\n';
  spiAssignments.forEach(function(a) {
    out += 'SPI Bus ' + a.bus + ': ' + a.device + '\n';
  });
  out += '\n';
  pinDiagram.forEach(function(line) {
    out += line + '\n';
  });
  return out;
}

function updatePreview() {
  var el = document.getElementById('preview-text');
  if (!el) return;
  el.textContent = buildPreviewText();
}

function schedulePreviewUpdate() {
  if (_previewTimer) clearTimeout(_previewTimer);
  _previewTimer = setTimeout(updatePreview, 2000);
}

// ============================================================
// Output generation: myoptions.h
// ============================================================
function generateOptionsH() {
  var bd = gData.boardData;
  var boardName = bd ? bd.name : 'Unknown Board';

  // Determine display name
  var dspSel = document.getElementById('dsp-sel');
  var dspItems = gData.display.slice(1);
  var dspItem = dspSel ? dspItems[parseInt(dspSel.value)] : null;
  var dspName = dspItem ? dspItem.name : '';

  // Determine audio name
  var audSel = document.getElementById('aud-sel');
  var audItems = gData.audio.slice(1);
  var audItem = audSel ? audItems[parseInt(audSel.value)] : null;
  var audName = audItem ? audItem.name : '';

  // SPI bus assignments
  var spiAssignments = collectSpiAssignments();

  // Build pin diagram
  var pinDiagram = buildPinDiagram();

  var out = '';
  out += '#ifndef myoptions_h\n';
  out += '#define myoptions_h\n';
  out += '\n';
  out += '/*        ************************************************************************      */\n';
  out += '/*        *        This file must be in the root folder of the sketch !!!        *      */\n';
  out += '/*        ************************************************************************      */\n';
  out += '/*        . . .  CHECK options.h for full options, examples, and overrides   . . .      */\n';
  out += '\n';

  // Summary comments
  out += '// ' + boardName + '\n';
  out += '// Display: ' + dspName + '\n';
  out += '// Audio Decoder: ' + audName + '\n';

  // SPI bus usage
  spiAssignments.forEach(function(a) {
    out += '// SPI Bus ' + a.bus + ': ' + a.device + '\n';
  });
  out += '//\n';

  // Pin diagram
  pinDiagram.forEach(function(line) {
    out += '// ' + line + '\n';
  });
  out += '\n';

  // --- Sections ---
  out += outputNameSection();
  out += outputSpiSection();
  out += outputSingleSelectSection(gData.display, 'display-section', 'dsp-sel', 'Display');
  out += outputSingleSelectSection(gData.audio, 'audio-section', 'aud-sel', 'Audio Decoder');
  out += outputCheckboxGroupSection(gData.input, 'input-section', 'Input');
  out += outputCheckboxGroupSection(gData.peripherals, 'peripherals-section', 'Peripherals');
  out += outputDefaultsSection();
  out += outputExtraDefinesSection();

  out += '\n#endif // myoptions_h\n';
  return out;
}

function sectionHeader(title) {
  return '\n/* --- ' + title + ' --- */\n';
}

function outputLine(define, value, comment, padWidth) {
  padWidth = padWidth || 21;
  var pad = Math.max(1, padWidth - define.length);
  var line = '#define ' + define;
  for (var j = 0; j < pad; j++) line += ' ';
  line += String(value);
  if (comment) {
    // Align /* to column 40 (1-indexed); minimum 1 space gap
    var commentPad = Math.max(1, 39 - line.length);
    for (var k = 0; k < commentPad; k++) line += ' ';
    line += '/* ' + commentText(comment) + ' */';
  }
  return line + '\n';
}

function outputFlagLine(define) {
  return '#define ' + define + '\n';
}

function outputNameSection() {
  var meta = getSectionMeta(gData.name);
  if (!meta.items.length) return '';
  var out = sectionHeader(meta.title || 'Firmware File & Board');
  var item = meta.items[0];
  if (item && item.defines) {
    item.defines.forEach(function(def) {
      var ctrl = findVisibleControl(def.define, null);
      if (!ctrl) return;
      var val = getControlValue(ctrl, def.type);
      out += outputLine(def.define, formatValue(val, def.type, def.define), def.comment || null);
    });
  }
  return out;
}

function outputSpiSection() {
  var bd = gData.boardData;
  if (!bd) return '';
  var spiItems = gData.spi.slice(1);
  var hasAny = false;
  var body = '';
  spiItems.forEach(function(busObj) {
    var busKey = busObj.spi;
    if (!bd.spi || !bd.spi.includes(busKey)) return;
    // Only output pins for SPI buses that the user has checked
    var chk = document.getElementById('spi-chk-' + busKey);
    if (!chk || !chk.checked) return;
    if (busObj.pins) {
      busObj.pins.forEach(function(pinName) {
        var el = document.querySelector('input[data-pin="' + pinName + '"]');
        if (el && isElementVisible(el)) {
          body += outputLine(pinName, el.value, null);
          hasAny = true;
        }
      });
    }
  });
  if (!hasAny) return '';
  return sectionHeader(getSectionMeta(gData.spi).title || 'SPI BUS PINS') + body;
}

function outputSingleSelectSection(dataArr, secId, selId, defaultTitle) {
  var meta = getSectionMeta(dataArr);
  var items = meta.items;
  var sel = document.getElementById(selId);
  if (!sel) return '';
  var item = items[parseInt(sel.value)];
  if (!item) return '';

  var out = sectionHeader(meta.title || defaultTitle);

  var isValueDefine = item.define && item.define.indexOf(' ') >= 0;
  var isBareFlag    = item.define && item.define.indexOf(' ') < 0;

  // 1. Item define if it carries a value (e.g. DSP_MODEL DSP_ILI9341)
  if (isValueDefine) {
    var vparts = item.define.split(' ');
    out += outputLine(vparts[0], vparts.slice(1).join(' '), null);
  }

  // 2. Pins
  var pins = item.pins;
  if (pins) {
    if (typeof pins === 'string') pins = [pins];
    pins.forEach(function(pinName) {
      var el = document.querySelector('#' + secId + '-detail input[data-pin="' + pinName + '"]');
      if (!el) el = document.querySelector('#' + secId + '-detail [data-define="' + pinName + '"]');
      if (el && isElementVisible(el)) {
        out += outputLine(pinName, el.value, null);
      }
    });
  }

  // 3. Optional defines (enabled by checkbox) + always-output bare flags with optional:false
  if (item.defines) {
    item.defines.forEach(function(def) {
      // Always-output embedded bare flags (e.g. DTYPE INITR_BLACKTAB with optional:false)
      if (def.type === 'flag' && def.optional === false) {
        var fp = def.define.split(' ');
        if (fp.length >= 2) {
          out += outputLine(fp[0], fp.slice(1).join(' '), null);
        } else {
          out += outputFlagLine(def.define);
        }
        return;
      }
      // Skip mandatory (handled in step 5)
      if (def.optional === false) return;

      var ctrl = findControlInContainer(def.define, secId + '-detail');
      if (!ctrl) return;
      var optChk = findOptCheckbox(def.define, secId + '-detail');
      if (optChk && !optChk.checked) return;
      if (!isElementVisible(ctrl)) return;

      // Optional flag: just output bare define when checkbox is enabled
      if (def.type === 'flag') {
        var ofp = def.define.split(' ');
        if (ofp.length >= 2) out += outputLine(ofp[0], ofp.slice(1).join(' '), null);
        else out += outputFlagLine(def.define);
        return;
      }

      var val = getControlValue(ctrl, def.type);
      out += outputLine(def.define, formatValue(val, def.type, def.define), def.comment || null);
    });
  }

  // 4. Bare-flag item.define (e.g. USE_ES8311) - after pins and optional defines
  if (isBareFlag) {
    out += outputFlagLine(item.define);
  }

  // 5. Mandatory defines (optional === false, non-flag)
  if (item.defines) {
    item.defines.forEach(function(def) {
      if (def.optional !== false) return; // skip optional
      if (def.type === 'flag') return;   // already handled in step 3

      var ctrl = findControlInContainer(def.define, secId + '-detail');
      if (!ctrl || !isElementVisible(ctrl)) return;

      var val = getControlValue(ctrl, def.type);
      out += outputLine(def.define, formatValue(val, def.type, def.define), def.comment || null);
    });
  }

  return out;
}

function outputCheckboxGroupSection(dataArr, secId, defaultTitle) {
  var meta = getSectionMeta(dataArr);
  var items = meta.items;
  var out = '';
  var body = '';

  items.forEach(function(item, idx) {
    var itemId = secId + '-item-' + idx;
    var chk = document.getElementById(itemId + '-chk');
    if (!chk || !chk.checked) return;

    // Top-level define
    if (item.define) {
      var parts = item.define.split(' ');
      if (parts.length >= 2) {
        body += outputLine(parts[0], parts.slice(1).join(' '), null);
      } else {
        body += outputFlagLine(item.define);
      }
    }

    // Pins
    var pins = item.pins;
    if (pins) {
      if (typeof pins === 'string') pins = [pins];
      pins.forEach(function(pinName) {
        var el = document.querySelector('#' + itemId + '-body input[data-pin="' + pinName + '"]');
        if (el && isElementVisible(el)) {
          body += outputLine(pinName, el.value, null);
        }
      });
    }

    // Defines
    if (item.defines) {
      item.defines.forEach(function(def) {
        if (def.type === 'flag' && def.optional === false) {
          var flagParts = def.define.split(' ');
          if (flagParts.length >= 2) {
            body += outputLine(flagParts[0], flagParts.slice(1).join(' '), null);
          } else {
            body += outputFlagLine(def.define);
          }
          return;
        }

        var ctrl = findControlInContainer(def.define, itemId + '-body');
        if (!ctrl || !isElementVisible(ctrl)) return;

        var optChk = findOptCheckbox(def.define, itemId + '-body');
        if (optChk && !optChk.checked) return;

        var val = getControlValue(ctrl, def.type);
        var formatted = formatValue(val, def.type, def.define);

        if (def.type === 'flag') {
          var fp = def.define.split(' ');
          if (fp.length >= 2) {
            body += outputLine(fp[0], fp.slice(1).join(' '), def.comment || null);
          } else {
            body += outputFlagLine(def.define);
          }
        } else {
          body += outputLine(def.define, formatted, def.comment || null);
        }
      });
    }
  });

  if (!body) return '';
  return sectionHeader(meta.title || defaultTitle) + body;
}

function outputDefaultsSection() {
  var meta = getSectionMeta(gData.defaults);
  var items = meta.items;
  var body = '';

  items.forEach(function(item) {
    // Locale select — output DSP_LANGUAGE
    if (item.type === 'locale_select') {
      var locChk = document.getElementById('locale-chk');
      if (!locChk || !locChk.checked) return;
      var locSel = document.getElementById('locale-sel');
      if (locSel) body += outputLine(item.define, '"' + locSel.value + '"');
      return;
    }
    // Timezone select — output TIMEZONE_NAME and TIMEZONE_POSIX
    if (item.type === 'timezone_select') {
      var tzChk = document.getElementById('tz-chk');
      if (!tzChk || !tzChk.checked) return;
      var tzSel = document.getElementById('tz-sel');
      if (!tzSel) return;
      var tzName = tzSel.value;
      var tzPosix = gData.timezones[tzName] || '';
      body += outputLine(item.define + '_NAME', '"' + tzName + '"', null);
      body += outputLine(item.define + '_POSIX', '"' + tzPosix + '"', null);
      return;
    }

    var chk = document.querySelector('[data-def-item="' + item.define + '"]');
    if (!chk || !chk.checked) return;

    // flag type: bare define, no value widget
    if (item.type === 'flag') {
      body += item.comment ? outputLine(item.define, '', item.comment) : outputFlagLine(item.define);
      return;
    }

    var ctrlDiv = chk.closest('.default-item').querySelector('.default-item-ctrl');
    if (!ctrlDiv) return;
    var ctrl = findControlInContainer(item.define, null, ctrlDiv);
    if (!ctrl) return;

    var val = getControlValue(ctrl, item.type);
    var formatted = formatValue(val, item.type, item.define);
    body += outputLine(item.define, formatted, item.comment || null);
  });

  if (!body) return '';
  return sectionHeader(meta.title || 'User Defaults') + body;
}

function outputExtraDefinesSection() {
  var chk = document.getElementById('extra-defines-chk');
  var txt = document.getElementById('extra-defines-text');
  if (!chk || !chk.checked || !txt) return '';

  var raw = txt.value || '';
  if (!raw.trim()) return '';

  var normalized = raw.replace(/\r\n?/g, '\n');
  var out = sectionHeader('Extra defines');
  out += normalized;
  if (!normalized.endsWith('\n')) out += '\n';
  return out;
}

// ============================================================
// Control value helpers
// ============================================================
function findControlInContainer(defineName, containerId, containerEl) {
  var container = containerEl || (containerId ? document.getElementById(containerId) : document);
  if (!container) container = document;

  // Radio groups: find checked radio
  var radios = container.querySelectorAll('input[type="radio"][data-define="' + defineName + '"]');
  if (radios.length > 0) {
    for (var i = 0; i < radios.length; i++) {
      if (radios[i].checked) return radios[i];
    }
    return radios[0];
  }

  var el = container.querySelector('[data-define="' + defineName + '"]');
  return el;
}

function findVisibleControl(defineName, containerId) {
  var container = containerId ? document.getElementById(containerId) : document;
  if (!container) container = document;
  var els = container.querySelectorAll('[data-define="' + defineName + '"]');
  for (var i = 0; i < els.length; i++) {
    if (isElementVisible(els[i])) return els[i];
  }
  return null;
}

function findOptCheckbox(defineName, containerId) {
  var container = containerId ? document.getElementById(containerId) : document;
  if (!container) return null;
  return container.querySelector('input[type="checkbox"][data-opt-for="' + defineName + '"]');
}

function getControlValue(ctrl, type) {
  if (!ctrl) return '';
  if (ctrl.tagName === 'INPUT') {
    if (ctrl.type === 'checkbox') return ctrl.checked;
    return ctrl.value;
  }
  if (ctrl.tagName === 'SELECT') return ctrl.value;
  return ctrl.value || ctrl.textContent || '';
}

function formatValue(val, type, defineName) {
  if (type === 'text') return '"' + val + '"';
  if (type === 'char' || type === 'spi') return "'" + val + "'";
  // boolean, numeric, enum, pin: raw value
  return String(val);
}

// ============================================================
// SPI assignments for summary comment
// ============================================================
function collectSpiAssignments() {
  var assignments = [];

  // Display - SPI type always uses Bus A
  var dspSel = document.getElementById('dsp-sel');
  var dspItems = gData.display.slice(1);
  if (dspSel) {
    var dspItem = dspItems[parseInt(dspSel.value)];
    if (dspItem && dspItem.type === 'SPI') {
      assignments.push({ bus: 'A', device: dspItem.name });
    }
  }

  // Check all spi-type selectors in visible sections
  var spiSels = document.querySelectorAll('select[data-type="spi"]');
  spiSels.forEach(function(sel) {
    if (!isElementVisible(sel)) return;
    var bus = sel.value;
    var scope = sel.dataset.scope || '';
    // Find device name from scope
    var deviceName = getDeviceNameForScope(scope);
    if (deviceName) {
      assignments.push({ bus: bus, device: deviceName });
    }
  });

  return assignments;
}

function getDeviceNameForScope(scope) {
  if (!scope) return null;

  // Check input items
  var inputItems = gData.input.slice(1);
  for (var i = 0; i < inputItems.length; i++) {
    var itemId = 'input-section-item-' + i;
    if (scope.startsWith(itemId)) return inputItems[i].name;
  }

  // Check peripherals items
  var periphItems = gData.peripherals.slice(1);
  for (var j = 0; j < periphItems.length; j++) {
    var pId = 'peripherals-section-item-' + j;
    if (scope.startsWith(pId)) return periphItems[j].name;
  }

  // Audio
  if (scope === 'audio') {
    var audSel = document.getElementById('aud-sel');
    var audItems = gData.audio.slice(1);
    if (audSel) return audItems[parseInt(audSel.value)].name;
  }

  return null;
}

// ============================================================
// ASCII Pin Diagram + Pins Inventory
// ============================================================
function buildPinDiagram() {
  var bd = gData.boardData;
  var lines = [];

  // Only match pure integers (avoids "3V3" parsing as 3, "5V" as 5, etc.)
  function parsePin(s) {
    var str = String(s).trim();
    if (!/^-?\d+$/.test(str)) return NaN;
    return parseInt(str, 10);
  }

  // Build pin -> [define names] map for all visible pin inputs
  // A pin can serve multiple functions (e.g. SPI MISO shared by display + SD card)
  var pinDefMap = {};
  var allPinInputs = document.querySelectorAll('input[data-type="pin"]');
  allPinInputs.forEach(function(el) {
    if (!isElementVisible(el)) return;
    var val = parsePin(el.value);
    if (isNaN(val) || val === 255) return; // 255 = unused sentinel, exclude from diagram
    var defName = el.dataset.define || el.dataset.pin;
    if (!defName) return;
    if (pinDefMap[val] === undefined) pinDefMap[val] = [];
    if (pinDefMap[val].indexOf(defName) === -1) pinDefMap[val].push(defName);
  });

  // Helper: get joined label for a pin number (e.g. "TFT_RST + VS1053_RST")
  function pinLabel(n) {
    return (pinDefMap[n] !== undefined) ? pinDefMap[n].join(' + ') : '';
  }

  var hasAscii = bd && bd.right_pins && bd.left_pins;

  // Track which pin numbers appear in the ASCII diagram
  var diagramPins = {};

  if (hasAscii) {
    // In the JSON: left_pins = left column of diagram, right_pins = right column
    var diagLeft  = bd.left_pins;
    var diagRight = bd.right_pins;

    diagLeft.forEach(function(p) { var n = parsePin(p); if (!isNaN(n)) diagramPins[n] = true; });
    diagRight.forEach(function(p) { var n = parsePin(p); if (!isNaN(n)) diagramPins[n] = true; });

    var rows = Math.max(diagLeft.length, diagRight.length);

    // Compute max left cell width
    var maxLeftLen = 0;
    for (var i = 0; i < rows; i++) {
      var lp = String(i < diagLeft.length ? diagLeft[i] : '');
      var lpDef = pinLabel(parsePin(lp));
      var lCell = (lpDef ? lpDef + ' ' : '') + lp;
      if (lCell.length > maxLeftLen) maxLeftLen = lCell.length;
    }

    var leftW = maxLeftLen;
    lines.push(padLeft('', leftW) + ' |----------|');

    for (var r = 0; r < rows; r++) {
      var lp2 = String(r < diagLeft.length ? diagLeft[r] : '');
      var rp  = String(r < diagRight.length ? diagRight[r] : '');
      var lpDef2 = pinLabel(parsePin(lp2));
      var rpDef  = pinLabel(parsePin(rp));
      var leftCell  = (lpDef2 ? lpDef2 + ' ' : '') + lp2;
      var rightCell = rp + (rpDef ? ' ' + rpDef : '');
      lines.push(padLeft(leftCell, leftW) + ' |          | ' + rightCell);
    }

    lines.push(padLeft('', leftW) + ' |----------|');
  }

  // Build pins inventory: pins not shown in ASCII diagram (or all pins if no diagram)
  var inventoryPins = [];
  Object.keys(pinDefMap).forEach(function(pinStr) {
    var n = parseInt(pinStr, 10);
    if (hasAscii && diagramPins[n]) return; // already shown in diagram
    inventoryPins.push({ num: n, name: pinDefMap[n].join(' + ') });
  });
  // Sort numerically (-1 first, then ascending)
  inventoryPins.sort(function(a, b) { return a.num - b.num; });

  if (inventoryPins.length > 0) {
    if (lines.length > 0) lines.push(''); // blank separator after ASCII diagram
    lines.push(' Pin  Function');
    lines.push(' ---  --------');
    inventoryPins.forEach(function(p) {
      var pinStr = String(p.num);
      // Pin starts at column 5, Function at column 10 (1-indexed from start of full line)
      // Since '// ' (3 chars) is prepended, we add 1 leading space then pad pin to 5 chars
      var padded = pinStr;
      while (padded.length < 5) padded += ' ';
      lines.push(' ' + padded + p.name);
    });
  }

  return lines;
}

function padLeft(str, width) {
  while (str.length < width) str = ' ' + str;
  return str;
}

// ============================================================
// platformio.ini generation
// ============================================================
function generatePlatformioIni() {
  var bd = gData.boardData;
  if (!bd) return '; No board selected\n';

  // Get firmware name for the env
  var firmwareNameCtrl = document.querySelector('[data-define="FIRMWARE_NAME"]');
  var firmwareName = firmwareNameCtrl ? firmwareNameCtrl.value : 'my_ehradio';
  var envName = firmwareName.toLowerCase().replace(/[^a-z0-9_]/g, '_').replace(/^_+|_+$/g, '') || 'my_ehradio';

  // Compute shareable URL to embed in the platformio.ini header comment
  var pioState = serializeState();
  var pioCompressed = LZString.compressToBase64(JSON.stringify(pioState));
  var shareableUrl = window.location.href.split('#')[0] + '#' + encodeURIComponent(pioCompressed);

  // Build the header using simple [radio_name] / [url] placeholder replacement
  var header = buildPioHeader(firmwareName, shareableUrl);

  // Board section
  var boardSection = buildBoardSection_pio(bd);

  // Env section with deduped lib_deps and build_src_filter
  var envSection = buildEnvSection_pio(bd, envName, firmwareName);

  return header + boardSection + envSection;
}

function buildPioHeader(firmwareName, shareableUrl) {
  // Simple placeholder replacement - preserves template exactly as written
  return gPioIni
    .replace('[radio_name]', firmwareName)
    .replace('[url]', shareableUrl || window.location.href.split('#')[0]);
}

function buildBoardSection_pio(bd) {
  var out = '\n';
  out += '; -----------------------------------------------------------------------\n';
  out += '; Board: ' + bd.name + '\n';
  out += '; -----------------------------------------------------------------------\n';
  out += '[' + bd.env + ']\n';
  out += 'extends = ehradio\n';
  out += 'board = ' + bd.board + '\n';

  // board_build.*, board_upload.*, and any other generic platformio.ini fields
  var skipFields = ['name', 'env', 'board', 'build_flags', 'default_pins', 'default_selects', 'spi', 'valid_pins', 'safe_pins', 'left_pins', 'right_pins', 'image', 'info', 'url'];
  Object.keys(bd).forEach(function(key) {
    if (skipFields.includes(key)) return;
    out += key + ' = ' + bd[key] + '\n';
  });

  // build_flags
  if (bd.build_flags) {
    out += 'build_flags =\n';
    out += '  ${ehradio.build_flags}\n';
    out += '  ' + bd.build_flags + '\n';
  }

  return out;
}

function buildEnvSection_pio(bd, envName, firmwareName) {
  var out = '\n';
  out += '; -----------------------------------------------------------------------\n';
  out += '; RADIO FIRMWARES\n';
  out += '; -----------------------------------------------------------------------\n';
  out += '[env:' + envName + ']\n';
  out += 'extends = ' + bd.env + '\n';
  out += 'build_flags =\n';
  out += '  ${' + bd.env + '.build_flags}\n';
  out += '  -D' + envName + '\n';

  // Collect lib_deps from all active items
  var libDepsStr = '';
  var buildFilterStr = '';

  function addLibDep(val) {
    if (!val || val === '') return;
    // Handle JSON arrays (multiple library references)
    if (Array.isArray(val)) {
      val.forEach(function(v) { addLibDep(v); });
      return;
    }
    // Keep ${library.xxx} references unresolved - the [library] section is in the header
    var trimmed = val.trim();
    if (!trimmed) return;
    if (!libDepsStr.includes(trimmed)) {
      libDepsStr += (libDepsStr ? '\n  ' : '') + trimmed;
    }
  }

  function addBuildFilter(val) {
    if (!val || val === '') return;
    // Handle JSON arrays (multiple source filter patterns)
    if (Array.isArray(val)) {
      val.forEach(function(v) { addBuildFilter(v); });
      return;
    }
    var trimmed = val.trim();
    if (!trimmed) return;
    if (!buildFilterStr.includes(trimmed)) {
      buildFilterStr += (buildFilterStr ? '\n  ' : '') + trimmed;
    }
  }

  // Display
  var dspSel = document.getElementById('dsp-sel');
  var dspItems = gData.display.slice(1);
  if (dspSel) {
    var dspItem = dspItems[parseInt(dspSel.value)];
    if (dspItem) {
      addLibDep(dspItem.lib_deps);
      addBuildFilter(dspItem.build_src_filter);
    }
  }

  // Audio
  var audSel = document.getElementById('aud-sel');
  var audItems = gData.audio.slice(1);
  if (audSel) {
    var audItem = audItems[parseInt(audSel.value)];
    if (audItem) {
      addLibDep(audItem.lib_deps);
      addBuildFilter(audItem.build_src_filter);
    }
  }

  // Input items
  gData.input.slice(1).forEach(function(item, idx) {
    var chk = document.getElementById('input-section-item-' + idx + '-chk');
    if (!chk || !chk.checked) return;
    addLibDep(item.lib_deps);
    addBuildFilter(item.build_src_filter);
  });

  // Peripherals items
  gData.peripherals.slice(1).forEach(function(item, idx) {
    var chk = document.getElementById('peripherals-section-item-' + idx + '-chk');
    if (!chk || !chk.checked) return;
    addLibDep(item.lib_deps);
    addBuildFilter(item.build_src_filter);
  });

  // lib_deps
  out += 'lib_deps =\n';
  out += '  ${ehradio.lib_deps}\n';
  if (libDepsStr) {
    libDepsStr.split('\n').forEach(function(l) {
      if (l.trim()) out += '  ' + l.trim() + '\n';
    });
  }

  // build_src_filter
  out += 'build_src_filter =\n';
  out += '  ${ehradio.build_src_filter}\n';
  if (buildFilterStr) {
    buildFilterStr.split('\n').forEach(function(l) {
      if (l.trim()) out += '  ' + l.trim() + '\n';
    });
  }

  return out;
}

function resolveLibraryRef(val) {
  if (!val) return '';
  // Match ${library.xxx}
  return val.replace(/\$\{library\.([^}]+)\}/g, function(match, libKey) {
    // Extract from gPioIni [library] section
    var lines = gPioIni.split('\n');
    var inLib = false;
    for (var i = 0; i < lines.length; i++) {
      if (lines[i].trim() === '[library]') { inLib = true; continue; }
      if (inLib && lines[i].startsWith('[')) { inLib = false; }
      if (inLib) {
        var eqIdx = lines[i].indexOf('=');
        if (eqIdx > 0) {
          var key = lines[i].substring(0, eqIdx).trim();
          var libVal = lines[i].substring(eqIdx + 1).trim();
          // Strip comments
          var semiIdx = libVal.indexOf(';');
          if (semiIdx > 0) libVal = libVal.substring(0, semiIdx).trim();
          if (key === libKey) {
            // Handle aliases like ${library.st7735}
            return resolveLibraryRef(libVal);
          }
        }
      }
    }
    return match; // unresolved
  });
}

// ============================================================
// File download / copy helpers
// ============================================================
function getFile(type) {
  if (type === 'options') {
    var content = generateOptionsH();
    downloadFile('myoptions.h', content);
  } else {
    var content2 = generatePlatformioIni();
    downloadFile('platformio.ini', content2);
  }
  window.location.href = buildShareUrl();
}

function copyFile(type) {
  var content = type === 'options' ? generateOptionsH() : generatePlatformioIni();
  navigator.clipboard.writeText(content).then(function() {
    window.location.href = buildShareUrl();
    showAlert('info', type === 'options' ? 'myoptions.h copied to clipboard!' : 'platformio.ini copied to clipboard!', 2500);
  }, function() {
    window.location.href = buildShareUrl();
    showAlert('error', 'Could not copy to clipboard.', 3000);
  });
}

function downloadFile(filename, text) {
  var el = document.createElement('a');
  el.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
  el.setAttribute('download', filename);
  el.style.display = 'none';
  document.body.appendChild(el);
  el.click();
  document.body.removeChild(el);
}

// ============================================================
// Copy link (state -> LZ-string -> URL hash)
// ============================================================
function buildShareUrl() {
  var state = serializeState();
  var compressed = LZString.compressToBase64(JSON.stringify(state));
  return window.location.href.split('#')[0] + '#' + encodeURIComponent(compressed);
}

function copyLink() {
  var url = buildShareUrl();

  if (window.location.protocol !== 'http:' && window.location.protocol !== 'https:') {
    window.location.href = url;
    showAlert('warning', 'Local file: link updated in address bar. Copy it manually.', 4000);
    return;
  }

  navigator.clipboard.writeText(url).then(function() {
    window.location.href = url;
    showAlert('info', 'Link copied to clipboard!', 2500);
  }, function() {
    window.location.href = url;
    showAlert('warning', 'Link updated in address bar.', 3000);
  });
}

// ============================================================
// State serialization / deserialization
// ============================================================
function serializeState() {
  var state = {};

  // Board: name only (no positional index)
  var boardSel = document.getElementById('board-sel');
  if (boardSel) {
    var _bi = parseInt(boardSel.value);
    if (gData.boards[_bi]) state.bn = gData.boards[_bi].name;
  }

  // Display: define (most stable) + name (2nd) — no positional index
  var dspSel = document.getElementById('dsp-sel');
  if (dspSel) {
    var _di = parseInt(dspSel.value);
    var _dspItems = gData.display.slice(1);
    if (_dspItems[_di]) {
      if (_dspItems[_di].define) state.dd = _dspItems[_di].define;
      state.dn = _dspItems[_di].name;
    }
  }

  // Audio: define (most stable) + name
  var audSel = document.getElementById('aud-sel');
  if (audSel) {
    var _ai = parseInt(audSel.value);
    var _audItems = gData.audio.slice(1);
    if (_audItems[_ai]) {
      if (_audItems[_ai].define) state.ad = _audItems[_ai].define;
      state.an = _audItems[_ai].name;
    }
  }

  // Checked input items — identified by item.define (or item.name as fallback)
  var ci = [];
  gData.input.slice(1).forEach(function(item, idx) {
    var chk = document.getElementById('input-section-item-' + idx + '-chk');
    if (chk && chk.checked) ci.push(item.define || item.name);
  });
  if (ci.length) state.ci = ci;

  // Checked peripheral items
  var cp = [];
  gData.peripherals.slice(1).forEach(function(item, idx) {
    var chk = document.getElementById('peripherals-section-item-' + idx + '-chk');
    if (chk && chk.checked) cp.push(item.define || item.name);
  });
  if (cp.length) state.cp = cp;

  // Checked SPI buses (only save checked ones — unchecked = no SPI data)
  var cs = [];
  document.querySelectorAll('input[type="checkbox"][data-spi-bus]').forEach(function(chk) {
    if (chk.checked) cs.push(chk.dataset.spiBus);
  });
  if (cs.length) state.cs = cs;

  // Checked optional sub-define checkboxes (the per-define "include" toggle)
  var co = [];
  document.querySelectorAll('input[type="checkbox"][data-opt-for]').forEach(function(chk) {
    if (chk.checked) co.push(chk.dataset.optFor);
  });
  if (co.length) state.co = co;

  // Checked default items (by define name)
  var cd = [];
  document.querySelectorAll('input[type="checkbox"][data-def-item]').forEach(function(chk) {
    if (chk.checked) cd.push(chk.dataset.defItem);
  });
  if (cd.length) state.cd = cd;

  // All visible pin values
  var p = {};
  document.querySelectorAll('input[data-type="pin"]').forEach(function(el) {
    if (isElementVisible(el) && el.value !== '') {
      p[el.dataset.define || el.dataset.pin] = el.value;
    }
  });
  if (Object.keys(p).length) state.p = p;

  // All visible non-pin define values (selects, radios, text/number inputs)
  var v = {};
  var _skipIds = { 'board-sel': 1, 'dsp-sel': 1, 'aud-sel': 1 };
  document.querySelectorAll('select[data-define]').forEach(function(sel) {
    if (_skipIds[sel.id]) return;
    if (isElementVisible(sel)) v[sel.dataset.define] = sel.value;
  });
  document.querySelectorAll('input[type="radio"][data-define]:checked').forEach(function(rad) {
    if (isElementVisible(rad)) v[rad.dataset.define] = rad.value;
  });
  document.querySelectorAll('input[data-define]').forEach(function(inp) {
    if (inp.type === 'radio' || inp.type === 'checkbox') return;
    if (inp.dataset.type === 'pin') return;
    if (isElementVisible(inp)) v[inp.dataset.define] = inp.value;
  });
  if (Object.keys(v).length) state.v = v;

  // Extra custom text (enabled flag + text)
  var xChk = document.getElementById('extra-defines-chk');
  var xTxt = document.getElementById('extra-defines-text');
  if (xChk && xChk.checked) state.xe = true;
  if (xTxt && xTxt.value && xTxt.value.trim() !== '') state.xd = xTxt.value;

  return state;
}

function loadStateFromHash() {
  var hash = window.location.hash;
  if (!hash || hash.length < 2) {
    // No state - trigger initial board load
    onBoardChange(false);
    return;
  }
  try {
    var compressed = decodeURIComponent(hash.substring(1));
    var json = LZString.decompressFromBase64(compressed);
    var state = JSON.parse(json);
    applyState(state);
  } catch (e) {
    console.warn('Could not load state from URL hash:', e);
    onBoardChange(false);
  }
}

function applyState(state) {
  // Board resolution by name
  var resolvedBoardIdx = 0;
  if (state.bn) {
    var nameIdx = gData.boards.findIndex(function(b) { return b.name === state.bn; });
    if (nameIdx >= 0) resolvedBoardIdx = nameIdx;
  }
  var boardSel = document.getElementById('board-sel');
  if (boardSel) boardSel.value = resolvedBoardIdx;

  // Load board data, then apply state
  var boardEntry = gData.boards[resolvedBoardIdx];
  fetchJSON(boardEntry.file).then(function(data) {
    gData.boardData = data[0];
    updateBoardImageArea();
    updateSPISections();

    // Display: resolve by define + name
    if (state.dd !== undefined || state.dn !== undefined) {
      var dspSel = document.getElementById('dsp-sel');
      if (dspSel) {
        var _dsp = gData.display.slice(1);
        var dspIdx = -1;
        if (state.dd && state.dn) dspIdx = _dsp.findIndex(function(d) { return d.define === state.dd && d.name === state.dn; });
        if (dspIdx < 0 && state.dn) dspIdx = _dsp.findIndex(function(d) { return d.name === state.dn; });
        if (dspIdx < 0 && state.dd) dspIdx = _dsp.findIndex(function(d) { return d.define === state.dd; });
        if (dspIdx >= 0) dspSel.value = dspIdx;
        onDisplayChange();
      }
    } else {
      onDisplayChange();
    }

    // Audio: resolve by define + name
    if (state.ad !== undefined || state.an !== undefined) {
      var audSel = document.getElementById('aud-sel');
      if (audSel) {
        var _aud = gData.audio.slice(1);
        var audIdx = -1;
        if (state.ad && state.an) audIdx = _aud.findIndex(function(a) { return a.define === state.ad && a.name === state.an; });
        if (audIdx < 0 && state.an) audIdx = _aud.findIndex(function(a) { return a.name === state.an; });
        if (audIdx < 0 && state.ad) audIdx = _aud.findIndex(function(a) { return a.define === state.ad; });
        if (audIdx >= 0) audSel.value = audIdx;
        onAudioChange();
      }
    } else {
      onAudioChange();
    }

    // Apply control values and finish
    applyStateNew(state);
    validateAllPins();
    updatePreview();
  });
}

// ---- Compact format restore ----
function applyStateNew(state) {
  // 0. Check SPI buses (cs = array of checked bus letters, e.g. ["A","B"])
  if (state.cs) {
    state.cs.forEach(function(bus) {
      var chk = document.getElementById('spi-chk-' + bus);
      if (!chk) return;
      chk.checked = true;
      var pinsWrap = document.getElementById('spi-pins-' + bus);
      if (pinsWrap) pinsWrap.classList.remove('hidden');
      var nl = chk.nextElementSibling;
      if (nl && nl.tagName === 'SPAN') nl.style.color = '#fff';
    });
  }

  // Helper: reveal a checked check-item body
  function revealItem(itemId) {
    var chk = document.getElementById(itemId + '-chk');
    if (!chk) return;
    chk.checked = true;
    var body = document.getElementById(itemId + '-body');
    if (body && body.children.length > 0) {
      body.classList.remove('hidden');
      var nl = chk.nextElementSibling;
      if (nl && nl.tagName === 'SPAN') nl.style.color = '#fff';
    }
  }

  // 1. Check input items by define/name
  if (state.ci) {
    state.ci.forEach(function(key) {
      var idx = gData.input.slice(1).findIndex(function(item) {
        return (item.define || item.name) === key;
      });
      if (idx >= 0) revealItem('input-section-item-' + idx);
    });
  }

  // 2. Check peripheral items by define/name
  if (state.cp) {
    state.cp.forEach(function(key) {
      var idx = gData.peripherals.slice(1).findIndex(function(item) {
        return (item.define || item.name) === key;
      });
      if (idx >= 0) revealItem('peripherals-section-item-' + idx);
    });
  }

  // 3. Check optional sub-define checkboxes
  if (state.co) {
    state.co.forEach(function(defName) {
      document.querySelectorAll('input[type="checkbox"][data-opt-for="' + defName + '"]').forEach(function(chk) {
        chk.checked = true;
        var ctrlDiv = chk.closest('.ctrl-cell').querySelector('div[id]');
        var lbl = chk.closest('.opt-label');
        if (ctrlDiv) ctrlDiv.classList.remove('hidden');
        if (lbl) lbl.classList.add('checked');
      });
    });
  }

  // 4. Check default items
  if (state.cd) {
    state.cd.forEach(function(defName) {
      var chk = document.querySelector('input[type="checkbox"][data-def-item="' + defName + '"]');
      if (!chk) return;
      chk.checked = true;
      var ctrlDiv = chk.closest('.default-item').querySelector('.default-item-ctrl');
      if (ctrlDiv) ctrlDiv.classList.remove('hidden');
      var lbl = chk.nextElementSibling;
      if (lbl) lbl.style.color = '#fff';
    });
  }

  // 5. Apply pin values
  if (state.p) {
    Object.keys(state.p).forEach(function(pinName) {
      document.querySelectorAll('input[data-pin="' + pinName + '"], input[data-define="' + pinName + '"][data-type="pin"]')
        .forEach(function(el) { el.value = state.p[pinName]; });
    });
  }

  // 6. Apply non-pin values (selects, radios, text/number inputs)
  if (state.v) {
    Object.keys(state.v).forEach(function(defName) {
      var val = state.v[defName];
      document.querySelectorAll('select[data-define="' + defName + '"]')
        .forEach(function(sel) { sel.value = val; });
      document.querySelectorAll('input[type="radio"][data-define="' + defName + '"]')
        .forEach(function(rad) { rad.checked = (rad.value === String(val)); });
      document.querySelectorAll('input[data-define="' + defName + '"]:not([type="radio"]):not([type="checkbox"])')
        .forEach(function(inp) { inp.value = val; });
    });
  }

  // 7. Extra custom text (xe = enabled flag, xd = text)
  var xChk = document.getElementById('extra-defines-chk');
  var xCtrl = document.getElementById('extra-defines-ctrl');
  var xTxt = document.getElementById('extra-defines-text');
  var xText = (typeof state.xd === 'string') ? state.xd : '';
  var hasXText = xText.trim() !== '';

  if (xTxt && typeof state.xd === 'string') {
    xTxt.value = xText;
  }

  if (xChk && (state.xe || hasXText)) {
    xChk.checked = true;
    if (xCtrl) xCtrl.classList.remove('hidden');
    var xLbl = xChk.nextElementSibling;
    if (xLbl) xLbl.style.color = '#fff';
  }
}

// ============================================================
// Board zoom
// ============================================================
function openZoom(src) {
  var overlay = document.getElementById('zoom-overlay');
  var img = document.getElementById('zoom-img');
  img.src = src;
  overlay.classList.remove('hidden');
}

function closeZoom() {
  document.getElementById('zoom-overlay').classList.add('hidden');
}

// ============================================================
// Help popup
// ============================================================
function showHelp(defineName, comment) {
  document.getElementById('help-define-name').textContent = defineName;

  // Linkify URLs in comment
  var html = comment.replace(/(https?:\/\/[^\s)]+)/g, function(url) {
    return '<a href="' + url + '" target="_blank" rel="noopener">' + url + '</a>';
  });
  document.getElementById('help-text').innerHTML = html;
  document.getElementById('help-overlay').classList.remove('hidden');
}

function closeHelp(event, force) {
  if (force || (event && event.target === document.getElementById('help-overlay'))) {
    document.getElementById('help-overlay').classList.add('hidden');
  }
}

// ============================================================
// Alert system
// ============================================================
var _alertTimer = null;
function showAlert(type, msg, duration) {
  var bar = document.getElementById('alert-bar');
  bar.className = type;
  bar.textContent = msg;
  bar.classList.remove('hidden');
  bar.onclick = function() { bar.classList.add('hidden'); };
  if (_alertTimer) clearTimeout(_alertTimer);
  if (duration) {
    _alertTimer = setTimeout(function() { bar.classList.add('hidden'); }, duration);
  }
}

// ============================================================
// Initial page load trigger
// ============================================================
// (The Promise.all above triggers buildPage and then loadStateFromHash)
