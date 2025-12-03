/**
 * @file bme280.h
 * @brief Driver interface for BME280 Environmental Sensor.
 *
 * Provides functions to initialize the sensor and read compensated data.
 * Communication is handled via I2C (TWI).
 * @defgroup drivers Sensor Drivers
 * @brief Hardware abstraction layer for external sensors.
 * @{
 */

#ifndef BME280_H
#define BME280_H

#include <stdint.h>

/**
 * @brief I2C address of the BME280 sensor.
 * - 0x76: SDO pin connected to GND.
 * - 0x77: SDO pin connected to VCC.
 */
#define BME280_I2C_ADDR 0x76

/**
 * @brief Initialize the BME280 sensor.
 *
 * Performs the following:
 * - Reads factory calibration parameters from the sensor's ROM.
 * - Configures oversampling settings for Humidity, Temperature, and Pressure.
 * - Sets the sensor mode to Normal.
 */
void bme280_init(void);

/**
 * @brief Read and compensate sensor data.
 *
 * Reads raw ADC values from the sensor and applies the Bosch compensation formulas
 * using the stored calibration parameters.
 *
 * @param[out] temperature Pointer to float variable for Temperature [°C].
 * @param[out] pressure    Pointer to float variable for Pressure [hPa].
 * @param[out] humidity    Pointer to float variable for Humidity [%RH].
 */
void bme280_read(float *temperature, float *pressure, float *humidity);

#endif // BME280_H

/** @} */