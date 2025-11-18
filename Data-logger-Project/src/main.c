#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uart.h>      // Fleury UART
#include <twi.h>       // Fryza TWI (I2C)
#include <gpio.h>      // GPIO library for AVR-GCC
#include "timer.h"     // Fryza Timer utilities (1ms overflow)
#include "bme280.h"    // Our BME280 driver
/* Note: lcd.h is used inside encoder.c, no need to include it here unless main uses it */
#include "encoder.h"   // Our KY040 Encoder driver
#include "sdlog.h"

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

/* ----------------------------------------------------
   Global software time (milliseconds)
   ---------------------------------------------------- */
volatile uint32_t g_millis = 0;

ISR(TIMER0_OVF_vect)
{
    g_millis++;    /* called every ~1 ms */
}

/* Thread-safe access to g_millis */
static uint32_t millis(void)
{
    uint32_t t;
    uint8_t sreg = SREG;
    cli();
    t = g_millis;
    SREG = sreg;
    return t;
}

/* Simple println wrapper */
static void uart_println(const char *s)
{
    uart_puts(s);
    uart_puts("\r\n");
}

/* ----------------------------------------------------
   Main application (BME280 logger)
   ---------------------------------------------------- */
int main(void)
{
    /* UART 9600 baud (works reliably with ATmega328P/16 MHz) */
    uart_init(UART_BAUD_SELECT(9600, F_CPU));

    /* Initialize I2C/TWI */
    twi_init();

    /* Timer0 overflow every ~1 ms */
    tim0_ovf_1ms();
    tim0_ovf_enable();

    /* Initialize display/encoder module and its timer (Timer2) */
    encoder_init();        /* initializes minimal LCD and encoder pins */
    encoder_timer_init();  /* configures Timer2 overflow + ISR to request periodic redraw */

    sei();  /* enable interrupts */

    uart_println("BME280 data logger starting...");

    /* Check if sensor is connected */
    if (twi_test_address(BME280_I2C_ADDR) != 0)
        uart_println("ERROR: BME280 not found!");

    bme280_init();
    uart_println("BME280 initialized.");

    float T, P, H;
    char bufT[16], bufP[16], bufH[16];
    char line[64];

    uint32_t last = millis();

    /* Request an initial redraw via the encoder module */
    encoder_request_redraw();

    while (1)
    {
        /* Poll encoder frequently so UI feels responsive */
        encoder_poll();

        /* after polling encoder, handle start/stop request */
        if (flag_sd_toggle)
        {
            flag_sd_toggle = 0;
            if (sd_logging) sd_log_stop();
            else sd_log_start();
        }

        /* Update every 1000 ms: read sensor, print to UART, update encoder display values */
        if (millis() - last >= 1000)
        {
            last += 1000;

            /* Read sensor */
            bme280_read(&T, &P, &H);

            /* Convert float -> string (AVR does not support float printf) */
            dtostrf(T, 6, 2, bufT);
            dtostrf(P, 7, 2, bufP);
            dtostrf(H, 6, 2, bufH);

            /* Format final output line and send via UART */
            snprintf(line, sizeof(line), "T=%s C, P=%s hPa, H=%s %%", bufT, bufP, bufH);
            uart_println(line);

            /* Update encoder/display module with new measured values and request redraw */
            encoder_set_values(T, P, H);
            encoder_request_redraw();

            /* If logging is active, append measurement to buffer */
            if (sd_logging) sd_log_append_line(T, P, H);
        }

        /* If encoder/display requests a redraw, this will draw it now */
        encoder_draw_if_needed();

        /* Minimal idle - do not block (no delays) */
    }

    return 0;
}
