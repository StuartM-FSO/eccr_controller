#include "eeprom_library_basic.h"

Simple24LC256 EEPROM24;

void Simple24LC256::begin(void)
{
    Wire.begin();
}

bool Simple24LC256::isReady(void)
{
    Wire.beginTransmission(EEPROM_ADDRESS);

    const uint8_t result = Wire.endTransmission();

    return (result == 0U);
}

bool Simple24LC256::write_uint16(uint16_t address, uint16_t value)
{
    Wire.beginTransmission(EEPROM_ADDRESS);

    Wire.write(static_cast<uint8_t>((address >> 8U) & 0xFFU));
    Wire.write(static_cast<uint8_t>(address & 0xFFU));

    const uint16_t raw =
        static_cast<uint16_t>(value);

    Wire.write(static_cast<uint8_t>((raw >> 8U) & 0xFFU));
    Wire.write(static_cast<uint8_t>(raw & 0xFFU));

    const uint8_t result = Wire.endTransmission();

    return (result == 0U);
}

bool Simple24LC256::read_uint16(uint16_t address, uint16_t* value)
{
    if (value == nullptr)
    {
        return false;
    }

    Wire.beginTransmission(EEPROM_ADDRESS);

    Wire.write(static_cast<uint8_t>((address >> 8U) & 0xFFU));
    Wire.write(static_cast<uint8_t>(address & 0xFFU));

    uint8_t result = Wire.endTransmission(false);

    if (result != 0U)
    {
        return false;
    }

    const uint8_t bytesRequested =
        Wire.requestFrom(EEPROM_ADDRESS, static_cast<uint8_t>(2U));

    if (bytesRequested != 2U)
    {
        return false;
    }

    const uint8_t highByte =
        static_cast<uint8_t>(Wire.read());

    const uint8_t lowByte =
        static_cast<uint8_t>(Wire.read());

    const uint16_t combined =
        (static_cast<uint16_t>(highByte) << 8U) |
        static_cast<uint16_t>(lowByte);

    *value = static_cast<int16_t>(combined);

    return true;
}