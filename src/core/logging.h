#ifndef LOGGING_H
#define LOGGING_H
#pragma once

#include <Arduino.h>
#include <stdio.h>

#ifndef LOG_BUF_LEN
  #define LOG_BUF_LEN 256
#endif

void logToTelnetLine(const char* text);
void logToTelnetRaw(const char* text);

#define SERIALLOG(fmt, ...) \
  do { \
    char _log_buf[LOG_BUF_LEN]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
    logToTelnetLine(_log_buf); \
    Serial.printf("%s\r\n", _log_buf); \
  } while (0)

#define FUNCTIONLOG(category, fmt, ...) \
  do { \
    SERIALLOG("[%s]\t" fmt, category, ##__VA_ARGS__); \
  } while (0)

#define BOOTLOG(fmt, ...) \
  do { \
    FUNCTIONLOG("BOOT", fmt, ##__VA_ARGS__); \
  } while (0)

#define BOOTLOGX(fmt, ...) \
  do { \
    char _boot_payload[LOG_BUF_LEN]; \
    char _boot_msg[LOG_BUF_LEN]; \
    snprintf(_boot_payload, sizeof(_boot_payload), fmt, ##__VA_ARGS__); \
    snprintf(_boot_msg, sizeof(_boot_msg), "[BOOT]\t%s", _boot_payload); \
    logToTelnetRaw(_boot_msg); \
    Serial.printf("%s", _boot_msg); \
  } while (0)

#define ERRORLOG(fmt, ...) \
  do { \
    FUNCTIONLOG("ERROR", fmt, ##__VA_ARGS__); \
  } while (0)

#define SERIALLOGDOT() \
  do { \
    Serial.print("."); \
  } while (0)

#endif
