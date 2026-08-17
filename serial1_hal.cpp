#include <cstddef>
#include <sys/_stdint.h>
#include "api/Common.h"
#include "Arduino.h"
#include "serial1_hal.h"
#include <SerialTransfer.h>
#include "time_helpers.h"

constexpr uint8_t SERIAL1_BAUD_RATE = 115200U;
constexpr uint8_t ID_INCREMENT = 1U;
constexpr uint32_t SERIAL_STARTUP_TIME_OUT_MS = 5000U;

struct __attribute__((packed)) STRUCT {
  uint32_t id;
  uint16_t ppo2_x1000[THREE_CELLS];
  operational_state_t operational_state;
  uint16_t crc;
} payload;

static SerialTransfer comdevice;

typedef struct{
  bool initialised;
  serial_command_t latest_command_received;
  uint32_t last_packet_id;
} internal_state_t;

internal_state_t state = {};


static uint16_t crc16_ccitt(const uint8_t *data, size_t length);


// Public API

ser_return_t serial1_init(void){
  uint32_t start_up_timer_ms = 0U;

  if(state.initialised){
    return SER_OK;
  }
  Serial1.begin(SERIAL1_BAUD_RATE);
  start_up_timer_ms = millis();
  while(!Serial1){
    delay(1);
    if(has_timer_elapsed(millis(), start_up_timer_ms, SERIAL_STARTUP_TIME_OUT_MS)){
      Serial.println("Timed out starting Serial1");
      return SER_TIMEOUT_FAILURE;
    }
  }
  comdevice.begin(Serial1);
  state.latest_command_received = TXCOMMAND_EMPTY;
  state.last_packet_id = 0U;
  state.initialised = true;
  return SER_OK;
}

ser_return_t serial1_flush_serial_buffer(void){
  uint32_t flush_timer_ms = 0U;

  if(!state.initialised){
    return SER_UNINITIALISED;
  }
  flush_timer_ms = millis();
  while(Serial1.available() > 0){
    Serial1.read();
    if(has_timer_elapsed(millis(), flush_timer_ms, SERIAL_STARTUP_TIME_OUT_MS)){
      Serial.println("Failure flushing buffer");
      return SER_TIMEOUT_FAILURE;
    }
  }
  Serial.println("Buffer flushed");
  return SER_OK;
}

// Public 01 - Handshake functions

ser_return_t serial1_send_handshake_request(void){
  uint16_t tx_size = 0U;

  if(!state.initialised){
    return SER_UNINITIALISED;
  }
  tx_size = comdevice.txObj(TXCOMMAND_HANDSHAKE_REQUEST, tx_size);
  comdevice.sendData(tx_size);
  return SER_OK;
}

bool serial1_handshake_received(void){
  if(state.latest_command_received == TXCOMMAND_HANDSHAKE_ACKNOWLEDGED){
    return true;
  }
  return false;
}

ser_return_t serial1_send_handshake_acknowledgement(void){
  uint16_t tx_size = 0U;

  if(!state.initialised){
    return SER_UNINITIALISED;
  }

  tx_size = comdevice.txObj(TXCOMMAND_HANDSHAKE_ACKNOWLEDGED, tx_size);
  comdevice.sendData(tx_size);
  return SER_OK;
}

// Public 02 - Command handling

ser_return_t serial1_listen_for_command(void){
  uint16_t rx_size = 0U;
  serial_command_t rx_command = TXCOMMAND_EMPTY;
  
  if(!state.initialised){
    return SER_UNINITIALISED;
  }
  if(!comdevice.available()){
    state.latest_command_received = TXCOMMAND_EMPTY;
    return SER_NOTHING_SENT;
  }
  rx_size = comdevice.rxObj(rx_command, rx_size);
  state.latest_command_received = rx_command;
  return SER_COMMAND_RECEIVED;
}

ser_return_t serial1_get_latest_command(serial_command_t * const latest_command){
  if(!state.initialised){
    return SER_UNINITIALISED;
  }
  if(latest_command == NULL){
    return SER_INVALID_PARAMETER;
  }

  *latest_command = state.latest_command_received;
  return SER_OK;
}

// Public 03 - Data packet handling

ser_return_t serial1_send_data_packet_request(void){
  uint16_t tx_size = 0U;

  if(!state.initialised){
    return SER_UNINITIALISED;
  }

  tx_size = comdevice.txObj(TXCOMMAND_DATA_PACKET_REQUEST, tx_size);
  comdevice.sendData(tx_size);
  return SER_OK;
}

ser_return_t serial1_send_payload(void){
  uint16_t tx_size = 0U;

  if(!state.initialised){
    return SER_UNINITIALISED;
  }

  tx_size = comdevice.txObj(payload, tx_size);
  comdevice.sendData(tx_size);
  state.last_packet_id = payload.id;
  Serial.print("ID Sent: ");
  Serial.print(state.last_packet_id);
  Serial.print(" CRC: ");
  Serial.println(payload.crc, HEX);
  return SER_OK;
}

ser_return_t serial1_load_data_packet(uint16_t *ppo2_x1000, const operational_state_t op_state){
  if(!state.initialised){
    return SER_UNINITIALISED;
  }
  if(ppo2_x1000 == NULL){
    return SER_INVALID_PARAMETER;
  }

  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    payload.ppo2_x1000[channel] = ppo2_x1000[channel];
  }
  payload.id = state.last_packet_id + ID_INCREMENT;
  payload.operational_state = op_state;
  payload.crc = crc16_ccitt((const uint8_t *)&payload, (sizeof(payload) - sizeof(payload.crc)));
  return SER_OK;
}

ser_return_t serial1_listen_for_data_packet(void){
  uint16_t rx_size = 0U;
  uint16_t crc_calculated = 0U;

  if(!state.initialised){
    return SER_UNINITIALISED;
  }

  if(!comdevice.available()){
    return SER_NOTHING_SENT;
  }

  rx_size = comdevice.rxObj(payload, rx_size);
  Serial.print("ID packet received: ");
  Serial.println(payload.id);
  state.last_packet_id = payload.id;
  for(uint8_t i = 0U; i < THREE_CELLS; i++){
    Serial.print(payload.ppo2_x1000[i]);
    Serial.print(" ");
  }
  Serial.println();
  Serial.print("Rx CRC: ");
  Serial.print(payload.crc, HEX);
  crc_calculated = crc16_ccitt((const uint8_t *)&payload, (sizeof(payload) - sizeof(payload.crc)));
  Serial.print(" Calc CRC: ");
  Serial.println(crc_calculated, HEX);
  if(payload.crc == crc_calculated){
    return SER_OK;
  } else {
    return SER_CRC_MISMATCH;
  }
}

// Public 04 - Setters & Getters

ser_return_t serial1_get_latest_packet_id(uint32_t * const latest_id){
  if(!state.initialised){
    return SER_UNINITIALISED;
  }
  if(latest_id == NULL){
    return SER_INVALID_PARAMETER;
  }
  *latest_id = state.last_packet_id;
  return SER_OK;
}

ser_return_t serial1_get_ppo2(uint16_t * const ppo2_x1000, const uint8_t channel){
  if(!state.initialised){
    return SER_UNINITIALISED;
  }
  if((ppo2_x1000 == NULL) || (channel >= THREE_CELLS)){
    return SER_INVALID_PARAMETER;
  }
  *ppo2_x1000 = payload.ppo2_x1000[channel];
  return SER_OK;
}

// Private

static uint16_t crc16_ccitt(const uint8_t *data, size_t length){
  uint16_t crc = 0xFFFF;    // Initial value

  while (length--){
    crc ^= (uint16_t)(*data++) << 8;
    for (uint8_t i = 0; i < 8; i++){
      if (crc & 0x8000){
        crc = (crc << 1) ^ 0x1021;
      } else{
        crc <<= 1;}
    }
  }
  return crc;
}