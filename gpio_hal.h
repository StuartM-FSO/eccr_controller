
#ifndef GPIO_HAL_H
#define GPIO_HAL_H

#include <Arduino.h>
#include <stdint.h>

typedef enum{
  GPIO_STATUS_OK,
  GPIO_STATUS_UNINITIALISED
} gpiostate_t;

typedef enum
{
  SWITCH_OFF = 0,
  SWITCH_ON,
  SWITCH_FAILURE,
  SWITCH_UNINITIALISED
} switchstate_t;

gpiostate_t gpio_init(void);
switchstate_t gpio_slide_switch_on(void);
switchstate_t gpio_momentary_pushed(void);
void gpio_led_flash_error_state(const uint8_t error);
void gpio_led_on(const bool state);

#endif