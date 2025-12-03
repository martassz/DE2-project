/**
 * @file main.c
 * @brief Main application entry point for the Environmental Data Logger.
 *
 * This project implements a portable data logger using an AVR ATmega328P.
 * It periodically samples sensors (BME280, Photoresistor), updates a UI
 * (LCD + Rotary Encoder), and logs data to an SD card.
 *
 * @author Team DE2-Project
 * @date 2025
 * @copyright MIT License
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>

/* Custom drivers */
#include "uart.h"
#include "twi.h"
#include "bme280.h"
#include "LightSensor.h"
#include "loggerControl.h"
#include "sdlog.h"
#include "lcd_i2c.h"
#include "ds1302.h"
#include "timer.h"
#include "utils.h"

/** @brief Sampling period in milliseconds. */
#define SAMPLE_PERIOD_MS 1000UL

/* --- Global Shared Variables --- */
/** @brief Global temperature value [°C]. */
volatile float g_T = 0.0f;
/** @brief Global pressure value [hPa]. */
volatile float g_P = 0.0f;
/** @brief Global humidity value [%]. */
volatile float g_H = 0.0f;
/** @brief Global light intensity [%]. */
volatile uint16_t g_Light = 0;
/** @brief Global system time structure. */
volatile rtc_time_t g_time = {0, 0, 0};

/** @brief System uptime counter in milliseconds. */
volatile uint32_t g_millis = 0;

/**
 * @brief Timer0 Overflow Interrupt Service Routine.
 * Increments the system uptime counter every 1 ms.
 */
ISR(TIMER0_OVF_vect) {
    g_millis++;
}

/**
 * @brief  Get current system uptime safely (atomic read).
 * @return System uptime in milliseconds.
 */
static uint32_t millis(void) {
    uint32_t t;
    uint8_t sreg = SREG;
    cli();
    t = g_millis;
    SREG = sreg;
    return t;
}

/**
 * @brief  Initialize Timer0 for 1ms overflow interrupts.
 * Uses macros from timer.h library.
 */
void timer0_init_system_tick(void) {
    tim0_ovf_1ms();
    tim0_ovf_enable();
}

/**
 * @brief  Reads the RTC, converts BCD to Binary, and updates the global time structure.
 * Should be called periodically or before data logging.
 */
void sys_update_time(void) {
    ds1302_time_t raw_time;

    // 1. Read raw BCD data from DS1302
    ds1302_read_time(&raw_time);

    // 2. Convert BCD to Binary
    uint8_t hh = ((raw_time.hour >> 4) * 10) + (raw_time.hour & 0x0F);
    uint8_t mm = ((raw_time.min  >> 4) * 10) + (raw_time.min  & 0x0F);
    uint8_t ss = ((raw_time.sec  >> 4) * 10) + (raw_time.sec  & 0x0F);

    // 3. Atomically update global structure
    uint8_t sreg = SREG;
    cli();
    g_time.hh = hh;
    g_time.mm = mm;
    g_time.ss = ss;
    SREG = sreg;
}

/**
 * @brief Main application function.
 * @return 0 (Should never return)
 */
int main(void) {

    /* --- 1. Low-Level Initialization --- */
    uart_init(UART_BAUD_SELECT(9600, F_CPU));
    twi_init();

    // Initialize RTC
    ds1302_init();
    uart_puts("RTC: Initialized.\r\n");

    /* -----------------------------------------------------------
     * OPTIONAL: Run once to set time, then comment out.
     * Values example: 13:39:00, 1.1.2024
     * ----------------------------------------------------------- */
    /*
    ds1302_time_t t_setup;
    t_setup.sec = 0;
    t_setup.min = 39;
    t_setup.hour = 13;
    t_setup.date = 1;
    t_setup.month = 1;
    t_setup.day = 1;
    t_setup.year = 24;
    ds1302_set_time(&t_setup);
    uart_puts("Time set!\r\n");
    */
    /* ----------------------------------------------------------- */

    // Initial time read
    sys_update_time();

    /* --- 2. Peripherals Initialization --- */
    // LCD & UI default state
    lcdValue = 0;
    flag_update_lcd = 1;
    logger_display_draw();

    // SD Card Init (Internal flags only)
    sd_log_init();

    // Timer setup & Interrupts enable
    timer0_init_system_tick();
    sei();

    uart_puts("--- System Boot Complete ---\r\n");

    // Optional: Scan I2C bus for debugging
    i2c_scan();

    /* --- 3. Sensor Initialization --- */
    uart_puts("Sensors: Init BME280...\r\n");
    bme280_init();

    uart_puts("Sensors: Init Light Sensor...\r\n");
    lightSensor_init(0); // Analog pin A0
    lightSensor_setCalibration(10, 750);

    // UI Controls
    logger_display_init();
    logger_encoder_init();

    /* --- 4. Loop Variables --- */
    uint32_t last_sample_time = 0;
    char debug_buffer[80];
    float temp, press, hum;
    uint16_t calLight;

    /* === Main Super-Loop === */
    while(1) {
        uint32_t current_time = millis();

        // -- TASK 1: UI Input Polling --
        // Polls the rotary encoder and button state
        logger_encoder_poll();

        // -- TASK 2: Display Update --
        // Redraws LCD if the flag was set by encoder or timer
        if (flag_update_lcd) {
            logger_display_draw();
        }

        // -- TASK 3: Periodic Sampling (1000 ms) --
        if (current_time - last_sample_time >= SAMPLE_PERIOD_MS) {
            last_sample_time = current_time;

            // A) Acquire Sensor Data
            bme280_read(&temp, &press, &hum);
            calLight = lightSensor_readCalibrated();

            // B) Update Global State (Atomic)
            uint8_t sreg = SREG; cli();
            g_T = temp;
            g_P = press;
            g_H = hum;
            g_Light = calLight;
            SREG = sreg;

            // C) Debug Output via UART
            char bufT[10], bufP[10], bufH[10];
            dtostrf(temp, 4, 1, bufT);
            dtostrf(press, 6, 1, bufP);
            dtostrf(hum, 4, 1, bufH);

            sprintf(debug_buffer, "DATA: T=%s C, P=%s hPa, H=%s %%, L=%u %%\r\n",
                    bufT, bufP, bufH, calLight);
            uart_puts(debug_buffer);

            // D) Update System Time from RTC
            sys_update_time();

            // E) Data Logging to SD
            // If logging is enabled, append a new line to the file.
            if(sd_logging) {
                sd_log_append_line(g_T, g_P, g_H, g_Light);
            }

            // F) Request UI Refresh (to update values on screen)
            flag_update_lcd = 1;
        }

        // -- TASK 4: SD Control Logic (Triggered by Encoder Button) --
        if(flag_sd_toggle) {
            flag_sd_toggle = 0;

            if(!sd_logging) {
                // User requested START
                if (sd_log_start() != 0) {
                    uart_puts("ERR: SD Start failed\r\n");
                }
            } else {
                // User requested STOP
                sd_log_stop();
            }
            // Update LCD to show/hide '*' recording icon
            flag_update_lcd = 1;
        }
    }

    return 0;
}