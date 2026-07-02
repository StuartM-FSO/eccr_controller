#include <sys/_stdint.h>
//     NOTES
//  1. flash_counter must not exceed 255

#include "Adafruit_NeoPixel.h"
#include <Arduino.h>
#include <stdint.h>
#include "rgb_led_hal.h"

static const uint8_t NUM_PIXELS = 1U;

static Adafruit_NeoPixel rgb_led(NUM_PIXELS, RGB_BUILTIN, NEO_GRB + NEO_KHZ800);

typedef struct{
  bool initialised;
  uint32_t flash_timer;
  bool flash_on;
  uint8_t flash_counter;
} rgb_t;

static rgb_t state = {
  .initialised = false,
  .flash_timer = 0,
  .flash_on = false,
  .flash_counter = 0
};

// Constants


// Public API

rgb_status_t rgb_init(void){
  if(state.initialised){
    return RGB_OK;
  }
  pinMode(PIN_RGB_EN, OUTPUT);
  digitalWrite(PIN_RGB_EN, HIGH);
  if(!rgb_led.begin()){
    state.initialised = false;
    return RGB_FAILED;
  }
  state.flash_timer = 0;
  state.flash_counter = 0;
  state.flash_on = false;
  state.initialised = true;
  return RGB_OK;
}

rgb_status_t rgb_on(const rgb_colour_t colour){
  if(!state.initialised){
    return RGB_UNINITIALISED;
  }
  if((colour < 0) || (colour > 0xFFFFFF)){
    return RGB_INVALID_PARAMETER;
  }
  rgb_led.setPixelColor(0, colour);
  rgb_led.show();
  return RGB_OK;
}

rgb_status_t rgb_off(void){
  if(!state.initialised){
    return RGB_UNINITIALISED;
  }
  rgb_led.clear();
  rgb_led.show();
}

rgb_status_t rgb_get_flash_timer(uint32_t *flash_timer){
  if(!state.initialised){
    return RGB_UNINITIALISED;
  }
  if(flash_timer == NULL){
    return RGB_INVALID_PARAMETER;
  }
  *flash_timer = state.flash_timer;
  return RGB_OK;
}

rgb_status_t rgb_set_flash_timer(const uint32_t flash_timer){
  if(!state.initialised){
    return RGB_UNINITIALISED;
  }
  state.flash_timer = flash_timer;
  return RGB_OK;
}

rgb_status_t rgb_get_flash_on(bool *flash_on){
  if(!state.initialised){
    return RGB_UNINITIALISED;
  }
  if(flash_on == NULL){
    return RGB_INVALID_PARAMETER;
  }
  *flash_on = state.flash_on;
  return RGB_OK;
}

rgb_status_t rgb_set_flash_on(const bool flash_on){
  if(!state.initialised){
    return RGB_UNINITIALISED;
  }
  state.flash_on = flash_on;
  return RGB_OK;
}

rgb_status_t rgb_increment_counter(void){
  if(!state.initialised){
    return RGB_UNINITIALISED;
  }
  if(state.flash_counter < UINT8_MAX){
    state.flash_counter++;
  }
  return RGB_OK;
}
rgb_status_t rgb_reset_counter(void){
  if(!state.initialised){
    return RGB_UNINITIALISED;
  }
  state.flash_counter = 0U;
  return RGB_OK;
}

rgb_status_t rgb_get_counter(uint8_t *counter){
  if(!state.initialised){
    return RGB_UNINITIALISED;
  }
  if(counter == NULL){
    return RGB_INVALID_PARAMETER;
  }
  *counter = state.flash_counter;
  return RGB_OK;
}

bool rgb_get_init(void){
  return state.initialised;
}