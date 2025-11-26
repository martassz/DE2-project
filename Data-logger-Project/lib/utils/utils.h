/*
 * utils.h
 *
 * Utility helper functions for debugging and maintenance.
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <stdint.h>

/**
 * @brief Scans the I2C bus and prints active addresses to UART.
 * * Uses the TWI library to check all addresses from 1 to 127.
 * Requires UART and TWI to be initialized before calling.
 */
void i2c_scan(void);

#endif /* UTILS_H_ */