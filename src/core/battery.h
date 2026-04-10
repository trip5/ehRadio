/*
 * Copyright (c) 2026 Aivaras Kasperaitis (@kasperaitis)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef battery_h
#define battery_h
#include "options.h"
#include <Arduino.h>

struct BatteryStatus {
  uint16_t voltage_mv;    // Battery voltage in millivolts
  uint8_t percentage;     // Battery percentage 0-100
  bool valid;             // True if readings are valid
  bool present;           // True if battery physically present (voltage in expected range)
  uint16_t adc;           // Last ADC average (0-4095)
  bool low_battery;       // True if battery below low threshold
  bool critical_battery;  // True if battery below critical threshold
  bool charging;          // True if charging detected (CHRG pin active LOW)
  bool charging_inferred;  // True if charging determined by inference logic (no CHRG pin)
  int32_t voltage_rate;   // Voltage rate of change (mV per minute)
  bool voltage_rate_valid; // True if voltage_rate has been calculated (requires 2+ readings)
  bool discharging_inferred; // True if discharging determined by inference logic
};

void battery_init();
void battery_boot_status();  // Print boot status message
void battery_loop();
void battery_recalc_now(); // Force an immediate ADC read and status update
const BatteryStatus& battery_get_status();
bool battery_is_initialized();
void battery_format_status_line(const BatteryStatus& status, char* buffer, size_t buffer_size, bool include_warning = false);
bool battery_calibrate(int meas_mv); // Compute and save a new ADC ref from a measured voltage

#endif // battery_h
