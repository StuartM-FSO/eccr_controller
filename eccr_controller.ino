#include "system_state.h"
#include "time_helpers.h"

constexpr uint32_t FREQUENCY_CELL_READ_MS = 1000U;

void setup() {
  serial_begin(9600);
  if(state_init() != STATE_OK){
    Serial.println("Init failed");
    while (true) {
      delay(1);
    }
  }
}

void loop() {
  fsm_state_t current_state = FSM_UNINITIALISED;
  uint32_t now = millis();

  if(system_get_fsm_state(&current_state) != STATE_OK){
    current_state = FSM_FAILED_SAFE;
    system_set_fsm_state(FSM_FAILED_SAFE);
  }

  switch(current_state){
    case FSM_UNINITIALISED:
      Serial.println("fsm uninitialised");
      delay(250);
      break;
    case FSM_WAITING:
      fsm_waiting(now);
      break;
    case FSM_FAILED_SAFE:
      Serial.println("fsm failed safe");
      delay(250);
      break;
    case FSM_READ_CELLS:
      fsm_read_cells(now);
      break;
  }
}

//  FUNCTIONS
//  0 - Work in progress
//  1 - FSM Handlers

//  0 - Work in progress



//  1 - FSM Handlers

void fsm_read_cells(uint32_t now){
  system_set_cell_read_ready(false);
  Serial.println("Reading cells");
  system_set_fsm_state(FSM_WAITING);
  system_set_cell_read_timer(now);
  system_set_cell_read_ready(true);
}

void fsm_waiting(uint32_t now){
  uint32_t last_cell_read_time_ms = 0U;

  if(system_get_cell_read_timer(&last_cell_read_time_ms) != STATE_OK){
    handle_error();
  }
  if(has_timer_elapsed(now, last_cell_read_time_ms, FREQUENCY_CELL_READ_MS)){
    system_set_cell_read_timer(now);
    system_set_fsm_state(FSM_READ_CELLS);
  }
}



// DEBUG

void serial_begin(uint32_t baud_rate){
  Serial.begin(baud_rate);
  delay(250);
  Serial.println("Starting");
}

void handle_error(void){
  Serial.println("Failed");
  while (true) {
    delay(1);
  }
}
