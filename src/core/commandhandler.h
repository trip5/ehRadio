#ifndef commandhandler_h
#define commandhandler_h

#include <cstdint>
#include <cstring>

enum class CommandSource : uint8_t {
  WebSocket = 0,
  HttpUrl,
  Mqtt,
  Telnet
};

class CommandHandler {
public:
  bool exec(const char *command, const char *value, uint8_t cid=0, CommandSource source=CommandSource::WebSocket);
  bool isBlockedForSource(const char *command, CommandSource source) const;
  static const char* sourceName(CommandSource source);

private:
  static bool cmdIs(const char* command) {
    return false;
  }
  template<typename... Args>
  static bool cmdIs(const char* command, const char* first, Args... rest) {
    return std::strcmp(command, first) == 0 || cmdIs(command, rest...);
  }
};

extern CommandHandler cmd;

#endif
