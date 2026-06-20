#include "format_for_print.h"

// Public

void format_ppo2_to_text(const uint16_t value, char output[FORMATTING_HUNDREDTHS_STR_LEN]){
    uint16_t integer_part;
    uint16_t fractional_part;
    bool within_bounds = (value <= 9999U);

    if(within_bounds){
      integer_part = value / 1000u;
      fractional_part = value % 1000u;
      output[0] = (char)('0' + integer_part);
      output[1] = '.';
      output[2] = (char)('0' + (fractional_part / 100u));
      output[3] = (char)('0' + ((fractional_part / 10u) % 10u));
      output[4] = '\0';
    } else {
      output[0] = '-';
      output[1] = '.';
      output[2] = '-';
      output[3] = '-';
      output[4] = '\0';
    }
}

// Private