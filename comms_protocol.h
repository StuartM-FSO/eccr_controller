#include <sys/_stdint.h>
#ifndef COMMS_PROTOCOL_H
#define COMMS_PROTOCOL_H

#include <Arduino.h>
#include <stdint.h>
#include "comms_shared_definitions.h"

typedef enum{
  COM_ZERO_COUNT = 0U,
  COM_TYPE_HOST,
  COM_TYPE_CLIENT,
  COM_END_COUNT
} comms_system_type_t;

typedef enum{
  COMMS_OK,
  COMMS_INVALID_PARAMETER,
  COMMS_UNINITIALISED,
  COMMS_SERIAL1_FAILED_INIT,
  COMMS_FAILED,
  COMMS_CRC_MISMATCH
} comms_return_t;

comms_return_t comms_init(const comms_system_type_t system_type);
comms_return_t comms_check(void);
comms_return_t comms_handshake(void);
comms_return_t comms_data_packet_request(void);
comms_return_t comms_prepare_payload(const uint16_t * ppo2_x1000, const controller_status_t controller_status);

comms_return_t comms_get_data_packet(data_packet_t *transfer_packet);

bool comms_payload_updated(void);
#endif