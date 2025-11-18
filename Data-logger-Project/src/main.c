#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uart.h>      // Fleury UART
#include <twi.h>       // Fryza TWI (I2C)
#include <gpio.h>      // GPIO library for AVR-GCC
#include "timer.h"     // Fryza Timer utilities (Timer0/Timer2 macros)
#include "bme280.h"    // BME280 driver (T, P, H)
#include "encoder.h"   // KY-040 encoder + LCD UI
#include "sdlog.h"     // SD card logging helpers

#ifndef F_CPU
# define F_CPU 16000000UL   // Arduino Uno, ATmega328P, 16 MHz
#endif

#define SAMPLE_PERIOD_MS 1000UL   // Sensor sampling interval

/* ----------------------------------------------------
   Global software time (milliseconds, from Timer0)
   ---------------------------------------------------- */
volatile uint32_t g_millis = 0;

/* Timer0 overflow ISR: increments millisecond counter */
ISR(TIMER0_OVF_vect)
{
    g_millis++;    // called every ~1 ms
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

/* Simple UART "println" helper */
static void uart_println(const char *s)
{
    uart_puts(s);
    uart_puts("\r\n");
}

/* ----------------------------------------------------
   Main application (BME280 + LCD + SD logger)
   ---------------------------------------------------- */
int main(void)
{
    /* Initialize UART (9600 baud) */
    uart_init(UART_BAUD_SELECT(9600, F_CPU));

    /* Initialize I2C/TWI for BME280 and potentially other sensors */
    twi_init();

    /* Timer0: 1 ms overflow, used for millis() */
    tim0_ovf_1ms();
    tim0_ovf_enable();

    /* Initialize LCD+encoder UI and its Timer2-based refresh mechanism */
    encoder_init();        // Configure LCD and encoder pins
    encoder_timer_init();  // Configure Timer2 interrupt for periodic UI refresh

    /* Enable global interrupts */
    sei();

    uart_println("BME280 data logger starting...");

    /* Optional: check presence of BME280 on I2C bus */
    if (twi_test_address(BME280_I2C_ADDR) != 0)
    {
        uart_println("ERROR: BME280 not found!");
    }

    /* Initialize BME280 calibration and measurement settings */
    bme280_init();
    uart_println("BME280 initialized.");

    float T, P, H;
    char bufT[16], bufP[16], bufH[16];
    char line[64];

    /* Time of last measurement */
    uint32_t last_sample = millis();

    /* Force initial UI redraw */
    encoder_request_redraw();

    while (1)
    {
        /* Poll encoder frequently so the UI feels responsive */
        encoder_poll();

        /* Handle encoder button: toggle SD logging on/off */
        if (flag_sd_toggle)
        {
            flag_sd_toggle = 0;

            if (sd_logging)
                sd_log_stop();
            else
                sd_log_start();
        }

        /* Periodic measurement & logging */
        uint32_t now = millis();
        if ((now - last_sample) >= SAMPLE_PERIOD_MS)
        {
            last_sample += SAMPLE_PERIOD_MS;

            /* Read sensor (compensated values) */
            bme280_read(&T, &P, &H);

            /* Convert float -> string (AVR libc has no float printf) */
            dtostrf(T, 6, 2, bufT);
            dtostrf(P, 7, 2, bufP);
            dtostrf(H, 6, 2, bufH);

            /* Format line and send via UART */
            snprintf(line, sizeof(line),
                     "T=%s C, P=%s hPa, H=%s %%", bufT, bufP, bufH);
            uart_println(line);

            /* Push new values into UI module and request redraw */
            encoder_set_values(T, P, H);
            encoder_request_redraw();

            /* If logging is active, append current measurement */
            if (sd_logging)
                sd_log_append_line(T, P, H);
        }

        /* Perform LCD redraw if requested by encoder module/Timer2 ISR */
        encoder_draw_if_needed();

        /* No blocking delays here: loop stays responsive */
    }

    return 0;
}
