#ifndef DS1302_H
#define DS1302_H

/*
 * DS1302 (3-wire RTC) driver - C API (AVR, no Arduino)
 *
 * Default pin mapping:
 *   CE  -> PORTB PB3
 *   IO  -> PORTB PB4
 *   SCLK-> PORTB PB5
 *
 * You can override DS1302_PORT / DS1302_DDR / DS1302_PIN and the pin
 * numbers by defining them before including this header.
 */

#include <stdint.h>

/* Allow user to override port/DDR/PIN at compile time */
#ifndef DS1302_PORT
#include <avr/io.h>
#define DS1302_PORT PORTC
#define DS1302_DDR  DDRC
#define DS1302_PIN  PINC
#endif

/* Default pin numbers (can be changed before include) */
#ifndef DS1302_CE_PIN
#define DS1302_CE_PIN   PC1
#endif
#ifndef DS1302_IO_PIN
#define DS1302_IO_PIN   PC2
#endif
#ifndef DS1302_SCLK_PIN
#define DS1302_SCLK_PIN PC3
#endif

/* Register/command addresses (LSB-first addressing scheme used by device) */
#define DS1302_CMD_WRITE_PROTECT 0x8E
#define DS1302_CMD_WRITE_SECONDS 0x80
#define DS1302_CMD_READ_SECONDS  (0x80 | 0x01)
#define DS1302_CMD_BURST_WRITE   0xBE
#define DS1302_CMD_BURST_READ    0xBF

/* Time structure (matches DS1302 order) */
typedef struct {
    uint8_t sec;    /* seconds (BCD) */
    uint8_t min;    /* minutes (BCD) */
    uint8_t hour;   /* hours (BCD) */
    uint8_t date;   /* day of month (BCD) */
    uint8_t month;  /* month (BCD) */
    uint8_t day;    /* day of week (BCD) */
    uint8_t year;   /* year (BCD) */
} ds1302_time_t;

/* Public API */
void ds1302_init(void);

/* Read/write a single register (command should include R/W bit). */
uint8_t ds1302_read_register(uint8_t cmd);
void ds1302_write_register(uint8_t cmd, uint8_t data);

/* Burst read/write full time (seconds,min,hour,date,month,day,year) */
void ds1302_burst_read(ds1302_time_t *t);
void ds1302_burst_write(const ds1302_time_t *t);

/* Convenience: read/write using the human-friendly functions */
void ds1302_read_time(ds1302_time_t *t);            /* returns binary-coded fields */
void ds1302_set_time(const ds1302_time_t *t);      /* expects binary values (not necessarily BCD) */

#endif /* DS1302_H */
