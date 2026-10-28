#ifndef LOGGING_H
#define LOGGING_H
#pragma once

#include <Arduino.h>
#include <stdio.h>

#define LOG_BUF_LEN 256

#if defined(__GNUC__)
  #define LOG_PRINTF_ATTR(fmtIndex, firstArg) __attribute__((format(printf, fmtIndex, firstArg)))
#else
  #define LOG_PRINTF_ATTR(fmtIndex, firstArg)
#endif

void logToTelnetLine(const char* text);
void logToTelnetRaw(const char* text);
void serialLog(const char* fmt, ...) LOG_PRINTF_ATTR(1, 2);
void serialLogX(const char* fmt, ...) LOG_PRINTF_ATTR(1, 2);
void functionLog(const char* category, const char* fmt, ...) LOG_PRINTF_ATTR(2, 3);
void bootLog(const char* fmt, ...) LOG_PRINTF_ATTR(1, 2);
void bootLogX(const char* fmt, ...) LOG_PRINTF_ATTR(1, 2);
void errorLog(const char* fmt, ...) LOG_PRINTF_ATTR(1, 2);
void warningLog(const char* fmt, ...) LOG_PRINTF_ATTR(1, 2);
void serialLogDot();
void serialLogLf();
void audioLog(const char* category, const char* fmt, ...) LOG_PRINTF_ATTR(2, 3);

/* Boot stage timing.  Each of the three prints a stage name and the time since ITS OWN previous call, so consecutive
   deltas attribute a slow boot to its stages with no temporary instrumentation.  They are declared here whatever the
   build - only their bodies are compiled out when BOOTLOG_TIME is undefined - so a translation unit that cannot see
   options.h still gets one consistent signature instead of a macro that quietly means something else (the trap
   ESPFILEUPDATER_VERBOSE fell into, see the notes).
     BOOTTIMELOG   - setup() in main.cpp; the first call measures from power-on.
     LITTLEFSTIMELOG - the LittleFS path in startup.cpp.  LITTLEFSTIMELOGRESET() starts a fresh measurement at the entry to a
                     function whose first stage would otherwise be measured from the previous marker.
     CONFIGTIMELOG - the config path in config.cpp, same idea, with CONFIGTIMELOGRESET() at each entry point. */
void bootTimeLog(const char* name);
void littleFsTimeLog(const char* name);
void configTimeLog(const char* name);
void littleFsTimeLogReset();
void configTimeLogReset();

#define SERIALLOG(fmt, ...) \
  do { \
    serialLog(fmt, ##__VA_ARGS__); \
  } while (0)

  #define SERIALLOGX(fmt, ...) \
  do { \
    serialLogX(fmt, ##__VA_ARGS__); \
  } while (0)

#define FUNCTIONLOG(category, fmt, ...) \
  do { \
    functionLog(category, fmt, ##__VA_ARGS__); \
  } while (0)

#define BOOTLOG(fmt, ...) \
  do { \
    bootLog(fmt, ##__VA_ARGS__); \
  } while (0)

#define BOOTLOGX(fmt, ...) \
  do { \
    bootLogX(fmt, ##__VA_ARGS__); \
  } while (0)

#define ERRORLOG(fmt, ...) \
  do { \
    errorLog(fmt, ##__VA_ARGS__); \
  } while (0)

#define WARNINGLOG(fmt, ...) \
  do { \
    warningLog(fmt, ##__VA_ARGS__); \
  } while (0)

#define SERIALLOGDOT() \
  do { \
    serialLogDot(); \
  } while (0)

// A bare blank line with no prefix, for separating groups of related reports (the Core Monitor uses it between
// cycles). Unlike SERIALLOGDOT(), this one reaches telnet as well as serial.
#define SERIALLOGLF() \
  do { \
    serialLogLf(); \
  } while (0)

// Boot stage timing - no-ops without BOOTLOG_TIME, since the functions compile to empty bodies.
#define BOOTTIMELOG(name) \
  do { \
    bootTimeLog(name); \
  } while (0)

#define LITTLEFSTIMELOG(name) \
  do { \
    littleFsTimeLog(name); \
  } while (0)

#define CONFIGTIMELOG(name) \
  do { \
    configTimeLog(name); \
  } while (0)

#define LITTLEFSTIMELOGRESET() \
  do { \
    littleFsTimeLogReset(); \
  } while (0)

#define CONFIGTIMELOGRESET() \
  do { \
    configTimeLogReset(); \
  } while (0)

#endif
