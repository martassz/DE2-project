#ifndef BME280_H
#define BME280_H

#include <stdint.h>

/**
 * @brief I2C address of the BME280 sensor.
 *        0x76 if SDO is pulled to GND.
 *        0x77 if SDO is pulled to VCC.
 */
#define BME280_I2C_ADDR 0x76

/**
 * @brief Initialize the BME280 sensor.
 *
 * This:
 *  - reads all calibration registers from sensor memory,
 *  - configures oversampling for T/P/H (x1),
 *  - enables "normal" measurement mode.
 *
 * Must be called once before any data read operation.
 */
void bme280_init(void);

/**
 * @brief Read compensated temperature, pressure and humidity values.
 *
 * The function:
 *  - reads raw sensor registers,
 *  - applies Bosch official integer compensation formulas,
 *  - returns physical values:
 *      - temperature in °C
 *      - pressure in hPa
 *      - humidity in %RH
 *
 * @param temperature  Output: °C
 * @param pressure     Output: hPa
 * @param humidity     Output: %RH
 */
void bme280_read(float *temperature, float *pressure, float *humidity);

#endif // BME280_H
