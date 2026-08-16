#include <sys/_stdint.h>
#ifndef COMMS_LAYER_HOST_H
#define COMMS_LAYER_HOST_H

#include <Arduino.h>
#include <stdint.h>

typedef enum{
  HOST_OK,
  HOST_INITIALISATION_FAILED,
  HOST_UNINITIALISED,
  HOST_INVALID_PARAMETER,
  HOST_ERROR_HARD
} host_return_t;


host_return_t host_init(void);
host_return_t host_run(void);

host_return_t host_load_packet(const uint16_t *ppo2_x1000);

#endif