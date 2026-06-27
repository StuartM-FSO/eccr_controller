#include "CalibrationStorage.h"

/* Static state */
static uint8_t  g_hasValid = 0u;
static uint8_t  g_currentIndex = 0u;
static uint32_t g_currentSeq = 0u;
static CalibData_t g_currentData;

static uint8_t seq_is_newer(uint32_t a, uint32_t b);

/* -------- CRC16 (poly 0x1021, init 0xFFFF) -------- */
static uint16_t crc16_update(uint16_t crc, uint8_t data)
{
    uint8_t bit;

    crc ^= (uint16_t)((uint16_t)data << 8u);

    for (bit = 0u; bit < 8u; bit++)
    {
        if ((crc & 0x8000u) != 0u)
        {
            crc = (uint16_t)((crc << 1u) ^ 0x1021u);
        }
        else
        {
            crc = (uint16_t)(crc << 1u);
        }
    }

    return crc;
}

static uint16_t crc16_block(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;

    for (i = 0u; i < len; i++)
    {
        crc = crc16_update(crc, data[i]);
    }

    return crc;
}

/* -------- Internal helpers -------- */

static void read_record(uint8_t slot,
                        uint8_t *state,
                        uint32_t *seq,
                        uint16_t *vals,
                        uint16_t *crc)
{
    int base = (int)slot * 16;

    uint8_t b0, b1, b2, b3;
    uint8_t c0, c1;
    uint8_t i;

    *state = EEPROM.read(base);
    base++;

    b0 = EEPROM.read(base++);
    b1 = EEPROM.read(base++);
    b2 = EEPROM.read(base++);
    b3 = EEPROM.read(base++);

    *seq = ((uint32_t)b0 << 24u) |
           ((uint32_t)b1 << 16u) |
           ((uint32_t)b2 << 8u)  |
           ((uint32_t)b3);

    for (i = 0u; i < CALIB_VALUES_COUNT; i++)
    {
        uint8_t hi = EEPROM.read(base++);
        uint8_t lo = EEPROM.read(base++);
        vals[i] = (uint16_t)(((uint16_t)hi << 8u) | (uint16_t)lo);
    }

    c0 = EEPROM.read(base++);
    c1 = EEPROM.read(base++);
    *crc = (uint16_t)(((uint16_t)c0 << 8u) | (uint16_t)c1);
}

static void write_record(uint8_t slot,
                         uint8_t state,
                         uint32_t seq,
                         const uint16_t *vals,
                         uint16_t crc)
{
    int base = (int)slot * 16;
    uint8_t i;

    EEPROM.update(base++, state);

    EEPROM.update(base++, (uint8_t)((seq >> 24u) & 0xFFu));
    EEPROM.update(base++, (uint8_t)((seq >> 16u) & 0xFFu));
    EEPROM.update(base++, (uint8_t)((seq >> 8u) & 0xFFu));
    EEPROM.update(base++, (uint8_t)(seq & 0xFFu));

    for (i = 0u; i < CALIB_VALUES_COUNT; i++)
    {
        EEPROM.update(base++, (uint8_t)((vals[i] >> 8u) & 0xFFu));
        EEPROM.update(base++, (uint8_t)(vals[i] & 0xFFu));
    }

    EEPROM.update(base++, (uint8_t)((crc >> 8u) & 0xFFu));
    EEPROM.update(base++, (uint8_t)(crc & 0xFFu));
}

static uint8_t equal_data(const CalibData_t *a, const CalibData_t *b)
{
    uint8_t i;

    for (i = 0u; i < CALIB_VALUES_COUNT; i++)
    {
        if (a->values[i] != b->values[i])
        {
            return 0u;
        }
    }

    return 1u;
}

static void scan_records(void)
{
    uint8_t slot;

    g_hasValid = 0u;
    g_currentSeq = 0u;
    g_currentIndex = 0u;

    for (slot = 0u; slot < CALIB_RECORD_SLOTS; slot++)
    {
        uint8_t state;
        uint32_t seq;
        uint16_t vals[CALIB_VALUES_COUNT];
        uint16_t storedCrc;
        uint8_t buf[4u + (CALIB_VALUES_COUNT * 2u)];
        uint8_t i;
        uint16_t calcCrc;

        read_record(slot, &state, &seq, vals, &storedCrc);

        if (state != REC_STATE_VALID)
        {
            continue;
        }

        buf[0] = (uint8_t)((seq >> 24u) & 0xFFu);
        buf[1] = (uint8_t)((seq >> 16u) & 0xFFu);
        buf[2] = (uint8_t)((seq >> 8u) & 0xFFu);
        buf[3] = (uint8_t)(seq & 0xFFu);

        for (i = 0u; i < CALIB_VALUES_COUNT; i++)
        {
            buf[4u + (i * 2u)]     = (uint8_t)((vals[i] >> 8u) & 0xFFu);
            buf[4u + (i * 2u) + 1u] = (uint8_t)(vals[i] & 0xFFu);
        }

        calcCrc = crc16_block(buf, (uint16_t)sizeof(buf));
        if (calcCrc != storedCrc)
        {
            continue;
        }

        if ((!g_hasValid || seq_is_newer(seq, g_currentSeq)))
        {
            uint8_t j;

            g_hasValid = 1u;
            g_currentSeq = seq;
            g_currentIndex = slot;

            for (j = 0u; j < CALIB_VALUES_COUNT; j++)
            {
                g_currentData.values[j] = vals[j];
            }
        }
    }
}

/* -------- Public API -------- */

void Calib_Init(void)
{
    uint8_t i;

    g_hasValid = 0u;
    g_currentIndex = 0u;
    g_currentSeq = 0u;

    for (i = 0u; i < CALIB_VALUES_COUNT; i++)
    {
        g_currentData.values[i] = 0u;
    }

    scan_records();
}

CalibStatus_t Calib_Read(CalibData_t *outData)
{
    uint8_t i;

    if (g_hasValid == 0u)
    {
        scan_records();
    }

    if (g_hasValid == 0u)
    {
        return CALIB_STATUS_NO_VALID_RECORD;
    }

    for (i = 0u; i < CALIB_VALUES_COUNT; i++)
    {
        outData->values[i] = g_currentData.values[i];
    }

    return CALIB_STATUS_OK;
}

CalibStatus_t Calib_Write(const CalibData_t *inData)
{
    uint8_t nextSlot;
    uint32_t seq;
    uint8_t buf[4u + (CALIB_VALUES_COUNT * 2u)];
    uint8_t i;
    uint16_t crc;

    if (g_hasValid == 0u)
    {
        scan_records();
    }

    if ((g_hasValid != 0u) && (equal_data(inData, &g_currentData) != 0u))
    {
        return CALIB_STATUS_OK;
    }

    if (g_hasValid == 0u)
    {
        nextSlot = 0u;
        seq = 1u;
    }
    else
    {
        nextSlot = (uint8_t)((g_currentIndex + 1u) % CALIB_RECORD_SLOTS);
        seq = g_currentSeq + 1u;
    }

    buf[0] = (uint8_t)((seq >> 24u) & 0xFFu);
    buf[1] = (uint8_t)((seq >> 16u) & 0xFFu);
    buf[2] = (uint8_t)((seq >> 8u) & 0xFFu);
    buf[3] = (uint8_t)(seq & 0xFFu);

    for (i = 0u; i < CALIB_VALUES_COUNT; i++)
    {
        buf[4u + (i * 2u)]     = (uint8_t)((inData->values[i] >> 8u) & 0xFFu);
        buf[4u + (i * 2u) + 1u] = (uint8_t)(inData->values[i] & 0xFFu);
    }

    crc = crc16_block(buf, (uint16_t)sizeof(buf));

    /* Two‑phase write: WRITING then VALID */
    write_record(nextSlot, REC_STATE_WRITING, seq, inData->values, crc);
    write_record(nextSlot, REC_STATE_VALID,   seq, inData->values, crc);

    scan_records();

    if ((g_hasValid == 0u) || (equal_data(inData, &g_currentData) == 0u))
    {
        return CALIB_STATUS_WRITE_ERROR;
    }

    return CALIB_STATUS_OK;
}

static uint8_t seq_is_newer(uint32_t a, uint32_t b)
{
    return ((int32_t)(a - b) > 0);
}
