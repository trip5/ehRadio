/*
 * Copyright (c) 2026 Aivaras Kasperaitis (@kasperaitis)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "battery.h"

#if (defined(BATTERY_PIN) && (BATTERY_PIN!=255)) || (defined(BATTERY_CHARGE_PIN) && (BATTERY_CHARGE_PIN!=255))
#include <stdarg.h>
#include "common.h"
#include "config.h"
#include "display.h"
#include "netserver.h"
#include "telnet.h"

static bool _battery_inited = false;   // ADC available
static bool _charge_pin_present = false; // Charge-status pin available
static BatteryStatus _battery_status = {0, 0, false, false, 0, false, false, false, false, 0, false, false}; // added voltage_rate_valid and discharging_inferred
static unsigned long _last_read = 0;
static int16_t _last_percentage = -1;  // Track last percentage (-1 = unset)
static bool _last_low_battery = false;  // Track low battery state changes
static bool _last_critical_battery = false;  // Track critical battery state changes
static bool _last_charging = false; // Track charging state changes

// Inferred charging state when no charge pin is available
static bool _inferred_charging = false;
static bool _inferred_discharging = false;
// Remembered thresholds for inference hysteresis (peak/trough)
// - Remember peaks/troughs in percentage to make inference consistent with percent-based detection
// - _inferred_remember_peak_pct: last observed charging peak percentage
// - _inferred_remember_noncharging_pct: last observed non-charging percentage (trough)
static int _inferred_remember_peak_pct = -1;
static int _inferred_remember_noncharging_pct = -1;
// Time-gating for charging/discharging inference to prevent false positives from temporary spikes
static unsigned long _inferred_charging_candidate_start = 0;
static bool _inferred_charging_candidate = false;
static int _inferred_charging_candidate_start_pct = -1;
static unsigned long _inferred_discharging_candidate_start = 0;
static bool _inferred_discharging_candidate = false;
static int _inferred_discharging_candidate_start_pct = -1;
// Computed hold window (ms) = BATTERY_CHARGE_INFER_HOLD_SAMPLES * BATTERY_UPDATE_INTERVAL
static unsigned long _charge_infer_hold_ms = 0;

/* Time-gated percent samples for sliding-window confirmation */
static const int _pct_sample_max = 64;
struct _pct_sample_t { unsigned long t; int pct; };
static _pct_sample_t _pct_samples[_pct_sample_max];
static int _pct_head = 0;
static int _pct_count = 0;

/* EMA smoothing state (mV) */
static int32_t _ema_voltage_mv = 0; /* integer EMA (mV) */
static const int _ema_q_scale = 256;
// Fixed-point EMA alpha (Q256) hardcoded to 0.30 -> 0.30 * 256 ≈ 76.8 -> 77
static const int _ema_alpha_q = 77;
// Precomputed divider ratio scaled by 100 to avoid float in hot path
static const uint32_t _divider_ratio_x100 = (uint32_t)(BATTERY_DIVIDER_RATIO * 100.0 + 0.5);
// Rate-of-change tracking for charging detection
static uint32_t _last_voltage_mv = 0;
static unsigned long _last_voltage_time = 0; 


// Battery presence detection range (in millivolts)
/* BATTERY_PRESENT_MIN_MV and BATTERY_PRESENT_MAX_MV moved to src/core/options.h. */
static uint32_t _present_min_mv = (uint32_t)BATTERY_PRESENT_MIN_MV;
static uint32_t _present_max_mv = (uint32_t)BATTERY_PRESENT_MAX_MV;

// Debug helper (wrap telnet printf to avoid code duplication)
#ifdef BATTERY_DEBUG
static void _dbg_printf(const char* fmt, ...) {
  char _dbgbuf[160];
  va_list ap; va_start(ap, fmt);
  vsnprintf(_dbgbuf, sizeof(_dbgbuf), fmt, ap);
  va_end(ap);
  /* Do not append CRLF here: callers often include their own trailing "\r\n".
     Print the buffer verbatim to avoid producing an extra blank line. */
  telnet.printf("%s", _dbgbuf);
}
#else
static inline void _dbg_printf(const char*, ...) {}
#endif

// Candidate helpers
static void _start_charging_candidate(unsigned long now, const char* dbg_fmt, int pct_diff) {
  _inferred_charging_candidate = true;
  _inferred_charging_candidate_start = now;
  _inferred_charging_candidate_start_pct = _battery_status.percentage;
  _battery_status.charging = false;
  _battery_status.discharging_inferred = false;
  #ifdef BATTERY_DEBUG
    _dbg_printf(dbg_fmt, _inferred_charging_candidate_start_pct, pct_diff);
  #endif
}
static void _start_discharging_candidate(unsigned long now, const char* dbg_fmt, int pct_diff) {
  _inferred_discharging_candidate = true;
  _inferred_discharging_candidate_start = now;
  _inferred_discharging_candidate_start_pct = _battery_status.percentage;
  _battery_status.charging = false;
  _battery_status.discharging_inferred = false;
  #ifdef BATTERY_DEBUG
    _dbg_printf(dbg_fmt, _inferred_discharging_candidate_start_pct, pct_diff);
  #endif
}

// Clear both inferred states
static void _clear_inferred_states() {
  _inferred_charging = false;
  _inferred_discharging = false;
  _inferred_charging_candidate = false;
  _inferred_discharging_candidate = false;
} 

// Helper functions
static uint16_t _read_adc_median() {
  /* Read BATTERY_SAMPLES ADC values and return the median sample to avoid spikes. */
  uint16_t samples[BATTERY_SAMPLES];
  for (int i = 0; i < BATTERY_SAMPLES; ++i) {
    samples[i] = (uint16_t)analogRead(BATTERY_PIN);
    yield();
  }
  /* Simple insertion sort (BATTERY_SAMPLES is small) */
  for (int i = 1; i < BATTERY_SAMPLES; ++i) {
    uint16_t key = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      --j;
    }
    samples[j + 1] = key;
  }
  return samples[BATTERY_SAMPLES / 2];
}

static uint32_t _calculate_voltage(uint16_t adc_avg) {
  uint32_t adc_ref = config.store.battery_adc_ref_mv ? 
                     config.store.battery_adc_ref_mv : BATTERY_ADC_REF_MV;
  // Validate ADC reference is in reasonable range (2000-4000mV)
  if (adc_ref < 2000 || adc_ref > 4000) {
    #ifdef BATTERY_DEBUG
      Serial.printf("##[BATTERY WARNING]#: Invalid ADC ref %umV, using default %dmV\r\n", 
                    (unsigned)adc_ref, BATTERY_ADC_REF_MV);
    #endif
    adc_ref = BATTERY_ADC_REF_MV;
  }
  // Use uint64_t to prevent overflow, use precomputed divider ratio scaled by 100
  return ((uint64_t)adc_avg * adc_ref * (uint64_t)_divider_ratio_x100) / (4095 * 100);
}

// Battery discharge curve defaults are defined in src/core/options.h (override in myoptions.h)
static const uint16_t _battery_curve_mv[] = { BATTERY_CURVE_MV };
static const uint8_t _battery_curve_pct[] = { BATTERY_CURVE_PCT };

static uint8_t _calculate_percentage(uint32_t voltage_mv) {
  /* Piecewise LiPo discharge curve (mV -> %).
     Uses _battery_curve_mv and _battery_curve_pct arrays defined above. */
  const uint16_t* curve_mv = _battery_curve_mv;
  const uint8_t* curve_pct = _battery_curve_pct;
  const int points = sizeof(_battery_curve_mv)/sizeof(_battery_curve_mv[0]);
  const int points_pct = sizeof(_battery_curve_pct)/sizeof(_battery_curve_pct[0]);
  
  // Compile-time check for curve size mismatch (shows warning during build)
  #if defined(BATTERY_CURVE_MV) && defined(BATTERY_CURVE_PCT)
    // User-defined curves - can't check at compile time, but warn if mismatch at runtime
    #ifdef BATTERY_DEBUG
      if (points != points_pct) {
        Serial.printf("##[BATTERY]#: Curve size mismatch (%d vs %d) - using available range\r\n", points, points_pct);
      }
    #endif
  #else
    // Using default curves - verify they match at compile time
    static_assert(sizeof(_battery_curve_mv)/sizeof(_battery_curve_mv[0]) == sizeof(_battery_curve_pct)/sizeof(_battery_curve_pct[0]),
                  "BATTERY: Default discharge curve size mismatch! curve_mv and curve_pct arrays must have same length.");
  #endif

  if (voltage_mv >= curve_mv[0]) return curve_pct[0];
  if (voltage_mv <= curve_mv[points-1]) return curve_pct[points-1];

  for (int i = 0; i < points - 1; ++i) {
    uint16_t hi_mv = curve_mv[i];
    uint16_t lo_mv = curve_mv[i+1];
    uint8_t  hi_pct = curve_pct[i];
    uint8_t  lo_pct = curve_pct[i+1];
    if (voltage_mv <= hi_mv && voltage_mv >= lo_mv) {
      uint32_t span_mv = (uint32_t)hi_mv - (uint32_t)lo_mv;
      if (span_mv == 0) return lo_pct;
      uint32_t delta_mv = (uint32_t)voltage_mv - (uint32_t)lo_mv;
      uint32_t pct = lo_pct + (delta_mv * (uint32_t)(hi_pct - lo_pct)) / span_mv;
      return (uint8_t)pct;
    }
  }
  return 0;
}

// Helper: Handle candidate expiration/confirmation for charging/discharging
static void _handle_candidate_expiry(bool charging) {
  unsigned long now = millis();
  // Helper: compute min/max percent within the last window_ms (time-gated sliding window)
  auto _compute_window_minmax = [&](int &min_pct, int &max_pct, unsigned long window_ms) {
    min_pct = 127; max_pct = -127;
    if (_pct_count == 0) {
      min_pct = max_pct = _battery_status.percentage;
      return;
    }
    for (int i = 0; i < _pct_count; ++i) {
      int idx = (_pct_head - 1 - i + _pct_sample_max) % _pct_sample_max;
      unsigned long t = _pct_samples[idx].t;
      if (now - t > window_ms) break;
      int p = _pct_samples[idx].pct;
      if (p < min_pct) min_pct = p;
      if (p > max_pct) max_pct = p;
    }
    if (min_pct == 127 && max_pct == -127) {
      min_pct = max_pct = _battery_status.percentage;
    }
  };

  if (charging) {
    if (_inferred_charging_candidate && (now - _inferred_charging_candidate_start) >= _charge_infer_hold_ms) {
      unsigned long dt = now - _inferred_charging_candidate_start;

      int window_min = 0, window_max = 0;
      _compute_window_minmax(window_min, window_max, _charge_infer_hold_ms);
      int latest = _battery_status.percentage;
      int pct_diff_window = latest - window_min;

      if (pct_diff_window >= (int)BATTERY_SUSTAINED_PERCENT_WINDOW_THRESHOLD) {
        // Confirm charging
        _inferred_charging = true;
        _battery_status.charging = true;
        _inferred_charging_candidate = false;
        _inferred_discharging_candidate = false;
        _inferred_remember_peak_pct = _battery_status.percentage;
        _inferred_charging_candidate_start_pct = -1;
        _battery_status.discharging_inferred = false;
        #ifdef BATTERY_DEBUG
          telnet.printf("##[BATTERY]#: Charging inferred (time-gated, latest %d%%, delta %d%%)\r\n", latest, pct_diff_window);
        #endif
      } else {
        // Expired without confirmation
        _inferred_charging_candidate = false;
        _inferred_charging_candidate_start_pct = -1;
        #ifdef BATTERY_DEBUG
          telnet.printf("##[BATTERY]#: Charging candidate expired without confirmation (latest %d%%, window_min %d%%, delta %d%% over %lums)\r\n", latest, window_min, pct_diff_window, dt);
        #endif
      }
    }
  } else {
    if (_inferred_discharging_candidate && (now - _inferred_discharging_candidate_start) >= _charge_infer_hold_ms) {
      unsigned long dt = now - _inferred_discharging_candidate_start;

      int window_min = 0, window_max = 0;
      _compute_window_minmax(window_min, window_max, _charge_infer_hold_ms);
      int latest = _battery_status.percentage;
      int pct_diff_window = window_max - latest;

      if (pct_diff_window >= (int)BATTERY_SUSTAINED_PERCENT_WINDOW_THRESHOLD) {
        // Confirm discharging
        _inferred_discharging = true;
        _battery_status.discharging_inferred = true;
        _inferred_discharging_candidate = false;
        _inferred_charging_candidate = false;
        _inferred_discharging_candidate_start_pct = -1;
        _battery_status.charging = false;
        _inferred_remember_noncharging_pct = _battery_status.percentage; // Start remembering trough on confirmed discharging
        #ifdef BATTERY_DEBUG
          telnet.printf("##[BATTERY]#: Discharging inferred (time-gated, latest %d%%, delta %d%%)\r\n", latest, pct_diff_window);
        #endif
      } else {
        // Expired without confirmation
        _inferred_discharging_candidate = false;
        _inferred_discharging_candidate_start_pct = -1;
        #ifdef BATTERY_DEBUG
          telnet.printf("##[BATTERY]#: Discharging candidate expired without confirmation (latest %d%%, window_max %d%%, delta %d%% over %lums)\r\n", latest, window_max, pct_diff_window, dt);
        #endif
      }
    }
  }
}

void battery_init() {
  // Initialize ADC if BATTERY_PIN available
  #if defined(BATTERY_PIN) && (BATTERY_PIN!=255)
    pinMode(BATTERY_PIN, INPUT);
    analogSetAttenuation(ADC_11db);  // 0-3.3V range
    _battery_inited = true;
  #endif

  // Initialize charge status pin if present (TP4054 CHRG is active LOW)
  #if defined(BATTERY_CHARGE_PIN) && (BATTERY_CHARGE_PIN!=255)
    pinMode(BATTERY_CHARGE_PIN, INPUT_PULLUP);
    _charge_pin_present = true;
  #endif

  // Initialize and sanity-check presence thresholds
  _present_min_mv = (uint32_t)BATTERY_PRESENT_MIN_MV;
  _present_max_mv = (uint32_t)BATTERY_PRESENT_MAX_MV;
  if (_present_min_mv < 2500) {
    Serial.printf("##[BATTERY WARNING]#: BATTERY_PRESENT_MIN_MV too low (%u), clamping to 2500mV\r\n", (unsigned)_present_min_mv);
    _present_min_mv = 2500;
  }
  if (_present_max_mv > 5000) {
    Serial.printf("##[BATTERY WARNING]#: BATTERY_PRESENT_MAX_MV too high (%u), clamping to 5000mV\r\n", (unsigned)_present_max_mv);
    _present_max_mv = 5000;
  }
  if (_present_min_mv >= _present_max_mv) {
    Serial.printf("##[BATTERY WARNING]#: BATTERY_PRESENT_MIN_MV >= MAX (%u >= %u), resetting to 3000/4200\r\n", (unsigned)_present_min_mv, (unsigned)_present_max_mv);
    _present_min_mv = 3000;
    _present_max_mv = 4200;
  }

  // Compute the hold window in milliseconds from configured number of measurements
  _charge_infer_hold_ms = (unsigned long)BATTERY_CHARGE_INFER_HOLD_SAMPLES * (unsigned long)BATTERY_UPDATE_INTERVAL;
  if (_charge_infer_hold_ms == 0) {
    _charge_infer_hold_ms = (unsigned long)BATTERY_UPDATE_INTERVAL * 3; // fallback
  }

} // end battery_init

void battery_boot_status() {
  // Run boot status when either ADC or CHARGE pin is configured
  #if (defined(BATTERY_PIN) && (BATTERY_PIN!=255)) || (defined(BATTERY_CHARGE_PIN) && (BATTERY_CHARGE_PIN!=255))
    if (!_battery_inited && !_charge_pin_present) return;
    
    // If charge pin present, read initial charging state first
    #if defined(BATTERY_CHARGE_PIN) && (BATTERY_CHARGE_PIN!=255)
      bool charging = (digitalRead(BATTERY_CHARGE_PIN) == LOW); // active LOW
      _battery_status.charging = charging;
      _last_charging = charging;
    #endif
    
    // If ADC available, do initial reading to show battery status at boot
    if (_battery_inited) {
      delay(10);  // Let ADC stabilize (shorter to reduce blocking)
      uint16_t adc_sample = _read_adc_median();
      uint32_t voltage_mv = _calculate_voltage(adc_sample);

      _ema_voltage_mv = (int32_t)voltage_mv; /* initialize EMA at boot */
      _last_read = millis(); /* Set last read time to prevent immediate re-reading in battery_loop */

      // Check if battery is present
      bool present = (voltage_mv >= _present_min_mv && voltage_mv <= _present_max_mv);
      _battery_status.present = present;
      if (present) {
        uint8_t percentage = _calculate_percentage(voltage_mv);
        
        // Save initial readings to avoid false triggers on first loop
        _last_percentage = percentage;
        _battery_status.percentage = percentage;
        _battery_status.adc = adc_sample;
        _battery_status.voltage_mv = voltage_mv;
        _battery_status.valid = true; /* mark valid so GETBATTERY handlers will surface value immediately */

        // Print combined boot status with charging state if available
        if (_battery_status.charging) {
          Serial.printf("##[BOOT]#\tbattery\t\tADC:%d, %dmV, %d%%, Charging\r\n", adc_sample, voltage_mv, percentage);
        } else {
          Serial.printf("##[BOOT]#\tbattery\t\tADC:%d, %dmV, %d%%\r\n", adc_sample, voltage_mv, percentage);
        }
        // Update display immediately
        display.putRequest(DSPBATTERY, 0);
      } else {
        Serial.println("##[BOOT]#\tbattery\t\tnot detected");
        _battery_status.valid = false;
        _battery_status.present = false;
        _battery_status.percentage = 0;
        _battery_status.adc = 0;
        _battery_status.voltage_mv = 0;
        display.putRequest(DSPBATTERY, 0);
      }
    }

    /* Notify web clients about the initial battery status so UI can show it immediately */
    netserver.requestOnChange(GETBATTERY, 0);
  #endif
} 

// Return true if either ADC or charge pin is initialized
bool battery_is_initialized() {
  return (_battery_inited || _charge_pin_present);
}

const BatteryStatus& battery_get_status() {
  return _battery_status;
}

// Format battery status line to buffer (shared by CLI and debug output)
void battery_format_status_line(const BatteryStatus& status, char* buffer, size_t buffer_size, bool include_warning) {
  if (!status.valid) {
    snprintf(buffer, buffer_size, "##CLI.BATTERY#: not detected");
    return;
  }
  
  const char *warning = "";
  if (include_warning) {
    warning = status.critical_battery ? " [CRITICAL!]" : (status.low_battery ? " [LOW]" : "");
  }
  
  // Add voltage rate and remembered peak/trough info
  char remembered[80] = "";
  char rate_str[20] = "";
  
  // Show voltage rate if it has been calculated (requires 2+ readings)
  if (status.voltage_rate_valid) {
    snprintf(rate_str, sizeof(rate_str), " (%+ldmV/min)", status.voltage_rate);
  }
  
  // Add peak/trough info when available - show each independently if set
  if (_inferred_remember_peak_pct >= 0 && _inferred_remember_noncharging_pct >= 0)
    snprintf(remembered, sizeof(remembered), ", Peak:%d%%, Trough:%d%%", _inferred_remember_peak_pct, _inferred_remember_noncharging_pct);
  else if (_inferred_remember_peak_pct >= 0)
    snprintf(remembered, sizeof(remembered), ", Peak:%d%%", _inferred_remember_peak_pct);
  else if (_inferred_remember_noncharging_pct >= 0)
    snprintf(remembered, sizeof(remembered), ", Trough:%d%%", _inferred_remember_noncharging_pct);
  
  if (status.charging) {
    if (status.charging_inferred)
      snprintf(buffer, buffer_size, "##CLI.BATTERY#: ADC:%d, Volt:%dmV, %d%%%s%s, Charging (inferred%s)",
               status.adc, status.voltage_mv, status.percentage, warning, rate_str, remembered);
    else
      snprintf(buffer, buffer_size, "##CLI.BATTERY#: ADC:%d, Volt:%dmV, %d%%%s%s, Charging%s",
               status.adc, status.voltage_mv, status.percentage, warning, rate_str, remembered);
  } else if (status.discharging_inferred) {
    snprintf(buffer, buffer_size, "##CLI.BATTERY#: ADC:%d, Volt:%dmV, %d%%%s%s, Discharging (inferred%s)",
             status.adc, status.voltage_mv, status.percentage, warning, rate_str, remembered);
  } else {
    snprintf(buffer, buffer_size, "##CLI.BATTERY#: ADC:%d, Volt:%dmV, %d%%%s%s%s",
             status.adc, status.voltage_mv, status.percentage, warning, rate_str, remembered);
  }
}

// Internal helper: Read ADC, calculate voltage/percentage, update trends and status
static void _battery_read_and_update() {
  unsigned long now = millis();
  uint16_t adc_avg = 0;
  uint32_t voltage_mv = 0;

  // If ADC available, read it and compute volt/percentage
  if (_battery_inited) {
    uint16_t adc_sample = _read_adc_median();
    uint32_t raw_voltage_mv = _calculate_voltage(adc_sample);

    // Battery detection: Check if voltage is in valid Li-Po range
    if (raw_voltage_mv < _present_min_mv || raw_voltage_mv > _present_max_mv) {
      _battery_status.valid = false;
      _battery_status.present = false;
      _battery_status.charging = false;
      _battery_status.discharging_inferred = false;
      // Reset tracking state to avoid stale data on reconnect
      _last_voltage_mv = 0;
      _last_voltage_time = 0;
      _inferred_charging = false;
      _inferred_discharging = false;
      _inferred_charging_candidate = false;
      _inferred_discharging_candidate = false;
      _battery_status.voltage_rate_valid = false; // percent rate removed
      #ifdef BATTERY_DEBUG
        telnet.printf("##[BATTERY WARNING]#: No battery detected, %dmV out of range\r\n", raw_voltage_mv);
      #endif

      _ema_voltage_mv = 0; // reset EMA when battery absent

      return;
    }



    // Smooth voltage with integer EMA (fixed-point, Q format) and compute percentage from EMA
    if (_ema_voltage_mv <= 0) _ema_voltage_mv = (int32_t)raw_voltage_mv;
    else {
      _ema_voltage_mv = (int32_t)(( (int64_t)_ema_alpha_q * raw_voltage_mv + (int64_t)(_ema_q_scale - _ema_alpha_q) * _ema_voltage_mv + (_ema_q_scale/2) ) / _ema_q_scale);
    }
    voltage_mv = (uint32_t)_ema_voltage_mv; 

    // Calculate percentage (linear interpolation) from smoothed voltage
    uint8_t percentage = _calculate_percentage(voltage_mv);

    // Save raw ADC sample for external reporting
    _battery_status.adc = adc_sample;

    // Update status struct with all information
    _battery_status.voltage_mv = voltage_mv;
    _battery_status.percentage = percentage;

    // Append percent sample for sliding-window confirmation logic
    unsigned long _pct_now = now;
    _pct_samples[_pct_head].t = _pct_now;
    _pct_samples[_pct_head].pct = _battery_status.percentage;
    _pct_head = (_pct_head + 1) % _pct_sample_max;
    if (_pct_count < _pct_sample_max) _pct_count++;

    _battery_status.valid = true;
    _battery_status.present = true;

    // Low/critical thresholds
    bool low_batt = (_battery_status.percentage <= BATTERY_LOW_THRESHOLD);
    bool crit_batt = (_battery_status.percentage <= BATTERY_CRITICAL_THRESHOLD);
    _battery_status.low_battery = low_batt;
    _battery_status.critical_battery = crit_batt;

    // Print low battery warnings on state change
    if (crit_batt && !_last_critical_battery) {
      telnet.printf("##[BATTERY WARNING]#: CRITICAL BATTERY! %d%% remaining (%dmV)\r\n", 
                    _battery_status.percentage, voltage_mv);
    } else if (low_batt && !_last_low_battery && !crit_batt) {
      telnet.printf("##[BATTERY WARNING]#: Low battery! %d%% remaining (%dmV)\r\n", 
                    _battery_status.percentage, voltage_mv);
    } else if (!low_batt && _last_low_battery) {
      telnet.printf("##[BATTERY]#: Battery level normal (%d%%)\r\n", _battery_status.percentage);
    }

    // Note: Don't update _last_low_battery and _last_critical_battery here - 
    // they're updated after display update to detect changes
  } else {
    // No ADC: mark as not valid / not present
    _battery_status.valid = false;
    _battery_status.present = false;
  }

  // Calculate voltage rate-of-change (mV per minute) - needed regardless of charge pin
  uint32_t prev_voltage_mv = _last_voltage_mv;
  int32_t voltage_rate = 0;
  bool rate_calculated = false;

  /* Percentage-based helpers (declared here so they are visible to the later inference logic) */
  int32_t percent_diff = 0;
  bool sudden_spike_pct = false;
  bool sudden_drop_pct = false;
  
  if (_last_voltage_time > 0 && _last_voltage_mv > 0) {
    unsigned long time_diff = now - _last_voltage_time;
    if (time_diff > 0) {
      // Calculate rate using int64_t to prevent overflow: (voltage_change * 60000ms) / time_diff
      int64_t voltage_diff = (int64_t)voltage_mv - (int64_t)prev_voltage_mv;
      voltage_rate = (int32_t)((voltage_diff * 60000LL) / (int64_t)time_diff);
      rate_calculated = true;
      

      // Compute percentage rate-of-change (percent per minute) and instantaneous percent diff
      percent_diff = 0;
      sudden_spike_pct = false;
      sudden_drop_pct = false;
      if (_last_percentage >= 0) {
        percent_diff = (int32_t)_battery_status.percentage - (int32_t)_last_percentage;

        /* Immediate percent thresholds (single-reading change). */
        sudden_spike_pct = (percent_diff >= BATTERY_IMMEDIATE_PERCENT_THRESHOLD);
        sudden_drop_pct = (percent_diff <= -BATTERY_IMMEDIATE_PERCENT_THRESHOLD);
      }
    }
  }
  
  // Store voltage rate in status for external access
  _battery_status.voltage_rate = voltage_rate;
  if (rate_calculated) {
    _battery_status.voltage_rate_valid = true;
  } else {
    _battery_status.voltage_rate_valid = false;
  }

  (void)percent_diff; // keep compiler happy when built without BATTERY_DEBUG

  
  // Update voltage tracking for next calculation
  _last_voltage_mv = voltage_mv;
  _last_voltage_time = now;

  // Read charge pin if present
  if (_charge_pin_present) {
    bool charging = (digitalRead(BATTERY_CHARGE_PIN) == LOW); // active LOW
    _battery_status.charging = charging;
    if (charging && !_last_charging) {
      telnet.printf("##[BATTERY]#: Charging started\r\n");
    } else if (!charging && _last_charging) {
      telnet.printf("##[BATTERY]#: Charging stopped\r\n");
    }
  } else if (rate_calculated) {
    // No charge pin: infer charging/discharging state from voltage rate
    
    // Calculate curve points once for all inference logic
    const int curve_points = sizeof(_battery_curve_mv)/sizeof(_battery_curve_mv[0]);
    uint32_t threshold_95pct = (curve_points > 1) ? _battery_curve_mv[1] : 4100;
    uint32_t threshold_55pct = (curve_points > 5) ? _battery_curve_mv[5] : 3700;
    uint32_t threshold_10pct = (curve_points > 7) ? _battery_curve_mv[7] : 3400;

    if (!_inferred_charging && !_inferred_discharging) {
      // State: Neither charging nor discharging
      // Detect charging by: sudden voltage spike OR sustained voltage increase rate
      
      /* Use percentage-based thresholds for inference instead of mV-based thresholds. */

      /* Immediate confirmation on single-reading percent jump (spike). This is separate from trend detection. */
      if (sudden_drop_pct) {
        _inferred_discharging = true;
        _battery_status.discharging_inferred = true;
        _inferred_discharging_candidate = false;
        _inferred_charging_candidate = false;
        _battery_status.charging = false;
        #ifdef BATTERY_DEBUG
          telnet.printf("##[BATTERY]#: Discharging inferred (immediate, %d%% change)\r\n", (int)(_battery_status.percentage - _last_percentage));
        #endif
      } else if (sudden_spike_pct) {
        _inferred_charging = true;
        _battery_status.charging = true;
        _inferred_charging_candidate = false;
        _inferred_discharging_candidate = false;
        _inferred_remember_peak_pct = _battery_status.percentage;
        _battery_status.discharging_inferred = false;
        #ifdef BATTERY_DEBUG
          telnet.printf("##[BATTERY]#: Charging inferred (immediate, %d%% change)\r\n", (int)(_battery_status.percentage - _last_percentage));
        #endif
      }
      
      // Handle candidate confirmation or expiration using window-delta
      _handle_candidate_expiry(true);
      _handle_candidate_expiry(false);

      // If a time-gated confirmation just changed the state, skip starting new candidates
      if (_inferred_charging || _inferred_discharging) {
        /* State confirmed; nothing further to do in neutral processing */
      } else {
        // Check for small single-reading percent changes to start candidates (not immediate confirm)
        if (_last_percentage >= 0) {
          int pct_diff_now = (int)_battery_status.percentage - (int)_last_percentage;
          int dir_now = 0; /* -1 = discharging, +1 = charging */
          if (pct_diff_now <= -BATTERY_CANDIDATE_PERCENT_DELTA) dir_now = -1;
          else if (pct_diff_now >= BATTERY_CANDIDATE_PERCENT_DELTA) dir_now = 1;  

          if (dir_now == 0) {
            /* No significant delta: nothing to do */
          } else {
            /* Start candidate immediately on a qualifying delta */
            if (dir_now < 0) {
              // Start discharging candidate based on percent delta
              _inferred_charging_candidate = false;
              if (!_inferred_discharging_candidate) {
                _start_discharging_candidate(now, "##[BATTERY]#: Discharging candidate started (candidate, start %d%%, pct-diff %+d%%)\r\n", pct_diff_now);
              }
            } else {
              // Start charging candidate based on percent delta
              _inferred_discharging_candidate = false;
              if (!_inferred_charging_candidate) {
                _start_charging_candidate(now, "##[BATTERY]#: Charging candidate started (candidate, start %d%%, pct-diff %+d%%)\r\n", pct_diff_now);
              }
            }
          }
        } else {
          // Borderline reading - keep any existing candidates without clearing
          _battery_status.charging = false;
          _battery_status.discharging_inferred = false;
          // Clear both peak and trough in neutral (no active charge/discharge state)
          if (!_inferred_charging_candidate && !_inferred_discharging_candidate) {
            _inferred_remember_peak_pct = -1;
            _inferred_remember_noncharging_pct = -1;
          }
        }
      }
    } else if (_inferred_charging) {

      // State: Currently charging
      // Clear charging inference on:
      // 1. Sudden voltage drop
      // 2. Negative voltage rate (discharging)
      // 3. Reaching previously remembered non-charging trough
      // Use voltage-dependent threshold for detecting discharge during charging
      // Keep more aggressive than neutral state to quickly detect charge loss
      int32_t discharge_during_charge_threshold = (voltage_mv >= threshold_55pct) ? -15 : -30;
      bool discharging_rate = (voltage_rate < discharge_during_charge_threshold);
      
      // Compute percent delta since last reading
      int pct_diff_now = 0;
      if (_last_percentage >= 0) pct_diff_now = (int)_battery_status.percentage - (int)_last_percentage;

      // If percent dropped enough while charging, clear charging and start discharging candidate
      if (pct_diff_now <= -(int)BATTERY_CANDIDATE_PERCENT_DELTA) {
        _inferred_charging = false;
        _inferred_charging_candidate = false;
        _battery_status.charging = false;
        _battery_status.discharging_inferred = false;
        _inferred_remember_peak_pct = -1;  // Clear peak when charging stops

        if (!_inferred_discharging_candidate) {
          _start_discharging_candidate(now, "##[BATTERY]#: Discharging candidate started on charging drop (start %d%%, pct-diff %+d%%)\r\n", pct_diff_now);
        }

      } else if (sudden_drop_pct || discharging_rate) {
        // Sudden drop or discharge rate -> stop charging and possibly switch to discharging
        if (sudden_drop_pct || (pct_diff_now <= -BATTERY_IMMEDIATE_PERCENT_THRESHOLD)) {
          // Strong drop or immediate percent fall -> switch to discharging immediately
          _inferred_charging = false;
          _inferred_charging_candidate = false;
          _inferred_discharging = true;
          _inferred_discharging_candidate = false;
          _battery_status.charging = false;
          _battery_status.discharging_inferred = true;
          _inferred_remember_peak_pct = -1;  // Clear peak when charging stops
          _inferred_remember_noncharging_pct = _battery_status.percentage; // Start remembering trough
          #ifdef BATTERY_DEBUG
            telnet.printf("##[BATTERY]#: Charging cleared -> Discharging inferred (immediate, %+d%% change)\r\n", pct_diff_now);
          #endif
        } else {
          // Clear charging inference only
          _inferred_charging = false;
          _inferred_charging_candidate = false;
          _battery_status.charging = false;
          _battery_status.discharging_inferred = false;
          _inferred_remember_peak_pct = -1;  // Clear peak when charging stops

          /* If the percent drop is notable (>= BATTERY_CANDIDATE_PERCENT_DELTA),
             start discharging candidate immediately. */
          if (pct_diff_now <= -(int)BATTERY_CANDIDATE_PERCENT_DELTA) {
            _battery_status.charging = false;
            _battery_status.discharging_inferred = false;
            if (!_inferred_discharging_candidate) {
              _start_discharging_candidate(now, "##[BATTERY]#: Discharging candidate started on charging clear (start %d%%, pct-diff %+d%%)\r\n", pct_diff_now);
            }
          } else {
            #ifdef BATTERY_DEBUG
              telnet.printf("##[BATTERY]#: Charging cleared (pct %+d%% change)\r\n", pct_diff_now);
            #endif
          }
        }
      } else if (_inferred_remember_noncharging_pct >= 0 && _battery_status.percentage <= _inferred_remember_noncharging_pct) {
        // Percentage dropped to discharging level -> stop charging
        _inferred_charging = false;
        _inferred_charging_candidate = false;
        _battery_status.charging = false;
        _battery_status.discharging_inferred = false;
        _inferred_remember_peak_pct = -1;  // Clear peak when charging stops
        #ifdef BATTERY_DEBUG
          telnet.printf("##[BATTERY]#: Charging inference cleared (now %d%%)\r\n", _battery_status.percentage);
        #endif
      } else {
        // Still charging; update peak if a new peak is observed
        _battery_status.charging = true;
        _battery_status.discharging_inferred = false;
        if (_battery_status.percentage > _inferred_remember_peak_pct) {
          _inferred_remember_peak_pct = _battery_status.percentage;
        }
      }
    } else if (_inferred_discharging) {
      // State: Currently discharging
      // Clear discharging inference only on:
      // 1. Sudden voltage spike
      // 2. Positive voltage rate (charging)
      // Detect charging exit from discharge - align with neutral state thresholds
      int32_t charge_during_discharge_threshold = (voltage_mv >= threshold_95pct) ? 20 : 25;
      bool charging_rate = (voltage_rate > charge_during_discharge_threshold);
      
      // Compute percent delta since last reading
      int pct_diff_now = 0;
      if (_last_percentage >= 0) pct_diff_now = (int)_battery_status.percentage - (int)_last_percentage;

      // If percent rose enough while discharging, clear discharging and start charging candidate
      if (pct_diff_now >= (int)BATTERY_CANDIDATE_PERCENT_DELTA) {
        _inferred_discharging = false;
        _inferred_discharging_candidate = false;
        _battery_status.discharging_inferred = false;
        _battery_status.charging = false;
        _inferred_remember_noncharging_pct = -1;  // Clear trough when discharging stops

        if (!_inferred_charging_candidate) {
              _start_charging_candidate(now, "##[BATTERY]#: Charging candidate started on discharging rise (start %d%%, pct-diff %+d%%)\r\n", pct_diff_now);
        }

      } else if (sudden_spike_pct || charging_rate) {
        // Sudden spike or charging rate -> stop discharging and possibly switch to charging
        if (sudden_spike_pct || (pct_diff_now >= BATTERY_IMMEDIATE_PERCENT_THRESHOLD)) {
          // Strong spike or immediate percent jump -> switch to charging immediately
          _inferred_discharging = false;
          _inferred_discharging_candidate = false;
          _inferred_charging = true;
          _inferred_charging_candidate = false;
          _battery_status.discharging_inferred = false;
          _battery_status.charging = true;
          _inferred_remember_noncharging_pct = -1;  // Clear trough when discharging stops
          _inferred_remember_peak_pct = _battery_status.percentage; // Start remembering peak
          #ifdef BATTERY_DEBUG
            telnet.printf("##[BATTERY]#: Discharging cleared -> Charging inferred (immediate, %+d%% change)\r\n", pct_diff_now);
          #endif
        } else {
          // Clear discharging inference only
          _inferred_discharging = false;
          _inferred_discharging_candidate = false;
          _battery_status.discharging_inferred = false;
          _battery_status.charging = false;
          _inferred_remember_noncharging_pct = -1;  // Clear trough when discharging stops

          /* If the percent increase is notable (>= BATTERY_CANDIDATE_PERCENT_DELTA),
             start charging candidate immediately. */
          if (pct_diff_now >= (int)BATTERY_CANDIDATE_PERCENT_DELTA) {
            _battery_status.charging = false;
            _battery_status.discharging_inferred = false;
            if (!_inferred_charging_candidate) {
              _start_charging_candidate(now, "##[BATTERY]#: Charging candidate started on discharging clear (start %d%%, pct-diff %+d%%)\r\n", pct_diff_now);
            }
          } else {
            #ifdef BATTERY_DEBUG
              telnet.printf("##[BATTERY]#: Discharging cleared (pct %+d%% change)\r\n", pct_diff_now);
            #endif
          }
        }
      } else {
        // Still discharging; update trough if a new low is observed
        _battery_status.discharging_inferred = true;
        _battery_status.charging = false;
        if (_inferred_remember_noncharging_pct < 0) {
          _inferred_remember_noncharging_pct = _battery_status.percentage;  // Initialize trough on first discharging reading
        } else if (_battery_status.percentage < _inferred_remember_noncharging_pct) {
          _inferred_remember_noncharging_pct = _battery_status.percentage;  // Update if new low
        }
      }
    }
  
  }

  // Always request display update after battery status changes
  display.putRequest(DSPBATTERY, 0);
  // Notify web clients about battery status
  netserver.requestOnChange(GETBATTERY, 0);
  // Surface inferred state into the public status struct for CLI/debugging
  _battery_status.charging_inferred = _inferred_charging;
  _battery_status.discharging_inferred = _inferred_discharging;

  _last_percentage = _battery_status.percentage;
  _last_low_battery = _battery_status.low_battery;
  _last_critical_battery = _battery_status.critical_battery;
  _last_charging = _battery_status.charging;

  // Debug output (format aligned with CLI for easier grepping)
  #ifdef BATTERY_DEBUG
    char status_line[256];  // Increased buffer for extended charging info with peak/trough/rate
    battery_format_status_line(_battery_status, status_line, sizeof(status_line), false);
    telnet.printf("%s\r\n", status_line);

  #endif
} 

void battery_loop() {
  // If neither ADC nor charge pin is present, nothing to do
  if (!_battery_inited && !_charge_pin_present) return;

  unsigned long now = millis();

  // If no ADC but charge pin is present, just poll the charge pin for changes
  if (!_battery_inited && _charge_pin_present) {
    bool charging = (digitalRead(BATTERY_CHARGE_PIN) == LOW);
    if (charging != _last_charging) {
      _battery_status.charging = charging;
      if (charging) {
        telnet.printf("##[BATTERY]#: Charging started (charge-pin only)\r\n");
      } else {
        telnet.printf("##[BATTERY]#: Charging stopped (charge-pin only)\r\n");
      }
      display.putRequest(DSPBATTERY, 0);
      _last_charging = charging;
    }
    return;
  }
  
  // Update at configured interval
  if (now - _last_read < BATTERY_UPDATE_INTERVAL) return;
  _last_read = now;
  
  _battery_read_and_update();
} 

// Force immediate recalculation and update status (used by CLI after calibration)
void battery_recalc_now() {
  // If ADC present, force immediate recalculation
  if (_battery_inited) {
    _last_read = millis();
    _battery_read_and_update();
    return;
  }
  // If ADC not present but charge-pin is available, force a single charge-pin read
  if (_charge_pin_present && !_battery_inited) {
    bool charging = (digitalRead(BATTERY_CHARGE_PIN) == LOW);
    _battery_status.charging = charging;
    display.putRequest(DSPBATTERY, 0);
    _last_charging = charging;
    /* Notify web clients about the updated charge-only status */
    netserver.requestOnChange(GETBATTERY, 0);
  }
} 

// Compute and save a new ADC reference from a measured voltage (mV).
// Returns true if the calibration was accepted and saved.
bool battery_calibrate(int meas_mv) {
  if (meas_mv < 2500 || meas_mv > 4500) return false;
  BatteryStatus b = battery_get_status();
  if (!b.valid || b.voltage_mv == 0) return false;
  double ratio = ((double)meas_mv) / ((double)b.voltage_mv);
  if (ratio < 0.5 || ratio > 2.0) return false;
  uint32_t curr_ref = (uint32_t)(config.store.battery_adc_ref_mv ? config.store.battery_adc_ref_mv : BATTERY_ADC_REF_MV);
  uint32_t suggested_ref = (uint32_t)((double)curr_ref * ratio + 0.5);
  if (suggested_ref < 2000 || suggested_ref > 4000) return false;
  config.saveValue(&config.store.battery_adc_ref_mv, (uint16_t)suggested_ref);
  battery_recalc_now();
  return true;
}

#else
// No-op stubs when BATTERY_PIN not defined
void battery_init() {}
void battery_boot_status() {}
void battery_loop() {}
const BatteryStatus& battery_get_status() {
  static const BatteryStatus empty_status = {0, 0, false, false, 0, false, false, false, false, 0, false, false};
  return empty_status;
}
bool battery_is_initialized() { return false; }
void battery_recalc_now() {}
bool battery_calibrate(int) { return false; }
#endif
