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
  FSM_END_COUNT // DO NOT ADD STATES BEYOND THIS
} fsm_state_t;

system_state_t state_init(void);

system_state_t system_get_fsm_state(fsm_state_t *state);
system_state_t system_set_fsm_state(fsm_state_t state);

#endif