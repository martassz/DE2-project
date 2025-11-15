#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>

#include <uart.h>      // Fleury UART
#include <twi.h>       // Fryza TWI (I2C)
#include <timer.h>     // Fryza Timer utilities (1ms overflow)
#include "bme280.h"    // Our BME280 driver

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

// ----------------------------------------------------
// Global software time (milliseconds)
// ----------------------------------------------------
volatile uint32_t g_millis = 0;

ISR(TIMER0_OVF_vect)
{
    g_millis++;    // called every 1 ms
}

// Thread-safe access to g_millis
static uint32_t millis(void)
{
    uint32_t t;
    uint8_t sreg = SREG;
    cli();
    t = g_millis;
    SREG = sreg;
    return t;
}

// Simple println wrapper
static void uart_println(const char *s)
{
    uart_puts(s);
    uart_puts("\r\n");
}

// ----------------------------------------------------
// Main application
// ----------------------------------------------------
int main(void)
{
    // UART 9600 baud (works reliably with ATmega328P/16 MHz)
    uart_init(UART_BAUD_SELECT(9600, F_CPU));

    // Initialize I2C/TWI
    twi_init();

    // Timer0 overflow every ~1ms
    tim0_ovf_1ms();
    tim0_ovf_enable();
    sei();  // enable interrupts

    uart_println("BME280 data logger starting...");

    // Check if sensor is connected
    if (twi_test_address(BME280_I2C_ADDR) != 0)
        uart_println("ERROR: BME280 not found!");

    bme280_init();
    uart_println("BME280 initialized.");

    float T, P, H;
    char bufT[16], bufP[16], bufH[16];
    char line[64];

    uint32_t last = millis();

    while (1)
    {
        // Run every 1000 ms
        if (millis() - last >= 1000)
        {
            last += 1000;

            // Read sensor
            bme280_read(&T, &P, &H);

            // Convert float -> string (AVR does not support float printf)
            dtostrf(T, 6, 2, bufT);
            dtostrf(P, 7, 2, bufP);
            dtostrf(H, 6, 2, bufH);

            // Format final output line
            snprintf(line, sizeof(line),
                     "T=%s C, P=%s hPa, H=%s %%", bufT, bufP, bufH);

            uart_println(line);
        }

        // Here you can add future data logging, SD card writing, etc.
    }

    return 0;
}
