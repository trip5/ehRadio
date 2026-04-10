#ifndef myoptions_h
#define myoptions_h

// ==========================================================================
// CI TEST BUILD — builds/test/myoptions.h
//
// This file is used only by the GitHub Actions PR check workflow.
// It is copied to the repo root before running pio build.
// Do not use at as a good example of myoptions.h!
//
// Two test environments are defined in builds/test/platformio.ini:
//   TEST_CI_PCM      I2S PCM decoder, ILI9488, SD Card, Rotary, Button, RGB LED
//   TEST_CI_VS1053   VS1053 hardware decoder, SH1106 OLED, MQTT, Battery
//
// ==========================================================================

// Required by optionschecker.h - board module selection
#define ARDUINO_ESP32S3_DEV

/* --- FIRMWARE --- */
#if defined(TEST_CI_PCM)
  // I2S PCM decoder (matches esp32_s3_trip5_sh1106_pcm_1button pin config)
  #define I2S_DOUT          12
  #define I2S_BCLK          11
  #define I2S_LRC           10
  #define VS1053_CS         255     // VS1053 disabled
  #define SD_SPIPINS        21, 13, 14      /* SCK, MISO, MOSI */
  #define SDC_CS            47
  #define ENC_BTNR          7
  #define ENC_BTNL          15
  #define ENC_BTNB          16
  #define ONE_CLICK_SWITCH  true
  #define BTN_DOWN          0
  #define USE_BUILTIN_LED   false
  #define RGB_LED_PIN       42       /* for Adafruit NeoPixel */
#elif defined(TEST_CI_VS1053)
  #define DSP_MODEL         DSP_ILI9341
  #define VS_HSPI           false
  #define VS1053_CS         9
  #define VS1053_DCS        14
  #define VS1053_DREQ       10
  #define VS1053_RST        -1
  #define I2S_DOUT          255     // I2S PCM disabled
  #define VS_PATCH_ENABLE   false
  #define BATTERY_PIN            1
  #define BATTERY_DIVIDER_RATIO  2.0
  #define BATTERY_ADC_REF_MV     3300
  #define MQTT_ENABLE
#elif defined(BOARD_ESP32_S3_N16R8)
  #define USE_BUILTIN_LED     true
#endif

/* --- SYSTEM OVERRIDES & USER DEFAULTS --- */
#define LOOP_TASK_STACK_SIZE      16
#define CONFIG_ASYNC_TCP_QUEUE_SIZE 64
#define SMART_START       true
#define TIMEZONE_NAME     "UTC"
#define TIMEZONE_POSIX    "UTC0"
#define SNTP_1            "pool.ntp.org"
#define SNTP_2            "time.nist.gov"
#define WEATHER_LAT       "0.0"
#define WEATHER_LON       "0.0"

#endif // myoptions_h
