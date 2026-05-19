#ifndef myoptions_h
#define myoptions_h

/*        ************************************************************************      */
/*        *        This file must be in the root folder of the sketch !!!        *      */
/*        ************************************************************************      */
/*        . . .  CHECK options.h for full options, examples, and overrides   . . .      */

// ES3C28P (ESP32-S3-N16R8)
// Display: ILI9341 (SPI 320x240 TFT)
// Audio Decoder: ES8311 (PCM I2S Mono Decoder)
// SPI Bus A: ILI9341 (SPI 320x240 TFT)
//
//  Pin  Function
//  ---  --------
//  0    BTN_DOWN
//  1    I2S_DIN
//  4    I2S_MCLK
//  5    I2S_BCLK
//  7    I2S_LRC
//  8    I2S_DOUT
//  10   TFT_CS
//  11   SPIA_MOSI
//  12   SPIA_SCK
//  15   TS_SCL
//  16   TS_SDA
//  17   TS_INT
//  18   TS_RST
//  38   SPIB_SCK
//  39   SPIB_MISO
//  40   SPIB_MOSI
//  42   RGB_LED_PIN
//  46   TFT_DC


/* --- Firmware File & Board --- */
#define FIRMWARE_NAME        "My ehRadio" /* your ehRadio's name */

/* --- SPI Bus Pins --- */
#define SPIA_SCK             12
#define SPIA_MISO            255
#define SPIA_MOSI            11
#define SPIB_SCK             38
#define SPIB_MISO            39
#define SPIB_MOSI            40

/* --- Display --- */
#define DSP_MODEL            DSP_ILI9341
#define TFT_CS               10
#define TFT_DC               46
#define TFT_RST              -1        /* pin RST is attached to (-1 = EN pin) */
#define DSP_DIMMING_ENABLED  true      /* enable screen dimming (depends on brightness pin) */
#define DSP_INVERT_QUIRK     true      /* fixes display inversion quirk (very common) */

/* --- Audio Decoder --- */
#define I2S_MCLK             4
#define I2S_BCLK             5
#define I2S_LRC              7
#define I2S_DOUT             8
#define I2S_DIN              1
#define MUTE_PIN             255       /* pin MUTE is attached to (255 for unused) */
#define USE_ES8311
#define MUTE_VAL             HIGH      /* enables turning off audio amplifier */
#define ES8311_MAX_I2S       180       /* maximum I2S value to allow when mapping to ES8311 codec (0..254) */
#define PLAYER_FORCE_MONO    true      /* forces VU meter to mono mode */

/* --- Input --- */
#define TS_MODEL             TS_MODEL_FT6336
#define TS_SDA               16
#define TS_SCL               15
#define TS_INT               17
#define TS_RST               18
#define BTN_DOWN             0

/* --- Peripherals and Build Options --- */
#define RGB_LED_PIN          42
#define MQTT_ENABLE

/* --- Locale --- */
#define DSP_LANGUAGE_lt_LT

/* --- User Defaults --- */
#define ONE_CLICK_SWITCH     true
#define SS_PLAYING           true
#define SCREEN_FLIP          true
#define SHOW_AUDIO_INFO      true
#define SHOW_VU_METER        true
#define SMART_START          true
#define SNTP_1               "lt.pool.org"
#define SNTP_2               "0.lt.pool.org"
#define VOLUME_STEPS         5
#define WEATHER_LAT          "55.721924" /* latitude */
#define WEATHER_LON          "21.117868" /* longitude */
#define WIFI_SCAN_BEST_RSSI  true

/* --- Time Zone --- */
#define TIMEZONE_NAME        "Europe/Vilnius"
#define TIMEZONE_POSIX       "EET-2EEST,M3.5.0/3,M10.5.0/4"

#endif // myoptions_h
