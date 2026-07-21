#ifndef myoptions_h
#define myoptions_h

/*        ************************************************************************      */
/*        *        This file must be in the root folder of the sketch !!!        *      */
/*        ************************************************************************      */
/*        . . .  CHECK options.h for full options, examples, and overrides   . . .      */

// ESP32-S3 ES3C28P (ESP32-S3-N16R8)
// Display: ILI9341 (SPI 320x240 TFT)
// Audio Decoder: ES8311 (PCM I2S Mono Decoder)
// SPI Bus A: ILI9341 (SPI 320x240 TFT)
// SPI Bus B: SD Card Reader
//
//  Pin  Function
//  ---  --------
//  -1   TFT_RST
//  0    BTN_NEXT
//  1    MUTE_PIN
//  4    I2S_MCLK
//  5    I2S_BCLK
//  6    I2S_DIN
//  7    I2S_LRC
//  8    I2S_DOUT
//  10   TFT_CS
//  11   SPIA_MOSI
//  12   SPIA_SCK
//  15   ES8311_I2C_SCL + TS_SCL
//  16   ES8311_I2C_SDA + TS_SDA
//  17   TS_INT
//  18   TS_RST
//  38   SPIB_SCK
//  39   SPIB_MISO
//  40   SPIB_MOSI
//  42   RGB_LED_PIN
//  45   BRIGHTNESS_PIN
//  46   TFT_DC
//  47   SD_CS


/* --- Firmware File & Board --- */
#define FIRMWARE "kasperaitis_es3c28p.bin" // "esp32_s3_n16r8", "ESP32-S3", "Kasperaitis"
#define FIRMWARE_NAME "es3c28p" // "https://trip5.github.io/ehRadio/myoptions/generator.html#eyJibiI6IkVTUDMyLVMzIEVTM0MyOFAgKEVTUDMyLVMzLU4xNlI4KSIsImRkIjoiRFNQX01PREVMIERTUF9JTEk5MzQxIiwiZG4iOiJJTEk5MzQxIChTUEkgMzIweDI0MCBURlQpIiwiYWQiOiJVU0VfRVM4MzExIiwiYW4iOiJFUzgzMTEgKFBDTSBJMlMgTW9ubyBEZWNvZGVyKSIsImNpIjpbIlRTX01PREVMIFRTX01PREVMX0ZUNjMzNiIsIkJ1dHRvbjogTmV4dCBTdGF0aW9uL1RyYWNrIl0sImNwIjpbIlJHQiBMRUQiLCJNUVRUX0VOQUJMRSIsIlNEIENhcmQgUmVhZGVyIl0sImNzIjpbIkEiLCJCIl0sImNvIjpbIlRGVF9SU1QiLCJCUklHSFRORVNTX1BJTiIsIkRTUF9ESU1NSU5HX0VOQUJMRUQiLCJEU1BfSU5WRVJUX1FVSVJLIiwiTVVURV9QSU4iLCJFUzgzMTFfSTJDX1NEQSIsIkVTODMxMV9JMkNfU0NMIiwiTEVEX0lOVkVSVCJdLCJjZCI6WyJEU1BfTE9DQUxFIiwiVElNRVpPTkUiLCJPTkVfQ0xJQ0tfU1dJVENIIiwiU0NSRUVOX0ZMSVAiLCJTU19QTEFZSU5HIiwiU0hPV19WVV9NRVRFUiIsIlNNQVJUX1NUQVJUIiwiU05UUF8xIiwiU05UUF8yIiwiV0VBVEhFUl9MQVQiLCJXRUFUSEVSX0xPTiIsIldJRklfU0NBTl9CRVNUX1JTU0kiXSwicCI6eyJTUElBX1NDSyI6IjEyIiwiU1BJQV9NSVNPIjoiMjU1IiwiU1BJQV9NT1NJIjoiMTEiLCJTUElCX1NDSyI6IjM4IiwiU1BJQl9NSVNPIjoiMzkiLCJTUElCX01PU0kiOiI0MCIsIlRGVF9EQyI6IjQ2IiwiVEZUX0NTIjoiMTAiLCJURlRfUlNUIjoiLTEiLCJCUklHSFRORVNTX1BJTiI6IjQ1IiwiSTJTX01DTEsiOiI0IiwiSTJTX0JDTEsiOiI1IiwiSTJTX0xSQyI6IjciLCJJMlNfRE9VVCI6IjgiLCJJMlNfRElOIjoiNiIsIk1VVEVfUElOIjoiMSIsIkVTODMxMV9JMkNfU0RBIjoiMTYiLCJFUzgzMTFfSTJDX1NDTCI6IjE1IiwiVFNfU0RBIjoiMTYiLCJUU19TQ0wiOiIxNSIsIlRTX0lOVCI6IjE3IiwiVFNfUlNUIjoiMTgiLCJCVE5fTkVYVCI6IjAiLCJSR0JfTEVEX1BJTiI6IjQyIiwiU0RfQ1MiOiI0NyJ9LCJ2Ijp7Ik1VVEVfVkFMIjoiSElHSCIsIlNEX1NQSSI6IkIiLCJEU1BfTE9DQUxFIjoibHRfTFQiLCJUSU1FWk9ORSI6IkV1cm9wZS9WaWxuaXVzIiwiRFNQX0RJTU1JTkdfRU5BQkxFRCI6InRydWUiLCJEU1BfSU5WRVJUX1FVSVJLIjoidHJ1ZSIsIlBMQVlFUl9GT1JDRV9NT05PIjoidHJ1ZSIsIk9ORV9DTElDS19TV0lUQ0giOiJ0cnVlIiwiU0NSRUVOX0ZMSVAiOiJ0cnVlIiwiU1NfUExBWUlORyI6InRydWUiLCJTSE9XX1ZVX01FVEVSIjoidHJ1ZSIsIlNNQVJUX1NUQVJUIjoidHJ1ZSIsIldJRklfU0NBTl9CRVNUX1JTU0kiOiJ0cnVlIiwiRklSTVdBUkVfTkFNRSI6ImVzM2MyOHAiLCJFUzgzMTFfTUFYX0kyUyI6IjE4MCIsIlNOVFBfMSI6Imx0LnBvb2wubnRwLm9yZyIsIlNOVFBfMiI6InBvb2wubnRwLm9yZyIsIldFQVRIRVJfTEFUIjoiNTUuNzIxOTI0IiwiV0VBVEhFUl9MT04iOiIyMS4xMTc4NjgifSwieGUiOnRydWUsInhkIjoiLyogLS0tIEJhdHRlcnkgLS0tICovXG4jZGVmaW5lIEJBVFRFUllfUElOIDkgICAgICAgICAgICAgICAgICAvKiBHUElPOTogQURDIHBpbiBmb3IgYmF0dGVyeSB2b2x0YWdlICovXG4vLyNkZWZpbmUgQkFUVEVSWV9DSEFSR0VfUElOIDI1NSAgICAgICAvKiBObyBjaGFyZ2luZyBzdGF0dXMgR1BJTyBleHBvc2VkIChUUDQwNTQgQ0hSRyBwaW4gbm90IGNvbm5lY3RlZCBvbiBFUzNDMjhQKSAqL1xuXG4jZGVmaW5lIEJBVFRFUllfRElWSURFUl9SQVRJTyAyLjAgICAgICAvKiAxMDBrICsgMTAwayB2b2x0YWdlIGRpdmlkZXIgPSAxOjIgcmF0aW8gKi9cbiNkZWZpbmUgQkFUVEVSWV9BRENfUkVGX01WICAgIDM0MzggICAgIC8qIEVTUDMyLVMzIEFEQyByZWZlcmVuY2Ugdm9sdGFnZSAoY2FsaWJyYXRlZCBFTDEwMzU2NSAzMDAwbUFoIDExLjFXaCkgKi9cbiNkZWZpbmUgQkFUVEVSWV9VUERBVEVfSU5URVJWQUwgNjAgICAgIC8qIFVwZGF0ZSBldmVyeSA2MCBzZWNvbmRzICovXG5cbi8vI2RlZmluZSBCQVRURVJZX0RFQlVHICAgICAgICAgICAgICAgIC8qIFVuY29tbWVudCB0byBlbmFibGUgZGVidWcgb3V0cHV0ICovXG4jZGVmaW5lIEJBVFRFUllfQ0hBUkdFX0lORkVSX0hPTERfU0FNUExFUyAzIC8qIG51bWJlciBvZiBtZWFzdXJlbWVudHMgKHNhbXBsZXMpIHRvIGhvbGQgKGUuZy4sIDMgcmVhZGluZ3MgYXQgQkFUVEVSWV9VUERBVEVfSU5URVJWQUwpICovXG4jZGVmaW5lIEJBVFRFUllfSU1NRURJQVRFX1BFUkNFTlRfVEhSRVNIT0xEIDIwIC8qIHBlcmNlbnQgKi9cbiNkZWZpbmUgQkFUVEVSWV9DQU5ESURBVEVfUEVSQ0VOVF9ERUxUQSAxIC8qIHBlcmNlbnQgKi9cbiNkZWZpbmUgQkFUVEVSWV9TVVNUQUlORURfUEVSQ0VOVF9XSU5ET1dfVEhSRVNIT0xEIDAgLyogcGVyY2VudCBvdmVyIGhvbGQgd2luZG93ICovIn0%3D"
#define ENABLE_UPDATER // enables OTA updates

/* --- SPI Bus Pins --- */
#define SPIA_SCK             12
#define SPIA_MISO            255
#define SPIA_MOSI            11
#define SPIB_SCK             38
#define SPIB_MISO            39
#define SPIB_MOSI            40

/* --- Display --- */
#define DSP_MODEL            DSP_ILI9341
#define TFT_DC               46
#define TFT_RST              -1        /* pin RST is attached to (-1 = EN pin) */
#define BRIGHTNESS_PIN       45        /* pin that controls brightness / backlight (255 = unused) */
#define DSP_DIMMING_ENABLED  true      /* enable screen dimming (depends on brightness pin) */
#define DSP_INVERT_QUIRK     true      /* fixes display inversion quirk (very common) */
#define TFT_CS               10        /* pin CS is attached to (255 = tied to GND) */

/* --- Audio Decoder --- */
#define I2S_MCLK             4
#define I2S_BCLK             5
#define I2S_LRC              7
#define I2S_DOUT             8
#define I2S_DIN              6
#define MUTE_PIN             1         /* pin MUTE is attached to (255 for unused) */
#define ES8311_I2C_SDA       16        /* may fix volume control on boot */
#define ES8311_I2C_SCL       15        /* may fix volume control on boot */
#define USE_ES8311
#define MUTE_VAL             HIGH      /* enables turning off audio amplifier */
#define ES8311_MAX_I2S       180       /* maximum I2S value to allow when mapping to ES8311 codec (0..254) */
#define PLAYER_FORCE_MONO    true      /* forces VU meter to mono mode */

/* --- Inputs --- */
#define TS_MODEL             TS_MODEL_FT6336
#define TS_SDA               16
#define TS_SCL               15
#define TS_INT               17
#define TS_RST               18
#define BTN_NEXT             0

/* --- Peripherals and Build Options --- */
#define RGB_LED_PIN          42
#define MQTT_ENABLE
#define SD_CS                47
#define SD_SPI               'B'       /* assign SD to SPI bus */

/* --- User Defaults --- */
#define DSP_LOCALE           "lt_LT"
#define TIMEZONE_NAME        "Europe/Vilnius"
#define TIMEZONE_POSIX       "EET-2EEST,M3.5.0/3,M10.5.0/4"
#define ONE_CLICK_SWITCH     true
#define SCREEN_FLIP          true
#define SS_PLAYING           true
#define SHOW_VU_METER        true
#define SMART_START          true
#define SNTP_1               "lt.pool.ntp.org"
#define SNTP_2               "pool.ntp.org"
#define WEATHER_LAT          "55.721924" /* latitude */
#define WEATHER_LON          "21.117868" /* longitude */
#define WIFI_SCAN_BEST_RSSI  true

/* --- Extra defines --- */
/* --- Battery --- */
#define BATTERY_PIN 9                  /* GPIO9: ADC pin for battery voltage */
//#define BATTERY_CHARGE_PIN 255       /* No charging status GPIO exposed (TP4054 CHRG pin not connected on ES3C28P) */

#define BATTERY_DIVIDER_RATIO 2.0      /* 100k + 100k voltage divider = 1:2 ratio */
#define BATTERY_ADC_REF_MV    3438     /* ESP32-S3 ADC reference voltage (calibrated EL103565 3000mAh 11.1Wh) */
#define BATTERY_UPDATE_INTERVAL 60     /* Update every 60 seconds */

//#define BATTERY_DEBUG                /* Uncomment to enable debug output */
#define BATTERY_CHARGE_INFER_HOLD_SAMPLES 3 /* number of measurements (samples) to hold (e.g., 3 readings at BATTERY_UPDATE_INTERVAL) */
#define BATTERY_IMMEDIATE_PERCENT_THRESHOLD 20 /* percent */
#define BATTERY_CANDIDATE_PERCENT_DELTA 1 /* percent */
#define BATTERY_SUSTAINED_PERCENT_WINDOW_THRESHOLD 0 /* percent over hold window */

#endif // myoptions_h
