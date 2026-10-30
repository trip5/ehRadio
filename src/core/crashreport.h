#ifndef CRASHREPORT_H
#define CRASHREPORT_H
#pragma once

#include <Arduino.h>

/* The previous boot's crash, read back from the core dump ESP-IDF wrote to the "coredump" partition.
   Nothing here has to enable anything: every partition table in builds/partitions reserves that partition, and the
   prebuilt ESP32-S3 libraries are compiled with CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y and DATA_FORMAT_ELF=y, so a
   panic is already recorded on every crash - this only reads it back.

   The summary is BOOTLOGGED, so the next boot's serial log carries the crash.  It cannot be written from the panic
   itself - no application code runs after one - so the report happens on the NEXT boot.  Keeping it past that boot is
   crashlog's job, not this module's. */

bool crashDumpAvailable();        // a valid core dump is waiting in flash
void crashDumpReport();           // BOOTLOG its summary; leaves the dump in place for crashlog
bool crashDumpText(String& out);  // the same summary as file text; false when there is no dump
bool crashDumpErase();            // erase the partition - crashlog calls this after a successful write

#endif
