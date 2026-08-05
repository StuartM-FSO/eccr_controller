#ifndef COMMS_SHARED_DEFINITIONS_H
#define COMMS_SHARED_DEFINITIONS_H

typedef enum{
  CONTROLLER_ZERO_COUNT = 0,
  CONTROLLER_UNKNOWN_STATUS,
  CONTROLLER_OK,
  CONTROLLER_END_COUNT
} controller_status_t;

typedef struct __attribute__((packed)){
  uint32_t id;
  uint16_t cell[3];
  controller_status_t controller_status;
  uint16_t crc;
} data_packet_t;


#endif