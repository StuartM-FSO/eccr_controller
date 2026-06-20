
#ifndef FORMAT_FOR_PRINT_H
#define FORMAT_FOR_PRINT_H

#include <Arduino.h>
#include <stdint.h>

constexpr uint8_t FORMATTING_HUNDREDTHS_STR_LEN = 5U;

void format_ppo2_to_text(const uint16_t value, char output[FORMATTING_HUNDREDTHS_STR_LEN]);

#endif