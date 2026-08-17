#ifndef SHARED_H
#define SHARED_H

#include <Arduino.h>
#include <stdint.h>

constexpr uint8_t THREE_CELLS = 3U;

typedef enum{
  OPSTATE_ZERO,
  OPSTATE_DIVEMODE,
  OPSTATE_DATAMODE,
  OPSTATE_FAILURE,
  OPSTATE_TESTMODE,
  OPSTATE_END_COUNT
} operational_state_t;

typedef struct{
  uint16_t ppo2_x1000[THREE_CELLS];
  bool latest_crc_ok;
  uint32_t latest_packet_received_id;
  operational_state_t operational_state;
} eccr_state_t;

#endif