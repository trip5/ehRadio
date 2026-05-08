## Pre-built Firmwares & WebUI Assets
  You can download the firmware and flash yourself or you can use my [Web Installer](https://trip5.github.io/ehRadio/firmware.html).
### Notes
  - You can download .bin files and flash to your ESP with `esptool`
  - you will need a firmware file appropriate to your build (see below)
  - depending on your board, choose the appropriate `board_*_bootloader.bin` and `board_*_partitions.bin`
  - for most ESP32s, use:
    - `esptool --chip esp32 --port com14 --baud 460800 write_flash -z 0x1000 esp32_bootloader.bin 0x8000 esp32_partitions.bin 0x10000 [firmware.bin]`
    - to include spiffs, add this to the above command: `0x390000 esp32_spiffs.bin`
  - for ESP32-S3s (like the ESP32-S3-N16R8) use:
    - `esptool --chip esp32s3 --port com8 --baud 460800 write_flash -z 0x0000 esp32_s3_n16r8_bootloader.bin 0x8000 esp32_s3_n16r8_partitions.bin 0x10000 [firmware.bin]`
    - to include spiffs, add this to the above command: `0x670000 esp32_s3_n16r8_spiffs.bin`
## Boards
  - each of the pre-built board binaries have pre-set hardware configurations, they cannot be changed
  - to view pin usage, click the link and scroll down then click preview
### Bare board firmwares (no peripherals, may be used for testing)
  - `board_esp32.bin`
  - `board_esp32_s3_n16r8.bin`
### Board Shared Binaries (Bootloader/Partitions)
  - `board_esp32_s3_n16r8_bootloader.bin` / `board_esp32_s3_n16r8_partitions.bin` / `board_esp32_s3_n16r8_spiffs.bin`
### Trip5 Firmware
  - [`trip5_sh1106_pcm_remote.bin`](https://trip5.github.io/ehRadio_myoptions/generator.html?b=ESP32-S3-DevKitC-1_44Pin&r=72,2,3,4,6,7,15,43,49,51,52,53,54,75,66&i=5,6,15,16,17,22,23,24,25,26,27,28,29,30,39,45,46,47,40&v=42,41,12,11,10,7,18,15,17,16,255,40,39,38,47,21,13,14,8)
  - [`trip5_sh1106_pcm_1button.bin`](https://trip5.github.io/ehRadio_myoptions/generator.html?b=ESP32-S3-DevKitC-1_44Pin&r=72,2,3,4,6,15,43,49,51,52,53,54,75&i=5,6,15,16,17,22,23,24,25,26,27,28,29,30,39,45,46,47&v=42,41,12,11,10,255,255,255,255,17,255,7,15,16,47,21,13,14)
  - [`trip5_sh1106_vs1053_3buttons.bin`](https://trip5.github.io/ehRadio_myoptions/generator.html?b=ESP32-S3-DevKitC-1_44Pin&r=72,2,3,4,6,15,44,48,49,51,52,53,54,75&i=5,6,18,19,20,21,22,23,24,25,26,27,28,29,30,39,45,46,47&v=42,41,9,14,10,-1,255,255,255,17,18,16,40,39,38,47,21,2,1)
  - [`trip5_st7735_pcm_1button.bin`](https://trip5.github.io/ehRadio_myoptions/generator.html?b=ESP32-S3-DevKitC-1_44Pin&r=72,2,3,4,6,11,36,43,49,51,52,53,54,75&i=1,2,3,4,15,16,17,22,23,24,25,26,27,28,29,30,39,45,46,47&v=10,9,-1,4,15,7,6,255,255,255,255,42,255,40,39,38,47,21,13,14)
  - [`trip5_ili9488_pcm_1button.bin`](https://trip5.github.io/ehRadio_myoptions/generator.html?b=ESP32-S3-DevKitC-1_44Pin&r=72,2,3,4,6,31,43,49,51,52,53,54,75&i=1,2,3,4,15,16,17,22,23,24,25,26,27,28,29,30,39,45,46,47&v=10,9,-1,4,15,7,6,255,255,255,255,42,255,40,39,38,47,21,2,1)
  - [`trip5_es3c28p.bin`](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display)
### Kasperaitis Firmware
  - [`kasperaitis_es3c28p.bin`](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display)
### Add Yours
  - you can either fork my `ehradio` repo and edit these files or request a build be added
    - use [the generator](https://trip5.github.io/ehRadio_myoptions/generator.html) and copy the link and include it in your request
### Web Assets
  - all other files are needed for the WebUI functionality
  - they are available here to allow automatic downloading to SPIFFS
  - it is NOT necessary to download them (or the SPIFFS bin) - the radio will download them automatically after network connection is established
