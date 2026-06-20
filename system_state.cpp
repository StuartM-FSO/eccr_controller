#include <sys/_stdint.h>
#include "system_state.h"

typedef struct{
  bool initialised = false;
  bool display_on = false;
  fsm_state_t fsm_state = FSM_UNINITIALISED;
  uint32_t cell_read_timer = 0U;
} loop_state_t;

typedef struct{
  uint16_t cell_reading_raw[THREE_CELLS] = {0};
  bool initialised = false;
  bool cell_read_ready = false;
} cell_state_t;

static cell_state_t cells;

static loop_state_t current_state = {};

static bool is_fsm_state_valid(fsm_state_t this_state);

system_state_t state_init(void){
  if(current_state.initialised){
    return STATE_OK;
  }
  current_state.fsm_state = FSM_WAITING;
  current_state.cell_read_timer = 0U;
  current_state.display_on = false;
  current_state.initialised = true;
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    cells.cell_reading_raw[channel] = 0U;
  }
  cells.cell_read_ready = false;
  cells.initialised = true;
  return STATE_OK;
}

// Setters & getters

system_state_t system_get_fsm_state(fsm_state_t *state){
  if(state == NULL){
    return STATE_INVALID_PARAMETER;
  }
  if(!current_state.initialised){
    return STATE_UNINITIALISED;
  }
  *state = current_state.fsm_state;
  return STATE_OK;
}

system_state_t system_set_fsm_state(const fsm_state_t state){
  if(!current_state.initialised){
    return STATE_UNINITIALISED;
  }
  if(!is_fsm_state_valid(state)){
    return STATE_INVALID_PARAMETER;
  }
  current_state.fsm_state = state;
  return STATE_OK;
}

system_state_t system_get_cell_read_timer(uint32_t *timer){
  if(!current_state.initialised){
    return STATE_UNINITIALISED;
  }
  if(timer == NULL){
    return STATE_INVALID_PARAMETER;
  }
  *timer = current_state.cell_read_timer;
  return STATE_OK;
}

system_state_t system_set_cell_read_timer(const uint32_t timer){
  if(!current_state.initialised){
    return STATE_UNINITIALISED;
  }
  current_state.cell_read_timer = timer;
  return STATE_OK;
}

system_state_t system_get_cell_read_ready(bool *ready){
  if(!cells.initialised){
    return STATE_UNINITIALISED;
  }
  if(ready == NULL){
    return STATE_INVALID_PARAMETER;
  }
  *ready = cells.cell_read_ready;
  return STATE_OK;
}

system_state_t system_set_cell_read_ready(const bool state){
  if(!cells.initialised){
    return STATE_UNINITIALISED;
  }
  if((state != true) && (state != false)){
    return STATE_INVALID_PARAMETER;
  }
  cells.cell_read_ready = state;
  return STATE_OK;
}

system_state_t system_set_cell_reading(const uint16_t raw_reading, const uint8_t channel){
  if(!cells.initialised){
    return STATE_UNINITIALISED;
  }
  if(channel >= THREE_CELLS){
    return STATE_INVALID_PARAMETER;
  }
  cells.cell_reading_raw[channel] = raw_reading;
  return STATE_OK;
}

system_state_t system_get_cell_reading(uint16_t *raw_reading, uint8_t channel){
  if(!cells.initialised){
    return STATE_UNINITIALISED;
  }
  if(raw_reading == NULL){
    return STATE_INVALID_PARAMETER;
  }
  if(channel >= THREE_CELLS){
    return STATE_INVALID_PARAMETER;
  }
  *raw_reading = cells.cell_reading_raw[channel];
  return STATE_OK;
}

system_state_t system_get_display_on(bool * const status){
  if(!current_state.initialised){
    return STATE_UNINITIALISED;
  }
  if(status == NULL){
    return STATE_INVALID_PARAMETER;
  }
  *status = current_state.display_on;
  return STATE_OK;
}

system_state_t system_set_display_on(const bool status){
  if(!current_state.initialised){
    return STATE_UNINITIALISED;
  }
  current_state.display_on = status;
  return STATE_OK;
}

// Private functions

bool is_fsm_state_valid(fsm_state_t this_state){
  return ((this_state > FSM_ZERO) && (this_state < FSM_END_COUNT));
}