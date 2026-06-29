#include <sys/_stdint.h>
#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <stdint.h>

typedef enum{
  STATE_OK,
  STATE_INVALID_PARAMETER,
  STATE_FAILED_READ,
  STATE_FUNCTION_FAILED,
  STATE_UNINITIALISED,
  STATE_DISPLAY_FAILED,
  STATE_REQUIRES_CALIBRATION,
  STATE_CELLS_UNAVAILABLE,
  STATE_OVERFLOW
} system_state_t;

typedef enum{
  FSM_ZERO = 0, // DO NOT ADD STATES BEFORE THIS
  FSM_WAITING,
  FSM_FAILED_SAFE,
  FSM_UNINITIALISED,
  FSM_READ_CELLS,
  FSM_CALIBRATION_ACTIVATED,
  FSM_CALIBRATION_WRITING,
  FSM_END_COUNT // DO NOT ADD STATES BEYOND THIS
} fsm_state_t;

constexpr uint8_t THREE_CELLS = 3U;

system_state_t state_init(void);

system_state_t system_get_fsm_state(fsm_state_t * const state);
system_state_t system_set_fsm_state(const fsm_state_t state);

system_state_t system_get_cell_read_timer(uint32_t * const timer);
system_state_t system_set_cell_read_timer(const uint32_t timer);

system_state_t system_get_lcd_update_timer(uint32_t * const timer);
system_state_t system_set_lcd_update_timer(const uint32_t timer);

system_state_t system_get_divemode_led_timer(uint32_t * const timer);
system_state_t system_set_divemode_led_timer(const uint32_t timer);

system_state_t system_get_calibration_write_timer(uint32_t * const timer);
system_state_t system_set_calibration_write_timer(const uint32_t timer);

system_state_t system_get_cell_read_ready(bool * const ready);
system_state_t system_set_cell_read_ready(const bool state);

system_state_t system_set_cell_reading(const uint16_t raw_reading, const uint8_t channel);
system_state_t system_get_cell_reading(uint16_t * const raw_reading, const uint8_t channel);

system_state_t system_get_display_on(bool * const status);
system_state_t system_set_display_on(const bool status);

system_state_t system_get_divemode_led_on(bool * const status);
system_state_t system_set_divemode_led_on(const bool status);

system_state_t system_get_calibration_factor(uint16_t * const calibration_factor, const uint8_t channel);
system_state_t system_set_calibration_factor(const uint16_t calibration_factor, const uint8_t channel);

system_state_t system_get_calibration_hold_timer(uint32_t *timer);
system_state_t system_set_calibration_hold_timer(const uint32_t timer);

bool system_get_screen_written_once(void);
void system_set_screen_written_once(bool state);

system_state_t system_get_main_loop_timer(uint32_t * const timer);
system_state_t system_set_main_loop_timer(const uint32_t timer);

#endif