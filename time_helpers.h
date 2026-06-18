#ifndef TIME_HELPERS_H
#define TIME_HELPERS_H

#include <Arduino.h>
#include <stdint.h>

bool has_timer_elapsed(const uint32_t current_time, const uint32_t last_time, const uint32_t frequency);


#endif