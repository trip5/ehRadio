## Custom Libraries

The following libraries are custom to ehRadio, mostly inherited from yoRadio v0.9.533.  They cannot be easily replaced with an external library.

### Trip5's Thoughts

It seems to me most of this codebase was actually developed for Arduino IDE by hand and if it compiled, it was left alone until functionality was broke and it had to be fixed.

This file was my own (and AI-assisted) investigation of the source of the libraries.
Actually there used to be dozens of libraries just for displays but they were copies of libraries available on Platformio.
Some libraries like encoder and onebutton were outdated and benefitted from using online libraries.

But the audio decoding libraries are troublesome. They are extremely modified from whatever their original source was.
I have to assume most of it was cribbed from edzelf's code by e2002 in the early days of yoRadio, and as long as it worked, it wasn't updated.
See more about this below.

I don't like the idea of using a bunch of custom libraries... But...
I did a deep investigation into ILI9486/8 library to see if I could use another library and... it's irreplaceable.
Most of the libraries that remain here probably are - if simply because they are not available as libraries on platformio.

If you have trouble with one of these custom libraries or an online library...
Sorry and good luck.

- Trip5


### Display Drivers

#### Adafruit GC9106
  - https://github.com/prenticedavid/Adafruit_GC9102_kbv
  - not on Platformio (and not actually an Adafruit library)

#### Adafruit ST7796S
  - https://github.com/prenticedavid/Adafruit_ST7796S_kbv
  - not on Platformio (and not actually an Adafruit library)

#### Adafruit ILI9225 
  - from ёRadio v0.9.710, likely original or cribbed from other Adafruit drivers (like above)
  - Replaced ILI9225Fix (which required a lot of `#if DSP_MODEL==DSP_ILI9225` (or `!=`) fixes in the codebase)
    - original likely modified from https://github.com/arduinopavlodar/TFT_22_ILI9225
    - those fixes have been removed
  
#### FT6336_Touchscreen
  - made by https://github.com/kasperaitis for ehRadio
  - used as source: https://github.com/aselectroworks/Arduino-FT6336U

#### ILI9488
  - highly-modified from version 1.0.5 https://github.com/ZinggJM/ILI9486_SPI
  - possibly irreplaceable due to how it manipulates ILI9486/8 quirks
    - seriously, after some deep investigation, I'm impressed this display works as well as it does compared to other libraries

#### SSD1322
  - slightly-modified from initial commit https://github.com/JamesHagerman/Jamis_SSD1322


### Audio Decoder Drivers

#### ES8311_Audio
  - made by https://github.com/kasperaitis for ehRadio

#### I2S_Audio
  - likely adapted from https://github.com/schreibfaul1/ESP32-audioI2S (a long time ago)
  - ehRadio uses a library adapted from Maleksm's yoRadio mod https://4pda.to/forum/index.php?showtopic=1010378&st=11240#entry125839228
  - currently using v0.9.434m(04.04.25)
  - using Maleksm's v0.9.720m(23.06.26) will require GCC 11+

#### VS1053_Audio
  - original DNA in https://github.com/Edzelf/Esp-radio and https://github.com/Edzelf/ESP32Radio-V2
  - but then replaced with https://github.com/schreibfaul1/ESP32-vs1053_ext
  - ehRadio at first adapted Maleksm's yoRadio mod v0.9.434m(04.04.25): https://4pda.to/forum/index.php?showtopic=1010378&st=11240#entry125839228
  - attempts to update using Maleksm's mod v0.9.533m failed
  - ehRadio 2026.06.29 adapts nsteplanet's PR to yoRadio November 28, 2025: https://github.com/e2002/yoradio/pull/226
  - might update to Maleksm's v0.9.720m(23.06.26) after I2S is updated
    
#### Further notes

See also this issue: https://github.com/trip5/ehRadio/issues/39

Other folders (those with version notes in brackets) are kept as lineage: the upstream version each live
audio library was derived from, plus the two v0.9.720m copies a future graft would need. They are reference
material — **not** backups.

The only copies of our own libraries are `src/libraries/I2S_Audio/` and `src/libraries/VS1053_Audio/`, and
git history is their backup. The folder copies of them have been removed (2026-09-27): a copy of a library
we keep patching goes stale quietly, and restoring one drops every fix made since it was taken.
See the "frankenstein-surgery-notes" files for more information.
