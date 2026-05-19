// @ts-nocheck
/*  ehRadio myoptions Generator - generator.js
 *  Reads JSON config files and builds a dynamic hardware configuration page.
 *  State is serialized via LZ-string into the URL hash for shareable links.
 */

'use strict';

// ============================================================
// State encoding helpers (base64 + URI encoding for URL-safe storage)
// URL hash can hold thousands of characters; typical config ~500-900 chars.
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

  // 3. SPI Bus sections (rendered inside board section update)
  var spiDiv = document.createElement('div');
  spiDiv.id = 'spi-sections';
  root.appendChild(spiDiv);

  // 4. Display section
  root.appendChild(buildSingleSelectSection(gData.display, 'display-section', 'dsp-sel', onDisplayChange));

  // 5. Audio section
  root.appendChild(buildSingleSelectSection(gData.audio, 'audio-section', 'aud-sel', onAudioChange));

  // 6. Input section
  root.appendChild(buildCheckboxGroupSection(gData.input, 'input-section'));

  // 7. Peripherals section
  root.appendChild(buildCheckboxGroupSection(gData.peripherals, 'peripherals-section'));

  // 8. Locale section
  root.appendChild(buildLocaleSection());

  // 9. User Defaults section
  root.appendChild(buildDefaultsSection());

  // 10. Timezone section
  root.appendChild(buildTimezoneSection());

  // Show buttons
  document.getElementById('buttons-area').style.display = 'flex';
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
  var nameData = gData.name;
  var sectionTitle = typeof nameData[0] === 'string' ? nameData[0] : 'Firmware Name';
  var item = nameData[1];

  var sec = makeSection(sectionTitle);
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

function onBoardChange(reset) {
  var sel = document.getElementById('board-sel');
  var boardEntry = gData.boards[parseInt(sel.value)];
  if (!boardEntry) return;

  fetchJSON(boardEntry.file).then(function(data) {
    gData.boardData = data[0]; // boards json files are arrays with one item
    updateBoardImageArea();
    updateSPISections();
    if (reset) {
      applyDefaultPins();
    } else {
      applyDefaultPins();  // always apply defaults when board changes
    }
    validateAllPins();
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
}

function updateSPISections() {
  var bd = gData.boardData;
  var container = document.getElementById('spi-sections');
  container.innerHTML = '';
  if (!bd || !bd.spi) return;

  var spiItems = gData.spi.slice(1); // skip header string
  spiItems.forEach(function(busObj) {
    var busKey = busObj.spi; // "A" or "B"
    if (!bd.spi.includes(busKey)) return; // skip if board doesn't have this bus

    var sec = makeSection(busObj.name);
    sec.id = 'spi-section-' + busKey;

    if (busObj.info) {
      var infoDiv = document.createElement('div');
      infoDiv.className = 'spi-info';
      infoDiv.textContent = busObj.info;
      sec.appendChild(infoDiv);
    }

    var row = document.createElement('div');
    row.className = 'ctrl-row';
    busObj.pins.forEach(function(pinName) {
      var cell = makePinCell(pinName, null);
      row.appendChild(cell);
    });
    sec.appendChild(row);
    container.appendChild(sec);
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
    opt.textContent = b;
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
  var sectionTitle = typeof dataArr[0] === 'string' ? dataArr[0] : 'Section';
  var items = dataArr.slice(1);

  var sec = makeSection(sectionTitle);
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
    ctrlDiv.appendChild(makeControl(def, scopeId));
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
    cell.appendChild(makeControl(def, scopeId));
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

  var inp = document.createElement('input');
  inp.type = 'number';
  inp.value = '';
  inp.dataset.pin = pinName;
  inp.dataset.scope = scopeId || '';
  inp.dataset.define = pinName;
  inp.dataset.type = 'pin';
  inp.dataset.defaultVal = ''; // top-level pins have no JSON default
  inp.addEventListener('input', function() { validateAllPins(); });
  cell.appendChild(inp);

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
      opt.textContent = o;
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
    lbl.appendChild(document.createTextNode(' ' + v));
    wrap.appendChild(lbl);
  });
  return wrap;
}

// ============================================================
// Checkbox group sections (Input, Peripherals)
// ============================================================
function buildCheckboxGroupSection(dataArr, secId) {
  var sectionTitle = typeof dataArr[0] === 'string' ? dataArr[0] : 'Section';
  var items = dataArr.slice(1);

  var sec = makeSection(sectionTitle);
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
}

// ============================================================
// Locale section
// ============================================================
function buildLocaleSection() {
  var sectionTitle = typeof gData.locale[0] === 'string' ? gData.locale[0] : 'Locale';
  var locales = gData.locale.slice(1);

  var sec = makeSection(sectionTitle);
  sec.id = 'locale-section';

  var header = document.createElement('div');
  header.className = 'default-item-header';
  var chk = document.createElement('input');
  chk.type = 'checkbox';
  chk.id = 'locale-chk';
  var lbl = document.createElement('span');
  lbl.className = 'item-name';
  lbl.textContent = 'Display Language';
  header.appendChild(chk);
  header.appendChild(lbl);

  var ctrlDiv = document.createElement('div');
  ctrlDiv.id = 'locale-ctrl';
  ctrlDiv.className = 'hidden';
  ctrlDiv.style.marginTop = '10px';
  ctrlDiv.style.textAlign = 'center';

  var sel = document.createElement('select');
  sel.id = 'locale-sel';
  sel.className = 'wide';
  locales.forEach(function(loc) {
    var opt = document.createElement('option');
    opt.value = loc.locale_code;
    var parts = [loc.locale_code];
    if (loc.locale) parts.push(loc.locale);
    if (loc.locale_en) parts.push(loc.locale_en);
    opt.textContent = parts.join(' - ');
    sel.appendChild(opt);
  });
  ctrlDiv.appendChild(sel);

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
// User Defaults section
// ============================================================
function buildDefaultsSection() {
  var sectionTitle = typeof gData.defaults[0] === 'string' ? gData.defaults[0] : 'User Defaults';
  var items = gData.defaults.slice(1);

  var sec = makeSection(sectionTitle);
  sec.id = 'defaults-section';

  items.forEach(function(item) {
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
      // Non-flag items: create a collapsible control div
      var ctrlDiv = document.createElement('div');
      ctrlDiv.className = 'default-item-ctrl hidden';
      ctrlDiv.id = uid('defctrl');
      ctrlDiv.appendChild(makeControl(item, 'defaults'));
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
// Timezone section
// ============================================================
function buildTimezoneSection() {
  var tzKeys = Object.keys(gData.timezones);

  var sec = makeSection('Time Zone');
  sec.id = 'timezone-section';

  var header = document.createElement('div');
  header.className = 'default-item-header';
  var chk = document.createElement('input');
  chk.type = 'checkbox';
  chk.id = 'tz-chk';
  var lbl = document.createElement('span');
  lbl.className = 'item-name';
  lbl.textContent = 'Time Zone';
  header.appendChild(chk);
  header.appendChild(lbl);

  var ctrlDiv = document.createElement('div');
  ctrlDiv.id = 'tz-ctrl';
  ctrlDiv.className = 'hidden';
  ctrlDiv.style.marginTop = '10px';
  ctrlDiv.style.textAlign = 'center';

  var sel = document.createElement('select');
  sel.id = 'tz-sel';
  sel.className = 'wide';
  tzKeys.forEach(function(tz) {
    var opt = document.createElement('option');
    opt.value = tz;
    opt.textContent = tz;
    sel.appendChild(opt);
  });
  ctrlDiv.appendChild(sel);

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
function makeSection(title) {
  var sec = document.createElement('div');
  sec.className = 'gen-section';
  var titleDiv = document.createElement('div');
  titleDiv.className = 'gen-section-title';
  var span = document.createElement('span');
  span.textContent = title;
  titleDiv.appendChild(span);
  sec.appendChild(titleDiv);
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

  // Clear errors
  allPinInputs.forEach(function(el) { el.classList.remove('pin-error'); });

  var bd = gData.boardData;
  var validPins = bd ? bd.valid_pins : null;

  // Map of value -> [elements] for duplicate detection
  var pinValueMap = {};
  visiblePins.forEach(function(el) {
    var val = parseInt(el.value);
    if (isNaN(val)) return;
    if (val === -1 || val === 255) return; // these are "unused" sentinels, skip dup check
    if (!pinValueMap[val]) pinValueMap[val] = [];
    pinValueMap[val].push(el);
  });

  visiblePins.forEach(function(el) {
    var val = parseInt(el.value);
    if (isNaN(val)) { el.classList.add('pin-error'); return; }

    // Check valid_pins
    if (validPins && val !== -1 && val !== 255) {
      if (!validPins.includes(val)) {
        el.classList.add('pin-error');
        return;
      }
    }

    // Check duplicates
    if (val !== -1 && val !== 255 && pinValueMap[val] && pinValueMap[val].length > 1) {
      el.classList.add('pin-error');
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
// Reset pins button
// ============================================================
function resetPins() {
  // Step 1: Reset all pin inputs to their stored JSON defaults (or blank)
  document.querySelectorAll('input[data-type="pin"]').forEach(function(el) {
    el.value = (el.dataset.defaultVal !== undefined) ? el.dataset.defaultVal : '';
  });
  // Step 2: Apply board-specific default_pins (overrides matching pins)
  applyDefaultPins();
  validateAllPins();
  updatePreview(); // immediate update on reset (no debounce)
  showAlert('info', 'Pins reset to defaults.', 2000);
}

// ============================================================
// Pin Configuration Preview (live, debounced 5 s)
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
  _previewTimer = setTimeout(updatePreview, 5000);
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
  out += outputLocaleSection();
  out += outputDefaultsSection();
  out += outputTimezoneSection();

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
  var nameData = gData.name;
  if (!nameData || nameData.length < 2) return '';
  var out = sectionHeader(typeof nameData[0] === 'string' ? nameData[0] : 'Firmware File & Board');
  var item = nameData[1];
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
  return sectionHeader(typeof gData.spi[0] === 'string' ? gData.spi[0] : 'SPI BUS PINS') + body;
}

function outputSingleSelectSection(dataArr, secId, selId, defaultTitle) {
  var sectionTitle = typeof dataArr[0] === 'string' ? dataArr[0] : defaultTitle;
  var items = dataArr.slice(1);
  var sel = document.getElementById(selId);
  if (!sel) return '';
  var item = items[parseInt(sel.value)];
  if (!item) return '';

  var out = sectionHeader(sectionTitle);

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
  var sectionTitle = typeof dataArr[0] === 'string' ? dataArr[0] : defaultTitle;
  var items = dataArr.slice(1);
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
  return sectionHeader(sectionTitle) + body;
}

function outputLocaleSection() {
  var chk = document.getElementById('locale-chk');
  if (!chk || !chk.checked) return '';
  var sel = document.getElementById('locale-sel');
  if (!sel) return '';
  var code = sel.value;
  return sectionHeader(typeof gData.locale[0] === 'string' ? gData.locale[0] : 'Locale') +
    outputFlagLine('DSP_LANGUAGE_' + code);
}

function outputDefaultsSection() {
  var sectionTitle = typeof gData.defaults[0] === 'string' ? gData.defaults[0] : 'User Defaults';
  var items = gData.defaults.slice(1);
  var body = '';

  items.forEach(function(item) {
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
  return sectionHeader(sectionTitle) + body;
}

function outputTimezoneSection() {
  var chk = document.getElementById('tz-chk');
  if (!chk || !chk.checked) return '';
  var sel = document.getElementById('tz-sel');
  if (!sel) return '';
  var tzName = sel.value;
  var posix = gData.timezones[tzName] || '';
  var out = sectionHeader('Time Zone');
  out += outputLine('TIMEZONE_NAME', '"' + tzName + '"', null);
  out += outputLine('TIMEZONE_POSIX', '"' + posix + '"', null);
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
    // In the JSON: right_pins = left column of diagram, left_pins = right column
    var diagLeft  = bd.right_pins;
    var diagRight = bd.left_pins;

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

  // board_build.* and board_upload.* fields
  var skipFields = ['name', 'env', 'board', 'build_flags', 'default_pins', 'spi', 'valid_pins', 'right_pins', 'left_pins', 'image', 'url'];
  Object.keys(bd).forEach(function(key) {
    if (skipFields.includes(key)) return;
    if (key.startsWith('board_')) {
      out += key + ' = ' + bd[key] + '\n';
    }
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
    // Keep ${library.xxx} references unresolved - the [library] section is in the header
    var trimmed = val.trim();
    if (!trimmed) return;
    if (!libDepsStr.includes(trimmed)) {
      libDepsStr += (libDepsStr ? '\n  ' : '') + trimmed;
    }
  }

  function addBuildFilter(val) {
    if (!val || val === '') return;
    if (!buildFilterStr.includes(val)) {
      buildFilterStr += val;
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
    out += '  ' + buildFilterStr + '\n';
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
}

function copyFile(type) {
  var content = type === 'options' ? generateOptionsH() : generatePlatformioIni();
  navigator.clipboard.writeText(content).then(function() {
    showAlert('info', type === 'options' ? 'myoptions.h copied to clipboard!' : 'platformio.ini copied to clipboard!', 2500);
  }, function() {
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
function copyLink() {
  var state = serializeState();
  var compressed = LZString.compressToBase64(JSON.stringify(state));
  var url = window.location.href.split('#')[0] + '#' + encodeURIComponent(compressed);

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

  // Board selection - store both index (legacy) and name (stable)
  var boardSel = document.getElementById('board-sel');
  if (boardSel) {
    var _bi = parseInt(boardSel.value);
    state.board = _bi;
    if (gData.boards[_bi]) state.board_name = gData.boards[_bi].name;
  }

  // All inputs
  var inputs = document.querySelectorAll('input[data-define], select[data-define]');
  inputs.forEach(function(el) {
    if (el.type === 'radio' && !el.checked) return;
    if (el.type === 'checkbox' && !el.name) return; // skip item checkboxes
    var key = el.dataset.define;
    if (!key) return;
    state['i_' + key] = el.type === 'checkbox' ? el.checked : el.value;
  });

  // Optional checkboxes (data-opt-for)
  var optChks = document.querySelectorAll('input[type="checkbox"][data-opt-for]');
  optChks.forEach(function(chk) {
    state['opt_' + chk.dataset.optFor] = chk.checked;
  });

  // Section item checkboxes
  var itemChks = document.querySelectorAll('input[type="checkbox"][data-item-id]');
  itemChks.forEach(function(chk) {
    state['item_' + chk.dataset.itemId] = chk.checked;
  });

  // Default item checkboxes
  var defChks = document.querySelectorAll('input[type="checkbox"][data-def-item]');
  defChks.forEach(function(chk) {
    state['def_' + chk.dataset.defItem] = chk.checked;
  });

  // Locale
  var locChk = document.getElementById('locale-chk');
  var locSel = document.getElementById('locale-sel');
  if (locChk) state.locale_enabled = locChk.checked;
  if (locSel) state.locale_val = locSel.value;

  // Timezone
  var tzChk = document.getElementById('tz-chk');
  var tzSel = document.getElementById('tz-sel');
  if (tzChk) state.tz_enabled = tzChk.checked;
  if (tzSel) state.tz_val = tzSel.value;

  // Display / Audio selectors - store define (most stable), name (2nd), index (fallback)
  var dspSel = document.getElementById('dsp-sel');
  if (dspSel) {
    state.dsp_sel = dspSel.value;
    var _dspItems = gData.display.slice(1);
    var _di = parseInt(dspSel.value);
    if (_dspItems[_di]) {
      if (_dspItems[_di].define) state.dsp_define = _dspItems[_di].define;
      state.dsp_name = _dspItems[_di].name;
    }
  }
  var audSel = document.getElementById('aud-sel');
  if (audSel) {
    state.aud_sel = audSel.value;
    var _audItems = gData.audio.slice(1);
    var _ai = parseInt(audSel.value);
    if (_audItems[_ai]) {
      if (_audItems[_ai].define) state.aud_define = _audItems[_ai].define;
      state.aud_name = _audItems[_ai].name;
    }
  }

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
  // Board - prefer name lookup over numeric index for stability across JSON reorders
  var resolvedBoardIdx = 0;
  if (state.board_name) {
    var nameIdx = gData.boards.findIndex(function(b) { return b.name === state.board_name; });
    resolvedBoardIdx = nameIdx >= 0 ? nameIdx : (state.board !== undefined ? state.board : 0);
  } else if (state.board !== undefined) {
    resolvedBoardIdx = state.board;
  }
  var boardSel = document.getElementById('board-sel');
  if (boardSel) boardSel.value = resolvedBoardIdx;

  // Load board data first, then apply rest
  var boardEntry = gData.boards[resolvedBoardIdx];
  fetchJSON(boardEntry.file).then(function(data) {
    gData.boardData = data[0];
    updateBoardImageArea();
    updateSPISections();
    applyDefaultPins();

    // Display lookup priority:
    //   1. define + name together (handles shared defines like DSP_MODEL DSP_ST7735)
    //   2. name alone (stable human label, distinguishes variants)
    //   3. define alone (rescue if name changed slightly)
    //   4. numeric index (legacy URL fallback)
    if (state.dsp_sel !== undefined || state.dsp_name !== undefined || state.dsp_define !== undefined) {
      var dspSel = document.getElementById('dsp-sel');
      if (dspSel) {
        var _dsp = gData.display.slice(1);
        var dspIdx = -1;
        if (state.dsp_define && state.dsp_name)
          dspIdx = _dsp.findIndex(function(d) { return d.define === state.dsp_define && d.name === state.dsp_name; });
        if (dspIdx < 0 && state.dsp_name)
          dspIdx = _dsp.findIndex(function(d) { return d.name === state.dsp_name; });
        if (dspIdx < 0 && state.dsp_define)
          dspIdx = _dsp.findIndex(function(d) { return d.define === state.dsp_define; });
        dspSel.value = dspIdx >= 0 ? dspIdx : (state.dsp_sel || 0);
        onDisplayChange();
      }
    } else {
      onDisplayChange();
    }
    // Audio lookup priority: same four-step chain
    if (state.aud_sel !== undefined || state.aud_name !== undefined || state.aud_define !== undefined) {
      var audSel = document.getElementById('aud-sel');
      if (audSel) {
        var _aud = gData.audio.slice(1);
        var audIdx = -1;
        if (state.aud_define && state.aud_name)
          audIdx = _aud.findIndex(function(a) { return a.define === state.aud_define && a.name === state.aud_name; });
        if (audIdx < 0 && state.aud_name)
          audIdx = _aud.findIndex(function(a) { return a.name === state.aud_name; });
        if (audIdx < 0 && state.aud_define)
          audIdx = _aud.findIndex(function(a) { return a.define === state.aud_define; });
        audSel.value = audIdx >= 0 ? audIdx : (state.aud_sel || 0);
        onAudioChange();
      }
    } else {
      onAudioChange();
    }

    // Apply all control values
    setTimeout(function() {
      applyStateValues(state);
    }, 50);
  });
}

function applyStateValues(state) {
  // Section item checkboxes (must do first to reveal bodies)
  Object.keys(state).forEach(function(k) {
    if (k.startsWith('item_')) {
      var itemId = k.substring(5);
      var chk = document.getElementById(itemId + '-chk');
      if (chk && state[k]) {
        chk.checked = true;
        var body = document.getElementById(itemId + '-body');
        if (body) {
          body.classList.remove('hidden');
          var nameEl = chk.nextElementSibling;
          if (nameEl) nameEl.style.color = '#fff';
        }
      }
    }
  });

  // Input values
  Object.keys(state).forEach(function(k) {
    if (k.startsWith('i_')) {
      var defName = k.substring(2);
      var val = state[k];
      // Find all matching inputs (for radio groups)
      var els = document.querySelectorAll('[data-define="' + defName + '"]');
      els.forEach(function(el) {
        if (el.type === 'radio') {
          if (el.value === String(val)) el.checked = true;
        } else if (el.type === 'checkbox') {
          el.checked = !!val;
        } else {
          el.value = val;
        }
      });
    }
  });

  // Optional checkboxes
  Object.keys(state).forEach(function(k) {
    if (k.startsWith('opt_')) {
      var defName = k.substring(4);
      var chks = document.querySelectorAll('input[type="checkbox"][data-opt-for="' + defName + '"]');
      chks.forEach(function(chk) {
        chk.checked = !!state[k];
        var ctrlDiv = chk.closest('.ctrl-cell').querySelector('div[id]');
        var lbl = chk.closest('.opt-label');
        if (chk.checked) {
          if (ctrlDiv) ctrlDiv.classList.remove('hidden');
          if (lbl) lbl.classList.add('checked');
        } else {
          if (ctrlDiv) ctrlDiv.classList.add('hidden');
          if (lbl) lbl.classList.remove('checked');
        }
      });
    }
  });

  // Default item checkboxes
  Object.keys(state).forEach(function(k) {
    if (k.startsWith('def_')) {
      var defName = k.substring(4);
      var chk = document.querySelector('input[type="checkbox"][data-def-item="' + defName + '"]');
      if (chk && state[k]) {
        chk.checked = true;
        var ctrlDiv = chk.closest('.default-item').querySelector('.default-item-ctrl');
        if (ctrlDiv) ctrlDiv.classList.remove('hidden');
        var lbl = chk.nextElementSibling;
        if (lbl) lbl.style.color = '#fff';
      }
    }
  });

  // Locale
  if (state.locale_enabled) {
    var locChk = document.getElementById('locale-chk');
    var locCtrl = document.getElementById('locale-ctrl');
    if (locChk) { locChk.checked = true; if (locCtrl) locCtrl.classList.remove('hidden'); }
  }
  if (state.locale_val) {
    var locSel = document.getElementById('locale-sel');
    if (locSel) locSel.value = state.locale_val;
  }

  // Timezone
  if (state.tz_enabled) {
    var tzChk = document.getElementById('tz-chk');
    var tzCtrl = document.getElementById('tz-ctrl');
    if (tzChk) { tzChk.checked = true; if (tzCtrl) tzCtrl.classList.remove('hidden'); }
  }
  if (state.tz_val) {
    var tzSel = document.getElementById('tz-sel');
    if (tzSel) tzSel.value = state.tz_val;
  }

  validateAllPins();
  updatePreview();
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
