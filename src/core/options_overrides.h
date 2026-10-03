#ifndef options_overrides_h
#define options_overrides_h

/*******************************************************

THIS FILE IS DIRECTLY PULLED IN BY PLATFORMIO COMPILER
This is the safe way to tweak values in certain libraries
Trying to add options.h to pio directly can cause issues

Add to platformio.ini:
build_flags = -include src/core/options_overrides.h

Any includes here must carefully use #ifndef here and
in options.h so they stay out of each other's way!

********************************************************/

#if __has_include("../../myoptions.h")
  #include "../../myoptions.h" // Need it to check if VS1053 is in the build
#endif
#ifndef VS1053_CS
  #define VS1053_CS 255
#endif

// And this determines what core Network services are on
// The following tree is duplicated in options_system.h AND options_overrides.h so if changes are needed, change both files
#if defined(CONFIG_FREERTOS_UNICORE)
  #ifdef NETWORK_CORE // need this extra check because both files may be pulled into the build
    #if NETWORK_CORE!=0
      #error Do not try to define NETWORK_CORE on a single-core ESP - it will be handled automatically!
    #endif
  #else
    #define NETWORK_CORE 0
  #endif
#else
  #ifndef NETWORK_CORE
    #if VS1053_CS!=255
      #define NETWORK_CORE 0
    #else
      #define NETWORK_CORE 1
    #endif
  #endif
#endif

/* --- AsyncTCP --- */
#ifndef CONFIG_ASYNC_TCP_QUEUE_SIZE
  #if defined(ARDUINO_ESP32S3_DEV)
    #define CONFIG_ASYNC_TCP_QUEUE_SIZE 64
  #else
    #define CONFIG_ASYNC_TCP_QUEUE_SIZE 32
  #endif
#endif
// AsyncTCP's core. Defaults to NETWORK_CORE but it may be overriden in myoptions.h
// Change with caution: network tasks and AsyncTCP work together
#ifndef CONFIG_ASYNC_TCP_RUNNING_CORE
  #define CONFIG_ASYNC_TCP_RUNNING_CORE NETWORK_CORE
#endif
#ifndef CONFIG_ASYNC_TCP_USE_WDT
  #define CONFIG_ASYNC_TCP_USE_WDT 1 // library task subscribes to the task WDT (0 = disable)
#endif
#ifndef CONFIG_ASYNC_TCP_PRIORITY
  #define CONFIG_ASYNC_TCP_PRIORITY 4 // library default is 10 (2 is same as audio task, 3 is highest, 4 puts it above everything else)
#endif

#endif // options_overrides_h
