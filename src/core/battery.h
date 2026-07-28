/*
 * Battery voltage monitoring.
 * Reads ADC via BATTERY_PIN, applies EMA smoothing,
 * converts to percentage via discharge curve, and
 * notifies the display + web clients on change.
 */

#ifndef battery_h
#define battery_h
#include "options.h"
#include <Arduino.h>

struct BatteryStatus {
  uint8_t  percentage;   // 0-100
  uint16_t voltage_mv;   // millivolts
  bool     present;      // battery detected in valid voltage range
};

#if defined(BATTERY_PIN) && (BATTERY_PIN!=255)

class Battery {
public:
  void init();
  void bootStatus();
  void loop();
  void recalcNow();
  const BatteryStatus& getStatus();
  bool isInitialized();
  void formatStatusLine(const BatteryStatus& status, char* buffer, size_t buffer_size, bool = false);
  bool calibrate(int meas_mv);
private:
  bool     inited = false;
  unsigned long lastRead = 0;
  int16_t  lastPct = -1;
  int32_t  emaVoltageMv = 0;
  uint32_t presentMinMv = 0;
  uint32_t presentMaxMv = 0;
  BatteryStatus battStatus = {0, 0, false};

  uint16_t readAdcMedian();
  uint32_t calculateVoltage(uint16_t adc_avg);
  uint8_t  calculatePct(uint32_t voltage_mv);
  void     readAndUpdate();
};

#else

class Battery {
public:
  void init() {}
  void bootStatus() {}
  void loop() {}
  void recalcNow() {}
  const BatteryStatus& getStatus() {
    static const BatteryStatus empty = {0, 0, false};
    return empty;
  }
  bool isInitialized() { return false; }
  void formatStatusLine(const BatteryStatus&, char* buffer, size_t buffer_size, bool = false) {
    if (buffer && buffer_size > 0) buffer[0] = '\0';
  }
  bool calibrate(int) { return false; }
};

#endif // defined(BATTERY_PIN) && (BATTERY_PIN!=255)

extern Battery battery;

#endif // battery_h
