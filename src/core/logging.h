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

// The log ring (SAVE_LOGS_TO_FS): every line the loggers see, saved to LittleFS as a rotating file set.
// complete=false is a FRAGMENT - the X loggers' partial text and the progress dots - joined to the next line.
void     logRingWrite(const char* text, bool complete, bool critical);
void     logRingInit();     // after the LittleFS mount; reads /logs/idx
void     logRingFlush();    // call from loop()
uint32_t logRingPending();  // bytes waiting in RAM
uint32_t logRingDropped();  // lines lost to a full RAM ring
size_t   logRingSnapshot(); // fixes the layout for one /log request, returns its total length
size_t   logRingReadAt(size_t offset, uint8_t* out, size_t maxLen);
bool     logRingServeBusy();// a /log download is in flight: a second one is refused rather than sharing the layout
size_t   logRingClear();    // wipes the ring, returns the bytes removed
void     logRingShutdown(); // before a filesystem format: get the last lines out, then stop touching the files

#define LITTLEFSTIMELOGRESET() \
  do { \
    littleFsTimeLogReset(); \
  } while (0)

#define CONFIGTIMELOGRESET() \
  do { \
    configTimeLogReset(); \
  } while (0)

#endif
