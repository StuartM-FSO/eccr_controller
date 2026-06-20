// NOTES
// 1. Use of delay() accepted in gpio_led_flash_error_state(). This function is only ever called if total failure at startup is detected.
//    System is shut down and cannot be recovered without full reset.
// 2. gpiostate_t used for functions related to GPIO pins and switchstate_t used for switch/button functions.
// 3. Button debouncing to be added
// 4. Potential for excessively long flash cycle with high error numbers. BUT current maximum possible error code is 5.
// 5. For Xiao RA4M1 slide switch is on D10 and push button on D9;

#include <Arduino.h>
#include <stdint.h>
#include "gpio_hal.h"

static const uint8_t SELECT_BUTTON = D9; // See Note 5
static const uint8_t CALIBRATION_SWITCH = D10; // See Note 5

typedef struct{
  bool initialised;
} state_t;

// Static variables

static state_t gpio_state = {
  false
};

// Private function declarations

static void gpio_led_write(bool led_on);

// Public functions

gpiostate_t gpio_init(void){
  if(gpio_state.initialised){
    return GPIO_STATUS_OK;
  }
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(CALIBRATION_SWITCH, INPUT_PULLUP);
  pinMode(SELECT_BUTTON, INPUT_PULLUP);
  gpio_state.initialised = true;
  return GPIO_STATUS_OK;
}

switchstate_t gpio_slide_switch_on(void){  // See Note 3
  uint8_t state;

  if(!gpio_state.initialised){
    return SWITCH_UNINITIALISED;
  }
  state = digitalRead(CALIBRATION_SWITCH);
  switch (state) {
    case LOW:   return SWITCH_ON;
    case HIGH:  return SWITCH_OFF;
    default:    return SWITCH_FAILURE; // Defensive guard against possible failure, case not expected
  }
}

switchstate_t gpio_momentary_pushed(void){ // See Note 3
  uint8_t state;

  if(!gpio_state.initialised){
    return SWITCH_UNINITIALISED;
  }
  state = digitalRead(SELECT_BUTTON);
  switch(state){
    case LOW:   return SWITCH_ON;
    case HIGH:  return SWITCH_OFF;
    default:    return SWITCH_FAILURE; // Defensive guard against possible failure, case not expected
  }
}

void gpio_led_flash_error_state(const uint8_t error){
  if(!gpio_state.initialised){
    if(gpio_init() != GPIO_STATUS_OK){
      return;
    }
  }
  if(error == 0){
    return;
  }
  while(true){
    for(uint8_t i = 0U; i < error; i++){ // See Note 4
      gpio_led_write(true);
      delay(250);
      gpio_led_write(false);
      delay(250);
    }
    delay(2000);
  }
}

void gpio_led_on(const bool state){
  gpio_led_write(!state);
}

// Private functions

static void gpio_led_write(const bool led_on){
  if(led_on){
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
}