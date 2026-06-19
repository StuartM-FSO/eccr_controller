#include <sys/_stdint.h>
// NOTES
// 1. display_clear() only clears the buffer. User must call display_update to flush and clear screen
// 2. Potential for cursor set to overflow if larger text size is used. But text size (1) is only ever used.
// 3. Wire.begin() MUST be called in main code

// MISRA Deviations:
// 1. Use of C++ class Adafruit_SSD1306 required for hardware driver.
//    Isolated within HAL layer to contain non-compliant constructs.
// 2. Use of Arduino.h also required and contained.
// 3. Adafruit_SSD1306.h does not provide return values on basic functions.
// 4. Non re-entrant state acceptable as single instance driver



#include <Arduino.h>
#include <stdint.h>
#include "display_hal.h"

// Third-party code isolated here
#include <Adafruit_SSD1306.h>
#include <Wire.h>

static const uint8_t MAX_CONNECTION_CHECK_TRIES = 10U;

// Constants
static const uint8_t TEXT_COLOUR_WHITE = 1U;
static const uint8_t TEXT_COLOUR_BLACK = 0U;
static const uint8_t DISPLAY_WIDTH = 128U;
static const uint8_t DISPLAY_HEIGHT = 64U;
static const uint8_t DISPLAY_I2C_ADDR = 0x3C;

typedef struct{
    bool initialised;
    Adafruit_SSD1306 device;
} state_t;

static state_t state = {
    .initialised = false,
    .device = Adafruit_SSD1306(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1)
};



// Private function declarations
static void state_reset(state_t * const state);
static bool connected_check_multiple(void);
static bool is_connected(void);

// Public functions

display_status_t display_init(void){
    bool ok = false;

    if(state.initialised){
        return DISPLAY_STATUS_OK;
    }
    if(!connected_check_multiple()){
        state.initialised = false;
        return DISPLAY_STATUS_HW_ERROR;
    }
    state_reset(&state);
    ok = state.device.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDR);
    if(!ok){
        state.initialised = false;
        return DISPLAY_STATUS_INIT_FAILED;
    }
    state.device.clearDisplay();
    state.device.setTextSize(2);
    state.device.setTextColor(TEXT_COLOUR_WHITE);
    state.device.display();
    state.initialised = true;
    return DISPLAY_STATUS_OK;
}

display_status_t display_clear(void)
{
    if (!state.initialised)
    {
        return DISPLAY_STATUS_NOT_INITIALIZED;
    }
    state.device.clearDisplay();
    return DISPLAY_STATUS_OK;
}

display_status_t display_set_cursor(const uint8_t x, const uint8_t y)
{
    if (!state.initialised)
    {
        return DISPLAY_STATUS_NOT_INITIALIZED;
    }

    if ((x >= DISPLAY_WIDTH) || (y >= DISPLAY_HEIGHT))
    {
        return DISPLAY_STATUS_INVALID_PARAM;
    }
    state.device.setCursor(x, y);
    return DISPLAY_STATUS_OK;
}

display_status_t display_update(void)
{
    if (!state.initialised)
    {
        return DISPLAY_STATUS_NOT_INITIALIZED;
    }
    if(!connected_check_multiple()){
        return DISPLAY_STATUS_HW_ERROR;
    }
    state.device.display();
    return DISPLAY_STATUS_OK;
}

display_status_t display_font_size(const uint8_t size){
    if(!state.initialised){
        return DISPLAY_STATUS_NOT_INITIALIZED;
    }
    if((size == 0) || (size > 3)){
        return DISPLAY_STATUS_INVALID_PARAM;
    }
    state.device.setTextSize(size);
    return DISPLAY_STATUS_OK;
}

display_status_t display_set_colour(const uint8_t foreground, const uint8_t background){
    if(!state.initialised){
        return DISPLAY_STATUS_NOT_INITIALIZED;
    }
    if ((foreground > 1U) || (background > 1U)) {
        return DISPLAY_STATUS_INVALID_PARAM;
    }
    state.device.setTextColor(foreground, background);
    return DISPLAY_STATUS_OK;
}

display_status_t display_println(const char * const text){
    if(!state.initialised){
        return DISPLAY_STATUS_NOT_INITIALIZED;
    }
    if(text == NULL){
        return DISPLAY_STATUS_INVALID_PARAM;
    }
    state.device.println(text);
    return DISPLAY_STATUS_OK;
}

display_status_t display_print(const char * const text){
    if(!state.initialised){
        return DISPLAY_STATUS_NOT_INITIALIZED;
    }
    if(text == NULL){
        return DISPLAY_STATUS_INVALID_PARAM;
    }
    state.device.print(text);
    return DISPLAY_STATUS_OK;
}

display_status_t display_set_contrast(const uint8_t level){
    static const uint8_t contrast_values[] = { 0x10, 0x75, 0xFF };

    if(!state.initialised){
        return DISPLAY_STATUS_NOT_INITIALIZED;
    }
    if ((size_t)level >= (sizeof(contrast_values) / sizeof(contrast_values[0]))) {
        return DISPLAY_STATUS_INVALID_PARAM;
    }
    state.device.ssd1306_command(SSD1306_SETCONTRAST);
    state.device.ssd1306_command(contrast_values[level]);
    return DISPLAY_STATUS_OK;
}

// Private functions

static void state_reset(state_t *state){
    state->initialised = false;
}

static bool is_powered(void){
    // Pending HW design
    return true;
}

static bool is_connected(void){
    uint8_t result = 99;

    Wire.beginTransmission(DISPLAY_I2C_ADDR);
    result = Wire.endTransmission();
    return (result == 0);
}

static bool connected_check_multiple(void){
    uint8_t counter = 0;
    bool flag_connected = false;

    while((!flag_connected) && (counter < MAX_CONNECTION_CHECK_TRIES)){
        counter++;
        flag_connected = is_connected();
        if(!flag_connected){
            delay(1);
        }
    }
    return flag_connected;
}