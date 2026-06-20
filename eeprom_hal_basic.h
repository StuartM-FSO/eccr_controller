#ifndef EEPROM_HAL_BASIC_H
#define EEPROM_HAL_BASIC_H

#include <Arduino.h>
#include <stdint.h>
#include "eeprom_library_basic.h"

typedef enum{
  MEM_OK,
  MEM_FAIL,
  MEM_UNINITIALISED,
  MEM_INVALID_PARAMETER,
  MEM_READ_FAIL,
  MEM_WRITE_FAIL,
  MEM_DATA_FAIL,
  MEM_HW_FAIL
} mem_status_t;

typedef enum{
  HIGH_OUTPUT_CELL = 0,
  LOW_OUTPUT_CELL
} cell_type_t;

mem_status_t eeprom_hal_init(const cell_type_t cell);
mem_status_t eeprom_hal_read_array(uint16_t * reference_array);
mem_status_t eeprom_hal_write_array(const uint16_t * reference_array);

#endif