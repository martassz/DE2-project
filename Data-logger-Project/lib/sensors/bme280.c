#include <avr/io.h>
#include <twi.h>
#include "bme280.h"

// -----------------------------------------------------------------------------
// Global calibration data and internal fine temperature
// -----------------------------------------------------------------------------

static int32_t t_fine;

// Temperature calibration parameters
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;

// Pressure calibration parameters
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;

// Humidity calibration parameters
static uint8_t  dig_H1, dig_H3;
static int16_t  dig_H2, dig_H4, dig_H5, dig_H6;

// -----------------------------------------------------------------------------
// Low-level register access helpers using Fryza's TWI library
// -----------------------------------------------------------------------------

/**
 * @brief Read one 8-bit register from BME280.
 */
static uint8_t bme280_reg_read8(uint8_t reg)
{
    uint8_t val;

    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_WRITE);
    twi_write(reg);

    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_READ);

    val = twi_read(TWI_NACK);
    twi_stop();

    return val;
}

/**
 * @brief Read a 16-bit register pair (MSB:reg, LSB:reg+1).
 */
static uint16_t bme280_reg_read16(uint8_t reg)
{
    uint8_t msb, lsb;

    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_WRITE);
    twi_write(reg);

    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_READ);

    msb = twi_read(TWI_ACK);
    lsb = twi_read(TWI_NACK);

    twi_stop();
    return ((uint16_t)msb << 8) | lsb;
}

// -----------------------------------------------------------------------------
// Initialization: read calibration data and configure sensor mode
// -----------------------------------------------------------------------------

void bme280_init(void)
{
    // --- Temperature calibration (0x88 – 0x8D) ---
    dig_T1 = bme280_reg_read16(0x88);
    dig_T2 = (int16_t)bme280_reg_read16(0x8A);
    dig_T3 = (int16_t)bme280_reg_read16(0x8C);

    // --- Pressure calibration (0x8E – 0xA1) ---
    dig_P1 = bme280_reg_read16(0x8E);
    dig_P2 = (int16_t)bme280_reg_read16(0x90);
    dig_P3 = (int16_t)bme280_reg_read16(0x92);
    dig_P4 = (int16_t)bme280_reg_read16(0x94);
    dig_P5 = (int16_t)bme280_reg_read16(0x96);
    dig_P6 = (int16_t)bme280_reg_read16(0x98);
    dig_P7 = (int16_t)bme280_reg_read16(0x9A);
    dig_P8 = (int16_t)bme280_reg_read16(0x9C);
    dig_P9 = (int16_t)bme280_reg_read16(0x9E);

    // --- Humidity H1 (0xA1) ---
    dig_H1 = bme280_reg_read8(0xA1);

    // --- Humidity H2..H6 (0xE1 – 0xE7) ---
    dig_H2 = (int16_t)bme280_reg_read16(0xE1);
    dig_H3 = bme280_reg_read8(0xE3);

    uint8_t e4 = bme280_reg_read8(0xE4);
    uint8_t e5 = bme280_reg_read8(0xE5);
    uint8_t e6 = bme280_reg_read8(0xE6);

    dig_H4 = (int16_t)((e4 << 4) | (e5 & 0x0F));
    dig_H5 = (int16_t)((e6 << 4) | (e5 >> 4));
    dig_H6 = (int8_t) bme280_reg_read8(0xE7);

    // --- Configure oversampling and mode ---
    // Humidity oversampling (0xF2)
    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_WRITE);
    twi_write(0xF2);
    twi_write(0x01);  // humidity oversampling x1
    twi_stop();

    // Temperature/Pressure + normal mode (0xF4)
    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_WRITE);
    twi_write(0xF4);
    twi_write(0x27);  // temp x1, press x1, mode=normal
    twi_stop();
}

// -----------------------------------------------------------------------------
// Read raw values and apply Bosch official compensation formulas
// -----------------------------------------------------------------------------

void bme280_read(float *temperature, float *pressure, float *humidity)
{
    uint8_t p_msb, p_lsb, p_xlsb;
    uint8_t t_msb, t_lsb, t_xlsb;
    uint8_t h_msb, h_lsb;

    uint32_t raw_p, raw_t, raw_h;

    // ---------------------------------------------------------
    // Read raw block starting from 0xF7
    // ---------------------------------------------------------
    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_WRITE);
    twi_write(0xF7);

    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_READ);

    // Pressure (20-bit)
    p_msb  = twi_read(TWI_ACK);
    p_lsb  = twi_read(TWI_ACK);
    p_xlsb = twi_read(TWI_ACK);

    // Temperature (20-bit)
    t_msb  = twi_read(TWI_ACK);
    t_lsb  = twi_read(TWI_ACK);
    t_xlsb = twi_read(TWI_ACK);

    // Humidity (16-bit)
    h_msb  = twi_read(TWI_ACK);
    h_lsb  = twi_read(TWI_NACK);

    twi_stop();

    raw_p = ((uint32_t)p_msb << 12) | ((uint32_t)p_lsb << 4) | (p_xlsb >> 4);
    raw_t = ((uint32_t)t_msb << 12) | ((uint32_t)t_lsb << 4) | (t_xlsb >> 4);
    raw_h = ((uint32_t)h_msb << 8)  | (h_lsb);

    // =========================================================================
    //  TEMPERATURE COMPENSATION (Bosch official integer algorithm)
    // =========================================================================
    int32_t var1, var2, T;

    var1 = ((((int32_t)raw_t >> 3) - ((int32_t)dig_T1 << 1)) *
            (int32_t)dig_T2) >> 11;

    var2 = (((((int32_t)raw_t >> 4) - (int32_t)dig_T1) *
             (((int32_t)raw_t >> 4) - (int32_t)dig_T1)) >> 12) *
             (int32_t)dig_T3 >> 14;

    t_fine = var1 + var2;

    T = (t_fine * 5 + 128) >> 8;
    *temperature = T / 100.0f;

    // =========================================================================
    //  PRESSURE COMPENSATION (Bosch official integer algorithm)
    // =========================================================================
    int64_t p, varP1, varP2;

    varP1 = ((int64_t)t_fine) - 128000;
    varP2 = varP1 * varP1 * (int64_t)dig_P6;
    varP2 = varP2 + ((varP1 * (int64_t)dig_P5) << 17);
    varP2 = varP2 + (((int64_t)dig_P4) << 35);

    varP1 = ((varP1 * varP1 * (int64_t)dig_P3) >> 8) +
            ((varP1 * (int64_t)dig_P2) << 12);

    varP1 = (((((int64_t)1) << 47) + varP1) * (int64_t)dig_P1) >> 33;

    if (varP1 == 0)
    {
        *pressure = 0.0f;  // avoid division by zero
    }
    else
    {
        p = 1048576 - raw_p;
        p = (((p << 31) - varP2) * 3125) / varP1;

        varP1 = ((int64_t)dig_P9 * (p >> 13) * (p >> 13)) >> 25;
        varP2 = ((int64_t)dig_P8 * p) >> 19;

        p = ((p + varP1 + varP2) >> 8) + (((int64_t)dig_P7) << 4);

        *pressure = (float)p / 25600.0f; // Pa × 256 → hPa
    }

    // =========================================================================
    //  HUMIDITY COMPENSATION (Bosch official integer algorithm)
    // =========================================================================
    int32_t v_x1_u32r;

    v_x1_u32r = t_fine - ((int32_t)76800);

    v_x1_u32r =
        (((((int32_t)raw_h << 14) -
        (((int32_t)dig_H4) << 20) -
        (((int32_t)dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
        (((((((v_x1_u32r * (int32_t)dig_H6) >> 10) *
            (((v_x1_u32r * (int32_t)dig_H3) >> 11) + ((int32_t)32768))) >> 10) +
            ((int32_t)2097152)) * (int32_t)dig_H2 + 8192) >> 14);

    v_x1_u32r = v_x1_u32r -
        (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
        (int32_t)dig_H1) >> 4);

    if (v_x1_u32r < 0)
        v_x1_u32r = 0;
    if (v_x1_u32r > 419430400)
        v_x1_u32r = 419430400;

    *humidity = (float)(v_x1_u32r >> 12) / 1024.0f;
}
