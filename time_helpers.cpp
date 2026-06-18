#include "time_helpers.h"

// Public functions

bool has_timer_elapsed(const uint32_t current_time, const uint32_t last_time, const uint32_t frequency){
  return (current_time - last_time) >= frequency;
}