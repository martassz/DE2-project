/**
 * @file ds1302.h
 * @brief Driver interface for DS1302 Real Time Clock.
 *
 * Provides functions to set and read time using a 3-wire interface (bit-banged).
 *
 * @author Team DE2-Project
 * @date 2025
 */

#ifndef DS1302_H
#define DS1302_H

#include <stdint.h>

/* Allow user to override port/DDR/PIN at compile time */
#ifndef DS1302_PORT
#include <avr/io.h>
/** @brief Port register for DS1302 communication. */
#define DS1302_PORT PORTB
/** @brief Data Direction Register for DS1302. */
#define DS1302_DDR  DDRB
/** @brief Input Pin Register for DS1302. */
#define DS1302_PIN  PINB
#endif

/* Default pin numbers */
#ifndef DS1302_CE_PIN
#define DS1302_CE_PIN   PB2 /**< Chip Enable Pin */
#endif
#ifndef DS1302_IO_PIN
#define DS1302_IO_PIN   PB1 /**< I/O Data Pin */
#endif
#ifndef DS1302_SCLK_PIN
#define DS1302_SCLK_PIN PB0 /**< Serial Clock Pin */
#endif

/* Command Definitions */
#define DS1302_CMD_WRITE_PROTECT 0x8E
#define DS1302_CMD_WRITE_SECONDS 0x80
#define DS1302_CMD_READ_SECONDS  (0x80 | 0x01)
#define DS1302_CMD_BURST_WRITE   0xBE
#define DS1302_CMD_BURST_READ    0xBF

/**
 * @brief Structure holding the time and date.
 * Fields are typically stored in BCD format inside the chip,
 * but API functions may handle conversion.
 */
typedef struct {
    uint8_t sec;    /**< Seconds (0-59) */
    uint8_t min;    /**< Minutes (0-59) */
    uint8_t hour;   /**< Hours (0-23, 24h mode assumed) */
    uint8_t date;   /**< Day of month (1-31) */
    uint8_t month;  /**< Month (1-12) */
    uint8_t day;    /**< Day of week (1-7) */
    uint8_t year;   /**< Year (00-99) */
} ds1302_time_t;

/**
 * @brief Initialize the DS1302 driver.
 * Configures GPIO pins and disables write protection on the RTC.
 */
void ds1302_init(void);

/**
 * @brief Read a single byte from a specific register.
 * @param cmd Command byte (address + Read/Write bit).
 * @return Data read from the register.
 */
uint8_t ds1302_read_register(uint8_t cmd);

/**
 * @brief Write a single byte to a specific register.
 * @param cmd Command byte (address + Read/Write bit).
 * @param data Data to write.
 */
void ds1302_write_register(uint8_t cmd, uint8_t data);

/**
 * @brief Read all time/date registers in burst mode.
 * @param[out] t Pointer to the structure where time will be stored.
 */
void ds1302_burst_read(ds1302_time_t *t);

/**
 * @brief Write all time/date registers in burst mode.
 * @param[in] t Pointer to the structure containing the time to set.
 */
void ds1302_burst_write(const ds1302_time_t *t);

/**
 * @brief Convenience function: Read current time.
 * @param[out] t Pointer to structure to hold the read time.
 */
void ds1302_read_time(ds1302_time_t *t);

/**
 * @brief Convenience function: Set the current time.
 *
 * Automatically handles Write Protection disable/enable and Binary/BCD conversion if implemented.
 * @param[in] t Pointer to structure containing the time to set.
 */
void ds1302_set_time(const ds1302_time_t *t);

#endif /* DS1302_H */