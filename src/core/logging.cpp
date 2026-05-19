#include "logging.h"
#include <stdarg.h>
#include "telnet.h"

namespace {

void emitLogMessage(const char* category, bool appendNewline, const char* fmt, va_list args) {
  if (!fmt) return;

  char logBuffer[LOG_BUF_LEN] = {0};
  size_t prefixLen = 0;

  if (category && category[0] != '\0') {
    int written = snprintf(logBuffer, sizeof(logBuffer), "[%s]\t", category);
    if (written > 0) {
      prefixLen = static_cast<size_t>(written);
      if (prefixLen >= sizeof(logBuffer)) {
        prefixLen = sizeof(logBuffer) - 1;
      }
    }
  }

  if (prefixLen < sizeof(logBuffer)) {
    vsnprintf(logBuffer + prefixLen, sizeof(logBuffer) - prefixLen, fmt, args);
  }

  if (appendNewline) {
    logToTelnetLine(logBuffer);
    Serial.print(logBuffer);
    Serial.print("\r\n");
  } else {
    logToTelnetRaw(logBuffer);
    Serial.print(logBuffer);
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
