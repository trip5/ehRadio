/*
 * Copyright (c) 2026 Aivaras Kasperaitis (@kasperaitis)
 * SPDX-License-Identifier: GPL-3.0-only
 *  Refactored by Trip5 & Copilot
 */

#ifndef battery_h
#define battery_h
#include "options.h"
#include <Arduino.h>

struct BatteryStatus {
  uint16_t voltage_mv;       // Battery voltage in millivolts
  uint8_t percentage;        // Battery percentage 0-100
  bool valid;                // True if readings are valid
  bool present;              // True if battery physically present (voltage in expected range)
  uint16_t adc;              // Last ADC average (0-4095)
  bool low_battery;          // True if battery below low threshold
  bool critical_battery;     // True if battery below critical threshold
  bool charging;             // True if charging detected (CHRG pin active LOW)
  bool charging_inferred;    // True if charging determined by inference logic (no CHRG pin)
  int32_t voltage_rate;      // Voltage rate of change (mV per minute)
  bool voltage_rate_valid;   // True if voltage_rate has been calculated (requires 2+ readings)
  bool discharging_inferred; // True if discharging determined by inference logic
};

#if (defined(BATTERY_PIN) && (BATTERY_PIN!=255)) || (defined(BATTERY_CHARGE_PIN) && (BATTERY_CHARGE_PIN!=255))

class Battery {
public:
  void init();
  void bootStatus();
  void loop();
  void recalcNow();
  const BatteryStatus& getStatus();
  bool isInitialized();
  void formatStatusLine(const BatteryStatus& status, char* buffer, size_t buffer_size, bool include_warning = false);
  bool calibrate(int meas_mv);
private:
  // ADC / pin state
  bool inited = false;
  bool chargePinPresent = false;
  // Public-facing status struct
  BatteryStatus battStatus = {0, 0, false, false, 0, false, false, false, false, 0, false, false};
  // Polling interval tracking
  unsigned long lastRead = 0;
  // Change-detection state
  int16_t lastPct = -1;
  bool lastLowBattery = false;
  bool lastCritBattery = false;
  bool lastCharging = false;
  // Inferred charge/discharge state
  bool inferredCharging = false;
  bool inferredDischarging = false;
  int peakPct = -1;
  int troughPct = -1;
  // Candidate time-gating
  unsigned long chargingCandidateStart = 0;
  bool chargingCandidate = false;
  int chargingCandidateStartPct = -1;
  unsigned long dischargingCandidateStart = 0;
  bool dischargingCandidate = false;
  int dischargingCandidateStartPct = -1;
  unsigned long chargeInferHoldMs = 0;
  // Percent sample ring buffer (64 samples x 8 bytes = 512 bytes in BSS)
  struct PctSample { unsigned long t; int pct; };
  PctSample pctSamples[64];
  int pctHead = 0;
  int pctCount = 0;
  // EMA voltage smoothing state
  int32_t emaVoltageMv = 0;
  // Rate-of-change tracking
  uint32_t lastVoltageMv = 0;
  unsigned long lastVoltageTime = 0;
  // Presence detection range (initialised in init())
  uint32_t presentMinMv = 0;
  uint32_t presentMaxMv = 0;
  // Private methods
  void dbgPrintf(const char* fmt, ...);
  void startChargingCandidate(unsigned long now, const char* dbg_fmt, int pct_diff);
  void startDischargingCandidate(unsigned long now, const char* dbg_fmt, int pct_diff);
  void clearInferredStates();
  uint16_t readAdcMedian();
  uint32_t calculateVoltage(uint16_t adc_avg);
  uint8_t calculatePct(uint32_t voltage_mv);
  void handleCandidateExpiry(bool charging);
  void readAndUpdate();
};

#else

class Battery {
public:
  void init() {}
  void bootStatus() {}
  void loop() {}
  void recalcNow() {}
  const BatteryStatus& getStatus() {
    static const BatteryStatus empty = {0, 0, false, false, 0, false, false, false, false, 0, false, false};
    return empty;
  }
  bool isInitialized() { return false; }
  void formatStatusLine(const BatteryStatus&, char* buffer, size_t buffer_size, bool = false) {
    if (buffer && buffer_size > 0) buffer[0] = '\0';
  }
  bool calibrate(int) { return false; }
};

#endif // #if (defined(BATTERY_PIN)...) || (defined(BATTERY_CHARGE_PIN)...)

extern Battery battery;

#endif // battery_h
