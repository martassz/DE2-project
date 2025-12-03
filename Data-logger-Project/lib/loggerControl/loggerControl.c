/**
 * @file loggerControl.c
 * @brief User Interface Logic and Control.
 *
 * Handles the rotary encoder input using a state machine for debouncing
 * and manages the content displayed on the I2C LCD.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>

#include "loggerControl.h"
#include "lcd_i2c.h"
#include "twi.h"
#include "ds1302.h"
#include "sdlog.h"

/* --- Encoder Pin Configuration (PORTD) --- */
#define ENC_SW   PD7  /**< Encoder button pin */
#define ENC_DT   PD6  /**< Encoder DT pin */
#define ENC_CLK  PD5  /**< Encoder CLK pin */

#define ENC_PORT_REG  PORTD
#define ENC_DDR_REG   DDRD
#define ENC_PIN_REG   PIND

/* --- RTC Address Definitions --- */
#define RTC_ADR     0x68
#define RTC_SEC_MEM 0x00

/* --- External Variables --- */
extern volatile uint32_t g_millis; /**< System time from main.c used for debounce */

/* --- Encoder State Machine Variables --- */
/**
 * @brief Gray code state transition table.
 * Valid transitions return 1 or -1, invalid return 0.
 */
static const int8_t encoder_table[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};

static uint8_t old_AB = 0;         /**< Previous state of CLK and DT pins */
static int8_t enc_counter = 0;     /**< Accumulated steps */
static uint32_t last_btn_time = 0; /**< Timestamp of last button press */

/* --- Display Global Variables --- */
/** Current value displayed on LCD (0=Temp, 1=Pressure, 2=Humidity, 3=Light) */
volatile uint8_t lcdValue = 0;
/** Flag indicating that LCD needs to be redrawn */
volatile uint8_t flag_update_lcd = 0;

/* Helper: BCD to Decimal conversion */
static uint8_t bcd2dec(uint8_t v) { return ((v>>4)*10 + (v & 0x0F)); }

/* ==========================================
 * Initialization
 * ========================================== */

void logger_display_init(void)
{
    lcd_i2c_init();
    lcd_i2c_clrscr();

    lcd_i2c_gotoxy(0,0);
    lcd_i2c_puts("  DATA LOGGER  ");
    lcd_i2c_gotoxy(0,1);
    lcd_i2c_puts("   VUT FEKT    ");
}

void logger_encoder_init(void)
{
    // Set pins as Input (0)
    ENC_DDR_REG  &= ~((1 << ENC_CLK) | (1 << ENC_DT) | (1 << ENC_SW));
    // Enable internal Pull-ups (1)
    ENC_PORT_REG |=  ((1 << ENC_CLK) | (1 << ENC_DT) | (1 << ENC_SW));

    // Read initial state
    uint8_t clk = (ENC_PIN_REG & (1 << ENC_CLK)) ? 1 : 0;
    uint8_t dt  = (ENC_PIN_REG & (1 << ENC_DT))  ? 1 : 0;

    // Store initial state (bit 1 = CLK, bit 0 = DT)
    old_AB = (clk << 1) | dt;
}

/* ==========================================
 * Polling Routine
 * ========================================== */

void logger_encoder_poll(void)
{
    // --- 1. ROTATION HANDLING (Table Method) ---

    // Read current pin states
    uint8_t clk = (ENC_PIN_REG & (1 << ENC_CLK)) ? 1 : 0;
    uint8_t dt  = (ENC_PIN_REG & (1 << ENC_DT))  ? 1 : 0;

    // Construct new state (0-3)
    uint8_t current_AB = (clk << 1) | dt;

    // Only process if state changed
    if (current_AB != (old_AB & 0x03)) {

        // Create index for the table: (Old_State << 2) | New_State
        old_AB <<= 2;
        old_AB |= current_AB;

        // Add movement from table
        enc_counter += encoder_table[old_AB & 0x0F];

        // Sensitivity threshold (KY-040 usually has 4 steps per click)
        if (enc_counter >= 4) {
            // CLOCKWISE (Next Screen)
            lcdValue++;
            if (lcdValue > 3) lcdValue = 0;

            flag_update_lcd = 1;
            enc_counter = 0;
        }
        else if (enc_counter <= -4) {
            // COUNTER-CLOCKWISE (Previous Screen)
            if (lcdValue == 0) lcdValue = 3;
            else lcdValue--;

            flag_update_lcd = 1;
            enc_counter = 0;
        }
    }
    // Save state for next pass (mask to 2 bits)
    old_AB &= 0x03;

    // --- 2. BUTTON HANDLING (Debounced) ---

    if ((ENC_PIN_REG & (1 << ENC_SW)) == 0) {
        // Button pressed (Active Low)
        uint32_t now = g_millis;

        // Debounce: accept input only once per 250 ms
        if ((now - last_btn_time) > 250) {
            flag_sd_toggle = 1;      // Request start/stop logging
            flag_update_lcd = 1;     // Request redraw (to update icon)
            last_btn_time = now;
        }
    }
}

/* ==========================================
 * LCD Drawing
 * ========================================== */

void logger_rtc_read_time(void)
{
    uint8_t buf[3];
    // Read 3 bytes (sec, min, hour) from RTC via I2C
    twi_readfrom_mem_into(RTC_ADR, RTC_SEC_MEM, buf, 3);

    uint8_t sec  = bcd2dec(buf[0] & 0x7F);
    uint8_t min  = bcd2dec(buf[1]);
    uint8_t hour = bcd2dec(buf[2] & 0x3F);

    // Atomic write to global structure
    uint8_t sreg = SREG; cli();
    g_time.hh = hour;
    g_time.mm = min;
    g_time.ss = sec;
    SREG = sreg;
}

void logger_display_draw(void)
{
    // Clear flag
    flag_update_lcd = 0;

    // --- LINE 1: Header + Time + SD Icon ---
    lcd_i2c_gotoxy(0,0);

    char sd_icon = sd_logging ? '*' : ' ';
    char timeStr[9];
    snprintf(timeStr, 9, "%02d:%02d:%02d", g_time.hh, g_time.mm, g_time.ss);

    // Display quantity name
    switch (lcdValue) {
        case 0: lcd_i2c_puts("TEMP   "); break;
        case 1: lcd_i2c_puts("PRESS  "); break;
        case 2: lcd_i2c_puts("HUMID  "); break;
        case 3: lcd_i2c_puts("LIGHT  "); break;
    }

    // Display SD icon and time
    lcd_i2c_putc(sd_icon);
    lcd_i2c_puts(timeStr);

    // --- LINE 2: Value + Unit ---
    lcd_i2c_gotoxy(0,1);
    char valStr[16];

    switch (lcdValue)
    {
        case 0: // Temperature
            dtostrf(g_T, 6, 1, valStr);
            lcd_i2c_puts(valStr);
            lcd_i2c_puts(" \xDF""C   "); // \xDF is degree symbol on HD44780
            break;

        case 1: // Pressure
            dtostrf(g_P, 7, 1, valStr);
            lcd_i2c_puts(valStr);
            lcd_i2c_puts(" hPa  ");
            break;

        case 2: // Humidity
            dtostrf(g_H, 6, 1, valStr);
            lcd_i2c_puts(valStr);
            lcd_i2c_puts(" %    ");
            break;

        case 3: // Light (raw/percent)
            sprintf(valStr, "%u", g_Light);
            lcd_i2c_puts(valStr);
            lcd_i2c_puts(" %      ");
            break;

        default:
            lcd_i2c_puts("Error           ");
            break;
    }
}