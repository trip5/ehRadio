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
//   TEST_CI_PCM      I2S PCM decoder, DSP_ILI9341, Battery, RGB LED, MQTT
//   TEST_CI_VS1053   VS1053 hardware decoder, SH1106 OLED, SD Card, Rotary, Button
//
// ==========================================================================

/* --- FIRMWARE --- */
#if defined(TEST_CI_PCM)
  #define USE_BUILTIN_LED    false
  #define RGB_LED_PIN        42
  #define SPIA_DEFAULT_XMISO /* SCK 12, MOSI 11 */
  #define SPIB_SCK           21
  #define SPIB_MISO          2
  #define SPIB_MOSI          1
  #define DSP_MODEL          DSP_ILI9341
  #define SCREEN_INVERT      true
  #define TFT_DC             10
  #define TFT_CS             9
  #define BRIGHTNESS_PIN     4
  #define TFT_RST            -1
  #define I2S_DOUT           15
  #define I2S_BCLK           7
  #define I2S_LRC            6
  #define BATTERY_PIN        1
  #define BATTERY_DIVIDER_RATIO 2.0
  #define BATTERY_ADC_REF_MV 3300
  #define MQTT_ENABLE
#elif defined(TEST_CI_VS1053)
  #define USE_BUILTIN_LED    false
  #define LED_BUILTIN_S3     255
  #define SPIA_DEFAULT       /* SCK 12, MISO 13, MOSI 11 */
  #define DSP_MODEL          DSP_SH1106
  #define I2C_SDA            42
  #define I2C_SCL            41
  #define VS1053_SPI         'A'
  #define VS1053_CS          9
  #define VS1053_DCS         14
  #define VS1053_DREQ        10
  #define VS1053_RST         -1
  #define VS_PATCH_ENABLE    false
  #define ENC_BTNR           40
  #define ENC_BTNL           39
  #define ENC_BTNB           38
  #define SD_SPI             'B'
  #define SD_CS              47
  #define ONE_CLICK_SWITCH   true
  #define BTN_DOWN           0
#endif
/* --- SYSTEM OVERRIDES & USER DEFAULTS --- */
#define LOOP_TASK_STACK_SIZE 16
#define CONFIG_ASYNC_TCP_QUEUE_SIZE 64
#define SMART_START       true
#define TIMEZONE_NAME     "UTC"
#define TIMEZONE_POSIX    "UTC0"
#define SNTP_1            "pool.ntp.org"
#define SNTP_2            "time.nist.gov"
#define WEATHER_LAT       "0.0"
#define WEATHER_LON       "0.0"

#endif // myoptions_h
