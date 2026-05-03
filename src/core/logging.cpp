#include "logging.h"
#include "telnet.h"

void logToTelnetLine(const char* text) {
  telnet.printf("%s\r\n", text);
}

void logToTelnetRaw(const char* text) {
  telnet.printf("%s", text);
}
