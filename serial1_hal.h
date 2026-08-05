#include <sys/_stdint.h>
#ifndef SERIAL1_HAL_H
#define SERIAL1_HAL_H

#include <Arduino.h>
#include <stdint.h>
#include "comms_shared_definitions.h"

typedef enum{
  SER_OK,
  SER_UNINITIALISED,
  SER_INVALID_PARAMETER,
  SER_NOTHING_SENT,
  SER_DATA_PACKET_TX_REFUSED,
  SER_DATA_PACKET_REJECTED
} serial_state_t;

typedef enum{
  TX_ZERO_COUNT = 0,
  TX_UNINITIALISED,
  TX_HANDSHAKE_REQUEST,
  TX_HANDSHAKE_ACKNOWLEDGED,
  TX_REQUEST_DATA_PACKET,
  TX_PAYLOAD_ATTACHED,
  TX_READY_TO_TRANSMIT,
  TX_REFUSED_REQUEST,
  TX_DATA_PACKET_ACCEPTED,
  TX_DATA_PACKET_REJECTED,
  TX_END_COUNT
} tx_command_t;



serial_state_t serial1_init(void);
serial_state_t serial1_listen_for_command(tx_command_t * const command);
serial_state_t serial1_send_command(const tx_command_t command);
serial_state_t serial1_send_data_packet(const data_packet_t datapacket);
serial_state_t serial1_listen_for_data_packet(data_packet_t *datapacket);

#endif