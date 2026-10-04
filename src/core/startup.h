#ifndef startup_h
#define startup_h
#pragma once

#include <Arduino.h>

class Startup {
public:
  void deassertCsPins();
  void checkLittleFSandVer();
  void initNetwork();
  void startupServices();
  void checkSafeMode();
  void sdOfflineMode();
  void loop();
  void deferBootStable(const char* reason); // restart boot stable countdown
  void getDefaultPlaylist();
  void cleanStaleSearchResults();
  bool servicesBusy() const { return _servicesBusy; } // true only while the services task is actually downloading
  // Created but not finished: the task may still be parked (SD playback or the manager), counting down, or downloading.
  // Unlike servicesBusy() this covers the whole window, which is what a caller needs to hold a job back.
  bool servicesPending() const { return _services == SVC_WILL_RUN; }
  bool safeMode() const { return _safeMode; }
  // The boot-mode glyph drawn at the end of the boot dots line: the SD pair when the card is the source, PAUSE for a
  // boot that never proved itself, PLAY for smart start, VOL_75 otherwise. Read it where the boot screen is built -
  // checkSafeMode() clears bootStableMarker moments later, so a later read reports the wrong thing. The literal that
  // comes back is kept by the widget, so it must never become dynamic.
  const char* icon() const;

private:
  void markBootStable(const char* reason);
  void getRequiredFiles();
  void checkNewVersionFile();
  static void startupServicesAsync(void* param);

  uint32_t _bootStartMs = 0;
  // Whether the startup services run this boot, decided by setup() before loop() can run - there is one call site, in main.cpp, and it is inside setup().
  enum svcState_e : uint8_t {
    SVC_NONE,      // not run: no WiFi, no UPDATEURL, or the web files are missing
    SVC_WILL_RUN,  // the task was created; it may still be parked by SD mode
    SVC_DONE       // finished
  };
  volatile svcState_e _services = SVC_NONE;
  volatile uint32_t _servicesDoneMs = 0;
  volatile bool _servicesBusy = false;
  bool _bootStablePending = false;
  /* Declared last on purpose: it lands in the struct's existing tail padding, so sizeof(Startup) does not grow. */
  bool _safeMode = false;  // set by checkSafeMode(); suppresses this session's automatic autoupdate/smartstart
};

extern Startup startup;

#endif // startup_h
