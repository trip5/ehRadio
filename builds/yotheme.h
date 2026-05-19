#ifndef _my_theme_h
#define _my_theme_h

/*        ************************************************************************      */
/*        *        This file must be in the root folder of the sketch !!!        *      */
/*        ************************************************************************      */

/* This file contains the original yoRadio theme colors (gold & red)                    */
/*                                               Don't forget to rename it to mytheme.h */

#define ENABLE_THEME
#ifdef  ENABLE_THEME

/*-----------------------------------------------------------------------------------------------*/
/*       | COLORS             |   values (0-255)  |                                              */
/*       | color name         |    R    G    B    |                                              */
/*-----------------------------------------------------------------------------------------------*/
#define COLOR_BACKGROUND          0,   0,   0 // background
#define COLOR_STATION_NAME        0,   0,   0 // station text color
#define COLOR_STATION_BG        231, 211,  90 // current station background
#define COLOR_STATION_FILL      231, 211,  90 // fill color (outside bg)
#define COLOR_SNG_TITLE_1       255, 255, 255 // first title
#define COLOR_SNG_TITLE_2       165, 162, 132 // second title
#define COLOR_WEATHER           255, 150,   0 // weather string
#define COLOR_VU_MAX            231, 211,  90 // max of VU meter "FireBrick"
#define COLOR_VU_MIN            123, 125, 123 // min of VU meter "Green"
#define COLOR_CLOCK             231, 211,  90 // clock color
#define COLOR_CLOCK_BG           28,  28,  28 // clock color background
#define COLOR_SECONDS           231, 211,  90 // seconds color (DSP_ST7789, DSP_ILI9341, DSP_ILI9225)
#define COLOR_DAY_OF_W          255, 255, 255 // day of week color (for DSP_ST7789, DSP_ILI9341, DSP_ILI9225)
#define COLOR_DATE              165, 162, 132 // date color (DSP_ST7789, DSP_ILI9341, DSP_ILI9225)
#define COLOR_HEAP               41,  40,  41 // heap string
#define COLOR_BUFFER            165, 162, 132 // buffer line
#define COLOR_IP                165, 162, 132 // IP address
#define COLOR_VOLUME_VALUE      165, 162, 132 // volume number
#define COLOR_RSSI              165, 162, 132 // rssi
#define COLOR_BATTERY           165, 162, 132 // battery
#define COLOR_VOLBAR_OUT        231, 211,  90 // border of volume bar
#define COLOR_VOLBAR_IN         231, 211,  90 // inside volume bar
#define COLOR_DIGITS            255, 255, 255 // numbers...?
#define COLOR_DIVIDER           165, 162, 132 // lines around clock
#define COLOR_PL_CURRENT          0,   0,   0 // playlist current item
#define COLOR_PL_CURRENT_BG     231, 211,  90 // playlist current item background
#define COLOR_PL_CURRENT_FILL   231, 211,  90 // playlist current item fill background
#define COLOR_PLAYLIST_0        115, 115, 115 // playlist string 0
#define COLOR_PLAYLIST_1         89,  89,  89 // playlist string 1
#define COLOR_PLAYLIST_2         56,  56,  56 // playlist string 2
#define COLOR_PLAYLIST_3         35,  35,  35 // playlist string 3
#define COLOR_PLAYLIST_4         25,  25,  25 // playlist string 4
#define COLOR_BITRATE           231, 211,  90 // stream bitrate

#endif  /* #ifdef ENABLE_THEME */
#endif  /* #define _my_theme_h  */
