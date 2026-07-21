#ifndef myoptions_h
#define myoptions_h

/*        ************************************************************************      */
/*        *        This file must be in the root folder of the sketch !!!        *      */
/*        ************************************************************************      */
/*        . . .  CHECK options.h for full options, examples, and overrides   . . .      */

/* --- Trip5 Note --- */

// If you're looking at this file, you can see how I use it to contain various builds inside one myoptions.h file.
// It's a messy file, organized as best as I could keep it as my number of builds expands.
// The pins used are not neccessarily "best practice" as some pre-date my generator and correct knowledge of ESP32-S3.

// In VSCode, the first environment in platformio.ini is used to evaluate the #ifdef chains
// The first "build_flags = -DSH1106_PCM_REMOTE" is a #define... so to debug, we need to undefine it first:
//
//#undef SH1106_PCM_REMOTE
//
// Uncomment the next line for safety (pio does weird things otherwise)...
//
//#error YOU FORGOT TO COMMENT THE TOP OF MYOPTIONS.H AGAIN!
//
// Then uncomment the build to debug:
//
//#define SH1106_PCM_REMOTE            // Self-contained OLED with PCM, Remote
//#define SH1106_PCM_1BUTTON           // Mini OLED with PCM, 1 Button, Speakers built-in
//#define SH1106_VS1053_3BUTTONS       // Ali Speaker with OLED, VS1053, 3 Buttons
//#define ST7735_PCM_1BUTTON           // Color TFT (red board) with PCM I2S, 1 Button
//#define ILI9488_PCM_1BUTTON          // Big Screen with PCM, 1 button
//#define ILI9488_VS1053_1BUTTON       // the same but with a VS1053 decoder
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
//  Used by Web-flasher & ehDP as a friendly name    The URL shown on the Releases page (optional but highly recommended)
#if defined(SH1106_PCM_REMOTE)
  #define FIRMWARE "trip5_sh1106_pcm_remote.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "OLED with Remote" // "https://trip5.github.io/ehRadio/myoptions/generator.html#eyJibiI6IkVTUDMyLVMzLURldktpdEMtMSBOMTZSOCAoMTZNQiBGbGFzaCA4TUIgUFNSQU0pIiwiZGQiOiJEU1BfTU9ERUwgRFNQX1NIMTEwNiIsImRuIjoiU0gxMTA2IChJMkMgT0xFRCkiLCJhbiI6IkkyUyAoUENNIEkyUyBEZWNvZGVyKSIsImNpIjpbIlJvdGFyeSBFbmNvZGVyIiwiV2FrZSBQaW4iXSwiY3AiOlsiTEVEIiwiTVFUVF9FTkFCTEUiLCJTRCBDYXJkIFJlYWRlciJdLCJjcyI6WyJBIl0sImNvIjpbIkxFRF9JTlZFUlQiXSwiY2QiOlsiVElNRVpPTkUiLCJDTE9DS19UV0VMVkUiLCJPTkVfQ0xJQ0tfU1dJVENIIiwiUExBWUxJU1RfREVGQVVMVF9VUkwiLCJTQ1JFRU5fRkxJUCIsIlNTX1BMQVlJTkciLCJTRF9TSFVGRkxFIiwiU0hPV19CVUZGRVJCQVIiLCJTSE9XX1ZVX01FVEVSIiwiU01BUlRfU1RBUlQiLCJTTlRQXzEiLCJTTlRQXzIiLCJXRUFUSEVSX0xBVCIsIldFQVRIRVJfTE9OIiwiV0lGSV9TQ0FOX0JFU1RfUlNTSSJdLCJwIjp7IlNQSUFfU0NLIjoiMjEiLCJTUElBX01JU08iOiIxMyIsIlNQSUFfTU9TSSI6IjE0IiwiSTJDX1NEQSI6IjQyIiwiSTJDX1NDTCI6IjQxIiwiSTJTX0RPVVQiOiIxMiIsIkkyU19CQ0xLIjoiMTEiLCJJMlNfTFJDIjoiMTAiLCJFTkNfQ0xLIjoiNDAiLCJFTkNfRFQiOiIzOSIsIkVOQ19TVyI6IjM4IiwiV0FLRV9QSU4iOiIxOCIsIkxFRF9QSU4iOiI4IiwiU0RfQ1MiOiI0NyJ9LCJ2Ijp7IlNEX1NQSSI6IkEiLCJUSU1FWk9ORSI6IkNhbmFkYS9BdGxhbnRpYyIsIkxFRF9JTlZFUlQiOiJ0cnVlIiwiQ0xPQ0tfVFdFTFZFIjoidHJ1ZSIsIk9ORV9DTElDS19TV0lUQ0giOiJ0cnVlIiwiU0NSRUVOX0ZMSVAiOiJ0cnVlIiwiU1NfUExBWUlORyI6InRydWUiLCJTRF9TSFVGRkxFIjoidHJ1ZSIsIlNIT1dfQlVGRkVSQkFSIjoidHJ1ZSIsIlNIT1dfVlVfTUVURVIiOiJ0cnVlIiwiU01BUlRfU1RBUlQiOiJ0cnVlIiwiV0lGSV9TQ0FOX0JFU1RfUlNTSSI6InRydWUiLCJGSVJNV0FSRV9OQU1FIjoic2gxMTA2X3BjbV9yZW1vdGUiLCJQTEFZTElTVF9ERUZBVUxUX1VSTCI6Imh0dHBzOi8vZ2l0aHViLmNvbS90cmlwNS93ZWJzdGF0aW9ucy9yZWxlYXNlcy9sYXRlc3QvZG93bmxvYWQvdHJpcDUtcmFkaW8tcGxheWxpc3QuY3N2IiwiU05UUF8xIjoiY2EucG9vbC5udHAub3JnIiwiU05UUF8yIjoicG9vbC5udHAub3JnIiwiV0VBVEhFUl9MQVQiOiI0NC42NDczOCIsIldFQVRIRVJfTE9OIjoiLTYzLjU4MDIifX0%3D"
#endif
#if defined(SH1106_PCM_1BUTTON)
  #define FIRMWARE "trip5_sh1106_pcm_1button.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "OLED with 1-Button" // "https://trip5.github.io/ehRadio/myoptions/generator.html#eyJibiI6IkVTUDMyLVMzLURldktpdEMtMSBOMTZSOCAoMTZNQiBGbGFzaCA4TUIgUFNSQU0pIiwiZGQiOiJEU1BfTU9ERUwgRFNQX1NIMTEwNiIsImRuIjoiU0gxMTA2IChJMkMgT0xFRCkiLCJhbiI6IkkyUyAoUENNIEkyUyBEZWNvZGVyKSIsImNpIjpbIlJvdGFyeSBFbmNvZGVyIl0sImNwIjpbIk1RVFRfRU5BQkxFIiwiU0QgQ2FyZCBSZWFkZXIiXSwiY3MiOlsiQSJdLCJjbyI6WyJMRURfSU5WRVJUIl0sImNkIjpbIlRJTUVaT05FIiwiQ0xPQ0tfVFdFTFZFIiwiT05FX0NMSUNLX1NXSVRDSCIsIlBMQVlMSVNUX0RFRkFVTFRfVVJMIiwiU0NSRUVOX0ZMSVAiLCJTU19QTEFZSU5HIiwiU0RfU0hVRkZMRSIsIlNIT1dfQlVGRkVSQkFSIiwiU0hPV19WVV9NRVRFUiIsIlNNQVJUX1NUQVJUIiwiU05UUF8xIiwiU05UUF8yIiwiVk9MVU1FX1BBR0UiLCJXRUFUSEVSX0xBVCIsIldFQVRIRVJfTE9OIiwiV0lGSV9TQ0FOX0JFU1RfUlNTSSJdLCJwIjp7IlNQSUFfU0NLIjoiMjEiLCJTUElBX01JU08iOiIxMyIsIlNQSUFfTU9TSSI6IjE0IiwiSTJDX1NEQSI6IjQyIiwiSTJDX1NDTCI6IjQxIiwiSTJTX0RPVVQiOiIxMiIsIkkyU19CQ0xLIjoiMTEiLCJJMlNfTFJDIjoiMTAiLCJFTkNfQ0xLIjoiNyIsIkVOQ19EVCI6IjE1IiwiRU5DX1NXIjoiMzgiLCJTRF9DUyI6IjQ3In0sInYiOnsiU0RfU1BJIjoiQSIsIlRJTUVaT05FIjoiQ2FuYWRhL0F0bGFudGljIiwiQ0xPQ0tfVFdFTFZFIjoidHJ1ZSIsIk9ORV9DTElDS19TV0lUQ0giOiJ0cnVlIiwiU0NSRUVOX0ZMSVAiOiJ0cnVlIiwiU1NfUExBWUlORyI6InRydWUiLCJTRF9TSFVGRkxFIjoidHJ1ZSIsIlNIT1dfQlVGRkVSQkFSIjoidHJ1ZSIsIlNIT1dfVlVfTUVURVIiOiJ0cnVlIiwiU01BUlRfU1RBUlQiOiJ0cnVlIiwiVk9MVU1FX1BBR0UiOiJ0cnVlIiwiV0lGSV9TQ0FOX0JFU1RfUlNTSSI6InRydWUiLCJGSVJNV0FSRV9OQU1FIjoic2gxMTA2X3BjbV8xYnV0dG9uIiwiUExBWUxJU1RfREVGQVVMVF9VUkwiOiJodHRwczovL2dpdGh1Yi5jb20vdHJpcDUvd2Vic3RhdGlvbnMvcmVsZWFzZXMvbGF0ZXN0L2Rvd25sb2FkL3RyaXA1LXJhZGlvLXBsYXlsaXN0LmNzdiIsIlNOVFBfMSI6ImNhLnBvb2wubnRwLm9yZyIsIlNOVFBfMiI6InBvb2wubnRwLm9yZyIsIldFQVRIRVJfTEFUIjoiNDQuNjQ3MzgiLCJXRUFUSEVSX0xPTiI6Ii02My41ODAyIn19"
#endif
#if defined(SH1106_VS1053_3BUTTONS)
  #define FIRMWARE "trip5_sh1106_vs1053_3buttons.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "OLED with 3-Buttons" // "https://trip5.github.io/ehRadio/myoptions/generator.html#eyJibiI6IkVTUDMyLVMzLURldktpdEMtMSBOMTZSOCAoMTZNQiBGbGFzaCA4TUIgUFNSQU0pIiwiZGQiOiJEU1BfTU9ERUwgRFNQX1NIMTEwNiIsImRuIjoiU0gxMTA2IChJMkMgT0xFRCkiLCJhbiI6IlZTMTA1MyAoU1BJIEF1ZGlvIERlY29kZXIpIiwiY2kiOlsiUm90YXJ5IEVuY29kZXIiLCJXYWtlIFBpbiJdLCJjcCI6WyJNUVRUX0VOQUJMRSIsIlNEIENhcmQgUmVhZGVyIl0sImNzIjpbIkEiLCJCIl0sImNvIjpbIlZTMTA1M19SU1QiLCJWU19QQVRDSF9FTkFCTEUiLCJMRURfSU5WRVJUIl0sImNkIjpbIlRJTUVaT05FIiwiQ0xPQ0tfVFdFTFZFIiwiT05FX0NMSUNLX1NXSVRDSCIsIlBMQVlMSVNUX0RFRkFVTFRfVVJMIiwiU0NSRUVOX0ZMSVAiLCJTU19QTEFZSU5HIiwiU0RfU0hVRkZMRSIsIlNIT1dfQlVGRkVSQkFSIiwiU0hPV19WVV9NRVRFUiIsIlNNQVJUX1NUQVJUIiwiU05UUF8xIiwiU05UUF8yIiwiVk9MVU1FX1BBR0UiLCJXRUFUSEVSX0xBVCIsIldFQVRIRVJfTE9OIiwiV0lGSV9TQ0FOX0JFU1RfUlNTSSJdLCJwIjp7IlNQSUFfU0NLIjoiMTIiLCJTUElBX01JU08iOiIxMyIsIlNQSUFfTU9TSSI6IjExIiwiU1BJQl9TQ0siOiIyMSIsIlNQSUJfTUlTTyI6IjIiLCJTUElCX01PU0kiOiIxIiwiSTJDX1NEQSI6IjQyIiwiSTJDX1NDTCI6IjQxIiwiVlMxMDUzX0NTIjoiOSIsIlZTMTA1M19EQ1MiOiIxNCIsIlZTMTA1M19EUkVRIjoiMTAiLCJWUzEwNTNfUlNUIjoiLTEiLCJFTkNfQ0xLIjoiNDAiLCJFTkNfRFQiOiIzOSIsIkVOQ19TVyI6IjM4IiwiV0FLRV9QSU4iOiIxNiIsIlNEX0NTIjoiNDcifSwidiI6eyJWUzEwNTNfU1BJIjoiQSIsIlNEX1NQSSI6IkIiLCJUSU1FWk9ORSI6IkNhbmFkYS9BdGxhbnRpYyIsIlZTX1BBVENIX0VOQUJMRSI6ImZhbHNlIiwiQ0xPQ0tfVFdFTFZFIjoidHJ1ZSIsIk9ORV9DTElDS19TV0lUQ0giOiJ0cnVlIiwiU0NSRUVOX0ZMSVAiOiJ0cnVlIiwiU1NfUExBWUlORyI6InRydWUiLCJTRF9TSFVGRkxFIjoidHJ1ZSIsIlNIT1dfQlVGRkVSQkFSIjoidHJ1ZSIsIlNIT1dfVlVfTUVURVIiOiJ0cnVlIiwiU01BUlRfU1RBUlQiOiJ0cnVlIiwiVk9MVU1FX1BBR0UiOiJ0cnVlIiwiV0lGSV9TQ0FOX0JFU1RfUlNTSSI6InRydWUiLCJGSVJNV0FSRV9OQU1FIjoic2gxMTA2X3ZzMTA1M18zYnV0dG9ucyIsIlBMQVlMSVNUX0RFRkFVTFRfVVJMIjoiaHR0cHM6Ly9naXRodWIuY29tL3RyaXA1L3dlYnN0YXRpb25zL3JlbGVhc2VzL2xhdGVzdC9kb3dubG9hZC90cmlwNS1yYWRpby1wbGF5bGlzdC5jc3YiLCJTTlRQXzEiOiJjYS5wb29sLm50cC5vcmciLCJTTlRQXzIiOiJwb29sLm50cC5vcmciLCJXRUFUSEVSX0xBVCI6IjQ0LjY0NzM4IiwiV0VBVEhFUl9MT04iOiItNjMuNTgwMiJ9fQ%3D%3D"
#endif
#if defined(ST7735_PCM_1BUTTON)
  #define FIRMWARE "trip5_st7735_pcm_1button.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "Color Screen with 1-Button" // "https://trip5.github.io/ehRadio/myoptions/generator.html#eyJibiI6IkVTUDMyLVMzLURldktpdEMtMSBOMTZSOCAoMTZNQiBGbGFzaCA4TUIgUFNSQU0pIiwiZGQiOiJEU1BfTU9ERUwgRFNQX1NUNzczNSIsImRuIjoiU1Q3NzM1IChTUEkgVEZUKSIsImFuIjoiSTJTIChQQ00gSTJTIERlY29kZXIpIiwiY2kiOlsiUm90YXJ5IEVuY29kZXIiXSwiY3AiOlsiTVFUVF9FTkFCTEUiLCJTRCBDYXJkIFJlYWRlciJdLCJjcyI6WyJBIiwiQiJdLCJjbyI6WyJMRURfSU5WRVJUIl0sImNkIjpbIlRJTUVaT05FIiwiQ0xPQ0tfVFdFTFZFIiwiT05FX0NMSUNLX1NXSVRDSCIsIlBMQVlMSVNUX0RFRkFVTFRfVVJMIiwiU0NSRUVOX0ZMSVAiLCJTU19QTEFZSU5HIiwiU0RfU0hVRkZMRSIsIlNIT1dfQlVGRkVSQkFSIiwiU0hPV19WVV9NRVRFUiIsIlNNQVJUX1NUQVJUIiwiU05UUF8xIiwiU05UUF8yIiwiVk9MVU1FX1BBR0UiLCJXRUFUSEVSX0xBVCIsIldFQVRIRVJfTE9OIiwiV0lGSV9TQ0FOX0JFU1RfUlNTSSJdLCJwIjp7IlNQSUFfU0NLIjoiMTIiLCJTUElBX01JU08iOiIyNTUiLCJTUElBX01PU0kiOiIxMSIsIlNQSUJfU0NLIjoiMjEiLCJTUElCX01JU08iOiIxMyIsIlNQSUJfTU9TSSI6IjE0IiwiVEZUX0RDIjoiMTAiLCJURlRfQ1MiOiI5IiwiSTJTX0RPVVQiOiIxNSIsIkkyU19CQ0xLIjoiNyIsIkkyU19MUkMiOiI2IiwiRU5DX0NMSyI6IjQwIiwiRU5DX0RUIjoiMzkiLCJFTkNfU1ciOiIzOCIsIlNEX0NTIjoiNDcifSwidiI6eyJTRF9TUEkiOiJCIiwiVElNRVpPTkUiOiJDYW5hZGEvQXRsYW50aWMiLCJDTE9DS19UV0VMVkUiOiJ0cnVlIiwiT05FX0NMSUNLX1NXSVRDSCI6InRydWUiLCJTQ1JFRU5fRkxJUCI6InRydWUiLCJTU19QTEFZSU5HIjoidHJ1ZSIsIlNEX1NIVUZGTEUiOiJ0cnVlIiwiU0hPV19CVUZGRVJCQVIiOiJ0cnVlIiwiU0hPV19WVV9NRVRFUiI6InRydWUiLCJTTUFSVF9TVEFSVCI6InRydWUiLCJWT0xVTUVfUEFHRSI6InRydWUiLCJXSUZJX1NDQU5fQkVTVF9SU1NJIjoidHJ1ZSIsIkZJUk1XQVJFX05BTUUiOiJzdDc3MzVfcGNtXzFidXR0b24iLCJQTEFZTElTVF9ERUZBVUxUX1VSTCI6Imh0dHBzOi8vZ2l0aHViLmNvbS90cmlwNS93ZWJzdGF0aW9ucy9yZWxlYXNlcy9sYXRlc3QvZG93bmxvYWQvdHJpcDUtcmFkaW8tcGxheWxpc3QuY3N2IiwiU05UUF8xIjoiY2EucG9vbC5udHAub3JnIiwiU05UUF8yIjoicG9vbC5udHAub3JnIiwiV0VBVEhFUl9MQVQiOiI0NC42NDczOCIsIldFQVRIRVJfTE9OIjoiLTYzLjU4MDIifX0%3D"
#endif
#if defined(ILI9488_PCM_1BUTTON)
  #define FIRMWARE "trip5_ili9488_pcm_1button.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "Big Color Screen with 1-Button" // "https://trip5.github.io/ehRadio/myoptions/generator.html#eyJibiI6IkVTUDMyLVMzLURldktpdEMtMSBOMTZSOCAoMTZNQiBGbGFzaCA4TUIgUFNSQU0pIiwiZGQiOiJEU1BfTU9ERUwgRFNQX0lMSTk0ODgiLCJkbiI6IklMSTk0ODggKFNQSSBURlQpIiwiYW4iOiJJMlMgKFBDTSBJMlMgRGVjb2RlcikiLCJjaSI6WyJSb3RhcnkgRW5jb2RlciJdLCJjcCI6WyJNUVRUX0VOQUJMRSIsIlNEIENhcmQgUmVhZGVyIl0sImNzIjpbIkEiLCJCIl0sImNvIjpbIlRGVF9SU1QiLCJCUklHSFRORVNTX1BJTiIsIkRTUF9ESU1NSU5HX0VOQUJMRUQiLCJEU1BfSU5WRVJUX1FVSVJLIiwiQklHX0JPT1RfTE9HTyIsIkxFRF9JTlZFUlQiXSwiY2QiOlsiVElNRVpPTkUiLCJDTE9DS19UV0VMVkUiLCJPTkVfQ0xJQ0tfU1dJVENIIiwiUExBWUxJU1RfREVGQVVMVF9VUkwiLCJTQ1JFRU5fRkxJUCIsIlNTX1BMQVlJTkciLCJTRF9TSFVGRkxFIiwiU0hPV19CVUZGRVJCQVIiLCJTSE9XX1ZVX01FVEVSIiwiU01BUlRfU1RBUlQiLCJTTlRQXzEiLCJTTlRQXzIiLCJWT0xVTUVfUEFHRSIsIldFQVRIRVJfTEFUIiwiV0VBVEhFUl9MT04iLCJXSUZJX1NDQU5fQkVTVF9SU1NJIl0sInAiOnsiU1BJQV9TQ0siOiIxMiIsIlNQSUFfTUlTTyI6IjI1NSIsIlNQSUFfTU9TSSI6IjExIiwiU1BJQl9TQ0siOiIyMSIsIlNQSUJfTUlTTyI6IjIiLCJTUElCX01PU0kiOiIxIiwiVEZUX0RDIjoiMTAiLCJURlRfQ1MiOiI5IiwiVEZUX1JTVCI6Ii0xIiwiQlJJR0hUTkVTU19QSU4iOiI0IiwiSTJTX0RPVVQiOiIxNSIsIkkyU19CQ0xLIjoiNyIsIkkyU19MUkMiOiI2IiwiRU5DX0NMSyI6IjQwIiwiRU5DX0RUIjoiMzkiLCJFTkNfU1ciOiIzOCIsIlNEX0NTIjoiNDcifSwidiI6eyJTRF9TUEkiOiJCIiwiVElNRVpPTkUiOiJDYW5hZGEvQXRsYW50aWMiLCJEU1BfRElNTUlOR19FTkFCTEVEIjoidHJ1ZSIsIkRTUF9JTlZFUlRfUVVJUksiOiJ0cnVlIiwiQklHX0JPT1RfTE9HTyI6InRydWUiLCJDTE9DS19UV0VMVkUiOiJ0cnVlIiwiT05FX0NMSUNLX1NXSVRDSCI6InRydWUiLCJTQ1JFRU5fRkxJUCI6InRydWUiLCJTU19QTEFZSU5HIjoidHJ1ZSIsIlNEX1NIVUZGTEUiOiJ0cnVlIiwiU0hPV19CVUZGRVJCQVIiOiJ0cnVlIiwiU0hPV19WVV9NRVRFUiI6InRydWUiLCJTTUFSVF9TVEFSVCI6InRydWUiLCJWT0xVTUVfUEFHRSI6InRydWUiLCJXSUZJX1NDQU5fQkVTVF9SU1NJIjoidHJ1ZSIsIkZJUk1XQVJFX05BTUUiOiJpbGk5NDg4X3BjbV8xYnV0dG9uIiwiUExBWUxJU1RfREVGQVVMVF9VUkwiOiJodHRwczovL2dpdGh1Yi5jb20vdHJpcDUvd2Vic3RhdGlvbnMvcmVsZWFzZXMvbGF0ZXN0L2Rvd25sb2FkL3RyaXA1LXJhZGlvLXBsYXlsaXN0LmNzdiIsIlNOVFBfMSI6ImNhLnBvb2wubnRwLm9yZyIsIlNOVFBfMiI6InBvb2wubnRwLm9yZyIsIldFQVRIRVJfTEFUIjoiNDQuNjQ3MzgiLCJXRUFUSEVSX0xPTiI6Ii02My41ODAyIn19"
#endif
#if defined(ILI9488_VS1053_1BUTTON)
  #define FIRMWARE "trip5_ili9488_vs1053_1button.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "Big Color Screen with 1-Button Redux" // "https://trip5.github.io/ehRadio/myoptions/generator.html#eyJibiI6IkVTUDMyLVMzLURldktpdEMtMSBOMTZSOCAoMTZNQiBGbGFzaCA4TUIgUFNSQU0pIiwiZGQiOiJEU1BfTU9ERUwgRFNQX0lMSTk0ODgiLCJkbiI6IklMSTk0ODggKFNQSSBURlQpIiwiYW4iOiJWUzEwNTMgKFNQSSBBdWRpbyBEZWNvZGVyKSIsImNpIjpbIlJvdGFyeSBFbmNvZGVyIiwiQnV0dG9uOiBOZXh0IFN0YXRpb24vVHJhY2siXSwiY3AiOlsiTVFUVF9FTkFCTEUiLCJTRCBDYXJkIFJlYWRlciJdLCJjcyI6WyJBIiwiQiJdLCJjbyI6WyJURlRfUlNUIiwiQlJJR0hUTkVTU19QSU4iLCJEU1BfRElNTUlOR19FTkFCTEVEIiwiRFNQX0lOVkVSVF9RVUlSSyIsIkJJR19CT09UX0xPR08iLCJWUzEwNTNfUlNUIiwiTEVEX0lOVkVSVCJdLCJjZCI6WyJUSU1FWk9ORSIsIkNMT0NLX1RXRUxWRSIsIk9ORV9DTElDS19TV0lUQ0giLCJQTEFZTElTVF9ERUZBVUxUX1VSTCIsIlNTX1BMQVlJTkciLCJTRF9TSFVGRkxFIiwiU0hPV19CVUZGRVJCQVIiLCJTSE9XX1ZVX01FVEVSIiwiU01BUlRfU1RBUlQiLCJTTlRQXzEiLCJTTlRQXzIiLCJWT0xVTUVfUEFHRSIsIldFQVRIRVJfTEFUIiwiV0VBVEhFUl9MT04iLCJXSUZJX1NDQU5fQkVTVF9SU1NJIl0sInAiOnsiU1BJQV9TQ0siOiIxMiIsIlNQSUFfTUlTTyI6IjEzIiwiU1BJQV9NT1NJIjoiMTEiLCJTUElCX1NDSyI6IjE4IiwiU1BJQl9NSVNPIjoiMTciLCJTUElCX01PU0kiOiIxNiIsIlRGVF9EQyI6IjEwIiwiVEZUX0NTIjoiOSIsIlRGVF9SU1QiOiItMSIsIkJSSUdIVE5FU1NfUElOIjoiNCIsIlZTMTA1M19DUyI6IjE1IiwiVlMxMDUzX0RDUyI6IjciLCJWUzEwNTNfRFJFUSI6IjYiLCJWUzEwNTNfUlNUIjoiLTEiLCJFTkNfQ0xLIjoiNDAiLCJFTkNfRFQiOiIzOSIsIkVOQ19TVyI6IjM4IiwiQlROX05FWFQiOiI0MiIsIlNEX0NTIjoiNDcifSwidiI6eyJWUzEwNTNfU1BJIjoiQiIsIlNEX1NQSSI6IkEiLCJUSU1FWk9ORSI6IkNhbmFkYS9BdGxhbnRpYyIsIkRTUF9ESU1NSU5HX0VOQUJMRUQiOiJ0cnVlIiwiRFNQX0lOVkVSVF9RVUlSSyI6InRydWUiLCJCSUdfQk9PVF9MT0dPIjoidHJ1ZSIsIkNMT0NLX1RXRUxWRSI6InRydWUiLCJPTkVfQ0xJQ0tfU1dJVENIIjoidHJ1ZSIsIlNTX1BMQVlJTkciOiJ0cnVlIiwiU0RfU0hVRkZMRSI6InRydWUiLCJTSE9XX0JVRkZFUkJBUiI6InRydWUiLCJTSE9XX1ZVX01FVEVSIjoidHJ1ZSIsIlNNQVJUX1NUQVJUIjoidHJ1ZSIsIlZPTFVNRV9QQUdFIjoidHJ1ZSIsIldJRklfU0NBTl9CRVNUX1JTU0kiOiJ0cnVlIiwiRklSTVdBUkVfTkFNRSI6ImlsaTk0ODhfdnMxMDUzXzFidXR0b24iLCJQTEFZTElTVF9ERUZBVUxUX1VSTCI6Imh0dHBzOi8vZ2l0aHViLmNvbS90cmlwNS93ZWJzdGF0aW9ucy9yZWxlYXNlcy9sYXRlc3QvZG93bmxvYWQvdHJpcDUtcmFkaW8tcGxheWxpc3QuY3N2IiwiU05UUF8xIjoiY2EucG9vbC5udHAub3JnIiwiU05UUF8yIjoicG9vbC5udHAub3JnIiwiV0VBVEhFUl9MQVQiOiI0NC42NDczOCIsIldFQVRIRVJfTE9OIjoiLTYzLjU4MDIifX0%3D"
#endif
#if defined(ES3C28P)
  #define FIRMWARE "trip5_es3c28p.bin" // "esp32_s3_n16r8", "ESP32-S3", "Trip5"
  #define FIRMWARE_NAME "ES3C28P" // "https://trip5.github.io/ehRadio/myoptions/generator.html#eyJibiI6IkVTUDMyLVMzIEVTM0MyOFAgKEVTUDMyLVMzLU4xNlI4KSIsImRkIjoiRFNQX01PREVMIERTUF9JTEk5MzQxIiwiZG4iOiJJTEk5MzQxIChTUEkgMzIweDI0MCBURlQpIiwiYWQiOiJVU0VfRVM4MzExIiwiYW4iOiJFUzgzMTEgKFBDTSBJMlMgTW9ubyBEZWNvZGVyKSIsImNpIjpbIlRTX01PREVMIFRTX01PREVMX0ZUNjMzNiJdLCJjcCI6WyJSR0IgTEVEIiwiTVFUVF9FTkFCTEUiLCJTRCBDYXJkIFJlYWRlciJdLCJjcyI6WyJBIiwiQiJdLCJjbyI6WyJURlRfUlNUIiwiQlJJR0hUTkVTU19QSU4iLCJEU1BfRElNTUlOR19FTkFCTEVEIiwiRFNQX0lOVkVSVF9RVUlSSyIsIk1VVEVfUElOIiwiRVM4MzExX0kyQ19TREEiLCJFUzgzMTFfSTJDX1NDTCIsIkxFRF9JTlZFUlQiXSwiY2QiOlsiVElNRVpPTkUiLCJDTE9DS19UV0VMVkUiLCJPTkVfQ0xJQ0tfU1dJVENIIiwiUExBWUxJU1RfREVGQVVMVF9VUkwiLCJTU19QTEFZSU5HIiwiU0RfU0hVRkZMRSIsIlNIT1dfQlVGRkVSQkFSIiwiU0hPV19WVV9NRVRFUiIsIlNNQVJUX1NUQVJUIiwiU05UUF8xIiwiU05UUF8yIiwiVk9MVU1FX1BBR0UiLCJXRUFUSEVSX0xBVCIsIldFQVRIRVJfTE9OIiwiV0lGSV9TQ0FOX0JFU1RfUlNTSSJdLCJwIjp7IlNQSUFfU0NLIjoiMTIiLCJTUElBX01JU08iOiIyNTUiLCJTUElBX01PU0kiOiIxMSIsIlNQSUJfU0NLIjoiMzgiLCJTUElCX01JU08iOiIzOSIsIlNQSUJfTU9TSSI6IjQwIiwiVEZUX0RDIjoiNDYiLCJURlRfQ1MiOiIxMCIsIlRGVF9SU1QiOiItMSIsIkJSSUdIVE5FU1NfUElOIjoiNDUiLCJJMlNfTUNMSyI6IjQiLCJJMlNfQkNMSyI6IjUiLCJJMlNfTFJDIjoiNyIsIkkyU19ET1VUIjoiOCIsIkkyU19ESU4iOiI2IiwiTVVURV9QSU4iOiIxIiwiRVM4MzExX0kyQ19TREEiOiIxNiIsIkVTODMxMV9JMkNfU0NMIjoiMTUiLCJUU19TREEiOiIxNiIsIlRTX1NDTCI6IjE1IiwiVFNfSU5UIjoiMTciLCJUU19SU1QiOiIxOCIsIlJHQl9MRURfUElOIjoiNDIiLCJTRF9DUyI6IjQ3In0sInYiOnsiTVVURV9WQUwiOiJISUdIIiwiU0RfU1BJIjoiQiIsIlRJTUVaT05FIjoiQ2FuYWRhL0F0bGFudGljIiwiRFNQX0RJTU1JTkdfRU5BQkxFRCI6InRydWUiLCJEU1BfSU5WRVJUX1FVSVJLIjoidHJ1ZSIsIlBMQVlFUl9GT1JDRV9NT05PIjoidHJ1ZSIsIkNMT0NLX1RXRUxWRSI6InRydWUiLCJPTkVfQ0xJQ0tfU1dJVENIIjoidHJ1ZSIsIlNTX1BMQVlJTkciOiJ0cnVlIiwiU0RfU0hVRkZMRSI6InRydWUiLCJTSE9XX0JVRkZFUkJBUiI6InRydWUiLCJTSE9XX1ZVX01FVEVSIjoidHJ1ZSIsIlNNQVJUX1NUQVJUIjoidHJ1ZSIsIlZPTFVNRV9QQUdFIjoidHJ1ZSIsIldJRklfU0NBTl9CRVNUX1JTU0kiOiJ0cnVlIiwiRklSTVdBUkVfTkFNRSI6ImVzM2MyOHAiLCJFUzgzMTFfTUFYX0kyUyI6IjE4MCIsIlBMQVlMSVNUX0RFRkFVTFRfVVJMIjoiaHR0cHM6Ly9naXRodWIuY29tL3RyaXA1L3dlYnN0YXRpb25zL3JlbGVhc2VzL2xhdGVzdC9kb3dubG9hZC90cmlwNS1yYWRpby1wbGF5bGlzdC5jc3YiLCJTTlRQXzEiOiJjYS5wb29sLm50cC5vcmciLCJTTlRQXzIiOiJwb29sLm50cC5vcmciLCJXRUFUSEVSX0xBVCI6IjQ0LjY0NzM4IiwiV0VBVEhFUl9MT04iOiItNjMuNTgwMjkifX0%3D"
#endif

#define ENABLE_UPDATER // enables OTA updates

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
  #define SPIB_SCK           21        /* Bus B pins (SD & VS1053) */
  #define SPIB_MISO          2
  #define SPIB_MOSI          1
#elif defined(ILI9488_VS1053_1BUTTON)
  #define SPIA_DEFAULT                 /* SCK/CLK 12, MOSI/SDA 11, MISO 13 (Display and SD)*/
  #define SPIB_SCK           18        /* Bus B pins (VS1053) */
  #define SPIB_MISO          16
  #define SPIB_MOSI          17
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
#if defined(ILI9488_PCM_1BUTTON) || defined(ILI9488_VS1053_1BUTTON)
  #define DSP_MODEL          DSP_ILI9488 /* Big Display */
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
#elif defined(ILI9488_VS1053_1BUTTON)
  #define VS1053_SPI         'B'       /* assign VS1053 to Bus B */
  #define VS1053_CS          15
  #define VS1053_DCS         7
  #define VS1053_DREQ        6
  #define VS1053_RST         -1        /* set to -1 if connected to ESP EN pin */
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
  #define BTN_PREV           17
  #define BTN_NEXT           18
  #define BTN_MODE           16
  #define WAKE_PIN           16
#elif defined(SH1106_PCM_1BUTTON)
  #define BTN_NEXT           17
#elif defined(SH1106_PCM_REMOTE)
  #define BTN_PREV           17
  #define BTN_NEXT           16
  #define BTN_PLAY           18
  #define BTN_DOWN           7
  #define BTN_UP             15
  #define WAKE_PIN           18
#elif defined(ST7735_PCM_1BUTTON) || defined(ILI9488_PCM_1BUTTON) || defined(ILI9488_VS1053_1BUTTON)
  #define BTN_NEXT           42
#elif defined(ES3C28P)
  #define TS_MODEL           TS_MODEL_FT6336
  #define TS_SDA             16
  #define TS_SCL             15
  #define TS_INT             17
  #define TS_RST             18
  #define BTN_NEXT           0
#endif

#if defined(SH1106_VS1053_3BUTTONS) || defined(ST7735_PCM_1BUTTON) ||\
    defined(ILI9488_PCM_1BUTTON) || defined(ILI9488_VS1053_1BUTTON) || defined(SH1106_PCM_REMOTE)
  #define ENC_CLK            40
  #define ENC_DT             39
  #define ENC_SW             38
  #if defined(SH1106_PCM_REMOTE)
    #define ENC_STEPS          4
  #endif
  #if defined(ILI9488_VS1053_1BUTTON)
    #define VS_PATCH_ENABLE true
  #endif
#elif defined(SH1106_PCM_1BUTTON)
  #define ENC_CLK            7
  #define ENC_DT             15
  #define ENC_SW             16
  #define ENC_STEPS          4
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
#elif defined(ILI9488_VS1053_1BUTTON) || defined(SH1106_PCM_REMOTE) || defined(SH1106_PCM_1BUTTON)
  #define SD_SPI          'A'     /* assign SD to Bus B */
  #define SD_CS           47
#endif

#define MQTT_ENABLE

/* --- Battery --- */

#if defined(ES3C28P)
  /* Battery monitoring on ES3C28P board */
  #define BATTERY_PIN     9       /* GPIO9: ADC pin for battery voltage */
  //#define BATTERY_CHARGE_PIN 255  /* No charging status GPIO exposed (TP4054 CHRG pin not connected on ES3C28P) */

  #define BATTERY_DIVIDER_RATIO 2.0   /* 100k + 100k voltage divider = 1:2 ratio */
  #define BATTERY_ADC_REF_MV 3438     /* ESP32-S3 ADC reference voltage (calibrated EL103565 3000mAh 11.1Wh) */
  #define BATTERY_UPDATE_INTERVAL 60  /* Update interval in seconds */
  #define BATTERY_DEBUG               /* Uncomment to enable debug output */

  #define BATTERY_CHARGE_INFER_HOLD_SAMPLES 3 /* number of measurements (samples) to hold (e.g., 3 readings at BATTERY_UPDATE_INTERVAL) */
  #define BATTERY_IMMEDIATE_PERCENT_THRESHOLD 3 /* percent */
  #define BATTERY_CANDIDATE_PERCENT_DELTA 1 /* percent */
  #define BATTERY_SUSTAINED_PERCENT_WINDOW_THRESHOLD 0 /* percent over hold window */
#endif


/* --- User Defaults --- */

#define CLOCK_TWELVE         true
#define ONE_CLICK_SWITCH     true
#define PLAYLIST_DEFAULT_URL "https://github.com/trip5/webstations/releases/latest/download/trip5-radio-playlist.csv" /* downloaded on fresh install */
#define SS_PLAYING           true
#define SD_SHUFFLE           true
#define SHOW_BUFFERBAR       true
#define SHOW_VU_METER        true
#define SMART_START          true
#define SNTP_1               "ca.pool.ntp.org"
#define SNTP_2               "pool.ntp.org"
#define VOLUME_PAGE          true
#define WEATHER_LAT          "44.64738" /* latitude */
#define WEATHER_LON          "-63.5802" /* longitude */
#define WIFI_SCAN_BEST_RSSI  true

#if defined(SH1106_PCM_REMOTE)
  #undef VOLUME_PAGE
#endif


/* --- Time Zone --- */

#define TIMEZONE_NAME   "Canada/Atlantic"
#define TIMEZONE_POSIX  "AST4ADT,M3.2.0,M11.1.0"

/* Congratulations on searching around!  This myoptions.h really shows why I build the way that I do. */
/* It interacts with platformio.ini with a -D[define] to build a bunch of firmwares.  Cool, right? */


/* --- Other Useful Options --- */
/* options.h will list even more but it's a big file... */

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
/* ...or your firmware is not available from Trip5's Github... yet. */
/* Read the notes in the ./builds folder for more detailed information */
//#define GITHUBURL "https://github.com/myid/ehradio" // used by the radio to update firmware and files...

/* Disable automatic runtime downloads from GitHub (ESPFileUpdater) for this board only. */
//#define DISABLE_UPDATER

/* --- Locales Notes --- */
//#define DSP_LOCALE "de_DE" // sets the display language - see the available options by checking files in locale/display folder 
//#define WEBUI_LOCALE "de_DE" // can set a default WebUI locale different than the display - check locale/webui folder .json files (user-configurable)

/* --- Debug? --- */
//#define CORE_MONITOR
//#define ALL_DEBUG_LOGS
//#define BATTERY_DEBUG

/* --- Fonts --- */
// Display Text:
//#define DISPLAYFONT MATRIXCHUNKY
//#define DISPLAYFONT MATRIXLIGHT
//#define DISPLAYFONT X11

// Clock:
//#define CLOCKFONT CHUNKY6
//#define CLOCKFONT YO_MONO

// Transform:
//#define PRETEXT_ALLCAPS
//#define PRETEXT_FOLDACCENT
//#define WWW_CASETRANSFORM
//#define CLOCKGLOW false

//#define DEEP_SLEEP_DISABLE

#endif // myoptions_h
