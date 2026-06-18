#ifndef ADC_HAL_H
#define ADC_HAL_H

#include <Arduino.h>
#include <stdint.h>

//#define ADC_FULL_SCALE_COUNTS 32768
//#define ADC_MV_MAX 256
//#define ADC_MV_MIN 0


typedef enum
{
    ADC_STATUS_OK = 0,
    ADC_STATUS_NOT_INITIALIZED,
    ADC_STATUS_INIT_FAILED,
    ADC_STATUS_INVALID_CHANNEL,
    ADC_STATUS_HW_ERROR,
    ADC_STATUS_INVALID_PARAMETER,
    ADC_STATUS_NOT_POWERED
} hal_adc_status_t;

const uint8_t ADC_MIN_MV_HO_READING = 102U; // 97% oxygen at 22mV high output cell
const uint8_t ADC_MAX_MV_HO_READING = 136U; // 102% oxygen at 28mV high output cell
const uint8_t ADC_MIN_MV_LO_READING = 37U; // 97% oxygen at 8mV low output cell
const uint8_t ADC_MAX_MV_LO_READING = 67U; // 102% oxygen at 14mV low output cell



hal_adc_status_t adc_init();
hal_adc_status_t adc_raw_reading(uint16_t * raw_reading);
int16_t adc_convert_raw_to_mV(const uint16_t raw_reading);
hal_adc_status_t x_adc_convert_raw_to_mV(const int16_t raw_reading, int32_t *converted_val_mV);

#endif