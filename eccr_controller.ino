#include "system_state.h"
#include "time_helpers.h"
#include "adc_hal.h"
#include "gpio_hal.h"
#include "display_hal.h"
#include <Wire.h>

typedef enum{
  INIT_BEGIN,
  INIT_SUCCESS,
  INIT_FAILED_STATE,
  INIT_FAILED_ADC_START,
  INIT_FAILED_GPIO,
  INIT_FAILED_DISPLAY
} init_state_t;

constexpr uint32_t FREQUENCY_CELL_READ_MS = 1000U;

void setup() {
  init_state_t initialisation_state = INIT_BEGIN;
  fsm_state_t state_after_init = FSM_UNINITIALISED;
  Wire.begin();

  serial_begin(9600);
  if(state_init() != STATE_OK){
    initialisation_state = INIT_FAILED_STATE;
  } else if(adc_init() != ADC_STATUS_OK){
    initialisation_state = INIT_FAILED_ADC_START;
  } else if(gpio_init() != GPIO_STATUS_OK){
    initialisation_state = INIT_FAILED_GPIO;
  } else if(display_init() != DISPLAY_STATUS_OK){
    initialisation_state = INIT_FAILED_DISPLAY;
  } else {
    initialisation_state = INIT_SUCCESS;
  }
  if(initialisation_state != INIT_SUCCESS){
    system_set_fsm_state(FSM_UNINITIALISED);
  } else {
    system_set_fsm_state(FSM_WAITING);
    Serial.println("Success");
  }
  debug_test_display();
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
    default:
      system_set_fsm_state(FSM_FAILED_SAFE);
      break;
  }
}

//  FUNCTIONS
//  0 - Work in progress
//  1 - FSM Handlers
//  2 - Cell handling

//  0 - Work in progress



//  1 - FSM Handlers

void fsm_read_cells(uint32_t now){
  if(system_set_cell_read_ready(false) != STATE_OK){
    system_set_fsm_state(FSM_FAILED_SAFE);
    return;
  }
  if(cell_read() != STATE_OK){
    Serial.println("Failed at cell read");
    handle_error();
  }
  system_set_fsm_state(FSM_WAITING);
  if(system_set_cell_read_timer(now) != STATE_OK){
    system_set_fsm_state(FSM_FAILED_SAFE);
    return;
  }
  if(system_set_cell_read_ready(true) != STATE_OK){
    system_set_fsm_state(FSM_FAILED_SAFE);
    return;
  }
}

void fsm_waiting(uint32_t now){
  uint32_t last_cell_read_time_ms = 0U;

  if(system_get_cell_read_timer(&last_cell_read_time_ms) != STATE_OK){
    handle_error();
  }
  if(has_timer_elapsed(now, last_cell_read_time_ms, FREQUENCY_CELL_READ_MS)){
    //system_set_cell_read_timer(now);
    system_set_fsm_state(FSM_READ_CELLS);
  }
}

//  2 - Cell handling

system_state_t cell_read(void){
  uint16_t raw_reading = 0;

  for(uint8_t channel = 0; channel < THREE_CELLS; channel++){
    if(adc_raw_reading(&raw_reading, channel) != ADC_STATUS_OK){
      return STATE_FAILED_READ;
    }
    if(system_set_cell_reading(raw_reading, channel) != STATE_OK){
      return STATE_FUNCTION_FAILED;
    }
  }
  return STATE_OK;
}



// DEBUG

void serial_begin(uint32_t baud_rate){
  Serial.begin(baud_rate);
  while(!Serial){
    delay(1);
  }
  Serial.println("Starting");
}

void handle_error(void){
  Serial.println("Failed");
  while (true) {
    delay(1);
  }
}

void debug_print_stored_cell_values(void){
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    uint16_t raw_reading = 0;
    if(system_get_cell_reading(&raw_reading, channel) != STATE_OK){
      Serial.println("*");
    }
    Serial.print(channel);
    Serial.print(": ");
    Serial.print(adc_convert_raw_to_mV(raw_reading));
    Serial.println();
  }
}

void debug_test_display(){
  display_clear();
  display_font_size(1);
  display_set_colour(DISPLAY_WHITE, DISPLAY_BLACK);
  display_set_cursor(0, 0);
  display_println("1.00 1.00 1.00");
  display_println("12 12 12");
  display_update();
}

void debug_test_slide_switch(){
  switchstate_t switch_state = gpio_calibration_switch_on();

  Serial.println(switch_state);
}

void debug_test_momentary_switch(){
  switchstate_t switch_state = gpio_select_button_pressed();

  Serial.println(switch_state);
}
