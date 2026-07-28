/*
 * Battery voltage monitoring.
 * Reads ADC, applies EMA smoothing, converts to percentage via
 * discharge curve, and notifies the display + web clients on change.
 */

#include "battery.h"

#if defined(BATTERY_PIN) && (BATTERY_PIN!=255)

#define BATTERY_UPDATE_INTERVAL_MS ((unsigned long)BATTERY_UPDATE_INTERVAL * 1000UL)

#include "common.h"
#include "config.h"
#include "display.h"
#include "logging.h"
#include "netserver.h"

/* File-level compile-time constants */
static const int emaQScale = 256;
static const int emaAlphaQ = 77;  // 0.30 * 256
static const uint32_t dividerRatioX100 = (uint32_t)(BATTERY_DIVIDER_RATIO * 100.0 + 0.5);

/* Discharge curve arrays */
static const uint16_t batteryCurveMv[] = { BATTERY_CURVE_MV };
static const uint8_t  batteryCurvePct[] = { BATTERY_CURVE_PCT };


/* ---- ADC reading ---- */

uint16_t Battery::readAdcMedian() {
  uint16_t samples[BATTERY_SAMPLES];
  for (int i = 0; i < BATTERY_SAMPLES; ++i) {
    samples[i] = (uint16_t)analogRead(BATTERY_PIN);
    yield();
  }
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


/* ---- Voltage calculation ---- */

uint32_t Battery::calculateVoltage(uint16_t adc_avg) {
  uint32_t adc_ref = config.store.battery_adc_ref_mv
                       ? config.store.battery_adc_ref_mv
                       : BATTERY_ADC_REF_MV;
  if (adc_ref < 2000 || adc_ref > 4000) {
    adc_ref = BATTERY_ADC_REF_MV;
  }
  return ((uint64_t)adc_avg * adc_ref * (uint64_t)dividerRatioX100) / (4095 * 100);
}


/* ---- Percentage from discharge curve ---- */

uint8_t Battery::calculatePct(uint32_t voltage_mv) {
  const uint16_t* cv = batteryCurveMv;
  const uint8_t*  cp = batteryCurvePct;
  const int points = sizeof(batteryCurveMv)/sizeof(batteryCurveMv[0]);

  static_assert(sizeof(batteryCurveMv)/sizeof(batteryCurveMv[0]) ==
                sizeof(batteryCurvePct)/sizeof(batteryCurvePct[0]),
                "BATTERY: curve_mv and curve_pct arrays must match in length.");

  if (voltage_mv >= cv[0])          return cp[0];
  if (voltage_mv <= cv[points-1])   return cp[points-1];

  for (int i = 0; i < points - 1; ++i) {
    if (voltage_mv <= cv[i] && voltage_mv >= cv[i+1]) {
      uint32_t span  = (uint32_t)cv[i] - (uint32_t)cv[i+1];
      if (span == 0) return cp[i+1];
      uint32_t delta = (uint32_t)voltage_mv - (uint32_t)cv[i+1];
      return (uint8_t)(cp[i+1] + (delta * (uint32_t)(cp[i] - cp[i+1])) / span);
    }
  }
  return 0;
}


/* ---- Core read-and-update ---- */

void Battery::readAndUpdate() {
  if (!inited) {
    battStatus.present    = false;
    battStatus.percentage = 0;
    battStatus.voltage_mv = 0;
    return;
  }

  uint16_t adc_sample      = readAdcMedian();
  uint32_t raw_voltage_mv  = calculateVoltage(adc_sample);

  /* Presence check */
  if (raw_voltage_mv < presentMinMv || raw_voltage_mv > presentMaxMv) {
    if (battStatus.present) {
      battStatus.present    = false;
      battStatus.percentage = 0;
      battStatus.voltage_mv = 0;
      emaVoltageMv = 0;
      display.putRequest(DSPBATTERY, 0);
      netserver.requestOnChange(GETBATTERY, 0);
      lastPct = -1;
    }
    return;
  }

  /* EMA voltage smoothing */
  if (emaVoltageMv <= 0)
    emaVoltageMv = (int32_t)raw_voltage_mv;
  else
    emaVoltageMv = (int32_t)(((int64_t)emaAlphaQ * raw_voltage_mv +
                               (int64_t)(emaQScale - emaAlphaQ) * emaVoltageMv +
                               (emaQScale/2)) / emaQScale);

  uint8_t pct = calculatePct((uint32_t)emaVoltageMv);

  battStatus.present    = true;
  battStatus.voltage_mv = (uint32_t)emaVoltageMv;
  battStatus.percentage = pct;

  /* Notify on change */
  if (lastPct != pct) {
    display.putRequest(DSPBATTERY, 0);
    netserver.requestOnChange(GETBATTERY, 0);
    lastPct = pct;
  }
}


/* ---- Public API ---- */

void Battery::init() {
  pinMode(BATTERY_PIN, INPUT);
  analogSetAttenuation(ADC_11db);
  inited = true;

  presentMinMv = (uint32_t)BATTERY_PRESENT_MIN_MV;
  presentMaxMv = (uint32_t)BATTERY_PRESENT_MAX_MV;
  if (presentMinMv < 2500)  presentMinMv = 2500;
  if (presentMaxMv > 5000)  presentMaxMv = 5000;
  if (presentMinMv >= presentMaxMv) {
    presentMinMv = 3000;
    presentMaxMv = 4200;
  }
}

void Battery::bootStatus() {
  if (!inited) return;

  delay(10);
  uint16_t adc_sample   = readAdcMedian();
  uint32_t voltage_mv   = calculateVoltage(adc_sample);

  emaVoltageMv = (int32_t)voltage_mv;
  lastRead     = millis();

  bool present = (voltage_mv >= presentMinMv && voltage_mv <= presentMaxMv);
  battStatus.present = present;

  if (present) {
    uint8_t pct = calculatePct(voltage_mv);
    lastPct = pct;
    battStatus.percentage = pct;
    battStatus.voltage_mv = voltage_mv;
    BOOTLOG("battery\t\tADC:%d, %dmV, %d%%", adc_sample, voltage_mv, pct);
  } else {
    BOOTLOG("battery\t\tnot detected");
    battStatus.percentage = 0;
    battStatus.voltage_mv = 0;
  }

  display.putRequest(DSPBATTERY, 0);
  netserver.requestOnChange(GETBATTERY, 0);
}

void Battery::loop() {
  if (!inited) return;
  if (millis() - lastRead < BATTERY_UPDATE_INTERVAL_MS) return;
  lastRead = millis();
  readAndUpdate();
}

void Battery::recalcNow() {
  if (!inited) return;
  lastRead = millis();
  readAndUpdate();
}

bool Battery::isInitialized() {
  return inited;
}

const BatteryStatus& Battery::getStatus() {
  return battStatus;
}

void Battery::formatStatusLine(const BatteryStatus& status, char* buffer,
                               size_t buffer_size, bool /*include_warning*/) {
  if (!status.present) {
    snprintf(buffer, buffer_size, "##CLI.BATTERY#: not detected");
    return;
  }
  snprintf(buffer, buffer_size, "##CLI.BATTERY#: %dmV, %d%%",
           status.voltage_mv, status.percentage);
}

bool Battery::calibrate(int meas_mv) {
  if (meas_mv < 2500 || meas_mv > 4500) return false;
  BatteryStatus b = getStatus();
  if (!b.present || b.voltage_mv == 0) return false;

  double ratio = (double)meas_mv / (double)b.voltage_mv;
  if (ratio < 0.5 || ratio > 2.0) return false;

  uint32_t curr_ref = config.store.battery_adc_ref_mv
                        ? config.store.battery_adc_ref_mv
                        : BATTERY_ADC_REF_MV;
  uint32_t suggested_ref = (uint32_t)((double)curr_ref * ratio + 0.5);
  if (suggested_ref < 2000 || suggested_ref > 4000) return false;

  config.saveValue(&config.store.battery_adc_ref_mv, (uint16_t)suggested_ref);
  recalcNow();
  return true;
}

#else
// No-op stubs provided by Battery stub class in battery.h
#endif

Battery battery;
