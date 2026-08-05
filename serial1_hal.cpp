#include "api/Common.h"
#include <sys/_stdint.h>
#include "serial1_hal.h"
#include <Arduino.h>
#include <stdint.h>
#include <SerialTransfer.h>


constexpr uint16_t BAUD_RATE = 9600U;
constexpr uint8_t THREE_CELLS = 3U;

struct __attribute__((packed)) STRUCT {
  tx_command_t tx_command;
  uint32_t id;
  uint16_t tx_cell[THREE_CELLS];
  controller_status_t controller_status;
  uint16_t crc;
} tx_struct;

struct __attribute__((packed)) STRUCT_RX {
  tx_command_t rx_command;
  uint32_t rx_id;
  uint16_t rx_cell[THREE_CELLS];
  controller_status_t controller_status;
  uint16_t crc;
} rx_struct;

SerialTransfer comms;


typedef enum{
  COMM_HANDSHAKE,
  COMM_ACKNOWLEDGED,
  COMM_FAILED
} serial_command_t;

typedef struct{
  bool initialised;
} internal_state_t;

static internal_state_t state = {};

// Public API

serial_state_t serial1_init(){
  if(state.initialised){
    return SER_OK;
  }
  Serial1.begin(BAUD_RATE);
  while(!Serial1){
    delay(1);
  }
  comms.begin(Serial1);
  state.initialised = true;
  return SER_OK;
}

serial_state_t serial1_listen_for_command(tx_command_t * const command){
  uint16_t rx_size = 0U;
  tx_command_t rx_command = TX_UNINITIALISED;

  if(command == NULL){
    return SER_INVALID_PARAMETER;
  }
  if(!state.initialised){
    return SER_UNINITIALISED;
  }
  if(!comms.available()){
    return SER_NOTHING_SENT;
  }
  rx_size = comms.rxObj(rx_command, rx_size);
  *command = rx_command;
  return SER_OK;
}

serial_state_t serial1_send_command(const tx_command_t command){
  uint16_t tx_size = 0U;

  if(!state.initialised){
    return SER_UNINITIALISED;
  }
  if((command <= TX_ZERO_COUNT) || (command >= TX_END_COUNT)){
    return SER_INVALID_PARAMETER;
  }

  tx_size = comms.txObj(command, tx_size);
  comms.sendData(tx_size);
  return SER_OK;
}

serial_state_t serial1_send_data_packet(const data_packet_t datapacket){
  uint16_t tx_size = 0U;

  if(!state.initialised){
    return SER_UNINITIALISED;
  }

  tx_struct.id = datapacket.id;
  tx_struct.tx_command = TX_PAYLOAD_ATTACHED;
  tx_struct.controller_status = datapacket.controller_status;
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    tx_struct.tx_cell[channel] = datapacket.cell[channel];
  }
  tx_struct.crc = datapacket.crc;
  tx_size = comms.txObj(tx_struct, tx_size);
  comms.sendData(tx_size);

  return SER_OK;
}

serial_state_t serial1_listen_for_data_packet(data_packet_t *datapacket){
  uint16_t rx_size = 0U;
  
  if(!comms.available()){
    return SER_NOTHING_SENT;
  }
  rx_size = comms.rxObj(rx_struct);
  if(rx_struct.rx_command != TX_PAYLOAD_ATTACHED){
    return SER_DATA_PACKET_REJECTED;
  }
  datapacket->id = rx_struct.rx_id;
  datapacket->controller_status = rx_struct.controller_status;
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    datapacket->cell[channel] = rx_struct.rx_cell[channel];
  }
  datapacket->crc = rx_struct.crc;
  return SER_OK;
}



// Private