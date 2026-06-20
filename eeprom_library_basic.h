#ifndef EEPROMLIBRARYBASIC_H
#define EEPROMLIBRARYBASIC_H

#include <Arduino.h>
#include <Wire.h>

class Simple24LC256
{
public:

    void begin(void);

    bool write_uint16(uint16_t address, uint16_t value);

    bool read_uint16(uint16_t address, uint16_t* value);

    bool isReady(void);

private:

    static constexpr uint8_t EEPROM_ADDRESS = 0x50U;
};

extern Simple24LC256 EEPROM24;

#endif