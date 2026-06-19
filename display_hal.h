#ifndef DISPLAY_HAL_H
#define DISPLAY_HAL_H

#include <Arduino.h>
#include <stdint.h>

/* Explicit state and error reporting */
typedef enum
{
    DISPLAY_STATUS_OK = 0,
    DISPLAY_STATUS_NOT_INITIALIZED,
    DISPLAY_STATUS_INIT_FAILED,
    DISPLAY_STATUS_INVALID_PARAM,
    DISPLAY_STATUS_HW_ERROR
} display_status_t;

const uint8_t DISPLAY_WHITE = 1U;
const uint8_t DISPLAY_BLACK = 0U;
const uint8_t DISPLAY_X_OFFSET_SHOWPPO2 = 40U;
const uint8_t DISPLAY_Y_OFFSET_SHOWPPO2 = 8U;
const uint8_t DISPLAY_NEWLINE_PX = 16U;

/* Public interface */
display_status_t display_init(void);
display_status_t display_clear(void);
display_status_t display_set_cursor(uint8_t x, uint8_t y);
//display_status_t display_write_text(const char * text);
display_status_t display_update(void);
display_status_t display_font_size(uint8_t size);
display_status_t display_set_colour(uint8_t foreground, uint8_t background);
display_status_t display_println(const char * text);
display_status_t display_print(const char * text);
display_status_t display_set_contrast(const uint8_t level);

#endif /* DISPLAY_HAL_H */
