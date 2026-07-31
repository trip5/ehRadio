#ifndef myoptions_h
#define myoptions_h

/*        ************************************************************************      */
/*        *        This file must be in the root folder of the sketch !!!        *      */
/*        ************************************************************************      */
/*        . . .  CHECK options.h for full options, examples, and overrides   . . .      */

// ESP32-S3-DevKitC-1 N16R8 (16MB Flash 8MB PSRAM)
// Display: Dummy (no physical display)
// Audio Decoder: I2S (PCM I2S Decoder)
// SPI Bus A: SD Card Reader
//
//              |----------|
//          3V3 |          | G
//          3V3 |          | 43
//           -1 |          | 44
//            4 |          | 1 BTN_PLAY
//    I2S_LRC 5 |          | 2 BTN_PREV
//   I2S_BCLK 6 |          | 42 BTN_NEXT
//   I2S_DOUT 7 |          | 41 BTN_UP
//           15 |          | 40 BTN_DOWN
//   ENC_CLK 16 |          | 39 BTN_MODE
//    ENC_DT 17 |          | 38 IR_PIN
//    ENC_SW 18 |          | 37
//            8 |          | 36
//   ENC2_CLK 3 |          | 35
//   ENC2_DT 46 |          | 0
//    ENC2_SW 9 |          | 45
//           10 |          | 48
// SPIA_MOSI 11 |          | 47
//  SPIA_SCK 12 |          | 21 SD_CS
// SPIA_MISO 13 |          | 20
//           14 |          | 19
//           5V |          | G
//            G |          | G
//              |----------|


/* --- Firmware File --- */

#define FIRMWARE "no_display_test.bin" // "esp32_s3_n16r8", "ESP32-S3", "No Display"
#define FIRMWARE_NAME "No Display Test" // "https://trip5.github.io/ehRadio/myoptions/generator.html#eyJibiI6IkVTUDMyLVMzLURldktpdEMtMSBOMTZSOCAoMTZNQiBGbGFzaCA4TUIgUFNSQU0pIiwiZGQiOiJEU1BfTU9ERUwgRFNQX0RVTU1ZIiwiZG4iOiJEdW1teSAobm8gcGh5c2ljYWwgZGlzcGxheSkiLCJhbiI6IkkyUyAoUENNIEkyUyBEZWNvZGVyKSIsImNpIjpbIlJvdGFyeSBFbmNvZGVyIiwiUm90YXJ5IEVuY29kZXIgMiIsIkJ1dHRvbjogUGxheS9QYXVzZSIsIkJ1dHRvbjogUHJldmlvdXMgU3RhdGlvbi9UcmFjayIsIkJ1dHRvbjogTmV4dCBTdGF0aW9uL1RyYWNrIiwiQnV0dG9uOiBWb2x1bWUgVXAiLCJCdXR0b246IFZvbHVtZSBEb3duIiwiQnV0dG9uOiBNb2RlIFN3aXRjaCIsIkRpc2FibGUgRGVlcCBTbGVlcCJdLCJjcCI6WyJJUiBSZWNlaXZlciIsIlNEIENhcmQgUmVhZGVyIl0sImNzIjpbIkEiXSwiY28iOlsiRU5DX1BVTExVUCIsIkVOQ19TV19QVUxMVVAiXSwicCI6eyJTUElBX1NDSyI6IjEyIiwiU1BJQV9NSVNPIjoiMTMiLCJTUElBX01PU0kiOiIxMSIsIkkyU19ET1VUIjoiNyIsIkkyU19CQ0xLIjoiNiIsIkkyU19MUkMiOiI1IiwiRU5DX0NMSyI6IjE2IiwiRU5DX0RUIjoiMTciLCJFTkNfU1ciOiIxOCIsIkVOQzJfQ0xLIjoiMyIsIkVOQzJfRFQiOiI0NiIsIkVOQzJfU1ciOiI5IiwiQlROX1BMQVkiOiIxIiwiQlROX1BSRVYiOiIyIiwiQlROX05FWFQiOiI0MiIsIkJUTl9VUCI6IjQxIiwiQlROX0RPV04iOiI0MCIsIkJUTl9NT0RFIjoiMzkiLCJJUl9QSU4iOiIzOCIsIlNEX0NTIjoiMjEifSwidiI6eyJTRF9TUEkiOiJBIiwiRU5DX1BVTExVUCI6InRydWUiLCJFTkNfU1dfUFVMTFVQIjoidHJ1ZSIsIkZJUk1XQVJFX05BTUUiOiJUZXN0In19"
#define ENABLE_UPDATER // enables OTA updates

/* --- SPI Bus Pins --- */
#define SPIA_SCK             12
#define SPIA_MISO            13
#define SPIA_MOSI            11

/* --- Display --- */
#define DSP_MODEL            DSP_DUMMY

/* --- Audio Decoder --- */
#define I2S_DOUT             7
#define I2S_BCLK             6
#define I2S_LRC              5

/* --- Inputs --- */
#define ENC_CLK              16
#define ENC_DT               17
#define ENC_SW               18
#define ENC_PULLUP           true      /* use internal pullup on rotary (default true) */
#define ENC_SW_PULLUP        true      /* use internal pullup on button (default true) */
#define ENC2_CLK             3
#define ENC2_DT              46
#define ENC2_SW              9
#define BTN_PLAY             1
#define BTN_PREV             2
#define BTN_NEXT             42
#define BTN_UP               41
#define BTN_DOWN             40
#define BTN_MODE             39
#define DEEP_SLEEP_DISABLE

/* --- Peripherals and Build Options --- */
#define IR_PIN               38
#define SD_CS                21
#define SD_SPI               'A'       /* assign SD to SPI bus */

#endif // myoptions_h
