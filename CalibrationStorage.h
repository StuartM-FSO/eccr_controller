#ifndef CALIBRATION_STORAGE_H
#define CALIBRATION_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include <stdint.h>

#define CALIB_VALUES_COUNT   (3u)
#define CALIB_RECORD_SLOTS   (16u)

/* Record states */
#define REC_STATE_EMPTY      (0xFFu)
#define REC_STATE_WRITING    (0x55u)
#define REC_STATE_VALID      (0xA5u)

/* Status codes */
typedef enum
{
    CALIB_STATUS_OK = 0,
    CALIB_STATUS_NO_VALID_RECORD,
    CALIB_STATUS_CRC_ERROR,
    CALIB_STATUS_WRITE_ERROR
} CalibStatus_t;

/* Public calibration data container */
typedef struct
{
    uint16_t values[CALIB_VALUES_COUNT];
} CalibData_t;

/* API */
void Calib_Init(void);
CalibStatus_t Calib_Read(CalibData_t *outData);
CalibStatus_t Calib_Write(const CalibData_t *inData);

#endif /* CALIBRATION_STORAGE_H */
