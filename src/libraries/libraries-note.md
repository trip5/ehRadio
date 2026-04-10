## Custom Libraries

The following libraries are custom to ehRadio, mostly inherited from yoRadio v0.9.533.  They cannot be easily replaced with an external library.

### Display Drivers

#### Adafruit GC9106 https://github.com/prenticedavid/Adafruit_GC9102_kbv
  - not on Platformio (and not actually an Adafruit library)

#### Adafruit ST7796S https://github.com/prenticedavid/Adafruit_ST7796S_kbv
  - not on Platformio (and not actually an Adafruit library)
  
#### FT6336_Touchscreen
  - made by https://github.com/kasperaitis for ehRadio
  - used as source: https://github.com/aselectroworks/Arduino-FT6336U

#### ILI9225Fix https://github.com/arduinopavlodar/TFT_22_ILI9225
  - not on Platformio and also highly-modified from an unknown version

#### ILI9488 https://github.com/ZinggJM/ILI9486_SPI
  - highly-modified from version 1.0.5?

#### LiquidCrystalI2C https://github.com/johnrickman/LiquidCrystal_I2C
  - slightly-modified from version 1.1.3

#### SSD1322 https://github.com/JamesHagerman/Jamis_SSD1322
  - slightly-modified from initial commit

#### ST7920 https://github.com/BornaBiro/ST7920_GFX_Library
  - very similar or modified (or perhaps share a common source)
  - may be worth looking at as well: https://github.com/BornaBiro/ST7920_GFX_Library

### Audio Decoder Drivers

#### ES8311_Audio
  - made by https://github.com/kasperaitis for ehRadio

#### I2S_Audio
  - likely adapted https://github.com/schreibfaul1/ESP32-audioI2S
  - from Maleksm's yoRadio mod v0.9.512m: https://4pda.to/forum/index.php?showtopic=1010378&st=11240#entry125839228
    - Maleksm says source from Wolle (schreibfaul1) 3.3.2l on 2025.07.09
    - But source mostly matches 3.1.0 - so perhaps there's a few minor fixes from 3.3.2 or...?
  - attempt to update using Maleksm's mod v0.9.533m failed (probably due to similar issues as below)

#### VS1053_Audio
  - original DNA in https://github.com/Edzelf/Esp-radio and https://github.com/Edzelf/ESP32Radio-V2
  - but then replaced with https://github.com/schreibfaul1/ESP32-vs1053_ext
  - likely adapted from https://github.com/nstepanets/ESP32-vs1053_ext
  - from Maleksm's yoRadio mod v0.9.512m: https://4pda.to/forum/index.php?showtopic=1010378&st=11240#entry125839228
    - Maleksm says source from Wolle (schreibfaul1) 3.0.13t on 2024.11.16
  - attempt to update using Maleksm's mod v0.9.533m failed (probably due to similar issues)

#### Further notes

See also this issue: https://github.com/trip5/ehRadio/issues/39

Attempts were made to full upgrade to more recent versions from https://github.com/schreibfaul1/ESP32-audioI2S and https://github.com/nstepanets/ESP32-vs1053_ext

But these lines will be required in `platformio.ini`:

```
[env]
platform = espressif32@6.10.0 ; needed to support GCC 11.2.0
platform_packages = 
  ; this allows gnu++20 (for span function in I2C_Audio)
  toolchain-xtensa-esp32 @ ~11.2.0 ; ESP32 (original)
  toolchain-xtensa-esp32s2 @ ~11.2.0 ; ESP32-S2
  toolchain-xtensa-esp32s3 @ ~11.2.0 ; ESP32-S3
  toolchain-riscv32-esp @ ~11.2.0 ; ESP32-C3/C6/H2 (RISC-V)

build_unflags = 
  -std=gnu++11
  -std=gnu++14
  -std=gnu++17
build_flags =
  -std=gnu++20
```


A modification may be needed to `Audio.h`:
```
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <driver/i2s_std.h>
#else
#include <driver/i2s.h>
#endif
```

Also, it looks like:
```
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
```
Will all need to be replaced with:
```
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
```
Because new Arduino-ESP32 v3.x uses unified network API. ehRadio still uses the old v2.x API.

That would likely be a very large re-factor.

- Trip5, 2026-03
