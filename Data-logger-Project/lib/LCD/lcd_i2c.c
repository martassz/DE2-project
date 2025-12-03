/**
 * @file lcd_i2c.c
 * @brief I2C LCD Driver using PCF8574.
 *
 * Implements initialization and control of HD44780-based LCDs via I2C backpack.
 */

#include "lcd_i2c.h"
#include "../twi/twi.h"

/* PCF8574 Pin Definitions */
/* Standard backpack mapping: P0=RS, P1=RW, P2=EN, P3=BL, P4-P7=Data(D4-D7) */
#define LCD_RS_BIT 0x01
#define LCD_RW_BIT 0x02
#define LCD_EN_BIT 0x04
#define LCD_BL_BIT 0x08

/** Global variable to store current backlight state (ON/OFF) */
uint8_t _backlight_val = LCD_BL_BIT;

/**
 * @brief Internal function to send a byte to the PCF8574 expander.
 * @param val Byte to be written to the port.
 */
static void i2c_send_byte(uint8_t val)
{
    twi_start();
    twi_write(LCD_ADDR << 1);        // Address + Write bit (0)
    twi_write(val | _backlight_val); // Send data combined with backlight status
    twi_stop();
}

/**
 * @brief Pulse the Enable bit to latch data into LCD controller.
 * @param val Data currently on the bus.
 */
static void lcd_pulse_enable(uint8_t val)
{
    i2c_send_byte(val | LCD_EN_BIT);  // Enable High
    _delay_us(1);                     // Short delay
    i2c_send_byte(val & ~LCD_EN_BIT); // Enable Low
    _delay_us(50);                    // Execution time
}

/**
 * @brief Write 4 bits (nibble) to the LCD.
 */
static void lcd_write_4bit(uint8_t val)
{
    i2c_send_byte(val);
    lcd_pulse_enable(val);
}

/**
 * @brief Send a full byte to LCD (split into two nibbles).
 * @param value Byte to send.
 * @param mode 0 = Instruction, 1 = Data.
 */
static void lcd_send(uint8_t value, uint8_t mode)
{
    uint8_t high_nibble = value & 0xF0;
    uint8_t low_nibble = (value << 4) & 0xF0;
    uint8_t rs_bit = (mode == 1) ? LCD_RS_BIT : 0;

    // Send high nibble
    i2c_send_byte(high_nibble | rs_bit | _backlight_val);
    lcd_pulse_enable(high_nibble | rs_bit | _backlight_val);

    // Send low nibble
    i2c_send_byte(low_nibble | rs_bit | _backlight_val);
    lcd_pulse_enable(low_nibble | rs_bit | _backlight_val);
}

void lcd_i2c_init(void)
{
    twi_init();    // Initialize I2C bus
    _delay_ms(50); // Wait for power stabilization

    // Initialization sequence for 4-bit mode (HD44780 standard)
    lcd_write_4bit(0x30);
    _delay_ms(5);
    lcd_write_4bit(0x30);
    _delay_us(150);
    lcd_write_4bit(0x30);
    
    // Switch to 4-bit mode
    lcd_write_4bit(0x20);

    // 4-bit, 2 lines, 5x8 font
    lcd_send(0x28, 0);
    // Display Off
    lcd_send(0x08, 0);
    // Clear Display
    lcd_send(0x01, 0);
    _delay_ms(2);
    // Entry Mode Set
    lcd_send(0x06, 0);
    // Display On
    lcd_send(0x0C, 0);
}

void lcd_i2c_clrscr(void)
{
    lcd_send(0x01, 0); // Clear display command
    _delay_ms(2);
}

void lcd_i2c_gotoxy(uint8_t col, uint8_t row)
{
    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    lcd_send(0x80 | (col + row_offsets[row]), 0);
}

void lcd_i2c_putc(char c)
{
    lcd_send((uint8_t)c, 1);
}

void lcd_i2c_puts(const char* s)
{
    while (*s) {
        lcd_i2c_putc(*s++);
    }
}