#include <sys/_stdint.h>
#include "Arduino.h"
#include "comms_layer_host.h"
#include <stdint.h>
#include "serial1_hal.h"

typedef enum{
  COMSTATE_UNINITIALISED,
  COMSTATE_START_UP,
  COMSTATE_LISTEN,
  COMSTATE_ACKNOWLEDGE_HANDSHAKE,
  COMSTATE_SEND_DATA_PACKET,
  COMSTATE_ERROR_HARD
} comms_fsm_state_t;

typedef struct{
  bool initialised;
  comms_fsm_state_t current_state;
  operational_state_t operational_state;
} internal_state_t;

static eccr_state_t eccr = {};

static internal_state_t state = {};

static void comstate_start_up(void);
static void comstate_listen(void);
static void comstate_acknowledge_handshake(void);
static void comstate_send_data_packet(void);

static void next_state(comms_fsm_state_t fsm_state);


// Public API

host_return_t host_init(void){
  if(state.initialised){
    return HOST_OK;
  }
  if(serial1_init() != SER_OK){
    Serial.println("Serial1 init failed");
    return HOST_INITIALISATION_FAILED;
  }
  state.current_state = COMSTATE_START_UP;
  state.initialised = true;
  return HOST_OK;
}

host_return_t host_run(void){

  if(!state.initialised){
    return HOST_UNINITIALISED;
  }

  switch (state.current_state) {
    case COMSTATE_START_UP:
      comstate_start_up();
      break;
    case COMSTATE_LISTEN:
      comstate_listen();
      break;
    case COMSTATE_ACKNOWLEDGE_HANDSHAKE:
      comstate_acknowledge_handshake();
      break;
    case COMSTATE_SEND_DATA_PACKET:
      comstate_send_data_packet();
      break;
    case COMSTATE_ERROR_HARD:
      return HOST_ERROR_HARD;
    default:
      return HOST_ERROR_HARD;
  }
  return HOST_OK;
}

host_return_t host_load_packet(const uint16_t *ppo2_x1000, const operational_state_t opstate){
  if(!state.initialised){
    return HOST_UNINITIALISED;
  }
  if(ppo2_x1000 == NULL){
    return HOST_INVALID_PARAMETER;
  }
  if((opstate <= OPSTATE_ZERO) || (opstate >= OPSTATE_END_COUNT)){
    return HOST_INVALID_PARAMETER;
  }

  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    eccr.ppo2_x1000[channel] = ppo2_x1000[channel];
  }
  eccr.operational_state = opstate;
  return HOST_OK;
}



// Private functions

// WIP


//  01 - FSM

static void comstate_start_up(void){
  if(serial1_flush_serial_buffer() != SER_OK){
    next_state(COMSTATE_ERROR_HARD);
    return;
  }
  next_state(COMSTATE_LISTEN);
}

static void comstate_listen(void){
  serial_command_t rx_command = TXCOMMAND_EMPTY;

  switch (serial1_listen_for_command()) {
    case SER_UNINITIALISED:
      Serial.println("Error, serial1 not initialised");
      next_state(COMSTATE_ERROR_HARD);
      return;
    case SER_NOTHING_SENT:
      break;
    case SER_COMMAND_RECEIVED:
      Serial.print("Command received: ");
      if(serial1_get_latest_command(&rx_command) != SER_OK){
        Serial.println("Error getting latest command");
        next_state(COMSTATE_LISTEN);
        return;
      }
      Serial.println(rx_command);
      break;
    default:
      break;
  }

  switch (rx_command) {
    case TXCOMMAND_EMPTY:
      break;
    case TXCOMMAND_HANDSHAKE_REQUEST:
      next_state(COMSTATE_ACKNOWLEDGE_HANDSHAKE);
      break;
    case TXCOMMAND_DATA_PACKET_REQUEST:
      next_state(COMSTATE_SEND_DATA_PACKET);
      break;
    default:
      break;
  }
}

static void comstate_acknowledge_handshake(void){
  Serial.println("Acknowledge handshake");
  if(serial1_send_handshake_acknowledgement() != SER_OK){
    Serial.println("Error sending ack");
  }
  next_state(COMSTATE_LISTEN);
}

// 02 - General

static void comstate_send_data_packet(void){
  Serial.println("Sending data packet");
  if(serial1_load_data_packet(eccr.ppo2_x1000, eccr.operational_state) != SER_OK){
    Serial.println("Error loading data packet");
  }
  if(serial1_send_payload() != SER_OK){
    Serial.println("Error sending payload");
  }
  next_state(COMSTATE_LISTEN);
}

static void next_state(comms_fsm_state_t fsm_state){
  state.current_state = fsm_state;
}
