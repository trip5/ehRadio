#ifndef startup_h
#define startup_h
#pragma once

#include <Arduino.h>

class Startup {
public:
  void deassertCsPins();
  void checkSpiffsandVer();
  void initNetwork();
  void startupServices();
  void checkSafeMode();
  void sdOfflineMode();
  void loop();

private:
  void markBootStable();
  void cleanStaleSearchResults();
  void getRequiredFiles();
  void checkNewVersionFile();
  static void startupServicesAsync(void* param);

  uint32_t _bootStartMs = 0;
  bool _bootStablePending = false;
};

extern Startup startup;

#endif // startup_h
