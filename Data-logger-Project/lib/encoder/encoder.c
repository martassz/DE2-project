#include "encoder.h"
#include "sdlog.h"    /* to set flag_sd_toggle when button pressed */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <lcd.h>        // Peter Fleury LCD library
#include "timer.h"      // Fryza Timer utilities (1ms overflow)

/*
 * This module relies on the following global variable defined in main.c:
 *   volatile uint32_t g_millis;
 * which is incremented every ~1 ms by Timer0 overflow ISR.
 */
extern volatile uint32_t g_millis;

/* Internal module state */
volatile uint8_t enc_lcdValue = 0;        // 0 = Temp, 1 = Pressure, 2 = Humidity
volatile uint8_t enc_flag_update_lcd = 0; // set by ISR or encoder poll

static volatile float enc_T = 0.0f;
static volatile float enc_P = 0.0f;
static volatile float enc_H = 0.0f;

/* Encoder debounce and state tracking */
static uint8_t enc_lastStateCLK = 0;
static uint32_t enc_lastButtonPress = 0;

/* Safe atomic read of g_millis */
static uint32_t enc_millis(void)
{
    uint32_t t;
    uint8_t sreg = SREG;
    cli();
    t = g_millis;
    SREG = sreg;
    return t;
}

/* ------------------------- PUBLIC API ------------------------- */

void encoder_init(void)
{
    /* Initialize LCD (simple splash screen) */
    lcd_init(LCD_DISP_ON);
    lcd_clrscr();
    lcd_gotoxy(0,0);
    lcd_puts("Env. logger");
    lcd_gotoxy(0,1);
    lcd_puts("Starting...");

    /* Configure encoder pins: inputs + internal pull-ups */
    ENC_DDR_REG &= ~((1 << ENC_CLK) | (1 << ENC_DT) | (1 << ENC_SW));
    ENC_PORT_REG |= ((1 << ENC_CLK) | (1 << ENC_DT) | (1 << ENC_SW));

    /* Read initial state of CLK */
    enc_lastStateCLK = (ENC_PIN_REG & (1 << ENC_CLK)) ? 1 : 0;

    enc_flag_update_lcd = 1;   // request initial redraw
}

void encoder_timer_init(void)
{
    /* Configure Timer2 for ~16 ms overflow and enable its interrupt.
       The ISR is implemented below in this file. */
    tim2_ovf_16ms();
    tim2_ovf_enable();
}

/* Must be called frequently from main loop for responsive UI */
void encoder_poll(void)
{
    uint8_t currentStateCLK = (ENC_PIN_REG & (1 << ENC_CLK)) ? 1 : 0;

    if (currentStateCLK != enc_lastStateCLK)
    {
        uint8_t dt = (ENC_PIN_REG & (1 << ENC_DT)) ? 1 : 0;
        if (dt != currentStateCLK)
        {
            /* counter-clockwise */
            if (enc_lcdValue == 0) enc_lcdValue = 2;
            else enc_lcdValue--;
        }
        else
        {
            /* clockwise */
            enc_lcdValue++;
            if (enc_lcdValue > 2) enc_lcdValue = 0;
        }
        enc_flag_update_lcd = 1;
    }
    enc_lastStateCLK = currentStateCLK;

    /* Button (active LOW) - debounced; now triggers SD toggle request */
    if ( (ENC_PIN_REG & (1 << ENC_SW)) == 0 )
    {
        uint32_t now = enc_millis();
        if (now - enc_lastButtonPress > 50)
        {
            /* Short press: request SD start/stop and refresh LCD */
            flag_sd_toggle = 1;       /* request start/stop logging */
            enc_flag_update_lcd = 1;  /* also refresh LCD so user sees state */
            enc_lastButtonPress = now;
        }
    }
}

/* Update displayed sensor values (atomic enough for floats on AVR) */
void encoder_set_values(float T, float P, float H)
{
    uint8_t sreg = SREG;
    cli();
    enc_T = T;
    enc_P = P;
    enc_H = H;
    SREG = sreg;
}

/* Force display refresh */
void encoder_request_redraw(void)
{
    enc_flag_update_lcd = 1;
}

/* Draw to LCD only if needed */
void encoder_draw_if_needed(void)
{
    if (!enc_flag_update_lcd) return;

    char buf[16];
    uint8_t i;

    /* Write label on first row */
    lcd_gotoxy(0,0);
    switch (enc_lcdValue)
    {
        case 0: lcd_puts("Temperature:   "); break;
        case 1: lcd_puts("Pressure:      "); break;
        case 2: lcd_puts("Humidity:      "); break;
        default: lcd_puts("Unknown        "); break;
    }

    /* Clear second row */
    lcd_gotoxy(0,1);
    for (i = 0; i < 16; i++) lcd_putc(' ');
    lcd_gotoxy(0,1);

    /* Read sensor values atomically */
    float T, P, H;
    uint8_t sreg = SREG;
    cli();
    T = enc_T;
    P = enc_P;
    H = enc_H;
    SREG = sreg;

    /* Format and print the selected value */
    switch (enc_lcdValue)
    {
        case 0:
            dtostrf((double)T, 5, 1, buf);
            lcd_puts(buf);
            lcd_puts(" C");
            break;

        case 1:
            dtostrf((double)P, 6, 1, buf);
            lcd_puts(buf);
            lcd_puts(" hPa");
            break;

        case 2:
            dtostrf((double)H, 5, 1, buf);
            lcd_puts(buf);
            lcd_puts(" %");
            break;

        default:
            lcd_puts("N/A");
            break;
    }

    enc_flag_update_lcd = 0;
}

uint8_t encoder_get_selected(void)
{
    return enc_lcdValue;
}

/* ------------------------- Timer2 ISR -------------------------
 * Generates a ~1 second event by counting ~62 overflows (~16 ms each).
 */
ISR(TIMER2_OVF_vect)
{
    static uint8_t n_ovfs = 0;
    n_ovfs++;

    if (n_ovfs >= 62)      // ≈1 second
    {
        n_ovfs = 0;
        enc_flag_update_lcd = 1;   // request LCD update
    }
}
