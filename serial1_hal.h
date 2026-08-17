#include <sys/_stdint.h>
#ifndef SERIAL1_HAL_H
#define SERIAL1_HAL_H

#include <Arduino.h>
#include <stdint.h>
#include "shared.h"

typedef enum{
  TXCOMMAND_EMPTY = 0,
  TXCOMMAND_HANDSHAKE_REQUEST,
  TXCOMMAND_HANDSHAKE_ACKNOWLEDGED,
  TXCOMMAND_DATA_PACKET_REQUEST
} serial_command_t;

typedef enum{
  SER_OK,
  SER_UNINITIALISED,
  SER_INVALID_PARAMETER,
  SER_NOTHING_SENT,
  SER_COMMAND_RECEIVED,
  SER_CRC_MISMATCH,
  SER_TIMEOUT_FAILURE
} ser_return_t;

ser_return_t serial1_init(void);
ser_return_t serial1_flush_serial_buffer(void);
ser_return_t serial1_send_handshake_request(void);
ser_return_t serial1_send_handshake_acknowledgement(void);
ser_return_t serial1_listen_for_command(void);
ser_return_t serial1_send_data_packet_request(void);
ser_return_t serial1_load_data_packet(uint16_t *ppo2_x1000, const operational_state_t op_state);
ser_return_t serial1_send_payload(void);
ser_return_t serial1_listen_for_data_packet(void);
bool serial1_handshake_received(void);

ser_return_t serial1_get_latest_command(serial_command_t * const latest_command);
ser_return_t serial1_get_latest_packet_id(uint32_t * const latest_id);
ser_return_t serial1_get_ppo2(uint16_t * const ppo2_x1000, const uint8_t channel);

#endif