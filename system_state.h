#include <sys/_stdint.h>
#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <stdint.h>

typedef enum{
  STATE_OK,
  STATE_INVALID_PARAMETER
} system_state_t;

typedef enum{
  FSM_ZERO = 0, // DO NOT ADD STATES BEFORE THIS
  FSM_WAITING,
  FSM_FAILED_SAFE,
  FSM_UNINITIALISED,
  FSM_READ_CELLS,
  FSM_END_COUNT // DO NOT ADD STATES BEYOND THIS
} fsm_state_t;

constexpr uint8_t THREE_CELLS = 3U;

system_state_t state_init(void);

system_state_t system_get_fsm_state(fsm_state_t *state);
system_state_t system_set_fsm_state(fsm_state_t state);

system_state_t system_get_cell_read_timer(uint32_t *timer);
system_state_t system_set_cell_read_timer(uint32_t timer);

bool system_get_cell_read_ready(void);
void system_set_cell_read_ready(bool state);

#endif