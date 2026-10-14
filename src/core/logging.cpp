#include "options.h"
#include "logging.h"
#include <stdarg.h>
#include <string.h>
#include "telnet.h"

namespace {

void emitLogMessage(const char* category, bool appendNewline, const char* fmt, va_list args) {
  if (!fmt) return;

  char logBuffer[LOG_BUF_LEN] = {0};
  size_t prefixLen = 0;

  if (category && category[0] != '\0') {
    // Pad bracketed category to 16 chars for aligned columns (e.g. "[PSRAM]       ")
    char catBuf[20];
    snprintf(catBuf, sizeof(catBuf), "[%s]", category);
    int written = snprintf(logBuffer, sizeof(logBuffer), "%-16s", catBuf);
    if (written > 0) {
      prefixLen = static_cast<size_t>(written);
      if (prefixLen >= sizeof(logBuffer)) {
        prefixLen = sizeof(logBuffer) - 1;
      }
    }
  }

  #ifdef BOOTLOG_TIME
    if (appendNewline && category && (strcmp(category, "BOOT") == 0 && prefixLen < sizeof(logBuffer))) {
      int written = snprintf(logBuffer + prefixLen, sizeof(logBuffer) - prefixLen, "%05lums: ", (unsigned long)millis());
      if (written > 0) {
        prefixLen += static_cast<size_t>(written);
        if (prefixLen >= sizeof(logBuffer)) {
          prefixLen = sizeof(logBuffer) - 1;
        }
      }
    }
  #endif

  if (prefixLen < sizeof(logBuffer)) {
    vsnprintf(logBuffer + prefixLen, sizeof(logBuffer) - prefixLen, fmt, args);
  }

  if (appendNewline) {
    logToTelnetLine(logBuffer);
    size_t outLen = strlen(logBuffer);
    if (outLen + 2 < sizeof(logBuffer)) {
      logBuffer[outLen++] = '\r';
      logBuffer[outLen++] = '\n';
    }
    Serial.write(reinterpret_cast<const uint8_t*>(logBuffer), outLen);
  } else {
    logToTelnetRaw(logBuffer);
    Serial.write(reinterpret_cast<const uint8_t*>(logBuffer), strlen(logBuffer));
  }
}

} // namespace

void logToTelnetLine(const char* text) {
  telnet.logLine(text);
}

void logToTelnetRaw(const char* text) {
  telnet.logRaw(text);
}

void serialLog(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage(nullptr, true, fmt, args);
  va_end(args);
}

void serialLogX(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage(nullptr, false, fmt, args);
  va_end(args);
}

void functionLog(const char* category, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage(category, true, fmt, args);
  va_end(args);
}

void bootLog(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage("BOOT", true, fmt, args);
  va_end(args);
}

void bootLogX(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage("BOOT", false, fmt, args);
  va_end(args);
}

void errorLog(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage("ERROR", true, fmt, args);
  va_end(args);
}

void serialLogDot() {
  Serial.print(".");
}

void serialLogLf() {
  serialLog("%s", ""); // blank line (or line-ending)
}

// Boot stage timing - see logging.h. Three separate stamps on purpose: each helper measures against its own previous
// call, so the config and LittleFS markers inside a setup() stage do not disturb the setup() deltas (a single shared
// stamp would redefine every number in the log). Both the line and the stamp sit inside the #ifdef, so without
// BOOTLOG_TIME these are empty calls: no stage lines, and nothing else changes.
#ifdef BOOTLOG_TIME
  static uint32_t _bootTimeAt = 0;    // last BOOTTIMELOG   marker
  static uint32_t _fsTimeAt = 0;  // last LITTLEFSTIMELOG marker
  static uint32_t _configTimeAt = 0;  // last CONFIGTIMELOG marker
#endif

void bootTimeLog(const char* name) {
  #ifdef BOOTLOG_TIME
    const uint32_t now = millis();
    BOOTLOG("Boot: %-30s %6lums", name, (unsigned long)(now - _bootTimeAt));
    _bootTimeAt = now;
  #else
    (void)name;
  #endif
}

void littleFsTimeLog(const char* name) {
  #ifdef BOOTLOG_TIME
    const uint32_t now = millis();
    BOOTLOG("LittleFS: %-30s %6lums", name, (unsigned long)(now - _fsTimeAt));
    _fsTimeAt = now;
  #else
    (void)name;
  #endif
}

void configTimeLog(const char* name) {
  #ifdef BOOTLOG_TIME
    const uint32_t now = millis();
    BOOTLOG("Config: %-30s %6lums", name, (unsigned long)(now - _configTimeAt));
    _configTimeAt = now;
  #else
    (void)name;
  #endif
}

void littleFsTimeLogReset() {
  #ifdef BOOTLOG_TIME
    _fsTimeAt = millis();
  #endif
}

void configTimeLogReset() {
  #ifdef BOOTLOG_TIME
    _configTimeAt = millis();
  #endif
}
