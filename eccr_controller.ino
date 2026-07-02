//   NOTES
//   1. This value assumes that a loop flush will be imperfect and a max 97% oxygen is only achievable. Do not exceed 100% (1000U)

#include "system_state.h"
#include "time_helpers.h"
#include "adc_hal.h"
#include "gpio_hal.h"
#include "display_hal.h"
#include "format_for_print.h"
#include "eeprom_hal.h"
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

typedef enum {
  SENSOR_0_REJECTED = 0U,
  SENSOR_1_REJECTED = 1U,
  SENSOR_2_REJECTED = 2U,
  SENSOR_ALL_VALID  = 3U,
  SENSOR_FAULT      = 4U,
  SENSOR_UNINITIALISED = 5U,
  SENSOR_COUNT_END // Do not add types beyond this
} sensor_vote_result_t;

constexpr uint32_t FREQUENCY_CELL_READ_MS = 1000U;
constexpr uint32_t FREQUENCY_LCD_UPDATE_MS = 1000U;
constexpr uint32_t FREQUENCY_DIVEMODE_LED_ON_MS = 200U;
constexpr uint32_t FREQUENCY_DIVEMODE_LED_OFF_MS = 3000U;
constexpr uint32_t FREQUENCY_DATAMODE_LED_ON_MS = 100U;
constexpr uint32_t FREQUENCY_DATAMODE_LED_OFF_MS = 400U;
constexpr uint32_t FREQUENCY_REQUIRES_CAL_LED_ON_MS = 1000U;
constexpr uint32_t FREQUENCY_REQUIRES_CAL_LED_OFF_MS = 250U;
constexpr uint32_t FREQUENCY_MAIN_LOOP_MS = 1000U;
constexpr uint32_t MAX_CALIBRATION_HOLD_MS = 5000U;
constexpr uint16_t CALIBRATION_PPO2x1000 = 970U; // See Note 1
constexpr uint16_t MAX_DEVIATION_FROM_SETPOINT = 100U;
constexpr uint16_t CALIBRATION_ACCEPTABLE_LIMIT_MINIMUM_RAW = 100U; // Limit to be established through testing
constexpr uint16_t CALIBRATION_ACCEPTABLE_LIMIT_MAXIMUM_RAW = 10000U; // Limit to be established through testing

void setup() {
  init_state_t initialisation_state = INIT_BEGIN;
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
  } else if(eeprom_init() != MEM_OK){
    initialisation_state = INIT_FAILED_EEPROM;
  } else {
    initialisation_state = INIT_SUCCESS;
  }

  if(initialisation_state != INIT_SUCCESS){
    system_set_fsm_state(FSM_UNINITIALISED);
  } else {
    system_set_fsm_state(FSM_READ_CELLS);
    Serial.println("Success");
  }

  if(assign_cell_calibration_factors() != STATE_OK){
    system_set_fsm_state(FSM_FAILED_SAFE);
    Serial.println("EEPROM read failed");
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
    case FSM_CALIBRATION_ACTIVATED:
      fsm_calibration_activated(now);
      break;
    case FSM_CALIBRATION_WRITING:
      fsm_calibration_writing(now);
      break;
    case FSM_DATA_DISPLAY:
      fsm_data_display(now);
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

bool is_initial_calibration_required(void){
  uint16_t calibration_factor = 0U;

  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    if(system_get_calibration_factor(&calibration_factor, channel) != STATE_OK){
      Serial.println("Read error in is_calibration_required");
      handle_error();
    }
    if(calibration_factor == 0U){
      return true;
    }
  }
  return false;
}



//  1 - FSM Handlers



void fsm_read_cells(const uint32_t now){
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
}


bool helper_load_waiting_timer_state(uint32_t *cell_timer, uint32_t *led_timer, bool *led_on, uint32_t *main_loop_timer){
  return ((system_get_cell_read_timer(cell_timer) == STATE_OK) &&
    (system_get_divemode_led_timer(led_timer) == STATE_OK) &&
    (system_get_divemode_led_on(led_on) == STATE_OK)) &&
    (system_get_main_loop_timer(main_loop_timer) == STATE_OK);
}

void fsm_waiting(const uint32_t now){
  uint32_t last_cell_read_time_ms = 0U;
  //uint32_t last_lcd_update_time_ms = 0U;
  uint32_t divemode_led_timer_ms = 0U;
  uint32_t divemode_flash_interval_ms = 0U;
  uint32_t main_loop_timer_ms = 0U;
  uint16_t cells_raw[THREE_CELLS] = {0U};
  sensor_vote_result_t voted_cell = SENSOR_UNINITIALISED;
  uint16_t voted_ppo2 = 0U;
  bool divemode_led_on = false;
  bool initial_calibration_required = is_initial_calibration_required();

  if(initial_calibration_required){
    // To be done later when
    // solenoid management added
  }

  

  if(helper_assign_current_cell_raw_to_array(cells_raw) != STATE_OK){
    Serial.println("cell assignment error in fsm_waiting");
    handle_error();
  }

  voted_cell = get_voted_sensor(cells_raw, &voted_ppo2);

  if (!helper_load_waiting_timer_state(&last_cell_read_time_ms,
                                        &divemode_led_timer_ms,
                                        &divemode_led_on,
                                        &main_loop_timer_ms)){
    Serial.println("Error loading timer data in fsm_waiting");
    handle_error();
  }

  if(has_timer_elapsed(now, last_cell_read_time_ms, FREQUENCY_CELL_READ_MS)){
    system_set_fsm_state(FSM_READ_CELLS);
    system_set_cell_read_timer(now);
    return;
  }

  divemode_flash_interval_ms = get_divemode_flash_interval_ms(divemode_led_on, false, initial_calibration_required);
  if(has_timer_elapsed(now, divemode_led_timer_ms, divemode_flash_interval_ms)){
    divemode_led_on = !divemode_led_on;
    gpio_led_on(divemode_led_on);
    system_set_divemode_led_on(divemode_led_on);
    system_set_divemode_led_timer(now);
  }

  if(gpio_slide_switch_on() == SWITCH_ON){
    system_set_fsm_state(FSM_DATA_DISPLAY);
    return;
  }

  if(display_handler_screen_off() != DISPLAY_STATUS_OK){
    Serial.println("display_handler_off failed in fsm_waiting");
    handle_error();
  }

  if(has_timer_elapsed(now, main_loop_timer_ms, FREQUENCY_MAIN_LOOP_MS)){
    Serial.println("Main loop run");
    system_set_main_loop_timer(now);
  }
}

void fsm_data_display(uint32_t now){
  uint32_t last_lcd_update_time_ms = 0U;
  uint32_t last_cell_read_time_ms = 0U;
  uint16_t cells_raw[THREE_CELLS] = {0U};
  bool initial_calibration_required = is_initial_calibration_required();
  sensor_vote_result_t voted_cell = SENSOR_UNINITIALISED;
  uint16_t voted_ppo2 = 0U;
  switchstate_t display_switch = gpio_slide_switch_on();
  switchstate_t calibration_button = gpio_momentary_pushed();

  if(display_switch == SWITCH_OFF){
    system_set_fsm_state(FSM_WAITING);
    return;
  }

  if((display_switch == SWITCH_ON) && (calibration_button == SWITCH_ON)){
    system_set_calibration_hold_timer(now);
    system_set_fsm_state(FSM_CALIBRATION_ACTIVATED);
    return;
  }

  if(initial_calibration_required){
    // To be done later when
    // solenoid management added
  }

  if(system_get_cell_read_timer(&last_cell_read_time_ms) != STATE_OK){
    Serial.println("Failed get cell timer fsm_data_display");
    handle_error();
  }

  if(has_timer_elapsed(now, last_cell_read_time_ms, FREQUENCY_CELL_READ_MS)){
    system_set_fsm_state(FSM_READ_CELLS);
    system_set_cell_read_timer(now);
    return;
  }  

  voted_cell = get_voted_sensor(cells_raw, &voted_ppo2);

  if(helper_assign_current_cell_raw_to_array(cells_raw) != STATE_OK){
    Serial.println("cell assignment error in fsm_waiting");
    handle_error();
  }

  if(system_get_lcd_update_timer(&last_lcd_update_time_ms) != STATE_OK){
    Serial.println("Error getting lcd timer in fsm_data_display");
    handle_error();
  }

  if(has_timer_elapsed(now, last_lcd_update_time_ms, FREQUENCY_LCD_UPDATE_MS)){
    if(display_handler_screen_on(cells_raw, initial_calibration_required, voted_cell) != DISPLAY_STATUS_OK){
      Serial.println("display_handler_on failed in fsm_waiting");
      handle_error();
    }
    system_set_lcd_update_timer(now);
    Serial.println("fsm_data_display");
  }
}

uint32_t get_divemode_flash_interval_ms(bool divemode_led_on, bool display_switch_on, bool initial_calibration_required){
  if(display_switch_on){
    return ((divemode_led_on) ? FREQUENCY_DATAMODE_LED_ON_MS : FREQUENCY_DATAMODE_LED_OFF_MS);
  } else {
    if(initial_calibration_required){
      return ((divemode_led_on) ? FREQUENCY_REQUIRES_CAL_LED_ON_MS : FREQUENCY_REQUIRES_CAL_LED_OFF_MS);
    } else {
      return ((divemode_led_on) ? FREQUENCY_DIVEMODE_LED_ON_MS : FREQUENCY_DIVEMODE_LED_OFF_MS);
    }
  }
}

void fsm_calibration_activated(const uint32_t now){
  bool button_pushed = gpio_momentary_pushed();
  bool slide_switch_on = gpio_slide_switch_on();
  bool screen_written_once = system_get_screen_written_once();
  uint32_t calibration_hold_time_ms = 0U;

  if(!slide_switch_on || !button_pushed){
    system_set_screen_written_once(false);
    system_set_fsm_state(FSM_WAITING);
    return;
  }
  system_get_calibration_hold_timer(&calibration_hold_time_ms);
  if(has_timer_elapsed(now, calibration_hold_time_ms, MAX_CALIBRATION_HOLD_MS)){
    system_set_screen_written_once(false);
    system_set_calibration_hold_timer(now);
    system_set_fsm_state(FSM_CALIBRATION_WRITING);
    return;
  }
  if(!screen_written_once){
    display_clear();
    display_set_cursor(0, 0);
    display_println("HOLD TO");
    display_println("CALIBRATE");
    if(display_update() != DISPLAY_STATUS_OK){
      Serial.println("Failed fsm_calibration_active");
      handle_error();
    }
    system_set_screen_written_once(true);
  }
}

void fsm_calibration_writing(const uint32_t now){
  uint32_t last_ms = 0U;
  uint16_t reading_raw[THREE_CELLS] = {0U};

  if(system_get_calibration_write_timer(&last_ms) != STATE_OK){
    Serial.println("Failed at cal write");
    handle_error();
  }
  if(has_timer_elapsed(now, last_ms, (ONE_SECOND_MS * 3)) && !gpio_momentary_pushed()){
    for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
      system_get_cell_reading(&reading_raw[channel], channel);
      system_set_calibration_factor(reading_raw[channel], channel);
    }
    if(eeprom_write_calibration(reading_raw) != MEM_OK){
      Serial.println("Cal write failed");
      handle_error();
    }
    system_set_fsm_state(FSM_WAITING);
    return;
  }
  display_clear();
  display_set_cursor(0, 0);
  display_println("WRITING");
  if(display_update() != DISPLAY_STATUS_OK){
    Serial.println("Failed fsm_calibration_writing");
    handle_error();
  }
}

//  2 - Cell handling

system_state_t helper_assign_current_cell_raw_to_array(uint16_t cells_raw[]){
  bool cell_read_ready = false;
  
  if(cells_raw == NULL){
    return STATE_INVALID_PARAMETER;
  }
  if(system_get_cell_read_ready(&cell_read_ready) != STATE_OK){
    Serial.println("Get cell read ready in helper_assign_current_cell_read_to_array");
    handle_error();
  }
  if(cell_read_ready){
    for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
      if(system_get_cell_reading(&cells_raw[channel], channel) != STATE_OK){
        Serial.println("Error getting cell readings in helper_assign_current_cell_read_to_array");
        handle_error();
      }
    }
  } else {
    Serial.println("Cells unavailable in helper_assign_current_cell_read_to_array");
    handle_error();
  }
  return STATE_OK;
}

sensor_vote_result_t get_voted_sensor(uint16_t cells_raw[], uint16_t *voted_ppo2){
  if((voted_ppo2 == NULL) || (cells_raw == NULL)){
    return SENSOR_FAULT;
  }

  uint16_t readings[THREE_CELLS] = {0U};
  const uint8_t SENSOR_0 = 0U;
  const uint8_t SENSOR_1 = 1U;
  const uint8_t SENSOR_2 = 2U;
  const uint8_t AVERAGE_OF_3_SENSORS = 3U;
  const uint8_t AVERAGE_OF_2_SENSORS = 2U;
  uint16_t d01;
  uint16_t d02;
  uint16_t d12;

  for (uint8_t channel = 0U; channel < THREE_CELLS; channel++){    
    if(convert_raw_to_ppo2(cells_raw[channel], channel, &readings[channel]) != STATE_OK){
      return SENSOR_FAULT;
    }
  }
  d01 = diff_u16(readings[SENSOR_0], readings[SENSOR_1]);
  d02 = diff_u16(readings[SENSOR_0], readings[SENSOR_2]);
  d12 = diff_u16(readings[SENSOR_1], readings[SENSOR_2]);
  if ((d01 <= MAX_DEVIATION_FROM_SETPOINT) && (d02 <= MAX_DEVIATION_FROM_SETPOINT) && (d12 <= MAX_DEVIATION_FROM_SETPOINT)){
    *voted_ppo2 = (uint16_t)((readings[SENSOR_0] + readings[SENSOR_1] + readings[SENSOR_2]) / AVERAGE_OF_3_SENSORS);
    return SENSOR_ALL_VALID;
  }

  uint8_t pair_a;
  uint8_t pair_b;
  sensor_vote_result_t rejected;
  uint16_t min_deviation;

  pair_a = SENSOR_0;
  pair_b = SENSOR_1;
  rejected = SENSOR_2_REJECTED;
  min_deviation = d01;
  if(d02 < min_deviation){
    pair_a = SENSOR_0;
    pair_b = SENSOR_2;
    rejected = SENSOR_1_REJECTED;
    min_deviation = d02;
  }
  if(d12 < min_deviation){
    pair_a = SENSOR_1;
    pair_b = SENSOR_2;
    rejected = SENSOR_0_REJECTED;
    min_deviation = d12;
  }
  *voted_ppo2 = (uint16_t)((readings[pair_a] + readings[pair_b]) / AVERAGE_OF_2_SENSORS);
  return rejected;
}

static uint16_t diff_u16(uint16_t a, uint16_t b){
  return (a > b) ? (a - b) : (b - a);
}

system_state_t convert_raw_to_ppo2(const uint16_t raw, const uint8_t channel, uint16_t * const raw_converted_to_ppo2){
  uint16_t reference_value = 0U;
  uint32_t ppo2 = 0U;
  const uint32_t scale = CALIBRATION_PPO2x1000;
  uint32_t temp = 0U;

  if(system_get_calibration_factor(&reference_value, channel) != STATE_OK){
    return STATE_FUNCTION_FAILED;
  }
  if(raw_converted_to_ppo2 == NULL){
    return STATE_INVALID_PARAMETER;
  }
  if(reference_value == 0){
    return STATE_REQUIRES_CALIBRATION;
  }
  temp = ((uint32_t)raw) * scale;
  ppo2 = temp / reference_value;
  if (ppo2 > UINT16_MAX) return STATE_OVERFLOW;
  *raw_converted_to_ppo2 = (uint16_t)ppo2;
  return STATE_OK;
}

system_state_t cell_read(void){
  uint16_t reading_raw = 0U;

  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    if(adc_raw_reading(&reading_raw, channel) != ADC_STATUS_OK){
      return STATE_FAILED_READ;
    }
    if(system_set_cell_reading(reading_raw, channel) != STATE_OK){
      return STATE_FUNCTION_FAILED;
    }
  }
  return STATE_OK;
}

system_state_t assign_cell_calibration_factors(void){
  uint16_t temp_calibration_factors[THREE_CELLS];

  if(eeprom_read_calibration(temp_calibration_factors) != MEM_OK){
    Serial.println("Cal read failed");
    handle_error();
  }
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    if(system_set_calibration_factor(temp_calibration_factors[channel], channel) != STATE_OK){
      return STATE_FUNCTION_FAILED;
    }
  }
  return STATE_OK;
}


//  3 - Display
display_status_t display_handler_screen_off(void){
  display_clear();
  return display_update();
}

system_state_t print_ppo2_top_line_oled(uint16_t cells_ppo2[], sensor_vote_result_t voted_cell){
  char buffer_ppo2[FORMATTING_PPO2_STR_LEN];

  if(cells_ppo2 == NULL){
    return STATE_INVALID_PARAMETER;
  }
  if(voted_cell >= SENSOR_COUNT_END){
    return STATE_INVALID_PARAMETER;
  }
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    if(voted_cell == channel){
      display_set_colour(DISPLAY_BLACK, DISPLAY_WHITE);
    }
    format_ppo2_to_text(cells_ppo2[channel], buffer_ppo2);
    display_print(buffer_ppo2);
    display_set_colour(DISPLAY_WHITE, DISPLAY_BLACK);
    display_print(" ");
  }
  return STATE_OK;
}

system_state_t print_mv_bottom_line_oled(uint16_t cells_mv[], sensor_vote_result_t voted_cell){
  char buffer_mv[FORMATTING_INTEGER_STR_LEN];

  if(cells_mv == NULL){
    return STATE_INVALID_PARAMETER;
  }
  if(voted_cell >= SENSOR_COUNT_END){
    return STATE_INVALID_PARAMETER;
  }

  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    if(voted_cell == channel){
      display_set_colour(DISPLAY_BLACK, DISPLAY_WHITE);
    }
    format_integer_for_display(cells_mv[channel], buffer_mv);
    display_print(buffer_mv);
    display_print("mV");
    display_set_colour(DISPLAY_WHITE, DISPLAY_BLACK);
    display_print(" ");
  }
  return STATE_OK;
}

display_status_t display_handler_screen_on(uint16_t cells_raw[], bool calibration_required, sensor_vote_result_t voted_cell){
  uint16_t cells_ppo2[THREE_CELLS] = {0U};
  uint16_t cells_mv[THREE_CELLS] = {0U};
  //sensor_vote_result_t voted_cell;
  //uint16_t voted_ppo2;

  if(cells_raw == NULL){
    return DISPLAY_STATUS_INVALID_PARAM;
  }

  display_clear();
  display_set_cursor(0, 0);
  display_font_size(1);

  if(calibration_required){
    display_println("CALIBRATION REQUIRED");
  } else {
    for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
      if(convert_raw_to_ppo2(cells_raw[channel], channel, &cells_ppo2[channel]) != STATE_OK){
        Serial.println("Conversion failed in display_handler_screen_on");
        handle_error();
      }
      cells_mv[channel] = (uint16_t)adc_convert_raw_to_mV(cells_raw[channel]);
    }
    //voted_cell = get_voted_sensor(cells_raw, &voted_ppo2);
    if(voted_cell == SENSOR_FAULT){
      Serial.println("voted cell fault");
      handle_error();
    }
    if(print_ppo2_top_line_oled(cells_ppo2, voted_cell) != STATE_OK){
      Serial.println("print_ppo2 in mode_screen_on");
      handle_error();
    }
    display_println("");
    if(print_mv_bottom_line_oled(cells_mv, voted_cell) != STATE_OK){
      Serial.println("print_mv in mode_screen_on");
      handle_error();
    }
  }
  if(display_update() != DISPLAY_STATUS_OK){
    return DISPLAY_STATUS_HW_ERROR;
  }
  return DISPLAY_STATUS_OK;
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
  for(;;) {
    delay(1);
  }
}
