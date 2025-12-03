/**
 * @file ds1302.c
 * @brief Minimal DS1302 driver (bit-banged) for AVR.
 *
 * Implements burst read/write and simple register access for the DS1302 RTC.
 * Uses LSB-first transfers and direct IO manipulation.
 */

#include "ds1302.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/** @brief Delay for bit-banging timing stability */
#define T_DELAY_US 1

/* Macros for pin manipulation */
#define CE_HIGH()   (DS1302_PORT |=  (1 << DS1302_CE_PIN))
#define CE_LOW()    (DS1302_PORT &= ~(1 << DS1302_CE_PIN))

#define SCLK_HIGH() (DS1302_PORT |=  (1 << DS1302_SCLK_PIN))
#define SCLK_LOW()  (DS1302_PORT &= ~(1 << DS1302_SCLK_PIN))

#define IO_HIGH()   (DS1302_PORT |=  (1 << DS1302_IO_PIN))
#define IO_LOW()    (DS1302_PORT &= ~(1 << DS1302_IO_PIN))

#define IO_OUTPUT() (DS1302_DDR  |=  (1 << DS1302_IO_PIN))
#define IO_INPUT()  (DS1302_DDR  &= ~(1 << DS1302_IO_PIN))
#define IO_READ()   ((DS1302_PIN & (1 << DS1302_IO_PIN)) ? 1 : 0)

/* Utility: convert between BCD and binary */
static inline uint8_t bcd_to_bin(uint8_t bcd) { return (bcd & 0x0F) + 10 * ((bcd >> 4) & 0x0F); }
static inline uint8_t bin_to_bcd(uint8_t bin) { return (uint8_t)((bin / 10) << 4) | (uint8_t)(bin % 10); }

/* --- Low level byte transfer (LSB first) --- */

/**
 * @brief Transmits a byte to the DS1302.
 * @param data Byte to send (LSB first).
 */
static void ds1302_write_byte(uint8_t data)
{
    IO_OUTPUT();
    for (uint8_t i = 0; i < 8; ++i) {
        if (data & 0x01) IO_HIGH();
        else              IO_LOW();

        _delay_us(T_DELAY_US);
        SCLK_HIGH();
        _delay_us(T_DELAY_US);
        SCLK_LOW();
        _delay_us(T_DELAY_US);

        data >>= 1;
    }
}

/**
 * @brief Reads a byte from the DS1302.
 * @return Byte received (LSB first).
 */
static uint8_t ds1302_read_byte(void)
{
    uint8_t data = 0;
    IO_INPUT();
    for (uint8_t i = 0; i < 8; ++i) {
        if (IO_READ()) {
            data |= (1 << i);
        }
        _delay_us(T_DELAY_US);
        SCLK_HIGH();
        _delay_us(T_DELAY_US);
        SCLK_LOW();
        _delay_us(T_DELAY_US);
    }
    return data;
}

void ds1302_write_register(uint8_t cmd, uint8_t data)
{
    CE_LOW();
    SCLK_LOW();
    _delay_us(T_DELAY_US);

    CE_HIGH();
    _delay_us(T_DELAY_US);

    ds1302_write_byte(cmd);
    ds1302_write_byte(data);

    CE_LOW();
    _delay_us(T_DELAY_US);
}

uint8_t ds1302_read_register(uint8_t cmd)
{
    uint8_t val;
    CE_LOW();
    SCLK_LOW();
    _delay_us(T_DELAY_US);

    CE_HIGH();
    _delay_us(T_DELAY_US);

    ds1302_write_byte(cmd);
    val = ds1302_read_byte();

    CE_LOW();
    _delay_us(T_DELAY_US);
    return val;
}

void ds1302_burst_read(ds1302_time_t *t)
{
    CE_LOW();
    SCLK_LOW();
    _delay_us(T_DELAY_US);

    CE_HIGH();
    _delay_us(T_DELAY_US);

    ds1302_write_byte(DS1302_CMD_BURST_READ);

    // Read time registers in sequence
    t->sec   = ds1302_read_byte();
    t->min   = ds1302_read_byte();
    t->hour  = ds1302_read_byte();
    t->date  = ds1302_read_byte();
    t->month = ds1302_read_byte();
    t->day   = ds1302_read_byte();
    t->year  = ds1302_read_byte();
    // Read control register (ignored)
    uint8_t ctrl = ds1302_read_byte();
    (void)ctrl;

    CE_LOW();
    _delay_us(T_DELAY_US);
}

void ds1302_burst_write(const ds1302_time_t *t)
{
    CE_LOW();
    SCLK_LOW();
    _delay_us(T_DELAY_US);

    CE_HIGH();
    _delay_us(T_DELAY_US);

    ds1302_write_byte(DS1302_CMD_BURST_WRITE);

    ds1302_write_byte(t->sec);
    ds1302_write_byte(t->min);
    ds1302_write_byte(t->hour);
    ds1302_write_byte(t->date);
    ds1302_write_byte(t->month);
    ds1302_write_byte(t->day);
    ds1302_write_byte(t->year);
    ds1302_write_byte(0x00); // Control register (WP off)

    CE_LOW();
    _delay_us(T_DELAY_US);
}

void ds1302_init(void)
{
    // Set Control pins as Output
    DS1302_DDR |= (1 << DS1302_CE_PIN) | (1 << DS1302_SCLK_PIN);

    CE_LOW();
    SCLK_LOW();
    IO_INPUT();

    // Disable Write Protect
    ds1302_write_register(DS1302_CMD_WRITE_PROTECT, 0x00);

    // Check and clear Clock Halt (CH) bit if set
    uint8_t sec = ds1302_read_register(DS1302_CMD_READ_SECONDS);
    if (sec & 0x80) {
        ds1302_write_register(DS1302_CMD_WRITE_SECONDS, sec & 0x7F);
    }
}

void ds1302_read_time(ds1302_time_t *out)
{
    ds1302_burst_read(out);
}

void ds1302_set_time(const ds1302_time_t *in)
{
    ds1302_time_t t;

    // Convert decimal input to BCD
    t.sec   = bin_to_bcd(in->sec) & 0x7F;
    t.min   = bin_to_bcd(in->min);
    t.hour  = bin_to_bcd(in->hour);
    t.date  = bin_to_bcd(in->date ? in->date : 1);
    t.month = bin_to_bcd(in->month ? in->month : 1);
    t.day   = bin_to_bcd(in->day ? in->day : 1);
    t.year  = bin_to_bcd(in->year ? in->year : 0);

    ds1302_write_register(DS1302_CMD_WRITE_PROTECT, 0x00);
    ds1302_burst_write(&t);
}