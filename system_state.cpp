#include "system_state.h"

typedef struct{
  bool initialised = false;
  fsm_state_t fsm_state = FSM_UNINITIALISED;
  uint32_t cell_read_timer = 0U;
  bool cell_read_ready = false;
} loop_state_t;

static loop_state_t current_state = {};

static bool is_fsm_state_valid(fsm_state_t this_state);

system_state_t state_init(void){
  if(current_state.initialised){
    return STATE_OK;
  }
  current_state.initialised = true;
  current_state.fsm_state = FSM_WAITING;
  current_state.cell_read_timer = 0U;
  current_state.cell_read_ready = false;
  return STATE_OK;
}

// Setters & getters

system_state_t system_get_fsm_state(fsm_state_t *state){
  if(state == NULL){
    return STATE_INVALID_PARAMETER;
  }
  *state = current_state.fsm_state;
  return STATE_OK;
}

system_state_t system_set_fsm_state(fsm_state_t state){
  if(!is_fsm_state_valid(state)){
    return STATE_INVALID_PARAMETER;
  }
  current_state.fsm_state = state;
  return STATE_OK;
}

system_state_t system_get_cell_read_timer(uint32_t *timer){
  if(timer == NULL){
    return STATE_INVALID_PARAMETER;
  }
  *timer = current_state.cell_read_timer;
  return STATE_OK;
}

system_state_t system_set_cell_read_timer(uint32_t timer){
  current_state.cell_read_timer = timer;
  return STATE_OK;
}

bool system_get_cell_read_ready(void){
  return current_state.cell_read_ready;
}

void system_set_cell_read_ready(bool state){
  current_state.cell_read_ready = state;
}

// Private functions

bool is_fsm_state_valid(fsm_state_t this_state){
  return ((this_state > FSM_ZERO) && (this_state < FSM_END_COUNT));
}