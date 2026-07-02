#ifndef RGB_LED_HAL_H
#define RGB_LED_HAL_H

#include <stdint.h>


typedef enum{
  RGB_UNINITIALISED,
  RGB_OK,
  RGB_INVALID_PARAMETER,
  RGB_FAILED,
  RGB_FAILURE_TESTING_USE_ONLY,
  RGB_INIT_FAILED
} rgb_status_t;

typedef enum{
  RGB_RED = 0xFF0000,
  RGB_GREEN = 0x00FF00,
  RGB_YELLOW = 0xFFFF00,
  RGB_BLUE = 0x0000FF,
  RGB_WHITE = 0xFFFFFF,
  RGB_PINK = 0xF542F2
} rgb_colour_t;



// Public API
rgb_status_t rgb_init(void);
rgb_status_t rgb_on(const rgb_colour_t colour);
rgb_status_t rgb_off(void);
rgb_status_t rgb_get_flash_timer(uint32_t *flash_timer);
rgb_status_t rgb_set_flash_timer(const uint32_t flash_timer);
rgb_status_t rgb_get_flash_on(bool *flash_on);
rgb_status_t rgb_set_flash_on(const bool flash_on);
rgb_status_t rgb_increment_counter(void);
rgb_status_t rgb_reset_counter(void);
rgb_status_t rgb_get_counter(uint8_t *counter);
bool rgb_get_init(void);


#endif