//   NOTES
//   1. This value assumes that a loop flush will be imperfect and a max 97% oxygen is only achievable. Do not exceed 100% (1000U)

#include "system_state.h"
#include "time_helpers.h"
#include "adc_hal.h"
#include "gpio_hal.h"
#include "display_hal.h"
#include "format_for_print.h"
#include "eeprom_hal_basic.h"
#include <Wire.h>

typedef enum{
  INIT_BEGIN,
  INIT_SUCCESS,
  INIT_FAILED_STATE,
  INIT_FAILED_ADC_START,
  INIT_FAILED_GPIO,
  INIT_FAILED_DISPLAY,
  INIT_FAILED_EEPROM
} init_state_t;

constexpr uint32_t FREQUENCY_CELL_READ_MS = 1000U;
constexpr uint32_t FREQUENCY_LCD_UPDATE_MS = 1000U;
constexpr uint32_t FREQUENCY_DIVEMODE_LED_ON_MS = 200U;
constexpr uint32_t FREQUENCY_DIVEMODE_LED_OFF_MS = 3000U;
constexpr uint32_t FREQUENCY_DATAMODE_LED_ON_MS = 100U;
constexpr uint32_t FREQUENCY_DATAMODE_LED_OFF_MS = 400U;
constexpr uint16_t CALIBRATION_PPO2x1000 = 970U; // See Note 1

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
  } else if(eeprom_hal_init(LOW_OUTPUT_CELL) != MEM_OK)
    initialisation_state = INIT_FAILED_EEPROM;
  else {
    initialisation_state = INIT_SUCCESS;
  }

  if(initialisation_state != INIT_SUCCESS){
    system_set_fsm_state(FSM_UNINITIALISED);
  } else {
    system_set_fsm_state(FSM_WAITING);
    Serial.println("Success");
  }

  if(assign_cell_calibration_factors() != STATE_OK){
    system_set_fsm_state(FSM_FAILED_SAFE);
    Serial.println("EEPROM read failed");
  }

  debug_display_cal_factors();
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

system_state_t convert_raw_to_ppO2(const uint16_t raw, const uint8_t channel, uint16_t * const raw_converted_to_ppO2){
  uint16_t reference_value = 0;
  uint32_t ppO2 = 0;
  const uint32_t scale = CALIBRATION_PPO2x1000;
  uint32_t temp = 0;

  if(system_get_calibration_factor(&reference_value, channel) != STATE_OK){
    return STATE_FUNCTION_FAILED;
  }
  if((raw_converted_to_ppO2 == NULL) || (reference_value == 0)){
    return STATE_INVALID_PARAMETER;
  }
  temp = ((uint32_t)raw) * scale;
  ppO2 = temp / reference_value;
  if (ppO2 > UINT16_MAX) return STATE_INVALID_PARAMETER;
  *raw_converted_to_ppO2 = (uint16_t)ppO2;
  return STATE_OK;
}

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
  if(system_set_cell_read_timer(now) != STATE_OK){
    system_set_fsm_state(FSM_FAILED_SAFE);
    return;
  }
  if(system_set_cell_read_ready(true) != STATE_OK){
    system_set_fsm_state(FSM_FAILED_SAFE);
    return;
  }
  system_set_fsm_state(FSM_WAITING);

  debug_test_ppo2_conversion();
}

void fsm_waiting(uint32_t now){
  uint32_t last_cell_read_time_ms = 0U;
  uint32_t last_lcd_update_time_ms = 0U;
  uint32_t divemode_led_timer_ms = 0U;
  uint32_t divemode_flash_interval_ms = 0U;
  bool divemode_led_on = false;
  bool display_switch_on = gpio_slide_switch_on();

  if(system_get_cell_read_timer(&last_cell_read_time_ms) != STATE_OK){
    handle_error();
    return;
  }
  if(system_get_lcd_update_timer(&last_lcd_update_time_ms) != STATE_OK){
    handle_error();
    return;
  }
  if(system_get_divemode_led_timer(&divemode_led_timer_ms) != STATE_OK){
    handle_error();
    return;
  }
  if(system_get_divemode_led_on(&divemode_led_on) != STATE_OK){
    handle_error();
    return;
  }
  if(!display_switch_on){
    divemode_flash_interval_ms = (divemode_led_on) ? FREQUENCY_DIVEMODE_LED_ON_MS : FREQUENCY_DIVEMODE_LED_OFF_MS;
  } else {
    divemode_flash_interval_ms = (divemode_led_on) ? FREQUENCY_DATAMODE_LED_ON_MS : FREQUENCY_DATAMODE_LED_OFF_MS;
  }


  if(has_timer_elapsed(now, last_cell_read_time_ms, FREQUENCY_CELL_READ_MS)){
    system_set_fsm_state(FSM_READ_CELLS);
  }
  if(has_timer_elapsed(now, last_lcd_update_time_ms, FREQUENCY_LCD_UPDATE_MS)){
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
  if(has_timer_elapsed(now, divemode_led_timer_ms, divemode_flash_interval_ms)){
    divemode_led_on = !divemode_led_on;
    gpio_led_on(divemode_led_on);
    system_set_divemode_led_on(divemode_led_on);
    system_set_divemode_led_timer(now);
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

system_state_t assign_cell_calibration_factors(void){
  uint16_t temp_calibration_factors[THREE_CELLS];

  if(eeprom_hal_read_array(temp_calibration_factors) != MEM_OK){
    Serial.println("---");
    handle_error();
    return STATE_FAILED_READ;
  }
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    if(system_set_calibration_factor(temp_calibration_factors[channel], channel) != STATE_OK){
      return STATE_FUNCTION_FAILED;
    }
  }
  return STATE_OK;
}


//  3 - Display
display_status_t screen_off(void){
  display_clear();
  return display_update();
}

display_status_t screen_on(void){
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

void debug_flash_led(){
  gpio_led_on(true);
  delay(5000);
  gpio_led_on(false);
  delay(1000);
  gpio_led_on(true);
  delay(5000);
  gpio_led_on(false);
  delay(1000);
  gpio_led_on(true);
  delay(5000);
  gpio_led_on(false);
  delay(1000);
}

void debug_test_eeprom(){
  uint16_t test[THREE_CELLS];
  uint16_t read[THREE_CELLS] = {0};

  test[0] = 1234;
  test[1] = 2468;
  test[2] = 3579;

  Serial.println(eeprom_hal_write_array(test));
  Serial.println(eeprom_hal_read_array(read));

  Serial.println(read[0]);

  system_set_calibration_factor(test[0], 0);
  
  uint16_t testRead = 0;
  system_get_calibration_factor(&testRead, 0);
  Serial.println(testRead);

}

void debug_display_cal_factors(void){
  uint16_t read = 0;

  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    system_get_calibration_factor(&read, channel);
    Serial.println(read);
  }
}

void debug_test_ppo2_conversion(){
  uint16_t raw_reading = 0U;
  uint16_t ppo2 = 0U;

  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    system_get_cell_reading(&raw_reading, channel);
    convert_raw_to_ppO2(raw_reading, channel, &ppo2);
    Serial.println(ppo2);
  }
}