#include "options.h"
#include <Arduino.h>
#include <esp_system.h>
#include "crashreport.h"
#include "logging.h"

// The summary API only exists in the ELF core-dump build of the IDF libraries, so the whole block is compiled only
// when that is what we are linking against - which, on this framework, it is.  Both macros come from sdkconfig.h.
#if defined(COREDUMP_SUMMARY_AT_BOOT) && CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
  #include <esp_core_dump.h>
  #define CRASHREPORT_ENABLED 1
#endif

#ifdef CRASHREPORT_ENABLED

bool crashDumpAvailable() {
  return esp_core_dump_image_check() == ESP_OK;
}

// One crash, one block of lines. Everything printed is either directly usable (the addresses go straight into
// addr2line) or needed to place it (the ELF SHA256 says which build, the task name says where). The dump is then
// erased, or every later boot - clean ones included - would report the same crash again.
void crashDumpReport() {
  size_t addr = 0, size = 0;
  if (esp_core_dump_image_get(&addr, &size) != ESP_OK || size == 0) return;

  esp_core_dump_summary_t* s = (esp_core_dump_summary_t*)malloc(sizeof(esp_core_dump_summary_t));
  if (!s) {
    BOOTLOG("a core dump of %u bytes is stored at 0x%08x (no RAM to summarise it)", (unsigned)size, (unsigned)addr);
    return;
  }

  BOOTLOG("------------------------------------------------------------");
  if (esp_core_dump_get_summary(s) == ESP_OK) {
    BOOTLOG("PREVIOUS BOOT CRASHED: task '%.15s'  pc 0x%08x  cause %u  vaddr 0x%08x",
            s->exc_task, (unsigned)s->exc_pc, (unsigned)s->ex_info.exc_cause, (unsigned)s->ex_info.exc_vaddr);
    BOOTLOG("core dump: %u bytes at 0x%08x, elf sha256 %.64s",
            (unsigned)size, (unsigned)addr, (const char*)s->app_elf_sha256);
    const uint32_t depth = s->exc_bt_info.depth;
    const uint32_t have  = (uint32_t)(sizeof(s->exc_bt_info.bt) / sizeof(s->exc_bt_info.bt[0]));
    BOOTLOG("backtrace: %u frame%s%s", (unsigned)depth, (depth == 1) ? "" : "s",
            s->exc_bt_info.corrupted ? " (flagged corrupt)" : "");
    for (uint32_t i = 0; i < depth && i < have; i++) {
      BOOTLOG("  #%-2u 0x%08x", (unsigned)i, (unsigned)s->exc_bt_info.bt[i]);
    }
    BOOTLOG("symbolise with the elf built from that sha256: xtensa-esp32s3-elf-addr2line -pfiaC -e firmware.elf <pc>");
  } else {
    BOOTLOG("a core dump is present (%u bytes at 0x%08x) but its summary could not be read",
            (unsigned)size, (unsigned)addr);
  }
  free(s);

  #ifdef COREDUMP_KEEP_DUMP
    BOOTLOG("keeping the dump in flash (COREDUMP_KEEP_DUMP) for esp-coredump");
  #else
    if (esp_core_dump_image_erase() == ESP_OK) BOOTLOG("core dump erased - this block is now the record of it");
  #endif
  BOOTLOG("------------------------------------------------------------");
}

#else

bool crashDumpAvailable() { return false; }
void crashDumpReport() {}

#endif  // CRASHREPORT_ENABLED
