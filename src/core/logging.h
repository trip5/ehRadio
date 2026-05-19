#ifndef LOGGING_H
#define LOGGING_H
#pragma once

#include <Arduino.h>
#include <stdio.h>

#ifdef ESPFILEUPDATER_DEBUG
  #define ESPFILEUPDATER_VERBOSE true
#else
  #define ESPFILEUPDATER_VERBOSE false
#endif

#define LOG_BUF_LEN 256

#if defined(__GNUC__)
  #define LOG_PRINTF_ATTR(fmtIndex, firstArg) __attribute__((format(printf, fmtIndex, firstArg)))
#else
  #define LOG_PRINTF_ATTR(fmtIndex, firstArg)
#endif

void logToTelnetLine(const char* text);
void logToTelnetRaw(const char* text);
void serialLog(const char* fmt, ...) LOG_PRINTF_ATTR(1, 2);
void functionLog(const char* category, const char* fmt, ...) LOG_PRINTF_ATTR(2, 3);
void bootLog(const char* fmt, ...) LOG_PRINTF_ATTR(1, 2);
void bootLogX(const char* fmt, ...) LOG_PRINTF_ATTR(1, 2);
void errorLog(const char* fmt, ...) LOG_PRINTF_ATTR(1, 2);
void serialLogDot();
void audioLog(const char* category, const char* fmt, ...) LOG_PRINTF_ATTR(2, 3);

#define SERIALLOG(fmt, ...) \
  do { \
    serialLog(fmt, ##__VA_ARGS__); \
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

#endif
