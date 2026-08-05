#include "api/Print.h"
#include <cstddef>
#include <sys/_stdint.h>
#include <Arduino.h>
#include <stdint.h>
#include "comms_protocol.h"
#include "time_helpers.h"
#include "serial1_hal.h"

constexpr uint32_t MAX_ACK_WAIT_MS = 2000U;
constexpr uint32_t MAX_DATA_PACKET_RQ_WAIT_MS = 500U;
constexpr uint8_t THREE_CELLS = 3U;
constexpr uint16_t ZERO_CELL_READING = 0U;

typedef enum{
  COMSTATE_ZERO_COUNT = 0U,
  COMSTATE_UNINITIALISED,
  COMSTATE_LISTEN,
  COMSTATE_SEND_HANDSHAKE,
  COMSTATE_WAIT_FOR_ACKNOWLEDGEMENT,
  COMSTATE_ACKNOWLEDGE_HANDSHAKE,
  COMSTATE_SEND_DATA_REQUEST,
  COMSTATE_WAIT_FOR_DATA_PACKET,
  COMSTATE_SEND_DATA_PACKET,
  COMSTATE_DEBUG_SEQUENCE_END,
  COMSTATE_TIMEOUT,
  COMSTATE_END_COUNT
} comstate_t;

typedef struct{
  bool initialised;
  bool handshake_timer_running;
  bool data_packet_request_timer_running;
  comms_system_type_t system_type;
  comstate_t internal_state;
  uint32_t ack_wait_timer_ms;
  uint32_t data_packet_request_timer_ms;
  bool payload_has_updated;
  bool payload_sent;
} comms_internal_state_t;


static comms_internal_state_t state = {};
static data_packet_t payload = {};


// Private function declarations
//    FSM
static void comstate_listen(void);
static void comstate_wait_for_acknowledgement(void);
static void comstate_send_handshake(void);
static void comstate_timeout(void);
static void comstate_acknowledge_handshake(void);
static void comstate_send_data_request(void);
static void comstate_wait_for_data_packet(void);
static void comstate_send_data_packet(void);

static void comstate_debug_sequence_end(void);

//    General
static void state_transition(comstate_t state);
static comstate_t process_command(const comstate_t current_state, const tx_command_t received_command);
static uint16_t crc16_ccitt(const uint8_t *data, size_t length);

// Public API

comms_return_t comms_init(const comms_system_type_t system_type){
  if(state.initialised){
    return COMMS_OK;
  }
  if((system_type <= COM_ZERO_COUNT) || (system_type >= COM_END_COUNT)){
    return COMMS_INVALID_PARAMETER;
  }
  if(serial1_init() != SER_OK){
    return COMMS_SERIAL1_FAILED_INIT;
  }
  state.system_type = system_type;
  state.internal_state = COMSTATE_LISTEN;
  state.ack_wait_timer_ms = 0U;
  state.handshake_timer_running = false;
  state.data_packet_request_timer_running = false;
  state.initialised = true;
  state.payload_has_updated = false;
  payload.id = 1U;
  payload.controller_status = CONTROLLER_UNKNOWN_STATUS;
  state.payload_sent = false;
  return COMMS_OK;
}

comms_return_t comms_check(void){
  if(!state.initialised){
    return COMMS_UNINITIALISED;
  }

  switch (state.internal_state) {
    case COMSTATE_LISTEN:
      comstate_listen();
      break;
    case COMSTATE_WAIT_FOR_ACKNOWLEDGEMENT:
      comstate_wait_for_acknowledgement();
      break;
    case COMSTATE_ACKNOWLEDGE_HANDSHAKE:
      comstate_acknowledge_handshake();
      break;
    case COMSTATE_SEND_HANDSHAKE:
      comstate_send_handshake();
      break;
    case COMSTATE_TIMEOUT:
      comstate_timeout();
      break;
    case COMSTATE_SEND_DATA_REQUEST:
      comstate_send_data_request();
      break;
    case COMSTATE_WAIT_FOR_DATA_PACKET:
      comstate_wait_for_data_packet();
      break;
    case COMSTATE_SEND_DATA_PACKET:
      comstate_send_data_packet();
      break;
    case COMSTATE_DEBUG_SEQUENCE_END:
      comstate_debug_sequence_end();
    default:
      break;
  }
  return COMMS_OK;
}

comms_return_t comms_handshake(void){
  if(!state.initialised){
    return COMMS_UNINITIALISED;
  }
  state_transition(COMSTATE_SEND_HANDSHAKE);
  return COMMS_OK;
}

comms_return_t comms_data_packet_request(void){
  if(state.system_type == COM_TYPE_HOST){
    return COMMS_INVALID_PARAMETER;
  }
  if(!state.initialised){
    return COMMS_UNINITIALISED;
  }
  state_transition(COMSTATE_SEND_DATA_REQUEST);
  return COMMS_OK;
}

comms_return_t comms_prepare_payload(const uint16_t * ppo2_x1000, const controller_status_t controller_status){
  if(!state.initialised){
    return COMMS_UNINITIALISED;
  }
  // Check data validity here

  if(state.payload_sent){
    payload.id++;
    state.payload_sent = false;
  }
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    payload.cell[channel] = ppo2_x1000[channel];
  }
  payload.controller_status = controller_status;
  payload.crc = crc16_ccitt((const uint8_t *)&payload, (sizeof(payload) - sizeof(payload.crc)));
  return COMMS_OK;
}

bool comms_payload_updated(void){
  return state.payload_has_updated;
}

comms_return_t comms_get_data_packet(data_packet_t *transfer_packet){
  uint16_t crc_calculated = crc16_ccitt((const uint8_t *)&payload, (sizeof(payload) - sizeof(payload.crc)));

  if(!state.initialised){
    return COMMS_UNINITIALISED;
  }
  if(crc_calculated != payload.crc){
    return COMMS_CRC_MISMATCH;
  }
  transfer_packet->id = payload.id;
  transfer_packet->controller_status = payload.controller_status;
  for(uint8_t channel = 0U; channel < THREE_CELLS; channel++){
    transfer_packet->cell[channel] = payload.cell[channel];
  }
  return COMMS_OK;
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

static comstate_t process_command(const comstate_t current_state, const tx_command_t received_command){
  comstate_t transition_to = COMSTATE_UNINITIALISED;

  if(current_state == COMSTATE_WAIT_FOR_ACKNOWLEDGEMENT){
    switch (received_command) {
      case TX_HANDSHAKE_ACKNOWLEDGED:
        Serial.println("Command processed - TX_HANDSHAKE_ACKNOWLEDGED");
        transition_to = COMSTATE_LISTEN;
        break;
      default:
        // Illegal command
        // How to process?
        transition_to = COMSTATE_DEBUG_SEQUENCE_END;
        Serial.print("Illegal command received, code: ");
        Serial.println(received_command);
        break;
    }
  }

  if(current_state == COMSTATE_LISTEN){
    switch (received_command) {
      case TX_HANDSHAKE_REQUEST:
        Serial.println("Command processed - TX_HANDSHAKE_REQUEST");
        transition_to = COMSTATE_ACKNOWLEDGE_HANDSHAKE;
        break;
      case TX_REQUEST_DATA_PACKET:
        Serial.println("Command processed - TX_REQUEST_DATA_PACKET");
        transition_to = COMSTATE_SEND_DATA_PACKET;
        break;
      default:
        // Illegal command
        // How to process?
        transition_to = COMSTATE_DEBUG_SEQUENCE_END;
        Serial.print("Illegal command received, code: ");
        Serial.println(received_command);
        break;
    }
  }
  return transition_to;
}

//    FSM States

static void state_transition(comstate_t new_state){
  state.internal_state = new_state;
}

static void comstate_wait_for_data_packet(void){
  uint32_t now = millis();
  uint32_t timeout = state.data_packet_request_timer_ms;
  serial_state_t listen_result = SER_UNINITIALISED;
  uint16_t calculated_crc = 0U;

  if(has_timer_elapsed(now, timeout, MAX_DATA_PACKET_RQ_WAIT_MS)){
    Serial.println("Data packet request timed out");
    state.data_packet_request_timer_running = false;
    state_transition(COMSTATE_LISTEN);
  }
  listen_result = serial1_listen_for_data_packet(&payload);
  if(listen_result == SER_OK){
    calculated_crc = crc16_ccitt((const uint8_t *)&payload, (sizeof(payload) - sizeof(payload.crc)));
    Serial.print("Packet received, id: ");
    Serial.println(payload.id);
    Serial.print("RX CRC: ");
    Serial.print(payload.crc, HEX);
    Serial.print(" Calc CRC: ");
    Serial.println(calculated_crc, HEX);
    state.data_packet_request_timer_running = false;
    state_transition(COMSTATE_LISTEN);
    state.payload_has_updated = true;
    return;
  }
  if(listen_result == SER_DATA_PACKET_REJECTED){
    Serial.println("Data packet rejected");
    state_transition(COMSTATE_DEBUG_SEQUENCE_END);
    return;
  }
  if(listen_result == SER_UNINITIALISED){
    // ???
  }
}

static void comstate_send_data_packet(void){
  Serial.print("Sending data packet ID: ");
  Serial.println(payload.id);
  if(serial1_send_data_packet(payload) != SER_OK){
    // Handle error
  }
  Serial.print("CRC: ");
  Serial.println(payload.crc, HEX);
  state.payload_sent = true;
  state_transition(COMSTATE_LISTEN);
}

static void comstate_send_data_request(void){
  if(state.system_type != COM_TYPE_CLIENT){
    return;
  }
  if(state.data_packet_request_timer_running){
    Serial.println("Data packet request sent while previous request");
    state_transition(COMSTATE_WAIT_FOR_DATA_PACKET);
    return;
  }
  if(serial1_send_command(TX_REQUEST_DATA_PACKET) != SER_OK){
    // Handle error
  }
  Serial.println("Data packet request sent");
  state.data_packet_request_timer_ms = millis();
  state.data_packet_request_timer_running = true;
  state.payload_has_updated = false;
  Serial.println("Wait for data packet");
  state_transition(COMSTATE_WAIT_FOR_DATA_PACKET);
}

static void comstate_listen(void){
  tx_command_t command = TX_UNINITIALISED;
  serial_state_t result = serial1_listen_for_command(&command);
  comstate_t transition_to = COMSTATE_UNINITIALISED;

  if(result == SER_OK){
    Serial.print("Command received: ");
    Serial.println(command);
    // Check if command is valid
    transition_to = process_command(COMSTATE_LISTEN, command);
    state_transition(transition_to);
  }
  if((result == SER_INVALID_PARAMETER) || (result == SER_UNINITIALISED)){
    // Handle error
  }
}

static void comstate_wait_for_acknowledgement(void){
  uint32_t now = millis();
  uint32_t ack_wait_timer_ms = state.ack_wait_timer_ms;
  tx_command_t command = TX_UNINITIALISED;
  serial_state_t result = serial1_listen_for_command(&command);
  comstate_t next_state_transition = COMSTATE_UNINITIALISED;

  if(result == SER_OK){
    next_state_transition = process_command(COMSTATE_WAIT_FOR_ACKNOWLEDGEMENT, command);
    state.handshake_timer_running = false;
    Serial.println("Handshake acknowledgement received");
  }

  if((result == SER_INVALID_PARAMETER) || (result == SER_UNINITIALISED)){
    Serial.println("Failed");
    state.handshake_timer_running = false;
    // Handle errors here
  }

  if(result == SER_NOTHING_SENT){
    next_state_transition = state.internal_state;
  }

  if(has_timer_elapsed(now, ack_wait_timer_ms, MAX_ACK_WAIT_MS)){
    Serial.println("Timer expired");
    state_transition(COMSTATE_TIMEOUT);
    state.handshake_timer_running = false;
    return;
  }

  state_transition(next_state_transition);
}

static void comstate_acknowledge_handshake(void){
  Serial.println("Handshake acknowledged");
  if(serial1_send_command(TX_HANDSHAKE_ACKNOWLEDGED) != SER_OK){
    // Handle error
  }
  state.handshake_timer_running = false;
  state_transition(COMSTATE_LISTEN);
}

static void comstate_send_handshake(void){
  if(state.handshake_timer_running){
    Serial.println("Handshake timer already running");
    return;
  }
  Serial.println("Handshake sent");
  if(serial1_send_command(TX_HANDSHAKE_REQUEST) != SER_OK){
    // Handle error
  }
  state.ack_wait_timer_ms = millis();
  state.handshake_timer_running = true;  
  state_transition(COMSTATE_WAIT_FOR_ACKNOWLEDGEMENT);
}

static void comstate_timeout(void){
  Serial.println("Timed out");
  state.handshake_timer_running = false;
  state_transition(COMSTATE_LISTEN);
}






static void comstate_debug_sequence_end(void){
  Serial.println("Sequence end");
  for(;;){
    // Intentional halt for debug & testing purposes
  }
}