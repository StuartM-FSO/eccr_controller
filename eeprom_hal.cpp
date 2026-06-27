#include "Arduino.h"
#include <sys/_stdint.h>
#include "eeprom_hal.h"
#include "CalibrationStorage.h"

typedef struct{
  bool initialised = false;
} eeprom_state_t;

constexpr uint8_t THREE_CELLS = 3U;

static eeprom_state_t mem;

// Public API

mem_status_t eeprom_init(void){
  if(mem.initialised){
    MEM_OK;
  }
  mem.initialised = true;
  return MEM_OK;
}

mem_status_t eeprom_write_calibration(uint16_t calibration_raw[]){
  CalibData_t cal = {0};
  
  if(!mem.initialised){
    return MEM_UNINITIALISED;
  }
  if(calibration_raw == NULL){
    return MEM_INVALID_PARAMETER;
  }
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    cal.values[channel] = calibration_raw[channel];
  }
  if(Calib_Write(&cal) != CALIB_STATUS_OK){
    return MEM_FAILED;
  }
  return MEM_OK;
}

mem_status_t eeprom_read_calibration(uint16_t calibration_raw[]){
  CalibData_t temp = {0};
  
  if(!mem.initialised){
    return MEM_UNINITIALISED;
  }
  if(calibration_raw == NULL){
    return MEM_INVALID_PARAMETER;
  }
  if(Calib_Read(&temp) != CALIB_STATUS_OK){
    return MEM_FAILED;
  }
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    calibration_raw[channel] = temp.values[channel];
  }
  return MEM_OK;
}

