#include <sys/_stdint.h>
#ifndef TIME_HELPERS_H
#define TIME_HELPERS_H

#include <Arduino.h>
#include <stdint.h>

constexpr uint32_t ONE_SECOND_MS = 1000U;

bool has_timer_elapsed(const uint32_t current_time, const uint32_t last_time, const uint32_t frequency);


#endif