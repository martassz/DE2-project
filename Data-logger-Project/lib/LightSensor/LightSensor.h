/**
 * @file LightSensor.h
 * @brief Driver interface for the analog Photoresistor module.
 * @addtogroup drivers
 * @{
 */

#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <stdint.h>

/**
 * @brief Initializes the ADC for reading a photoresistor.
 *
 * @param pin ADC channel number (0–5) depending on wiring.
 */
void lightSensor_init(uint8_t pin);

/**
 * @brief Reads the raw ADC value.
 *
 * @return Raw ADC value in the range 0–1023.
 */
uint16_t lightSensor_readRaw(void);

/**
 * @brief Sets calibration limits for converting raw ADC data to a percentage.
 *
 * @param minValue ADC value measured in complete darkness.
 * @param maxValue ADC value measured under maximum light.
 */
void lightSensor_setCalibration(uint16_t minValue, uint16_t maxValue);

/**
 * @brief Returns calibrated light intensity in percent.
 *
 * @return Light level from 0% (dark) to 100% (bright).
 */
uint16_t lightSensor_readCalibrated(void);

#endif  /* LIGHTSENSOR_H */

/** @} */