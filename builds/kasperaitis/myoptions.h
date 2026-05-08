#ifndef myoptions_h
#define myoptions_h

/*        ************************************************************************      */
/*        *        This file must be in the root folder of the sketch !!!        *      */
/*        ************************************************************************      */
/*        . . .  CHECK options.h for full options, examples, and overrides   . . .      */

/* --- FIRMWARE FILENAME & BOARD --- */
#define FIRMWARE "kasperaitis_es3c28p.bin" // "esp32_s3_n16r8", "ESP32-S3", "Kasperaitis"
#define FIRMWARE_NAME "ES3C28P" // "https://www.lcdwiki.com/2.8inch_ESP32-S3_Display"

/* --- LED --- */
#define USE_BUILTIN_LED     false
#define RGB_LED_PIN         42       /* for Adafruit NeoPixel */

/* --- SPI BUS PINS --- */
#define SPIA_DEFAULT_XMISO      /* SCK/CLK 12 and MOSI/SDA 11 (no MISO) */
#define SPIB_SCK        38      /* Bus B pins (SD) */
#define SPIB_MISO       39
#define SPIB_MOSI       40

/* --- DISPLAY --- */
#define DSP_LANGUAGE_lt_LT
#define DSP_MODEL       DSP_ILI9341
#define SCREEN_INVERT true
#define TFT_CS          10
#define TFT_DC          46
#define TFT_RST         -1
#define BRIGHTNESS_PIN  45

/* --- AUDIO DECODER --- */
#define USE_ES8311                  /* a special define for a special decoder */
/* ES3C28P I2S pins (from LCDWiki / user) */
#define I2S_MCLK        4
#define I2S_BCLK        5
#define I2S_DIN         6       /* mic in */
#define I2S_LRC         7
#define I2S_DOUT        8       /* speaker out */
#define PA_ENABLE       1       /* enable on-board power amp */
#define I2C_SCL         15
#define I2C_SDA         16
/* Audio amplifier control (IO1 low -> enable). 
   Default: write MUTE_VAL (HIGH) while stopped, write !MUTE_VAL while playing.
   Set MUTE_PIN to enable control (for FM8002/ES8311, etc). */
#define MUTE_PIN        1
#define MUTE_VAL        HIGH
/* Maximum I2S value to allow when mapping to ES8311 codec (0..254). */
#define ES8311_MAX_I2S 180
#define PLAYER_FORCE_MONO true  /* force mono audio for this board */

/* --- TOUCH --- */
// For some ES3C28P boards the touch controller may be D-FT6336G family — set to TS_MODEL_FT6336 if required
#define TS_MODEL            TS_MODEL_FT6336
#define TS_SDA              16
#define TS_SCL              15
#define TS_INT              17
#define TS_RST              18

/* --- BUTTONS --- */
#define ONE_CLICK_SWITCH true
#define BTN_DOWN        0       /* BOOT button - Next, Move Down */

/* --- SD CARD --- */
#define SPIB_SCK        38      /* Bus B pins */
#define SPIB_MISO       39
#define SPIB_MOSI       40
#define SD_SPI          'B'     /* assign SD to Bus B */
#define SD_CS           47

/* --- Battery --- */
/* Battery monitoring on ES3C28P board */
#define BATTERY_PIN     9       /* GPIO9: ADC pin for battery voltage */
//#define BATTERY_CHARGE_PIN 255  /* No charging status GPIO exposed (TP4054 CHRG pin not connected on ES3C28P) */
#define BATTERY_DIVIDER_RATIO 2.0   /* 100k + 100k voltage divider = 1:2 ratio */
#define BATTERY_ADC_REF_MV 3438     /* ESP32-S3 ADC reference voltage (calibrated EL103565 3000mAh 11.1Wh) */
#define BATTERY_UPDATE_INTERVAL 60000 /* Update every 60 seconds */
//#define BATTERY_DEBUG               /* Uncomment to enable debug output */
#define BATTERY_CHARGE_INFER_HOLD_SAMPLES 3 /* number of measurements (samples) to hold (e.g., 3 readings at BATTERY_UPDATE_INTERVAL) */
#define BATTERY_IMMEDIATE_PERCENT_THRESHOLD 20 /* percent */
#define BATTERY_CANDIDATE_PERCENT_DELTA 1 /* percent */
#define BATTERY_SUSTAINED_PERCENT_WINDOW_THRESHOLD 0 /* percent over hold window */

/* --- USER DEFAULTS --- */
#define DSP_LANGUAGE_lt_LT
#define SMART_START true
#define SHOW_AUDIO_INFO true
#define SS_PLAYING true
#define WIFI_SCAN_BEST_RSSI true
#define TIMEZONE_NAME   "Europe/Vilnius"
#define TIMEZONE_POSIX  "EET-2EEST,M3.5.0/3,M10.5.0/42"
#define SNTP_1          "lt.pool.ntp.org"
#define SNTP_2          "pool.ntp.org"
#define WEATHER_LAT     "55.721924"       /* latitude */
#define WEATHER_LON     "21.117868"      /* longitude */
#define SCREEN_FLIP     true
#define SHOW_VU_METER   true
#define VOLUME_STEPS    5

/* --- SYSTEM OVERRIDES --- */
#define LOOP_TASK_STACK_SIZE 16  /* Compiler default is 8KB but seems safe on ESP32-S3 to increase to 16KB for audio decoding + concurrent tasks / 8KB is safe when using a VS1053 decoder */
#define CONFIG_ASYNC_TCP_QUEUE_SIZE 64
#define SEARCHRESULTS_BUFFER 1024*32 // 32KB matches chunk sizes from radio-browser.info but likely only good for ESP32-S3
#define SEARCHRESULTS_YIELDINTERVAL 0 // With a large buffer, skipping is almost eliminated with 0

#endif // myoptions_h
