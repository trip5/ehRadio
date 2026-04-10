#ifndef commandhandler_h
#define commandhandler_h

class CommandHandler {
public:
  bool exec(const char *command, const char *value, uint8_t cid=0);

private:
  static bool cmdIs(const char* command) {
    return false;
  }
  template<typename... Args>
  static bool cmdIs(const char* command, const char* first, Args... rest) {
    return strcmp(command, first) == 0 || cmdIs(command, rest...);
  }
};

extern CommandHandler cmd;

#endif
