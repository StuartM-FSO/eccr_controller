#include "system_state.h"

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
      Serial.println("fsm waiting");
      delay(250);
      break;
    case FSM_FAILED_SAFE:
      Serial.println("fsm failed safe");
      delay(250);
      break;
  }
}




// DEBUG

void serial_begin(uint32_t baud_rate){
  Serial.begin(baud_rate);
  delay(250);
  Serial.println("Starting");
}
