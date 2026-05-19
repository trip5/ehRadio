#ifndef startup_h
#define startup_h
#pragma once

#include <Arduino.h>

class Startup {
public:
  void checkVerAndSpiffs();
  void initNetwork();
  void startupServices();

private:
  void cleanStaleSearchResults();
  void fixPlaylistFileEnding();
  void getRequiredFiles();
  void checkNewVersionFile();
  bool checkLocaleFile();
  static void startupServicesAsync(void* param);
};

extern Startup startup;

#endif // startup_h
