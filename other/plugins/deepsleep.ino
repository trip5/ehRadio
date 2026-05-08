/*
 *******************************************************************************************
 * Attention!
 * This method of connecting plugins no longer works and is left here for history.
 *******************************************************************************************

*/
/******************************************************************************************************************

    Example of esp32 deep sleep when playback is stopped.
    This file must be in the root directory of the sketch.

*******************************************************************************************************************/
#include <Ticker.h>
#include "src/core/options.h"
#include "src/core/display.h"

#define SLEEP_DELAY     60        /* 1 min        deep sleep delay                                                */
#define WAKEUP_PIN      ENC_BTNB  /*              wakeup pin (one of: BTN_XXXX, ENC_BTNB, ENC2_BTNB)              */
                                  /*              ESP32: must be RTC-capable: 0,2,4,12-15,25-27,32-39            */
                                  /*              ESP32-S3: any digital-input-capable GPIO                        */
                                  /*              ESP32-C3: must be RTC-capable: 0, 1, 2, 3, 4, 5                 */
#define WAKEUP_LEVEL    LOW       /*              wakeup level (usually LOW)                                      */

#if WAKEUP_PIN!=255
Ticker deepSleepTicker;

void goToSleep(){
  if(BRIGHTNESS_PIN!=255) analogWrite(BRIGHTNESS_PIN, 0);               /*  BRIGHTNESS_PIN added in v0.7.330      */
  if(display.deepsleep()) {                                             /*  if deep sleep is possible             */
    esp_deep_sleep_start();                                             /*  go to sleep                           */
  }else{                                                                /*  else                                  */
    deepSleepTicker.detach();                                           /*  detach the timer                      */
  }
}

void yoradio_on_setup(){                                                /*  occurs during loading                 */
  #if defined(ARDUINO_ESP32C3_DEV)
  esp_deep_sleep_enable_gpio_wakeup((1ULL << WAKEUP_PIN), (WAKEUP_LEVEL == LOW) ? ESP_GPIO_WAKEUP_GPIO_LOW : ESP_GPIO_WAKEUP_GPIO_HIGH);
  #else
  esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKEUP_PIN, WAKEUP_LEVEL);   /*  enable wakeup pin                     */
  #endif
  deepSleepTicker.attach(SLEEP_DELAY, goToSleep);                       /*  attach to delay                       */
}

void player_on_start_play(){                                            /*  occurs during player is start playing */
  deepSleepTicker.detach();                                             /*  detach the timer                      */
}

void player_on_stop_play(){                                             /*  occurs during player is stop playing  */
  deepSleepTicker.attach(SLEEP_DELAY, goToSleep);                       /*  attach to delay                       */
}
#endif  /*  #if WAKEUP_PIN!=255 */
