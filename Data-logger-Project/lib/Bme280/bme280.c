/**
 * @file bme280.c
 * @brief Driver for BME280 Temperature, Humidity and Pressure Sensor.
 *
 * Implements I2C communication and integer compensation formulas
 * provided by Bosch Sensortec.
 */

#include <avr/io.h>
#include <twi.h>
#include "bme280.h"

// -----------------------------------------------------------------------------
// Calibration data (Compensation parameters)
// -----------------------------------------------------------------------------
static int32_t t_fine;

// Temperature calibration
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;

// Pressure calibration
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;

// Humidity calibration
static uint8_t  dig_H1, dig_H3;
static int16_t  dig_H2, dig_H4, dig_H5;
static int8_t   dig_H6;

// -----------------------------------------------------------------------------
// Low-level register access
// -----------------------------------------------------------------------------

/**
 * @brief Reads an 8-bit value from a specific register.
 * @param reg Register address.
 * @return Value read from the register.
 */
static uint8_t bme280_reg_read8(uint8_t reg)
{
    uint8_t val;
    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_WRITE);
    twi_write(reg);
    twi_start(); // repeated start
    twi_write((BME280_I2C_ADDR << 1) | TWI_READ);
    val = twi_read(TWI_NACK);
    twi_stop();
    return val;
}

/**
 * @brief Reads a 16-bit value (Little Endian) from two consecutive registers.
 * @param reg Starting register address.
 * @return 16-bit unsigned value.
 */
static uint16_t bme280_reg_read16(uint8_t reg)
{
    uint8_t lsb, msb;
    lsb = bme280_reg_read8(reg);
    msb = bme280_reg_read8(reg + 1);
    return (uint16_t)((msb << 8) | lsb);
}

static int16_t bme280_reg_readS16(uint8_t reg)
{
    return (int16_t)bme280_reg_read16(reg);
}

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------
void bme280_init(void)
{
    // --- Read Trimming Parameters ---
    // Temperature calibration 0x88..0x8D
    dig_T1 = bme280_reg_read16(0x88);
    dig_T2 = bme280_reg_readS16(0x8A);
    dig_T3 = bme280_reg_readS16(0x8C);

    // Pressure calibration 0x8E..0x9F
    dig_P1 = bme280_reg_read16(0x8E);
    dig_P2 = bme280_reg_readS16(0x90);
    dig_P3 = bme280_reg_readS16(0x92);
    dig_P4 = bme280_reg_readS16(0x94);
    dig_P5 = bme280_reg_readS16(0x96);
    dig_P6 = bme280_reg_readS16(0x98);
    dig_P7 = bme280_reg_readS16(0x9A);
    dig_P8 = bme280_reg_readS16(0x9C);
    dig_P9 = bme280_reg_readS16(0x9E);

    // Humidity calibration
    dig_H1 = bme280_reg_read8(0xA1);
    dig_H2 = bme280_reg_readS16(0xE1);
    dig_H3 = bme280_reg_read8(0xE3);

    uint8_t e4 = bme280_reg_read8(0xE4);
    uint8_t e5 = bme280_reg_read8(0xE5);
    uint8_t e6 = bme280_reg_read8(0xE6);

    dig_H4 = (int16_t)((e4 << 4) | (e5 & 0x0F));
    dig_H5 = (int16_t)((e6 << 4) | (e5 >> 4));
    dig_H6 = (int8_t)bme280_reg_read8(0xE7);

    // --- Configure Sensor Settings ---
    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_WRITE);
    twi_write(0xF2); // ctrl_hum
    twi_write(0x01); // Humidity oversampling x1
    twi_stop();

    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_WRITE);
    twi_write(0xF4); // ctrl_meas
    twi_write(0x27); // Temp x1, Pressure x1, Mode Normal
    twi_stop();
}

// -----------------------------------------------------------------------------
// Read sensor and apply compensation
// -----------------------------------------------------------------------------
void bme280_read(float *temperature, float *pressure, float *humidity)
{
    uint8_t data[8];

    // Burst read 0xF7..0xFE (pressure, temp, humidity)
    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_WRITE);
    twi_write(0xF7);
    twi_start();
    twi_write((BME280_I2C_ADDR << 1) | TWI_READ);

    for (uint8_t i = 0; i < 7; i++)
        data[i] = (i == 6) ? twi_read(TWI_NACK) : twi_read(TWI_ACK);

    twi_stop();

    // Construct raw values
    uint32_t raw_p = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | (data[2] >> 4);
    uint32_t raw_t = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | (data[5] >> 4);
    uint32_t raw_h = ((uint32_t)data[6] << 8) | data[7];

    // ----- Temperature compensation -----
    int32_t var1 = ((((int32_t)raw_t >> 3) - ((int32_t)dig_T1 << 1)) * (int32_t)dig_T2) >> 11;
    int32_t var2 = (((((int32_t)raw_t >> 4) - (int32_t)dig_T1) * (((int32_t)raw_t >> 4) - (int32_t)dig_T1)) >> 12) * (int32_t)dig_T3 >> 14;
    t_fine = var1 + var2;
    int32_t T = (t_fine * 5 + 128) >> 8;
    *temperature = T / 100.0f;

    // ----- Pressure compensation -----
    int64_t varP1 = (int64_t)t_fine - 128000;
    int64_t varP2 = varP1 * varP1 * (int64_t)dig_P6;
    varP2 += ((varP1 * (int64_t)dig_P5) << 17);
    varP2 += ((int64_t)dig_P4 << 35);
    varP1 = ((varP1 * varP1 * (int64_t)dig_P3) >> 8) + ((varP1 * (int64_t)dig_P2) << 12);
    varP1 = ((((int64_t)1 << 47) + varP1) * (int64_t)dig_P1) >> 33;

    int64_t p;
    if (varP1 == 0)
        p = 0;
    else
    {
        p = 1048576 - raw_p;
        p = (((p << 31) - varP2) * 3125) / varP1;
        varP1 = ((int64_t)dig_P9 * (p >> 13) * (p >> 13)) >> 25;
        varP2 = ((int64_t)dig_P8 * p) >> 19;
        p = ((p + varP1 + varP2) >> 8) + ((int64_t)dig_P7 << 4);
    }
    *pressure = (float)p / 25600.0f; // hPa

    // ----- Humidity compensation -----
    int32_t v_x1 = t_fine - 76800;
    v_x1 = (((((raw_h << 14) - ((int32_t)dig_H4 << 20) - ((int32_t)dig_H5 * v_x1)) + 16384) >> 15) *
           (((((((v_x1 * (int32_t)dig_H6) >> 10) * (((v_x1 * (int32_t)dig_H3) >> 11) + 32768)) >> 10) + 2097152) * dig_H2 + 8192) >> 14));
    v_x1 = v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7) * dig_H1) >> 4);
    if (v_x1 < 0) v_x1 = 0;
    if (v_x1 > 419430400) v_x1 = 419430400;
    *humidity = (float)(v_x1 >> 12) / 1024.0f;
}