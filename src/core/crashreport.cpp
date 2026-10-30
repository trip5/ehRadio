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

namespace {

typedef void (*crashLine_t)(const char*);

String* textTarget = nullptr;   // set while crashDumpText() runs, so one emitter serves both consumers

void lineToText(const char* s) {
  if (!textTarget) return;
  *textTarget += s;
  *textTarget += '\n';
}

void lineToBoot(const char* s) { BOOTLOG("%s", s); }

/* One formatting path for the boot print and for the file crashlog keeps: every line is composed once here and handed
   to the caller, so the two can never describe the same crash differently.  Everything printed is either directly
   usable (the addresses go straight into addr2line) or needed to place it (the ELF SHA256 says which build, the task
   name says where). */
void emitReport(crashLine_t line) {
  size_t addr = 0, size = 0;
  if (esp_core_dump_image_get(&addr, &size) != ESP_OK || size == 0) return;

  esp_core_dump_summary_t* s = (esp_core_dump_summary_t*)malloc(sizeof(esp_core_dump_summary_t));
  if (!s) {
    char buf[112];
    snprintf(buf, sizeof(buf), "a core dump of %u bytes is stored at 0x%08x (no RAM to summarise it)",
             (unsigned)size, (unsigned)addr);
    line(buf);
    return;
  }

  char buf[224];
  if (esp_core_dump_get_summary(s) == ESP_OK) {
    snprintf(buf, sizeof(buf), "PREVIOUS BOOT CRASHED: task '%.15s'  pc 0x%08x  cause %u  vaddr 0x%08x",
             s->exc_task, (unsigned)s->exc_pc, (unsigned)s->ex_info.exc_cause, (unsigned)s->ex_info.exc_vaddr);
    line(buf);
    snprintf(buf, sizeof(buf), "core dump: %u bytes at 0x%08x, elf sha256 %.64s",
             (unsigned)size, (unsigned)addr, (const char*)s->app_elf_sha256);
    line(buf);
    const uint32_t depth = s->exc_bt_info.depth;
    const uint32_t have  = (uint32_t)(sizeof(s->exc_bt_info.bt) / sizeof(s->exc_bt_info.bt[0]));
    snprintf(buf, sizeof(buf), "backtrace: %u frame%s%s", (unsigned)depth, (depth == 1) ? "" : "s",
             s->exc_bt_info.corrupted ? " (flagged corrupt)" : "");
    line(buf);
    for (uint32_t i = 0; i < depth && i < have; i++) {
      snprintf(buf, sizeof(buf), "  #%-2u 0x%08x", (unsigned)i, (unsigned)s->exc_bt_info.bt[i]);
      line(buf);
    }
    line("symbolise with the elf built from that sha256: xtensa-esp32s3-elf-addr2line -pfiaC -e firmware.elf <pc>");
  } else {
    snprintf(buf, sizeof(buf), "a core dump is present (%u bytes at 0x%08x) but its summary could not be read",
             (unsigned)size, (unsigned)addr);
    line(buf);
  }
  free(s);
}

}  // namespace

bool crashDumpAvailable() {
  return esp_core_dump_image_check() == ESP_OK;
}

void crashDumpReport() {
  BOOTLOG("------------------------------------------------------------");
  emitReport(lineToBoot);
  // The dump is deliberately NOT erased here: crashlog::archive() writes it to LittleFS and erases it only after that
  // write succeeds, so a boot that cannot write the file (no clock, no mount) does not throw the evidence away.
  BOOTLOG("------------------------------------------------------------");
}

bool crashDumpText(String& out) {
  if (!crashDumpAvailable()) return false;
  const size_t before = out.length();
  textTarget = &out;
  emitReport(lineToText);
  textTarget = nullptr;
  return out.length() > before;
}

bool crashDumpErase() {
  return esp_core_dump_image_erase() == ESP_OK;
}

#else

bool crashDumpAvailable() { return false; }
void crashDumpReport() {}
bool crashDumpText(String&) { return false; }
bool crashDumpErase() { return false; }

#endif  // CRASHREPORT_ENABLED
