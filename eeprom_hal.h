#include <sys/_stdint.h>
#ifndef EEPROM_HAL_H
#define EEPROM_HAL_H

#include <Arduino.h>
#include <stdint.h>

typedef enum{
  MEM_OK,
  MEM_INVALID_PARAMETER,
  MEM_UNINITIALISED,
  MEM_FAILED
} mem_status_t;

mem_status_t eeprom_init(void);
mem_status_t eeprom_write_calibration(uint16_t calibration_raw[]);
mem_status_t eeprom_read_calibration(uint16_t calibration_raw[]);

#endif