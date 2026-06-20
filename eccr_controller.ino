#include "system_state.h"
#include "time_helpers.h"
#include "adc_hal.h"
#include "gpio_hal.h"
#include "display_hal.h"
#include "format_for_print.h"
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
//  3 - Display

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
  uint32_t last_lcd_update_time_ms = 0U;
  bool display_switch_on = gpio_slide_switch_on();

  if(system_get_cell_read_timer(&last_cell_read_time_ms) != STATE_OK){
    handle_error();
  }
  if(system_get_lcd_update_timer(&last_lcd_update_time_ms) != STATE_OK){
    handle_error();
  }
  if(has_timer_elapsed(now, last_cell_read_time_ms, FREQUENCY_CELL_READ_MS)){
    //system_set_cell_read_timer(now);
    system_set_fsm_state(FSM_READ_CELLS);
  }
  if(has_timer_elapsed(now, last_lcd_update_time_ms, 100)){
    if(display_switch_on){
      if(screen_on() != DISPLAY_STATUS_OK){
        // Handle failure
      }
    } else {
      if(screen_off() != DISPLAY_STATUS_OK){
        // Handle failure
      }
    }
    system_set_lcd_update_timer(now);
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

system_state_t assign_mv(int16_t cell_reading_mv[]){
  uint16_t raw_reading = 0;

  if(cell_reading_mv == NULL){
    return STATE_INVALID_PARAMETER;
  }
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    if(system_get_cell_reading(&raw_reading, channel) != STATE_OK){
      return STATE_FUNCTION_FAILED;
    }
    cell_reading_mv[channel] = adc_convert_raw_to_mV(raw_reading);
  }
  return STATE_OK;
}


//  3 - Display
display_status_t screen_off(void){
  display_clear();
  return display_update();
}

display_status_t screen_on(){
  char buffer_mv[FORMATTING_INTEGER_STR_LEN];
  int16_t current_mv[THREE_CELLS] = {0};

  if(assign_mv(current_mv) != STATE_OK){
    return DISPLAY_STATUS_INVALID_PARAM;
  }

  display_clear();
  display_font_size(1);
  display_set_cursor(0, 0);
  display_println("xxx");

  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    format_integer_for_display(current_mv[channel], buffer_mv);
    display_print(buffer_mv);
    display_print("mV ");
  }

  return display_update();
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
  switchstate_t switch_state = gpio_slide_switch_on();

  Serial.println(switch_state);
}

void debug_test_momentary_switch(){
  switchstate_t switch_state = gpio_momentary_pushed();

  Serial.println(switch_state);
}
