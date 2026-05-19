#ifndef myoptions_h
#define myoptions_h

/*        ************************************************************************      */
/*        *        This file must be in the root folder of the sketch !!!        *      */
/*        ************************************************************************      */
/*        . . .  CHECK options.h for full options, examples, and overrides   . . .      */

/* - - - = = = - - - Choose the Radio (defined by platformio.ini env) - - - = = = - - - */
/* automatic builds define the board -- be sure to comment these lines if not debugging */

// in VSCode, the first environment in platformio.ini is used to evaluate the #ifdef chains
// The build_flags = -DSH1106_PCM_REMOTE is a #define... so to debug, we need to undefine it first:
//#undef SH1106_PCM_REMOTE
//
// Then uncomment the build to debug:
//
//#define SH1106_PCM_REMOTE            // Self-contained OLED with PCM, Remote
//#define SH1106_PCM_1BUTTON           // Mini OLED with PCM, 1 Button, Speakers built-in
//#define SH1106_VS1053_3BUTTONS       // Ali Speaker with OLED, VS1053, 3 Buttons
//#define ST7735_PCM_1BUTTON           // Color TFT (red board) with PCM I2S, 1 Button
//#define ILI9488_PCM_1BUTTON          // Big Screen with PCM, 1 button
//#define ES3C28P                      // ESP32-S3 ES3C28P Dev Board (attached 240x320 screen and ES8311 + FM8002E Decoder)


/* --- Firmware File & Board --- */

//  This shows the format of this section which is processed by fix_web_assets_and_releases.py to create Releases
//
//  filename used for OTA update                 ESP Board family (all boards in the same family will share bootloader, partitions)      * If any of these 3 fields are missing
//  prefix is contributor/builds folder name + _  |                ESP chip family (used by flasher)                                     * (or not formatted exactly like this),
//                    |    the build name + .bin  |                 |          Contributer's Name / Github ID (shown on Releases page)   * this firmware will not be added to Releases
//                    ↓     ↓                     ↓                 ↓           ↓
//  #define FIRMWARE "trip5_my_eh_radio.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
//  #define FIRMWARE_NAME "Nice ehRadio" // "https://nice.com/ehRadio/"
//                         ↑                          ↑
//  Used by Web-flasher & ehDP as a friendly name    A helpful URL shown on the Releases page (optional but highly recommended)
#if defined(SH1106_PCM_REMOTE)
  #define FIRMWARE "trip5_sh1106_pcm_remote.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "OLED with Remote" // "https://trip5.github.io/ehRadio_myoptions/generator.html?b=ESP32-S3-DevKitC-1_44Pin&r=72,2,3,4,6,7,15,43,49,51,52,53,54,75,66&i=5,6,15,16,17,22,23,24,25,26,27,28,29,30,39,45,46,47,40&v=42,41,12,11,10,7,18,15,17,16,255,40,39,38,47,21,13,14,8"
#elif defined(SH1106_PCM_1BUTTON)
  #define FIRMWARE "trip5_sh1106_pcm_1button.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "OLED with 1-Button" // "https://trip5.github.io/ehRadio_myoptions/generator.html?b=ESP32-S3-DevKitC-1_44Pin&r=72,2,3,4,6,15,43,49,51,52,53,54,75&i=5,6,15,16,17,22,23,24,25,26,27,28,29,30,39,45,46,47&v=42,41,12,11,10,255,255,255,255,17,255,7,15,16,47,21,13,14"
#elif defined(SH1106_VS1053_3BUTTONS)
  #define FIRMWARE "trip5_sh1106_vs1053_3buttons.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "OLED with 3-Buttons" // "https://trip5.github.io/ehRadio_myoptions/generator.html?b=ESP32-S3-DevKitC-1_44Pin&r=72,2,3,4,6,15,44,48,49,51,52,53,54,75&i=5,6,18,19,20,21,22,23,24,25,26,27,28,29,30,39,45,46,47&v=42,41,9,14,10,-1,255,255,255,17,18,16,40,39,38,47,21,2,1"
#elif defined(ST7735_PCM_1BUTTON)
  #define FIRMWARE "trip5_st7735_pcm_1button.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "Color Screen with 1-Button" // "https://trip5.github.io/ehRadio_myoptions/generator.html?b=ESP32-S3-DevKitC-1_44Pin&r=72,2,3,4,6,11,36,43,49,51,52,53,54,75&i=1,2,3,4,15,16,17,22,23,24,25,26,27,28,29,30,39,45,46,47&v=10,9,-1,4,15,7,6,255,255,255,255,42,255,40,39,38,47,21,13,14"
#elif defined(ILI9488_PCM_1BUTTON)
  #define FIRMWARE "trip5_ili9488_pcm_1button.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "Big Color Screen with 1-Button" // "https://trip5.github.io/ehRadio_myoptions/generator.html?b=ESP32-S3-DevKitC-1_44Pin&r=72,2,3,4,6,31,43,49,51,52,53,54,75&i=1,2,3,4,15,16,17,22,23,24,25,26,27,28,29,30,39,45,46,47&v=10,9,-1,4,15,7,6,255,255,255,255,42,255,40,39,38,47,21,2,1"
#elif defined(ES3C28P)
  #define FIRMWARE "trip5_es3c28p.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "ES3C28P" // "https://www.lcdwiki.com/2.8inch_ESP32-S3_Display"
#endif


/* --- SPI Bus Pins --- */

/* SPI Displays always use A, so if SPI Display + VS1053 then VS1053 should probably use B */
#if defined(ST7735_PCM_1BUTTON)
  #define SPIA_DEFAULT_XMISO           /* SCK/CLK 12 and MOSI/SDA 11 (no MISO) */
  #define SPIB_SCK           21        /* Bus B pins (SD) */
  #define SPIB_MISO          13
  #define SPIB_MOSI          14
#elif defined(SH1106_VS1053_3BUTTONS)
  #define SPIA_DEFAULT                 /* SCK 12, MISO 13, MOSI 11 */
  #define SPIB_SCK           21        /* Bus B pins (SD) */
  #define SPIB_MISO          2
  #define SPIB_MOSI          1
#elif defined(ILI9488_PCM_1BUTTON)
  #define SPIA_DEFAULT_XMISO           /* SCK/CLK 12 and MOSI/SDA 11 (no MISO) */
  #define SPIB_SCK           21        /* Bus B pins (SD) */
  #define SPIB_MISO          2
  #define SPIB_MOSI          1
#elif defined(SH1106_PCM_REMOTE) || defined(SH1106_PCM_1BUTTON)
  #define SPIA_SCK           21        /* Bus A pins (SD) - no Bus B device, no need for default pins */
  #define SPIA_MISO          13
  #define SPIA_MOSI          14
#elif defined(ES3C28P)
  #define SPIA_DEFAULT_XMISO           /* SCK/CLK 12 and MOSI/SDA 11 (no MISO) */
  #define SPIB_SCK           38        /* Bus B pins (SD) */
  #define SPIB_MISO          39
  #define SPIB_MOSI          40
#endif


/* --- Display --- */

/* Display config for I2C displays */
#if defined(SH1106_PCM_REMOTE) || defined(SH1106_PCM_1BUTTON) ||\
    defined(SH1106_VS1053_3BUTTONS)
  #define DSP_MODEL          DSP_SH1106 /* Regular OLED */
  #define I2C_SDA            42
  #define I2C_SCL            41
#endif

/* Display config for SPI displays */
#if defined(ILI9488_PCM_1BUTTON)
  #define DSP_MODEL          DSP_ILI9488 /* Big Display */
  #define BIG_BOOT_LOGO
  #define TFT_DC             10
  #define TFT_CS             9
  #define BRIGHTNESS_PIN     4
  #define TFT_RST            -1        /* set to -1 if connected to ESP EN pin */
  #define DSP_DIMMING_ENABLED true
  #define DSP_INVERT_QUIRK   true
  /* modify src\displays\displayILI9488.cpp -- in section DspCore::initDisplay and add setRotation(3); to do 180 degree rotation */
#elif defined(ST7735_PCM_1BUTTON)
  #define DSP_MODEL          DSP_ST7735         /* Red board / 1.8" Black Tab, if problems try one of DTYPE */
  /* DSP_ST7735 DTYPES BELOW (add if needed but so far, not needed)*/
  //#define DTYPE            INITR_GREENTAB      /* add for Green Tab */
  //#define DTYPE            INITR_REDTAB        /* add for Red Tab */
  //#define DTYPE            INITR_144GREENTAB   /* add for 1.44" Green Tab */
  //#define DTYPE            INITR_MINI160x80    /* add for 0.96" Mini 160x80 */
  #define TFT_DC             10
  #define TFT_CS             9
  #define BRIGHTNESS_PIN     255       /* Red Smaller TFT doesn't have brightness control so 255 */
  #define TFT_RST            -1        /* set to -1 if connected to ESP EN pin */
  #define DSP_DIMMING_ENABLED false
  #define DSP_INVERT_QUIRK   true
#elif defined(ES3C28P)
  #define DSP_MODEL          DSP_ILI9341
  #define TFT_CS             10
  #define TFT_DC             46
  #define TFT_RST            -1
  #define BRIGHTNESS_PIN     45
  #define DSP_INVERT_QUIRK   true
  #define DSP_DIMMING_ENABLED true
#endif


/* --- Audio Decoder --- */

#if defined(SH1106_VS1053_3BUTTONS)
  #define VS1053_SPI         'A'       /* assign VS1053 to Bus A */
  #define VS1053_CS          9
  #define VS1053_DCS         14
  #define VS1053_DREQ        10
  #define VS1053_RST         -1        /* set to -1 if connected to ESP EN pin */
  #define VS_PATCH_ENABLE    false     /* For the 2.5V boards with wrong voltage regulator.  See here: https://github.com/e2002/yoradio/issues/108 */
                                       /* Probably works on all */
#elif defined(ST7735_PCM_1BUTTON) || defined(ILI9488_PCM_1BUTTON)
  #define I2S_DOUT           15
  #define I2S_BCLK           7
  #define I2S_LRC            6
#elif defined(SH1106_PCM_REMOTE) || defined(SH1106_PCM_1BUTTON)
  #define I2S_DOUT           12
  #define I2S_BCLK           11
  #define I2S_LRC            10
#elif defined(ES3C28P)
  #define USE_ES8311
  #define I2S_MCLK           4
  #define I2S_BCLK           5
  #define I2S_DIN            6
  #define I2S_LRC            7
  #define I2S_DOUT           8
  #define ES8311_I2C_SDA     16        /* May be not needed? */
  #define ES8311_I2C_SCL     15
  /* Audio amplifier control (IO1 low -> enable). 
     Default: write MUTE_VAL (HIGH) while stopped, write !MUTE_VAL while playing.
     Set MUTE_PIN to enable control (for FM8002/ES8311, etc). */
  #define MUTE_PIN           1
  #define MUTE_VAL           HIGH
  /* Maximum I2S value to allow when mapping to ES8311 codec (0..254). */
  #define ES8311_MAX_I2S 180
  #define PLAYER_FORCE_MONO true
#endif


/* --- Input --- */

#if defined(SH1106_VS1053_3BUTTONS)
  #define BTN_UP             17
  #define BTN_DOWN           18
  #define BTN_MODE           16
  #define WAKE_PIN           16
#elif defined(SH1106_PCM_1BUTTON)
  #define BTN_DOWN           17
#elif defined(SH1106_PCM_REMOTE)
  #define VOLUME_STEPS       2
  #define BTN_UP             17
  #define BTN_DOWN           16
  #define BTN_PLAY           18
  #define BTN_PREV           7
  #define BTN_NEXT           15
  #define WAKE_PIN           18
#elif defined(ST7735_PCM_1BUTTON) || defined(ILI9488_PCM_1BUTTON)
  #define BTN_DOWN		       42
#elif defined(ES3C28P)
  #define TS_MODEL           TS_MODEL_FT6336
  #define TS_SDA             16
  #define TS_SCL             15
  #define TS_INT             17
  #define TS_RST             18
  #define BTN_DOWN           0
#endif

#if defined(SH1106_VS1053_3BUTTONS) || defined(ST7735_PCM_1BUTTON) ||\
    defined(ILI9488_PCM_1BUTTON) || defined(SH1106_PCM_REMOTE)
  #define ENC_BTNR           40
  #define ENC_BTNL           39
  #define ENC_BTNB           38
#elif defined(SH1106_PCM_1BUTTON)
  #define ENC_BTNR           7
  #define ENC_BTNL           15
  #define ENC_BTNB           16
#endif


/* --- Peripherals and Build Options --- */

#if defined(SH1106_PCM_REMOTE)
  #define LED_PIN            8
  #define LED_INVERT         true
#elif defined(ES3C28P)
  #define RGB_LED_PIN        42
#else
  /* LED config for all others - keep LEDs off */
#endif

#if defined(ST7735_PCM_1BUTTON) || defined(SH1106_VS1053_3BUTTONS) ||\
   defined(ILI9488_PCM_1BUTTON) || defined(ES3C28P)
  #define SD_SPI          'B'     /* assign SD to Bus B */
  #define SD_CS           47
#elif defined(SH1106_PCM_REMOTE) || defined(SH1106_PCM_1BUTTON)
  #define SD_SPI          'A'     /* assign SD to Bus B */
  #define SD_CS           47
#endif

#define MQTT_ENABLE
// #define CORE_MONITOR
// #define ALL_DEBUG_LOGS


/* --- Battery --- */

#if defined(ES3C28P)
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
#endif


/* --- User Defaults --- */

#define ONE_CLICK_SWITCH     true
#define SMART_START          true
#define SD_SHUFFLE           true
#define SHOW_AUDIO_INFO      true
#define SHOW_VU_METER        true
#define SS_PLAYING           true
#define WIFI_SCAN_BEST_RSSI  true
#define SNTP_1               "ca.pool.ntp.org"
#define SNTP_2               "pool.ntp.org"
#define WEATHER_LAT          "44.64738" /* latitude */
#define WEATHER_LON          "-63.58029" /* longitude */
#define PLAYLIST_DEFAULT_URL "https://github.com/trip5/webstations/releases/latest/download/trip5-radio-playlist.csv" /* can be CSV or JSON */


/* --- Time Zone --- */

#define TIMEZONE_NAME   "Canada/Atlantic"
#define TIMEZONE_POSIX  "AST4ADT,M3.2.0,M11.1.0"

/* Congratulations on searching around!  This myoptions.h really shows why I build the way that I do. */
/* It interacts with platformio.ini with a -D[define] to build a bunch of firmwares.  Cool, right? */


/* --- Other Useful Options --- */
/* options.h will list even more but it can be pretty tough... */

/* --- Hate the idea of your device reporting to Radio Browser API which stations you like? --- */
//#define RADIO_BROWSER_NO_SEND_CLICKS

/* --- Don't want to see curated lists? --- */
//#define CURATED_LISTS false

/* --- Want your weather in freedom units? --- */
//#define WEATHER_METRIC false
/* --- Can also default specifics */
//#define WEATHER_TEMPERATURE_F true
//#define WEATHER_PRESSURE_MMHG true
//#define WEATHER_WIND_SPEED_UNITS "kmh" // valid: "kmh" or "mph" or "kn" or "m/s"

/* Only use this if you've decided to use your own Github as the source of files */
/* ...or your firmware is not available from Trip5's Github... sorry! */
/* Read the notes in the ./builds folder for more detailed information */

//#define GITHUBURL "https://github.com/kasperaitis/ehradio" // used by the radio to update firmware and files...

/* Disable automatic runtime downloads from GitHub (ESPFileUpdater) for this board only. */
//#define DISABLE_UPDATER

/* --- Locales Notes --- */
//#define DSP_LANGUAGE_de_DE // sets the display language - see the available options by checking `displayL10n_*.h` files in `locale` folder 
//#define WEBUI_LOCALE "de_DE" // can set a default WebUI locale different than the display - check locale/webui folder .json files (user-configurable)

#endif // myoptions_h
