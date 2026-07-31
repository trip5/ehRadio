// ===== i18n loader: loads locale.json for translations =====
var i18n = {};

// Read ?l=XX from URL to request the correct locale directly from the server
var urlLocale = (window.location.search.match(/l=([^&]+)/) || [])[1] || '';
var localeUrl = 'locale.json' + (urlLocale ? '?l=' + urlLocale : '');

// Fetch locale.json — if 404, fall back to hardcoded text
var localePromise = fetch(localeUrl)
      .then(function(r){ return r.ok ? r.json() : Promise.reject('not-ok'); })
      .then(function(data){
          i18n = data;
          applyI18n(); // Only apply translations when successfully loaded
      })
      .catch(function(){
          console.warn('locale.json not found or failed to load, using hardcoded HTML text');
          // Don't run applyI18n() - let HTML fallbacks handle it
      });

function t(key, defaultText) {
  var args = Array.prototype.slice.call(arguments, 2);
  var s = (i18n && i18n[key]) ? i18n[key] : (defaultText || key);
  args.forEach(function(a, i){ s = s.replace('{' + i + '}', a); });
  return s;
}

function applyI18n(root) {
  (root || document).querySelectorAll('[data-i18n]').forEach(function(el) {
    var key = el.dataset.i18n;
    var val = i18n[key];
    if (!val) return;
    if (el.hasAttribute('title')) {
      el.title = val;
    } else if (el.hasAttribute('alt')) {
      el.alt = val;
    } else if (el.tagName === 'INPUT' && (el.type === 'button' || el.type === 'submit')) {
      el.value = val;
    } else if (el.tagName === 'INPUT' && el.placeholder !== undefined) {
      el.placeholder = val;
    } else if (el.tagName === 'INPUT') {
      el.value = val;
    } else {
      el.textContent = val;
    }
  });
  // Update knob on/off labels via CSS variables (must be quoted for content property)
  document.documentElement.style.setProperty('--knob-off', '"' + t('lbl_off', 'Off') + '"');
  document.documentElement.style.setProperty('--knob-on', '"' + t('lbl_on', 'On') + '"');
  applyCaseTransform();
}

// Apply text-transform casing based on server-injected 'casetransform' flag (from variables.js)
function applyCaseTransform() {
  if (typeof casetransform !== 'undefined' && casetransform) {
    document.documentElement.style.setProperty('--tt-uppercase', 'uppercase');
    document.documentElement.style.setProperty('--tt-lowercase', 'lowercase');
  }
}
