/*
 * Copyright (c) 2026 Aivaras Kasperaitis (@kasperaitis)
 * SPDX-License-Identifier: GPL-3.0-only
 *  Refactored by Trip5 & Copilot
 */

#include "battery.h"

#if (defined(BATTERY_PIN) && (BATTERY_PIN!=255)) || (defined(BATTERY_CHARGE_PIN) && (BATTERY_CHARGE_PIN!=255))
#include <stdarg.h>
#include "backlightcontrols.h"
#include "common.h"
#include "config.h"
#include "display.h"
#include "logging.h"
#include "netserver.h"
#include "player.h"
#include "telnet.h"
#include "utility.h"

/* File-level compile-time constants (instance state is in Battery class members — see battery.h) */
static const int pctSampleMax = 64;
static const int emaQScale = 256;
// Fixed-point EMA alpha (Q256): 0.30 * 256 ≈ 76.8 → 77
static const int emaAlphaQ = 77;
// Precomputed divider ratio scaled by 100 to avoid float in hot path
static const uint32_t dividerRatioX100 = (uint32_t)(BATTERY_DIVIDER_RATIO * 100.0 + 0.5);

// Debug helper (wrap telnet printf to avoid code duplication)
void Battery::dbgPrintf(const char* fmt, ...) {
  #ifdef BATTERY_DEBUG
    char dbgBuf[160];
    va_list ap; va_start(ap, fmt);
    vsnprintf(dbgBuf, sizeof(dbgBuf), fmt, ap);
    va_end(ap);
    // Callers may include trailing CR/LF in format strings; strip them before FUNCTIONLOG.
    size_t len = strlen(dbgBuf);
    while (len > 0 && (dbgBuf[len - 1] == '\r' || dbgBuf[len - 1] == '\n')) {
      dbgBuf[--len] = '\0';
    }
    FUNCTIONLOG("Battery", "%s", dbgBuf);
  #else
    (void)fmt;
  #endif
}

// Candidate helpers
void Battery::startChargingCandidate(unsigned long now, const char* dbg_fmt, int pct_diff) {
  chargingCandidate = true;
  chargingCandidateStart = now;
  chargingCandidateStartPct = battStatus.percentage;
  battStatus.charging = false;
  battStatus.discharging_inferred = false;
  #ifdef BATTERY_DEBUG
    dbgPrintf(dbg_fmt, chargingCandidateStartPct, pct_diff);
  #endif
}
void Battery::startDischargingCandidate(unsigned long now, const char* dbg_fmt, int pct_diff) {
  dischargingCandidate = true;
  dischargingCandidateStart = now;
  dischargingCandidateStartPct = battStatus.percentage;
  battStatus.charging = false;
  battStatus.discharging_inferred = false;
  #ifdef BATTERY_DEBUG
    dbgPrintf(dbg_fmt, dischargingCandidateStartPct, pct_diff);
  #endif
}

// Clear both inferred states
void Battery::clearInferredStates() {
  inferredCharging = false;
  inferredDischarging = false;
  chargingCandidate = false;
  dischargingCandidate = false;
} 

// Helper functions
uint16_t Battery::readAdcMedian() {
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

uint32_t Battery::calculateVoltage(uint16_t adc_avg) {
  uint32_t adc_ref = config.store.battery_adc_ref_mv ? 
                     config.store.battery_adc_ref_mv : BATTERY_ADC_REF_MV;
  // Validate ADC reference is in reasonable range (2000-4000mV)
  if (adc_ref < 2000 || adc_ref > 4000) {
    #ifdef BATTERY_DEBUG
      FUNCTIONLOG("Battery", "Invalid ADC ref %umV, using default %dmV", (unsigned)adc_ref, BATTERY_ADC_REF_MV);
    #endif
    adc_ref = BATTERY_ADC_REF_MV;
  }
  // Use uint64_t to prevent overflow, use precomputed divider ratio scaled by 100
  return ((uint64_t)adc_avg * adc_ref * (uint64_t)dividerRatioX100) / (4095 * 100);
}

// Battery discharge curve defaults are defined in src/core/options.h (override in myoptions.h)
static const uint16_t batteryCurveMv[] = { BATTERY_CURVE_MV };
static const uint8_t batteryCurvePct[] = { BATTERY_CURVE_PCT };

uint8_t Battery::calculatePct(uint32_t voltage_mv) {
  /* Piecewise LiPo discharge curve (mV -> %).
     Uses batteryCurveMv and batteryCurvePct arrays defined above. */
  const uint16_t* curve_mv = batteryCurveMv;
  const uint8_t* curve_pct = batteryCurvePct;
  const int points = sizeof(batteryCurveMv)/sizeof(batteryCurveMv[0]);
  const int points_pct = sizeof(batteryCurvePct)/sizeof(batteryCurvePct[0]);
  
  // Compile-time check for curve size mismatch (shows warning during build)
  #if defined(BATTERY_CURVE_MV) && defined(BATTERY_CURVE_PCT)
    // User-defined curves - can't check at compile time, but warn if mismatch at runtime
    #ifdef BATTERY_DEBUG
      if (points != points_pct) {
        FUNCTIONLOG("Battery", "Curve size mismatch (%d vs %d) - using available range", points, points_pct);
      }
    #endif
  #else
    // Using default curves - verify they match at compile time
    static_assert(sizeof(batteryCurveMv)/sizeof(batteryCurveMv[0]) == sizeof(batteryCurvePct)/sizeof(batteryCurvePct[0]),
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
void Battery::handleCandidateExpiry(bool charging) {
  unsigned long now = millis();
  // Helper: compute min/max percent within the last window_ms (time-gated sliding window)
  auto computeWindowMinmax = [&](int &min_pct, int &max_pct, unsigned long window_ms) {
    min_pct = 127; max_pct = -127;
    if (pctCount == 0) {
      min_pct = max_pct = battStatus.percentage;
      return;
    }
    for (int i = 0; i < pctCount; ++i) {
      int idx = (pctHead - 1 - i + pctSampleMax) % pctSampleMax;
      unsigned long t = pctSamples[idx].t;
      if (now - t > window_ms) break;
      int p = pctSamples[idx].pct;
      if (p < min_pct) min_pct = p;
      if (p > max_pct) max_pct = p;
    }
    if (min_pct == 127 && max_pct == -127) {
      min_pct = max_pct = battStatus.percentage;
    }
  };

  if (charging) {
    if (chargingCandidate && (now - chargingCandidateStart) >= chargeInferHoldMs) {
      unsigned long dt = now - chargingCandidateStart;

      int window_min = 0, window_max = 0;
      computeWindowMinmax(window_min, window_max, chargeInferHoldMs);
      int latest = battStatus.percentage;
      int pct_diff_window = latest - window_min;

      if (pct_diff_window >= (int)BATTERY_SUSTAINED_PERCENT_WINDOW_THRESHOLD) {
        // Confirm charging
        inferredCharging = true;
        battStatus.charging = true;
        chargingCandidate = false;
        dischargingCandidate = false;
        peakPct = battStatus.percentage;
        chargingCandidateStartPct = -1;
        battStatus.discharging_inferred = false;
        #ifdef BATTERY_DEBUG
          FUNCTIONLOG("Battery", "Charging inferred (time-gated, latest %d%%, delta %d%%)", latest, pct_diff_window);
        #endif
      } else {
        // Expired without confirmation
        chargingCandidate = false;
        chargingCandidateStartPct = -1;
        #ifdef BATTERY_DEBUG
          FUNCTIONLOG("Battery", "Charging candidate expired without confirmation (latest %d%%, window_min %d%%, delta %d%% over %lums)", latest, window_min, pct_diff_window, dt);
        #endif
      }
    }
  } else {
    if (dischargingCandidate && (now - dischargingCandidateStart) >= chargeInferHoldMs) {
      unsigned long dt = now - dischargingCandidateStart;

      int window_min = 0, window_max = 0;
      computeWindowMinmax(window_min, window_max, chargeInferHoldMs);
      int latest = battStatus.percentage;
      int pct_diff_window = window_max - latest;

      if (pct_diff_window >= (int)BATTERY_SUSTAINED_PERCENT_WINDOW_THRESHOLD) {
        // Confirm discharging
        inferredDischarging = true;
        battStatus.discharging_inferred = true;
        dischargingCandidate = false;
        chargingCandidate = false;
        dischargingCandidateStartPct = -1;
        battStatus.charging = false;
        troughPct = battStatus.percentage; // Start remembering trough on confirmed discharging
        #ifdef BATTERY_DEBUG
          FUNCTIONLOG("Battery", "Discharging inferred (time-gated, latest %d%%, delta %d%%)", latest, pct_diff_window);
        #endif
      } else {
        // Expired without confirmation
        dischargingCandidate = false;
        dischargingCandidateStartPct = -1;
        #ifdef BATTERY_DEBUG
          FUNCTIONLOG("Battery", "Discharging candidate expired without confirmation (latest %d%%, window_max %d%%, delta %d%% over %lums)", latest, window_max, pct_diff_window, dt);
        #endif
      }
    }
  }
}

void Battery::init() {
  #if defined(BATTERY_PIN) && (BATTERY_PIN!=255)
    pinMode(BATTERY_PIN, INPUT);
    analogSetAttenuation(ADC_11db);  // 0-3.3V range
    inited = true;
  #endif

  // Initialize charge status pin if present (TP4054 CHRG is active LOW)
  #if defined(BATTERY_CHARGE_PIN) && (BATTERY_CHARGE_PIN!=255)
    pinMode(BATTERY_CHARGE_PIN, INPUT_PULLUP);
    chargePinPresent = true;
  #endif

  // Initialize and sanity-check presence thresholds
  presentMinMv = (uint32_t)BATTERY_PRESENT_MIN_MV;
  presentMaxMv = (uint32_t)BATTERY_PRESENT_MAX_MV;
  if (presentMinMv < 2500) {
    FUNCTIONLOG("Battery", "BATTERY_PRESENT_MIN_MV too low (%u), clamping to 2500mV", (unsigned)presentMinMv);
    presentMinMv = 2500;
  }
  if (presentMaxMv > 5000) {
    FUNCTIONLOG("Battery", "BATTERY_PRESENT_MAX_MV too high (%u), clamping to 5000mV", (unsigned)presentMaxMv);
    presentMaxMv = 5000;
  }
  if (presentMinMv >= presentMaxMv) {
    FUNCTIONLOG("Battery", "BATTERY_PRESENT_MIN_MV >= MAX (%u >= %u), resetting to 3000/4200", (unsigned)presentMinMv, (unsigned)presentMaxMv);
    presentMinMv = 3000;
    presentMaxMv = 4200;
  }

  // Compute the hold window in milliseconds from configured number of measurements
  chargeInferHoldMs = (unsigned long)BATTERY_CHARGE_INFER_HOLD_SAMPLES * (unsigned long)BATTERY_UPDATE_INTERVAL;
  if (chargeInferHoldMs == 0) {
    chargeInferHoldMs = (unsigned long)BATTERY_UPDATE_INTERVAL * 3; // fallback
  }

} // end battery_init

void Battery::bootStatus() {
  // Run boot status when either ADC or CHARGE pin is configured
  #if (defined(BATTERY_PIN) && (BATTERY_PIN!=255)) || (defined(BATTERY_CHARGE_PIN) && (BATTERY_CHARGE_PIN!=255))
    if (!inited && !chargePinPresent) return;
    
    // If charge pin present, read initial charging state first
    #if defined(BATTERY_CHARGE_PIN) && (BATTERY_CHARGE_PIN!=255)
      bool charging = (digitalRead(BATTERY_CHARGE_PIN) == LOW); // active LOW
      battStatus.charging = charging;
      lastCharging = charging;
    #endif
    
    // If ADC available, do initial reading to show battery status at boot
    if (inited) {
      delay(10);  // Let ADC stabilize (shorter to reduce blocking)
      uint16_t adc_sample = readAdcMedian();
      uint32_t voltage_mv = calculateVoltage(adc_sample);

      emaVoltageMv = (int32_t)voltage_mv; /* initialize EMA at boot */
      lastRead = millis(); /* Set last read time to prevent immediate re-reading in battery_loop */

      // Check if battery is present
      bool present = (voltage_mv >= presentMinMv && voltage_mv <= presentMaxMv);
      battStatus.present = present;
      if (present) {
        uint8_t percentage = calculatePct(voltage_mv);
        
        // Save initial readings to avoid false triggers on first loop
        lastPct = percentage;
        battStatus.percentage = percentage;
        battStatus.adc = adc_sample;
        battStatus.voltage_mv = voltage_mv;
        battStatus.valid = true; /* mark valid so GETBATTERY handlers will surface value immediately */

        // Print combined boot status with charging state if available
        if (battStatus.charging) {
          BOOTLOG("battery\t\tADC:%d, %dmV, %d%%, Charging", adc_sample, voltage_mv, percentage);
        } else {
          BOOTLOG("battery\t\tADC:%d, %dmV, %d%%", adc_sample, voltage_mv, percentage);
        }
        // Update display immediately
        display.putRequest(DSPBATTERY, 0);
      } else {
        BOOTLOG("battery\t\tnot detected");
        battStatus.valid = false;
        battStatus.present = false;
        battStatus.percentage = 0;
        battStatus.adc = 0;
        battStatus.voltage_mv = 0;
        display.putRequest(DSPBATTERY, 0);
      }
    }

    /* Notify web clients about the initial battery status so UI can show it immediately */
    netserver.requestOnChange(GETBATTERY, 0);
  #endif
} 

// Return true if either ADC or charge pin is initialized
bool Battery::isInitialized() {
  return (inited || chargePinPresent);
}

const BatteryStatus& Battery::getStatus() {
  return battStatus;
}

// Format battery status line to buffer (shared by CLI and debug output)
void Battery::formatStatusLine(const BatteryStatus& status, char* buffer, size_t buffer_size, bool include_warning) {
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
  if (peakPct >= 0 && troughPct >= 0)
    snprintf(remembered, sizeof(remembered), ", Peak:%d%%, Trough:%d%%", peakPct, troughPct);
  else if (peakPct >= 0)
    snprintf(remembered, sizeof(remembered), ", Peak:%d%%", peakPct);
  else if (troughPct >= 0)
    snprintf(remembered, sizeof(remembered), ", Trough:%d%%", troughPct);
  
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
void Battery::readAndUpdate() {
  unsigned long now = millis();
  uint16_t adc_avg = 0;
  uint32_t voltage_mv = 0;

  // If ADC available, read it and compute volt/percentage
  if (inited) {
    uint16_t adc_sample = readAdcMedian();
    uint32_t raw_voltage_mv = calculateVoltage(adc_sample);

    // Battery detection: Check if voltage is in valid Li-Po range
    if (raw_voltage_mv < presentMinMv || raw_voltage_mv > presentMaxMv) {
      battStatus.valid = false;
      battStatus.present = false;
      battStatus.charging = false;
      battStatus.discharging_inferred = false;
      // Reset tracking state to avoid stale data on reconnect
      lastVoltageMv = 0;
      lastVoltageTime = 0;
      inferredCharging = false;
      inferredDischarging = false;
      chargingCandidate = false;
      dischargingCandidate = false;
      battStatus.voltage_rate_valid = false; // percent rate removed
      #ifdef BATTERY_DEBUG
        FUNCTIONLOG("Battery", "No battery detected, %dmV out of range", raw_voltage_mv);
      #endif

      emaVoltageMv = 0; // reset EMA when battery absent

      return;
    }



    // Smooth voltage with integer EMA (fixed-point, Q format) and compute percentage from EMA
    if (emaVoltageMv <= 0) emaVoltageMv = (int32_t)raw_voltage_mv;
    else {
      emaVoltageMv = (int32_t)(( (int64_t)emaAlphaQ * raw_voltage_mv + (int64_t)(emaQScale - emaAlphaQ) * emaVoltageMv + (emaQScale/2) ) / emaQScale);
    }
    voltage_mv = (uint32_t)emaVoltageMv; 

    // Calculate percentage (linear interpolation) from smoothed voltage
    uint8_t percentage = calculatePct(voltage_mv);

    // Save raw ADC sample for external reporting
    battStatus.adc = adc_sample;

    // Update status struct with all information
    battStatus.voltage_mv = voltage_mv;
    battStatus.percentage = percentage;

    // Append percent sample for sliding-window confirmation logic
    unsigned long pctNow = now;
    pctSamples[pctHead].t = pctNow;
    pctSamples[pctHead].pct = battStatus.percentage;
    pctHead = (pctHead + 1) % pctSampleMax;
    if (pctCount < pctSampleMax) pctCount++;

    battStatus.valid = true;
    battStatus.present = true;

    // Low/critical thresholds
    bool low_batt = (battStatus.percentage <= BATTERY_LOW_THRESHOLD);
    bool crit_batt = (battStatus.percentage <= BATTERY_CRITICAL_THRESHOLD);
    battStatus.low_battery = low_batt;
    battStatus.critical_battery = crit_batt;

    // Print low battery warnings on state change
    if (crit_batt && !lastCritBattery) {
      FUNCTIONLOG("Battery", "CRITICAL BATTERY! %d%% remaining (%dmV)", battStatus.percentage, voltage_mv);
    } else if (low_batt && !lastLowBattery && !crit_batt) {
      FUNCTIONLOG("Battery", "Low battery! %d%% remaining (%dmV)", battStatus.percentage, voltage_mv);
    } else if (!low_batt && lastLowBattery) {
      FUNCTIONLOG("Battery", "Battery level normal (%d%%)", battStatus.percentage);
    }

    // Note: Don't update lastLowBattery and lastCritBattery here - 
    // they're updated after display update to detect changes
  } else {
    // No ADC: mark as not valid / not present
    battStatus.valid = false;
    battStatus.present = false;
  }

  // Calculate voltage rate-of-change (mV per minute) - needed regardless of charge pin
  uint32_t prev_voltage_mv = lastVoltageMv;
  int32_t voltage_rate = 0;
  bool rate_calculated = false;

  /* Percentage-based helpers (declared here so they are visible to the later inference logic) */
  int32_t percent_diff = 0;
  bool sudden_spike_pct = false;
  bool sudden_drop_pct = false;
  
  if (lastVoltageTime > 0 && lastVoltageMv > 0) {
    unsigned long time_diff = now - lastVoltageTime;
    if (time_diff > 0) {
      // Calculate rate using int64_t to prevent overflow: (voltage_change * 60000ms) / time_diff
      int64_t voltage_diff = (int64_t)voltage_mv - (int64_t)prev_voltage_mv;
      voltage_rate = (int32_t)((voltage_diff * 60000LL) / (int64_t)time_diff);
      rate_calculated = true;
      

      // Compute percentage rate-of-change (percent per minute) and instantaneous percent diff
      percent_diff = 0;
      sudden_spike_pct = false;
      sudden_drop_pct = false;
      if (lastPct >= 0) {
        percent_diff = (int32_t)battStatus.percentage - (int32_t)lastPct;

        /* Immediate percent thresholds (single-reading change). */
        sudden_spike_pct = (percent_diff >= BATTERY_IMMEDIATE_PERCENT_THRESHOLD);
        sudden_drop_pct = (percent_diff <= -BATTERY_IMMEDIATE_PERCENT_THRESHOLD);
      }
    }
  }
  
  // Store voltage rate in status for external access
  battStatus.voltage_rate = voltage_rate;
  if (rate_calculated) {
    battStatus.voltage_rate_valid = true;
  } else {
    battStatus.voltage_rate_valid = false;
  }

  (void)percent_diff; // keep compiler happy when built without BATTERY_DEBUG

  
  // Update voltage tracking for next calculation
  lastVoltageMv = voltage_mv;
  lastVoltageTime = now;

  // Read charge pin if present
  if (chargePinPresent) {
    bool charging = (digitalRead(BATTERY_CHARGE_PIN) == LOW); // active LOW
    battStatus.charging = charging;
    if (charging && !lastCharging) {
      FUNCTIONLOG("Battery", "Charging started");
    } else if (!charging && lastCharging) {
      FUNCTIONLOG("Battery", "Charging stopped");
    }
  } else if (rate_calculated) {
    // No charge pin: infer charging/discharging state from voltage rate
    
    // Calculate curve points once for all inference logic
    const int curve_points = sizeof(batteryCurveMv)/sizeof(batteryCurveMv[0]);
    uint32_t threshold_95pct = (curve_points > 1) ? batteryCurveMv[1] : 4100;
    uint32_t threshold_55pct = (curve_points > 5) ? batteryCurveMv[5] : 3700;
    uint32_t threshold_10pct = (curve_points > 7) ? batteryCurveMv[7] : 3400;

    if (!inferredCharging && !inferredDischarging) {
      // State: Neither charging nor discharging
      // Detect charging by: sudden voltage spike OR sustained voltage increase rate
      
      /* Use percentage-based thresholds for inference instead of mV-based thresholds. */

      /* Immediate confirmation on single-reading percent jump (spike). This is separate from trend detection. */
      if (sudden_drop_pct) {
        inferredDischarging = true;
        battStatus.discharging_inferred = true;
        dischargingCandidate = false;
        chargingCandidate = false;
        battStatus.charging = false;
        #ifdef BATTERY_DEBUG
          FUNCTIONLOG("Battery", "Discharging inferred (immediate, %d%% change)", (int)(battStatus.percentage - lastPct));
        #endif
      } else if (sudden_spike_pct) {
        inferredCharging = true;
        battStatus.charging = true;
        chargingCandidate = false;
        dischargingCandidate = false;
        peakPct = battStatus.percentage;
        battStatus.discharging_inferred = false;
        #ifdef BATTERY_DEBUG
          FUNCTIONLOG("Battery", "Charging inferred (immediate, %d%% change)", (int)(battStatus.percentage - lastPct));
        #endif
      }
      
      // Handle candidate confirmation or expiration using window-delta
      handleCandidateExpiry(true);
      handleCandidateExpiry(false);

      // If a time-gated confirmation just changed the state, skip starting new candidates
      if (inferredCharging || inferredDischarging) {
        /* State confirmed; nothing further to do in neutral processing */
      } else {
        // Check for small single-reading percent changes to start candidates (not immediate confirm)
        if (lastPct >= 0) {
          int pct_diff_now = (int)battStatus.percentage - (int)lastPct;
          int dir_now = 0; /* -1 = discharging, +1 = charging */
          if (pct_diff_now <= -BATTERY_CANDIDATE_PERCENT_DELTA) dir_now = -1;
          else if (pct_diff_now >= BATTERY_CANDIDATE_PERCENT_DELTA) dir_now = 1;  

          if (dir_now == 0) {
            /* No significant delta: nothing to do */
          } else {
            /* Start candidate immediately on a qualifying delta */
            if (dir_now < 0) {
              // Start discharging candidate based on percent delta
              chargingCandidate = false;
              if (!dischargingCandidate) {
                startDischargingCandidate(now, "Discharging candidate started (candidate, start %d%%, pct-diff %+d%%)", pct_diff_now);
              }
            } else {
              // Start charging candidate based on percent delta
              dischargingCandidate = false;
              if (!chargingCandidate) {
                startChargingCandidate(now, "Charging candidate started (candidate, start %d%%, pct-diff %+d%%)", pct_diff_now);
              }
            }
          }
        } else {
          // Borderline reading - keep any existing candidates without clearing
          battStatus.charging = false;
          battStatus.discharging_inferred = false;
          // Clear both peak and trough in neutral (no active charge/discharge state)
          if (!chargingCandidate && !dischargingCandidate) {
            peakPct = -1;
            troughPct = -1;
          }
        }
      }
    } else if (inferredCharging) {

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
      if (lastPct >= 0) pct_diff_now = (int)battStatus.percentage - (int)lastPct;

      // If percent dropped enough while charging, clear charging and start discharging candidate
      if (pct_diff_now <= -(int)BATTERY_CANDIDATE_PERCENT_DELTA) {
        inferredCharging = false;
        chargingCandidate = false;
        battStatus.charging = false;
        battStatus.discharging_inferred = false;
        peakPct = -1;  // Clear peak when charging stops

        if (!dischargingCandidate) {
          startDischargingCandidate(now, "Discharging candidate started on charging drop (start %d%%, pct-diff %+d%%)", pct_diff_now);
        }

      } else if (sudden_drop_pct || discharging_rate) {
        // Sudden drop or discharge rate -> stop charging and possibly switch to discharging
        if (sudden_drop_pct || (pct_diff_now <= -BATTERY_IMMEDIATE_PERCENT_THRESHOLD)) {
          // Strong drop or immediate percent fall -> switch to discharging immediately
          inferredCharging = false;
          chargingCandidate = false;
          inferredDischarging = true;
          dischargingCandidate = false;
          battStatus.charging = false;
          battStatus.discharging_inferred = true;
          peakPct = -1;  // Clear peak when charging stops
          troughPct = battStatus.percentage; // Start remembering trough
          #ifdef BATTERY_DEBUG
            FUNCTIONLOG("Battery", "Charging cleared -> Discharging inferred (immediate, %+d%% change)", pct_diff_now);
          #endif
        } else {
          // Clear charging inference only
          inferredCharging = false;
          chargingCandidate = false;
          battStatus.charging = false;
          battStatus.discharging_inferred = false;
          peakPct = -1;  // Clear peak when charging stops

          /* If the percent drop is notable (>= BATTERY_CANDIDATE_PERCENT_DELTA),
             start discharging candidate immediately. */
          if (pct_diff_now <= -(int)BATTERY_CANDIDATE_PERCENT_DELTA) {
            battStatus.charging = false;
            battStatus.discharging_inferred = false;
            if (!dischargingCandidate) {
              startDischargingCandidate(now, "Discharging candidate started on charging clear (start %d%%, pct-diff %+d%%)", pct_diff_now);
            }
          } else {
            #ifdef BATTERY_DEBUG
              FUNCTIONLOG("Battery", "Charging cleared (pct %+d%% change)", pct_diff_now);
            #endif
          }
        }
      } else if (troughPct >= 0 && battStatus.percentage <= troughPct) {
        // Percentage dropped to discharging level -> stop charging
        inferredCharging = false;
        chargingCandidate = false;
        battStatus.charging = false;
        battStatus.discharging_inferred = false;
        peakPct = -1;  // Clear peak when charging stops
        #ifdef BATTERY_DEBUG
          FUNCTIONLOG("Battery", "Charging inference cleared (now %d%%)", battStatus.percentage);
        #endif
      } else {
        // Still charging; update peak if a new peak is observed
        battStatus.charging = true;
        battStatus.discharging_inferred = false;
        if (battStatus.percentage > peakPct) {
          peakPct = battStatus.percentage;
        }
      }
    } else if (inferredDischarging) {
      // State: Currently discharging
      // Clear discharging inference only on:
      // 1. Sudden voltage spike
      // 2. Positive voltage rate (charging)
      // Detect charging exit from discharge - align with neutral state thresholds
      int32_t charge_during_discharge_threshold = (voltage_mv >= threshold_95pct) ? 20 : 25;
      bool charging_rate = (voltage_rate > charge_during_discharge_threshold);
      
      // Compute percent delta since last reading
      int pct_diff_now = 0;
      if (lastPct >= 0) pct_diff_now = (int)battStatus.percentage - (int)lastPct;

      // If percent rose enough while discharging, clear discharging and start charging candidate
      if (pct_diff_now >= (int)BATTERY_CANDIDATE_PERCENT_DELTA) {
        inferredDischarging = false;
        dischargingCandidate = false;
        battStatus.discharging_inferred = false;
        battStatus.charging = false;
        troughPct = -1;  // Clear trough when discharging stops

        if (!chargingCandidate) {
              startChargingCandidate(now, "Charging candidate started on discharging rise (start %d%%, pct-diff %+d%%)", pct_diff_now);
        }

      } else if (sudden_spike_pct || charging_rate) {
        // Sudden spike or charging rate -> stop discharging and possibly switch to charging
        if (sudden_spike_pct || (pct_diff_now >= BATTERY_IMMEDIATE_PERCENT_THRESHOLD)) {
          // Strong spike or immediate percent jump -> switch to charging immediately
          inferredDischarging = false;
          dischargingCandidate = false;
          inferredCharging = true;
          chargingCandidate = false;
          battStatus.discharging_inferred = false;
          battStatus.charging = true;
          troughPct = -1;  // Clear trough when discharging stops
          peakPct = battStatus.percentage; // Start remembering peak
          #ifdef BATTERY_DEBUG
            FUNCTIONLOG("Battery", "Discharging cleared -> Charging inferred (immediate, %+d%% change)", pct_diff_now);
          #endif
        } else {
          // Clear discharging inference only
          inferredDischarging = false;
          dischargingCandidate = false;
          battStatus.discharging_inferred = false;
          battStatus.charging = false;
          troughPct = -1;  // Clear trough when discharging stops

          /* If the percent increase is notable (>= BATTERY_CANDIDATE_PERCENT_DELTA),
             start charging candidate immediately. */
          if (pct_diff_now >= (int)BATTERY_CANDIDATE_PERCENT_DELTA) {
            battStatus.charging = false;
            battStatus.discharging_inferred = false;
            if (!chargingCandidate) {
              startChargingCandidate(now, "Charging candidate started on discharging clear (start %d%%, pct-diff %+d%%)", pct_diff_now);
            }
          } else {
            #ifdef BATTERY_DEBUG
              FUNCTIONLOG("Battery", "Discharging cleared (pct %+d%% change)", pct_diff_now);
            #endif
          }
        }
      } else {
        // Still discharging; update trough if a new low is observed
        battStatus.discharging_inferred = true;
        battStatus.charging = false;
        if (troughPct < 0) {
          troughPct = battStatus.percentage;  // Initialize trough on first discharging reading
        } else if (battStatus.percentage < troughPct) {
          troughPct = battStatus.percentage;  // Update if new low
        }
      }
    }
  
  }

  // Always request display update after battery status changes
  display.putRequest(DSPBATTERY, 0);
  // Notify web clients about battery status
  netserver.requestOnChange(GETBATTERY, 0);
  // Surface inferred state into the public status struct for CLI/debugging
  battStatus.charging_inferred = inferredCharging;
  battStatus.discharging_inferred = inferredDischarging;

  lastPct = battStatus.percentage;
  lastLowBattery = battStatus.low_battery;
  lastCritBattery = battStatus.critical_battery;
  lastCharging = battStatus.charging;

  // Debug output (format aligned with CLI for easier grepping)
  #ifdef BATTERY_DEBUG
    char status_line[256];  // Increased buffer for extended charging info with peak/trough/rate
    formatStatusLine(battStatus, status_line, sizeof(status_line), false);
    FUNCTIONLOG("Battery", "%s", status_line);
  #endif
} 

void Battery::loop() {
  // If neither ADC nor charge pin is present, nothing to do
  if (!inited && !chargePinPresent) return;
  unsigned long now = millis();

  // If no ADC but charge pin is present, just poll the charge pin for changes
  if (!inited && chargePinPresent) {
    bool charging = (digitalRead(BATTERY_CHARGE_PIN) == LOW);
    if (charging != lastCharging) {
      battStatus.charging = charging;
      if (charging) {
        FUNCTIONLOG("Battery", "Charging started (charge-pin only)");
      } else {
        FUNCTIONLOG("Battery", "Charging stopped (charge-pin only)");
      }
      display.putRequest(DSPBATTERY, 0);
      lastCharging = charging;
    }
    return;
  }
  // Update at configured interval
  if (now - lastRead < BATTERY_UPDATE_INTERVAL) return;
  lastRead = now;
  readAndUpdate();
} 

void Battery::applyPowerPolicy() {
  #if BRIGHTNESS_PIN==255
    return;
  #else
    BatteryStatus bat = getStatus();
    bool isChargingPresent = bat.charging || bat.charging_inferred;

    if (bat.critical_battery && !criticalBatteryHandled && millis() > 300000) {
      if (isChargingPresent) {
        if (!criticalBatterySkipped) {
          criticalBatterySkipped = true;
          FUNCTIONLOG("Battery", "Critical (%d%%) but charging - skipping deep sleep", bat.percentage);
        }
      } else {
        criticalBatterySkipped = false;
        criticalBatteryHandled = true;
        FUNCTIONLOG("Battery", "Critical (%d%%) - entering deep sleep", bat.percentage);
        player.sendCommand({PR_STOP, 0});
        display.putRequest(NEWMODE, SCREENBLANK);
        delay(200);
        display.deepsleep();

        BatteryStatus finalCheck = getStatus();
        if (finalCheck.charging) {
          FUNCTIONLOG("Battery", "Charging detected before sleep - aborting deep sleep");
          criticalBatteryHandled = false;
          return;
        }
        utility.doSleepW();
      }
    }

    if (bat.low_battery && !lowBatteryHandled) {
      if (isChargingPresent) {
        #ifdef BATTERY_DEBUG
          FUNCTIONLOG("Battery", "Low (%d%%) but charging/trend detected - skipping forced brightness", bat.percentage);
        #endif
      } else {
        lowBatteryHandled = true;
        if (!savedBrightnessValid) {
          savedBrightness = config.store.brightness;
          savedBrightnessValid = true;
        }
        uint8_t targetPct = (uint8_t)BATTERY_DIM_BRIGHTNESS;
        if (targetPct > 100) targetPct = 100;
        FUNCTIONLOG("Battery", "Low (%d%%) - forcing brightness to %d%%", bat.percentage, targetPct);
        config.store.brightness = targetPct;
        #if BRIGHTNESS_PIN!=255 && DSP_DIMMING_ENABLED
          backlightControls.restart();
        #else
          config.setBrightness(false);
        #endif
      }
    }

    int recover = (int)BATTERY_LOW_THRESHOLD + (int)BATTERY_RECOVER_HYSTERESIS_PCT;
    if (recover > 100) recover = 100;
    uint8_t recoverPct = (uint8_t)recover;
    bool recoveredByPct = (bat.percentage >= recoverPct) && !bat.critical_battery;

    if ((lowBatteryHandled && recoveredByPct) || (criticalBatteryHandled && !bat.critical_battery) || isChargingPresent) {
      if (lowBatteryHandled) {
        lowBatteryHandled = false;
        if (savedBrightnessValid) {
          config.store.brightness = savedBrightness;
          savedBrightnessValid = false;
          FUNCTIONLOG("Battery", "Recovered - restoring brightness to %d%%", config.store.brightness);
        }
        #if BRIGHTNESS_PIN!=255 && DSP_DIMMING_ENABLED
          backlightControls.restart();
        #else
          config.setBrightness(false);
        #endif
      }

      if (criticalBatteryHandled && !bat.critical_battery) {
        criticalBatteryHandled = false;
        #if BRIGHTNESS_PIN!=255 && DSP_DIMMING_ENABLED
          backlightControls.restart();
        #else
          config.setBrightness(false);
        #endif
      }
    }
  #endif
}

// Force immediate recalculation and update status (used by CLI after calibration)
void Battery::recalcNow() {
  // If ADC present, force immediate recalculation
  if (inited) {
    lastRead = millis();
    readAndUpdate();
    return;
  }
  // If ADC not present but charge-pin is available, force a single charge-pin read
  if (chargePinPresent && !inited) {
    bool charging = (digitalRead(BATTERY_CHARGE_PIN) == LOW);
    battStatus.charging = charging;
    display.putRequest(DSPBATTERY, 0);
    lastCharging = charging;
    /* Notify web clients about the updated charge-only status */
    netserver.requestOnChange(GETBATTERY, 0);
  }
} 

// Compute and save a new ADC reference from a measured voltage (mV).
// Returns true if the calibration was accepted and saved.
bool Battery::calibrate(int meas_mv) {
  if (meas_mv < 2500 || meas_mv > 4500) return false;
  BatteryStatus b = getStatus();
  if (!b.valid || b.voltage_mv == 0) return false;
  double ratio = ((double)meas_mv) / ((double)b.voltage_mv);
  if (ratio < 0.5 || ratio > 2.0) return false;
  uint32_t curr_ref = (uint32_t)(config.store.battery_adc_ref_mv ? config.store.battery_adc_ref_mv : BATTERY_ADC_REF_MV);
  uint32_t suggested_ref = (uint32_t)((double)curr_ref * ratio + 0.5);
  if (suggested_ref < 2000 || suggested_ref > 4000) return false;
  config.saveValue(&config.store.battery_adc_ref_mv, (uint16_t)suggested_ref);
  recalcNow();
  return true;
}

#else
// No-op stubs are provided by the Battery stub class in battery.h
#endif

Battery battery;
