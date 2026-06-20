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

void format_integer_for_display(uint16_t value, char buffer[FORMATTING_INTEGER_STR_LEN]){
  uint16_t temp;
  uint8_t pos = 0U;

  if(value > 999U){
    value = 999U;
  }
  if(value >= 100U){
    buffer[pos] = (char)('0' + (uint8_t)(value / 100U));
    pos++;
    value %= 100U;
  }
  if((value >= 10U) || (pos > 0U)){
    buffer[pos] = (char)('0' + (uint8_t)(value / 10U));
    pos++;
    value %= 10U;
  }
  temp = value;
  buffer[pos] = (char)('0' + (uint8_t)temp);
  pos++;
  buffer[pos] = '\0';
}

// Private