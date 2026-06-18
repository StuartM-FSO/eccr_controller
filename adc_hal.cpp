// NOTES
// 1. Wire.begin() must be called in setup() in main sketch
// 2. Use of delay() has been accepted in this situation
// 3. MAX_CHANNELS shows 3 sensors even though ADS1115 has 4 channels because system physically cannot accomodate more than 3 sensors
// 4. Negative voltages are only possible if sensors are fitted in reverse. System design has a physical block to prevent this
// 5. -- deleted --
// 6. is_powered is read on every call because tying it to the is_connected timer is too slow to catch power failures
// 7. adc_initialised set to false if is_connected or is_powered fails. This isn't necessary at the moment because a HW fault being
//    returned to the caller immediately sends the main loop into a failed safe state. In the future there may be a need to add
//    recovery or better error handling which is why the reset is there.
// 8. Gain is only ever set at GAIN_SIXTEEN, system cannot operate at any other gain setting.

#include <Arduino.h>
#include <stdint.h>
#include "adc_hal.h"
#include <Adafruit_ADS1X15.h>

static const uint8_t MAX_POWER_CHECK_TRIES = 10U;
static const uint8_t MAX_CONNECTION_CHECK_TRIES = 10U;
static const uint8_t THREE_CELLS = 3U;


// Constants
static const uint8_t ADC_POWER_READ_PIN = D2;
static const uint8_t ADC_ADDRESS = 0x48;
static const uint16_t ADC_MINIMUM_POWER_READING_MV = 2500; 
//static const uint8_t THREE_CELLS = 3; // System is designed for reading 3 sensors, 4th channel is unused
static const uint8_t CHANNEL_ZERO = 0U;
static const uint16_t ADC_POWER_MAX_MV = 5000U;
static const uint16_t ADC_POWER_MAX_RAW = 1023U;
static const int32_t ADS1115_FULL_SCALE_MV = 256;
static const int32_t ADS1115_RESOLUTION = 32768;
static const int32_t MICROVOLTS_PER_MILLIVOLT = 1000;
static const uint32_t MAX_INTERVAL_FUNCTION_CHECK_MS = 250;
static const uint32_t MAX_WAIT_TIME_MS = 50;


typedef struct{
    bool adc_initialised;
    uint32_t last_function_check_time;
    Adafruit_ADS1115 device;
} state_t;

// Variables
static state_t current_state;

// Private function defs
static bool is_connected(void);
static bool is_powered(void);
static bool map_non_arduino(const uint16_t base_value, uint16_t *scaled_value,
    const uint16_t in_min, const uint16_t in_max, const uint16_t out_min, const uint16_t out_max);
static bool sort_values(uint16_t arr[3]);
static hal_adc_status_t read_sensor(uint16_t *reading, const uint8_t channel);
static bool power_check_multiple(void);
static bool connected_check_multiple(void);
static bool state_reset(state_t *state);



// Public functions


hal_adc_status_t adc_init(void){
    uint8_t counter = 0;
    bool flag_success = false;

    if(current_state.adc_initialised){
        return ADC_STATUS_OK;
    }
    state_reset(&current_state);
    if(!power_check_multiple()){
        return ADC_STATUS_NOT_POWERED;
    }
    if(!connected_check_multiple()){
        return ADC_STATUS_HW_ERROR;
    }
    if(current_state.device.begin() == false)
    {
        return ADC_STATUS_INIT_FAILED;
    }
    current_state.device.setGain(GAIN_SIXTEEN);
    current_state.device.setDataRate(RATE_ADS1115_128SPS);
    current_state.adc_initialised = true;
    current_state.last_function_check_time = millis();
    return ADC_STATUS_OK;
}

hal_adc_status_t adc_raw_reading(uint16_t * const raw_reading, const uint8_t channel){
    const uint8_t MAX_SAMPLES = 3;                            // System will never have anything other than 3 sensors connected to operate
    const uint8_t MEDIAN_SAMPLE_NUMBER = MAX_SAMPLES / 2;     // so the median will always be [1]
    uint16_t reading = 0;
    uint16_t sample[MAX_SAMPLES];
    
    if(!current_state.adc_initialised){
        return ADC_STATUS_NOT_INITIALIZED;
    }
    if(raw_reading == NULL){
        return ADC_STATUS_INVALID_PARAMETER;
    }
    if(channel >= THREE_CELLS){
        return ADC_STATUS_INVALID_PARAMETER;
    }
    if(!power_check_multiple()){
        current_state.adc_initialised = false;
        return ADC_STATUS_HW_ERROR;
    }
    if((millis() - current_state.last_function_check_time) > MAX_INTERVAL_FUNCTION_CHECK_MS){
        current_state.last_function_check_time = millis();
        if(!connected_check_multiple()){
            current_state.adc_initialised = false;
            return ADC_STATUS_HW_ERROR;
        }
    }
    for(uint8_t sample_number = 0U; sample_number < MAX_SAMPLES; sample_number++){
        if(read_sensor(&reading, channel) != ADC_STATUS_OK){
            current_state.adc_initialised = false;
            return ADC_STATUS_HW_ERROR;
        }
        // Add reading validity check here, if passes then write to array
        sample[sample_number] = reading;
    }
    if(sort_values(sample) != true){
        return ADC_STATUS_INVALID_PARAMETER;
    }
    *raw_reading = sample[MEDIAN_SAMPLE_NUMBER];
    return ADC_STATUS_OK;
}

int16_t adc_convert_raw_to_mV(const uint16_t raw_reading){
    uint64_t microvolts = 0;

    if (raw_reading < 0){
        return 0;
    }
    microvolts = ((uint64_t)raw_reading * (ADS1115_FULL_SCALE_MV * MICROVOLTS_PER_MILLIVOLT)) / ADS1115_RESOLUTION;
    //microvolts = ((int32_t)raw_reading * (ADS1115_FULL_SCALE_MV * MICROVOLTS_PER_MILLIVOLT)) / ADS1115_RESOLUTION;

    // Convert µV → mV
    return (uint16_t)(microvolts / MICROVOLTS_PER_MILLIVOLT);
}


// Private functions

static bool is_powered(void){
    uint16_t reading_raw = 0;
    uint16_t reading_mv = 0;

    reading_raw = analogRead(ADC_POWER_READ_PIN);
    if (map_non_arduino(reading_raw, &reading_mv, 0, ADC_POWER_MAX_RAW, 0, ADC_POWER_MAX_MV) == false){
        return false;
    }
    return (reading_mv >= ADC_MINIMUM_POWER_READING_MV);
}

static bool is_connected(void){
    uint8_t result = 99;

    Wire.beginTransmission(ADC_ADDRESS);
    result = Wire.endTransmission();
    return (result == 0);
}

bool map_non_arduino(const uint16_t base_value, uint16_t * const scaled_value,
    const uint16_t in_min, const uint16_t in_max, const uint16_t out_min, const uint16_t out_max)
{
    uint32_t numerator;
    uint32_t denominator;
    uint32_t result;
    uint16_t working_value = base_value;

    if (scaled_value == NULL) {
        return false;
    }
    if (in_min == in_max) {
        return false;
    }
    if (out_max < out_min) return false;
    if (base_value < in_min) {
        working_value = in_min;
    } else if (base_value > in_max) {
        working_value = in_max;
    }
    numerator = (uint32_t)(working_value - in_min) * (uint32_t)(out_max - out_min);
    denominator = (uint32_t)(in_max - in_min);
    result = numerator / denominator + out_min;
    *scaled_value = (uint16_t)result;
    return true;
}

bool sort_values(uint16_t arr[3]) {
    int16_t temp;

    if (arr[0] > arr[1]) {
        temp = arr[0]; arr[0] = arr[1]; arr[1] = temp;
    }
    if (arr[0] > arr[2]) {
        temp = arr[0]; arr[0] = arr[2]; arr[2] = temp;
    }
    if (arr[1] > arr[2]) {
        temp = arr[1]; arr[1] = arr[2]; arr[2] = temp;
    }

    return true;
}

static bool state_reset(state_t * const state){
    if(state == NULL){
        return false;
    }
    state->adc_initialised = false;
    state->last_function_check_time = 0;
    state->device = Adafruit_ADS1115();
    return true;
}

static hal_adc_status_t read_sensor(uint16_t * const reading, const uint8_t channel){
    bool wait_timeout_ok;
    uint32_t start_time_ms = 0;
    uint32_t elapsed_time_ms = 0;
    int32_t unfiltered_reading = 0;

    if(reading == NULL){
        return ADC_STATUS_INVALID_PARAMETER;
    }
    if(channel >= THREE_CELLS){
        return ADC_STATUS_INVALID_PARAMETER;
    }
    current_state.device.startADCReading(MUX_BY_CHANNEL[channel], false);
    wait_timeout_ok = true;
    start_time_ms = millis();
    while(!current_state.device.conversionComplete()){
        elapsed_time_ms = millis() - start_time_ms;
        if(elapsed_time_ms >= MAX_WAIT_TIME_MS){
            wait_timeout_ok = false;
            break;
        }
        delay(1); // Reduces CPU cycles. Accepting the penalty of using delay() in this situation.
    }
    if(wait_timeout_ok){
        unfiltered_reading = current_state.device.getLastConversionResults();
        *reading = (unfiltered_reading < 0) ? 0U : (uint16_t)unfiltered_reading;
        return ADC_STATUS_OK;
    } else {
        return ADC_STATUS_HW_ERROR;
    }
}

static bool power_check_multiple(void){
    uint8_t counter = 0;
    bool flag_powered = false;

    while((!flag_powered) && counter < MAX_POWER_CHECK_TRIES){
        counter++;
        flag_powered = is_powered();
        if(!flag_powered){
            delay(1);
        }
    }
    return flag_powered;
}

static bool connected_check_multiple(void){
    uint8_t counter = 0;
    bool flag_connected = false;

    while((!flag_connected) && counter < MAX_CONNECTION_CHECK_TRIES){
        counter++;
        flag_connected = is_connected();
        if(!flag_connected){
            delay(1);
        }
    }
    return flag_connected;
}
