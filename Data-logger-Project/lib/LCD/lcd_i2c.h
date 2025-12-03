/**
 * @file lcd_i2c.h
 * @brief Driver interface for I2C Character LCD (using PCF8574).
 *
 * Implements functions to control standard HD44780 LCDs connected via
 * an I2C I/O expander backpack.
 *
 * @author Team DE2-Project
 * @date 2025
 */

#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <avr/io.h>
#include <util/delay.h>

/**
 * @brief I2C Address of the display backpack.
 * Common values are 0x27 or 0x3F.
 */
#define LCD_ADDR 0x27

/** @brief Display width (characters per line). */
#define LCD_COLS 16
/** @brief Display height (number of lines). */
#define LCD_ROWS 2

/**
 * @brief Initialize the LCD via I2C.
 * Configures the I2C bus and runs the HD44780 initialization sequence (4-bit mode).
 */
void lcd_i2c_init(void);

/**
 * @brief Clear the display content.
 */
void lcd_i2c_clrscr(void);

/**
 * @brief Set the cursor position.
 * @param col Column index (0 to 15).
 * @param row Row index (0 to 1).
 */
void lcd_i2c_gotoxy(uint8_t col, uint8_t row);

/**
 * @brief Print a single character to the display.
 * @param c Character to print.
 */
void lcd_i2c_putc(char c);

/**
 * @brief Print a null-terminated string to the display.
 * @param s Pointer to the string.
 */
void lcd_i2c_puts(const char* s);

#endif /* LCD_I2C_H */