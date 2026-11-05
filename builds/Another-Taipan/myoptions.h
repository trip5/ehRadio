#ifndef myoptions_h
#define myoptions_h

/*        ************************************************************************      */
/*        *        This file must be in the root folder of the sketch !!!        *      */
/*        ************************************************************************      */
/*        . . .  CHECK options.h for full options, examples, and overrides   . . .      */

// ESP32-S3-DevKitC-1 N16R8 (16MB Flash 8MB PSRAM)
// Display: ST7796 (SPI TFT)
// Audio Decoder: I2S (PCM I2S Decoder)
// SPI Bus A: ST7796 (SPI TFT)
//
//                   |----------|
//               3V3 |          | G
//               3V3 |          | 43
//        TFT_RST -1 |          | 44
//                 4 |          | 1
//          ENC_DT 5 |          | 2
//         ENC_CLK 6 |          | 42 I2S_LRC
//          ENC_SW 7 |          | 41 I2S_DOUT
//         TS_SDA 15 |          | 40 I2S_BCLK
//         TS_SCL 16 |          | 39
//         TS_INT 17 |          | 38
//         TS_RST 18 |          | 37
//                 8 |          | 36
//                 3 |          | 35
//                46 |          | 0
//          TFT_DC 9 |          | 45
//         TFT_CS 10 |          | 48 RGB_LED_PIN
//      SPIA_MOSI 11 |          | 47
//       SPIA_SCK 12 |          | 21
//      SPIA_MISO 13 |          | 20
// BRIGHTNESS_PIN 14 |          | 19
//                5V |          | G
//                 G |          | G
//                   |----------|


/* --- Firmware File --- */
#define FIRMWARE "Another-Taipan_taipan_streamer_1.bin" // "esp32_s3_n16r8", "ESP32-S3", "Another-Taipan"
#define FIRMWARE_NAME "Taipan_streamer_1" // "https://trip5.github.io/ehRadio/myoptions/generator.html#eyJibiI6IkVTUDMyLVMzLURldktpdEMtMSBOMTZSOCAoMTZNQiBGbGFzaCA4TUIgUFNSQU0pIiwiZGQiOiJEU1BfTU9ERUwgRFNQX1NUNzc5NiIsImRuIjoiU1Q3Nzk2IChTUEkgVEZUKSIsImFuIjoiSTJTIChQQ00gSTJTIERlY29kZXIpIiwiY2kiOlsiVFNfTU9ERUwgVFNfTU9ERUxfRlQ2MzM2IiwiUm90YXJ5IEVuY29kZXIiXSwiY3AiOlsiUkdCIExFRCJdLCJjcyI6WyJBIl0sImNvIjpbIlRGVF9SU1QiLCJCUklHSFRORVNTX1BJTiIsIkRTUF9ESU1NSU5HX0VOQUJMRUQiLCJEU1BfSU5WRVJUX1FVSVJLIiwiQklHX0JPT1RfTE9HTyIsIkVOQ19TVEVQUyIsIkVOQ19QVUxMVVAiLCJFTkNfU1dfUFVMTFVQIl0sImNkIjpbIkRTUF9MT0NBTEUiLCJUSU1FWk9ORSIsIkNMT0NLX1RXRUxWRSIsIk5VTUJFUkVEX1BMQVlMSVNUIiwiU0hPV19WVV9NRVRFUiIsIlNNQVJUX1NUQVJUIiwiU05UUF8xIiwiU05UUF8yIl0sInAiOnsiU1BJQV9TQ0siOiIxMiIsIlNQSUFfTUlTTyI6IjEzIiwiU1BJQV9NT1NJIjoiMTEiLCJURlRfREMiOiI5IiwiVEZUX0NTIjoiMTAiLCJURlRfUlNUIjoiLTEiLCJCUklHSFRORVNTX1BJTiI6IjE0IiwiSTJTX0RPVVQiOiI0MSIsIkkyU19CQ0xLIjoiNDAiLCJJMlNfTFJDIjoiNDIiLCJUU19TREEiOiIxNSIsIlRTX1NDTCI6IjE2IiwiVFNfSU5UIjoiMTciLCJUU19SU1QiOiIxOCIsIkVOQ19DTEsiOiI2IiwiRU5DX0RUIjoiNSIsIkVOQ19TVyI6IjciLCJSR0JfTEVEX1BJTiI6IjQ4In0sInYiOnsiRU5DX1NURVBTIjoiMiIsIkRTUF9MT0NBTEUiOiJlbl9VUyIsIlRJTUVaT05FIjoiQXVzdHJhbGlhL01lbGJvdXJuZSIsIkRTUF9ESU1NSU5HX0VOQUJMRUQiOiJ0cnVlIiwiRFNQX0lOVkVSVF9RVUlSSyI6ImZhbHNlIiwiQklHX0JPT1RfTE9HTyI6InRydWUiLCJFTkNfUFVMTFVQIjoiZmFsc2UiLCJFTkNfU1dfUFVMTFVQIjoiZmFsc2UiLCJDTE9DS19UV0VMVkUiOiJ0cnVlIiwiTlVNQkVSRURfUExBWUxJU1QiOiJ0cnVlIiwiU0hPV19WVV9NRVRFUiI6InRydWUiLCJTTUFSVF9TVEFSVCI6InRydWUiLCJGSVJNV0FSRV9OQU1FIjoiVGFpcGFuX3N0cmVhbWVyXzEiLCJTTlRQXzEiOiIwLnBvb2wubnRwLm9yZyIsIlNOVFBfMiI6InBvb2wubnRwLm9yZyJ9fQ%3D%3D"
#define ENABLE_UPDATER // enables OTA updates


/* --- SPI Bus Pins --- */
#define SPIA_SCK             12
#define SPIA_MISO            13
#define SPIA_MOSI            11

/* --- Display --- */
#define DSP_MODEL            DSP_ST7796
#define TFT_DC               9
#define TFT_RST              -1        /* pin RST is attached to (-1 = EN pin) */
#define BRIGHTNESS_PIN       14        /* pin that controls brightness / backlight (255 = unused) */
#define DSP_DIMMING_ENABLED  true      /* enable screen dimming (depends on brightness pin) */
#define DSP_INVERT_QUIRK     false     /* fixes display inversion quirk (very common) */
#define BIG_BOOT_LOGO        true      /* set to false to save space */
#define TFT_CS               10        /* pin CS is attached to (255 = tied to GND) */

/* --- Audio Decoder --- */
#define I2S_DOUT             41
#define I2S_BCLK             40
#define I2S_LRC              42

/* --- Inputs --- */
#define TS_MODEL             TS_MODEL_FT6336
#define TS_SDA               15
#define TS_SCL               16
#define TS_INT               17
#define TS_RST               18
#define ENC_CLK              6
#define ENC_DT               5
#define ENC_SW               7
#define ENC_STEPS            2         /* rotary encoder steps (default 2) */
#define ENC_PULLUP           false     /* use internal pullup on rotary (default true) */
#define ENC_SW_PULLUP        false     /* use internal pullup on button (default true) */

/* --- Peripherals and Build Options --- */
#define RGB_LED_PIN          48

/* --- User Defaults --- */
#define DSP_LOCALE           "en_US"
#define TIMEZONE_NAME        "Australia/Melbourne"
#define TIMEZONE_POSIX       "AEST-10AEDT,M10.1.0,M4.1.0/3"
#define CLOCK_TWELVE         true
#define NUMBERED_PLAYLIST    true
#define SHOW_VU_METER        true
#define SMART_START          true
#define SNTP_1               "0.pool.ntp.org"
#define SNTP_2               "pool.ntp.org"

#endif // myoptions_h
