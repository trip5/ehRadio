#pragma once
#include <Arduino.h>
#include <pgmspace.h>

// ==========================================================================
// icons.h — Icon glyphs extracted from the classic glcdfont
// ==========================================================================
// Each icon is 8 bytes (8 rows tall × 6 pixels wide).
// 6-bit values: bit 5 = leftmost pixel (col 0), bit 0 = rightmost (col 5).
// Rendering uses 0x20>>col to unpack (matching 6-bit MSB alignment).
//  @ = pixel ON    . = pixel OFF
// ==========================================================================


// \001 RSSI bar 00__ (used in display.cpp RSSI rendering)
static const uint8_t RSSI_00[] PROGMEM = {
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b110000,  // @@....
    0b000000,  // ......
};

// \002 RSSI bar __00 (used in display.cpp RSSI rendering)
static const uint8_t RSSI__00[] PROGMEM = {
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
};

// \003 RSSI bar 10__ (used in display.cpp RSSI rendering)
static const uint8_t RSSI_10[] PROGMEM = {
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b110000,  // @@....
    0b110000,  // @@....
    0b000000,  // ......
};

// \004 RSSI bar 11__ (used in display.cpp RSSI rendering)
static const uint8_t RSSI_11[] PROGMEM = {
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000110,  // ...@@.
    0b110110,  // @@.@@.
    0b110110,  // @@.@@.
    0b000000,  // ......
};

// \005 RSSI bar __10 (used in display.cpp RSSI rendering)
static const uint8_t RSSI__10[] PROGMEM = {
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b110000,  // @@....
    0b110000,  // @@....
    0b110000,  // @@....
    0b110000,  // @@....
    0b000000,  // ......
};

// \006 RSSI bar __11 (used in display.cpp RSSI rendering)
static const uint8_t RSSI__11[] PROGMEM = {
    0b000000,  // ......
    0b000000,  // ......
    0b000110,  // ...@@.
    0b110110,  // @@.@@.
    0b110110,  // @@.@@.
    0b110110,  // @@.@@.
    0b110110,  // @@.@@.
    0b000000,  // ......
};


// \013 Battery bar 00__ (used in display.cpp Battery rendering)
static const uint8_t BATTERY_00[] PROGMEM = {
    0b111111,  // @@@@@@
    0b100000,  // @.....
    0b100000,  // @.....
    0b100000,  // @.....
    0b100000,  // @.....
    0b100000,  // @.....
    0b111111,  // @@@@@@
    0b000000,  // ......
};


// \014 Battery bar __00 (used in display.cpp Battery rendering)
static const uint8_t BATTERY__00[] PROGMEM = {
    0b111110,  // @@@@@.
    0b000010,  // ....@.
    0b000011,  // ....@@
    0b000001,  // .....@
    0b000011,  // ....@@
    0b000010,  // ....@.
    0b111110,  // @@@@@.
    0b000000,  // ......
};


// \015 Battery bar 10__ (used in display.cpp Battery rendering)
static const uint8_t BATTERY_10[] PROGMEM = {
    0b111111,  // @@@@@@
    0b111100,  // @@@@..
    0b111100,  // @@@@..
    0b111100,  // @@@@..
    0b111100,  // @@@@..
    0b111100,  // @@@@..
    0b111111,  // @@@@@@
    0b000000,  // ......
};

// \016 Battery bar 11__ (used in display.cpp Battery rendering)
static const uint8_t BATTERY_11[] PROGMEM = {
    0b111111,  // @@@@@@
    0b111111,  // @@@@@@
    0b111111,  // @@@@@@
    0b111111,  // @@@@@@
    0b111111,  // @@@@@@
    0b111111,  // @@@@@@
    0b111111,  // @@@@@@
    0b000000,  // ......
};


// \017 Battery bar __10 (used in display.cpp Battery rendering)
static const uint8_t BATTERY__10[] PROGMEM = {
    0b111110,  // @@@@@.
    0b110010,  // @@..@.
    0b110011,  // @@..@@
    0b110001,  // @@...@
    0b110011,  // @@..@@
    0b110010,  // @@..@.
    0b111110,  // @@@@@.
    0b000000,  // ......
};

// \020 Battery bar __11 (used in display.cpp Battery rendering)
static const uint8_t BATTERY__11[] PROGMEM = {
    0b111110,  // @@@@@.
    0b111110,  // @@@@@.
    0b111111,  // @@@@@@
    0b111111,  // @@@@@@
    0b111111,  // @@@@@@
    0b111110,  // @@@@@.
    0b111110,  // @@@@@.
    0b000000,  // ......
};

// \023 Speaker icon (used in conf files: voltxtFmt)
static const uint8_t SPEAKER[] PROGMEM = {
    0b000010,  // ....@.
    0b000110,  // ...@@.
    0b111110,  // @@@@@.
    0b111110,  // @@@@@.
    0b111110,  // @@@@@.
    0b000110,  // ...@@.
    0b000010,  // ....@.
    0b000000,  // ......
};

// \024 Volume 1-25% (used in display.cpp Volume rendering)
static const uint8_t VOL_25[] PROGMEM = {
    0b000000,  // ......
    0b100000,  // @.....
    0b010000,  // .@....
    0b010000,  // .@....
    0b010000,  // .@....
    0b100000,  // @.....
    0b000000,  // ......
    0b000000,  // ......
};

// \025 Volume 26-50% (used in display.cpp Volume rendering)
static const uint8_t VOL_50[] PROGMEM = {
    0b010000,  // .@....
    0b001000,  // ..@...
    0b001000,  // ..@...
    0b001000,  // ..@...
    0b001000,  // ..@...
    0b001000,  // ..@...
    0b010000,  // .@....
    0b000000,  // ......
};

// \026 Volume 51-75% (used in display.cpp Volume rendering and to show normal boot with no smart start)
static const uint8_t VOL_75[] PROGMEM = {
    0b001000,  // ..@...
    0b000100,  // ...@..
    0b100100,  // @..@..
    0b100100,  // @..@..
    0b100100,  // @..@..
    0b000100,  // ...@..
    0b001000,  // ..@...
    0b000000,  // ......
};

// \027 Volume 76-100% (used in display.cpp Volume rendering)
static const uint8_t VOL_100[] PROGMEM = {
    0b010100,  // .@.@..
    0b001010,  // ..@.@.
    0b101010,  // @.@.@.
    0b101010,  // @.@.@.
    0b101010,  // @.@.@.
    0b001010,  // ..@.@.
    0b010100,  // .@.@..
    0b000000,  // ......
};

// \030 SD Card A (used in display.cpp for SD Offline Mode and to show boot mode)
static const uint8_t SD_A[] PROGMEM = {
    0b011111,  // .@@@@@
    0b011010,  // @@@.@.
    0b101010,  // @.@.@.
    0b101111,  // @.@@@@
    0b111111,  // @@@@@@
    0b111111,  // @@@@@@
    0b111111,  // @@@@@@
    0b000000,  // ......
};

// \031 SD Card B (used in display.cpp for SD Offline Mode and to show boot mode)
static const uint8_t SD_B[] PROGMEM = {
    0b100000,  // @.....
    0b100000,  // @.....
    0b100000,  // @.....
    0b100000,  // @.....
    0b100000,  // @.....
    0b100000,  // @.....
    0b100000,  // @.....
    0b000000,  // ......
};

// \032 Shuffle icon A (used in display.cpp for SD Offline Mode in place of SSID)
static const uint8_t SDSHUFFLE_A[] PROGMEM = {
    0b000000,  // ......
    0b111000,  // @@@...
    0b000100,  // ...@..
    0b000011,  // ....@@
    0b000100,  // ...@..
    0b111000,  // @@@`..
    0b000000,  // ......
    0b000000,  // ......
};

// \033 Shuffle icon B (used in display.cpp for SD Offline Mode in place of SSID)
static const uint8_t SDSHUFFLE_B[] PROGMEM = {
    0b000100,  // ...@..
    0b011110,  // .@@@@.
    0b100100,  // @..@..
    0b000000,  // ......
    0b100100,  // @..@..
    0b011110,  // .@@@@.
    0b000100,  // ...@..
    0b000000,  // ......
};

// \034 Pause icon (used in display.cpp to show boot mode: safe mode)
static const uint8_t PAUSE[] PROGMEM = {
    0b110110,  // @@.@@.
    0b110110,  // @@.@@.
    0b110110,  // @@.@@.
    0b110110,  // @@.@@.
    0b110110,  // @@.@@.
    0b110110,  // @@.@@.
    0b110110,  // @@.@@.
    0b000000,  // ......
};

// \035 Play / Next icon (used in display.cpp as progress bar fill and to show boot mode: smart start)
static const uint8_t PLAY[] PROGMEM = {
    0b010000,  // .@....
    0b011000,  // .@@...
    0b011100,  // .@@@..
    0b011110,  // .@@@@.
    0b011100,  // .@@@..
    0b011000,  // .@@...
    0b010000,  // .@....
    0b000000,  // ......
};

// \037 IP address icon (used in conf files: iptxtFmt)
static const uint8_t IP[] PROGMEM = {
    0b110000,  // @@....
    0b111111,  // @@@@@@
    0b110000,  // @@....
    0b000000,  // ......
    0b000011,  // ....@@
    0b111111,  // @@@@@@
    0b000011,  // ....@@
    0b000000,  // ......
};

// \0?? Blank template
static const uint8_t BLANK[] PROGMEM = {
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
    0b000000,  // ......
};

// ==========================================================================
// Icon lookup table: map byte value (1-31) to icon bitmap pointer.
// Used by TextWidget::_draw() to decode icon markers in format strings.
// ==========================================================================
static const uint8_t* const ICON_TABLE[] PROGMEM = {
    NULL,            //  0: (unused, should probably stay unused)
    RSSI_00,         //  1: \001
    RSSI__00,        //  2: \002
    RSSI_10,         //  3: \003
    RSSI_11,         //  4: \004
    RSSI__10,        //  5: \005
    RSSI__11,        //  6: \006
    NULL,            //  7: \007
    NULL,            //  8: \010
    NULL,            //  9: \011 (DO NOT USE: TAB control char conflicts with TFT print())
    NULL,            // 10: \012 (DO NOT USE: LF control char conflicts with TFT print())
    BATTERY_00,      // 11: \013
    BATTERY__00,     // 12: \014
    BATTERY_10,      // 13: \015
    BATTERY_11,      // 14: \016
    BATTERY__10,     // 15: \017
    BATTERY__11,     // 16: \020
    NULL,            // 17: \021
    NULL,            // 18: \022
    SPEAKER,         // 19: \023
    VOL_25,          // 20: \024
    VOL_50,          // 21: \025
    VOL_75,          // 22: \026
    VOL_100,         // 23: \027
    SD_B,            // 24: \030
    SD_B,            // 25: \031
    SDSHUFFLE_A,     // 26: \032
    SDSHUFFLE_B,     // 27: \033
    PAUSE,           // 28: \034
    PLAY,            // 29: \035
    NULL,            // 30: \036 (DO NOT USE: 0x1E is the spacer character in TextWidget: it gets consumed as a 2px gap before reaching the icon renderer)
    IP,              // 31: \037
};