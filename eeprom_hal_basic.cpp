//   NOTES
//   1. init() assumes that Wire.begin() is called externally in main code
//   2. Memory size is intentionally capped to allow backwards compatibility with Arduino Nano
//   3. HW system design will always only ever use 3 cells
//   4. Inverse check is only carried out if reading is non-zero. Zero reading indicates full system reset has been carried out. Any
//      value of zero_count other than either 0 or 3 indicates the memory has been corrupted.
//   5. Delay calls are required for efficiency of while loop. Its use here has been accepted as a deviation of MISRA good practice. The
//      blocking function is minimal; calls to the eeprom hal are only possible in either startup or calibration conditions, not dive mode;
//      resets during dive mode will trigger it but this will be a fault condition anyway.
//   6. Backup written before primary so that if there is a reset between the two writes then the last good primary reading is still there

//#include "api/Common.h"
//#include <cstddef>
#include <sys/_stdint.h>
#include "eeprom_hal_basic.h"
#include <Arduino.h>
#include <stdint.h>

static const uint16_t MAX_IS_READY_TRIES = 30U;
static const uint8_t MAX_SINGLE_READ_TRIES = 5U;
static const uint16_t EEPROM_ENTRY_SIZE = (2U * sizeof(int16_t));
static const uint16_t INVERSE_MASK = ((uint16_t)0xFFFFU);

typedef struct{
  bool initialised;
  cell_type_t cell_type;
} eeprom_state_t;

static eeprom_state_t state = {false};
Simple24LC256 eeprom;

// Constants
static const uint8_t THREE_CELLS = 3U; // See Note 3
static const uint16_t MAX_MEMORY_SIZE_ARDUINO_COMPATIBILITY = 1024; // See Note 2
static const int16_t CELL_LOW_MAXIMUM = 8700;
static const int16_t CELL_HIGH_MAXIMUM = 18000;

// Private function defs
static bool eeprom_ack_test_success(void);
static bool read_value_is_valid(const uint16_t value);
static uint16_t calculate_primary_address(const uint16_t channel);
static mem_status_t eeprom_hal_read_single_value(const uint16_t address, uint16_t *value);
static mem_status_t eeprom_hal_write_single_value(const uint16_t address, uint16_t value);

// Public API

mem_status_t eeprom_hal_init(const cell_type_t cell){ // See Note 1
  if(state.initialised){
    return MEM_OK;
  }
  if((cell != HIGH_OUTPUT_CELL) && (cell != LOW_OUTPUT_CELL)){
    state.initialised = false;
    return MEM_INVALID_PARAMETER;
  }
  state.cell_type = cell;
  state.initialised = true;
  return MEM_OK;
}

mem_status_t eeprom_hal_read_array(uint16_t * reference_array){
  if(!state.initialised){
    return MEM_UNINITIALISED;
  }
  if(reference_array == NULL){
    return MEM_INVALID_PARAMETER;
  }

  int16_t temp_array[THREE_CELLS];
  uint8_t zero_count = 0;

  for(uint8_t channel = 0; channel < THREE_CELLS; channel++){
    uint16_t primary_address = calculate_primary_address(channel);
    uint16_t backup_address = primary_address + sizeof(int16_t);
    uint16_t primary_reading_from_eeprom = 0;
    uint16_t backup_reading_from_eeprom = 0;

    if(eeprom_hal_read_single_value(primary_address, &primary_reading_from_eeprom) != MEM_OK){
      return MEM_READ_FAIL;
    }
    if(eeprom_hal_read_single_value(backup_address, &backup_reading_from_eeprom) != MEM_OK){
      return MEM_READ_FAIL;
    }
    if(primary_reading_from_eeprom != 0){ // See Note 4
      if(((uint16_t)primary_reading_from_eeprom ^ (uint16_t)backup_reading_from_eeprom) != INVERSE_MASK){
        return MEM_DATA_FAIL;
      }
    } else {
      zero_count++;
    }
    if(!read_value_is_valid(primary_reading_from_eeprom)){
      return MEM_DATA_FAIL;
    }
    temp_array[channel] = primary_reading_from_eeprom;
  }
  if ((zero_count > 0) && (zero_count != 3)){ // See Note 4
    return MEM_DATA_FAIL;
  }
  for(uint8_t channel = 0; channel < THREE_CELLS; channel++){
    reference_array[channel] = temp_array[channel];
  }
  return MEM_OK;
}

mem_status_t eeprom_hal_write_array(const uint16_t * reference_array){
  if(!state.initialised){
    return MEM_UNINITIALISED;
  }
  if(reference_array == NULL){
    return MEM_INVALID_PARAMETER;
  }

  for(uint8_t channel = 0; channel < THREE_CELLS; channel++){
    uint16_t primary_address = calculate_primary_address(channel);
    uint16_t backup_address = primary_address + sizeof(int16_t);
    int16_t primary_reference = reference_array[channel];
    int16_t backup_reference = (int16_t)(~(uint16_t)primary_reference);

    if(eeprom_hal_write_single_value(backup_address, backup_reference) != MEM_OK){ // See Note 6
      return MEM_WRITE_FAIL;
    }
    if(eeprom_hal_write_single_value(primary_address, primary_reference) != MEM_OK){
      return MEM_WRITE_FAIL;
    }
  }
  return MEM_OK;
}




// Private functions

static mem_status_t eeprom_hal_read_single_value(const uint16_t address, uint16_t *value){

  if(value == NULL){
    return MEM_INVALID_PARAMETER;
  }
  if(!state.initialised){
    return MEM_UNINITIALISED;
  }
  if(address > MAX_MEMORY_SIZE_ARDUINO_COMPATIBILITY - sizeof(int16_t)){
    return MEM_INVALID_PARAMETER;
  }
  if(!eeprom_ack_test_success()){
    return MEM_HW_FAIL;
  }
  uint16_t read_from_memory = 0;
  if(!eeprom.read_uint16(address, &read_from_memory)){
    return MEM_READ_FAIL;
  }
  *value = read_from_memory;
  return MEM_OK;
}

static mem_status_t eeprom_hal_write_single_value(const uint16_t address, uint16_t value){
  uint16_t check_read = 0;
  uint8_t counter = 0;
  bool flag_success = false;
  
  if(!state.initialised){
    return MEM_UNINITIALISED;
  }
  if(address > MAX_MEMORY_SIZE_ARDUINO_COMPATIBILITY - sizeof(int16_t)){
    return MEM_INVALID_PARAMETER;
  }
  if(!eeprom_ack_test_success()){
    return MEM_HW_FAIL;
  }
  if(!eeprom.read_uint16(address, &check_read)){
    return MEM_READ_FAIL;
  }
  if(check_read == value){
    return MEM_OK;
  }
  if(!eeprom.write_uint16(address, value)){
    return MEM_WRITE_FAIL;
  }
  if(!eeprom_ack_test_success()){
    return MEM_HW_FAIL;
  }
  while((!flag_success) && (counter < MAX_SINGLE_READ_TRIES)){
    counter++;
    flag_success = eeprom.read_uint16(address, &check_read);
    if(!flag_success){
      delay(1); // See Note 5, max 5ms delay possible at worst case
    }
  }
  if(!flag_success){
    return MEM_READ_FAIL;
  }
  if(check_read != value){
    return MEM_DATA_FAIL;
  }
  return MEM_OK;
}


static bool eeprom_ack_test_success(void){
  uint16_t counter = 0;
  bool flag_ready = false;
  
  while((!flag_ready) && (counter < MAX_IS_READY_TRIES)){
    counter++;
    flag_ready = eeprom.isReady();
    if(!flag_ready){
      delay(1); // See Note 5
    }
  }
  return flag_ready;
}

static bool read_value_is_valid(const uint16_t value){
  int16_t maximum_valid_value;

  switch(state.cell_type){
    case LOW_OUTPUT_CELL:   maximum_valid_value = CELL_LOW_MAXIMUM;
                            break;
    case HIGH_OUTPUT_CELL:  maximum_valid_value = CELL_HIGH_MAXIMUM;
                            break;
    default:                return false;
  }

  if(value > maximum_valid_value){
    return false;
  }
  return true;
}

static uint16_t calculate_primary_address(const uint16_t channel){
  return channel * EEPROM_ENTRY_SIZE;
}