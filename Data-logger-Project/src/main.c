/***********************************************************************
 * Main file of the Data Logger project
 *
 * Architecture:
 * - Non-blocking Super-loop architecture
 * - Task scheduling based on system uptime (millis)
 * - Atomic data sharing between ISR/Tasks using global variables
 *
 * Hardware:
 * - Sensors: BME280 (I2C), Light Sensor (Analog A0)
 * - IO: Rotary Encoder, I2C LCD, SD Card (SPI)
 * - RTC: DS1302
 **********************************************************************/

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

/* Utilities */
#include "utils.h"  /* Includes the I2C scan function */

/* --- Configuration --- */
#define SAMPLE_PERIOD_MS 1000UL  /* Data logging interval in ms */

/* --- Global Shared Variables --- */
volatile float g_T = 0.0f;
volatile float g_P = 0.0f;
volatile float g_H = 0.0f;
volatile uint16_t g_Light = 0;
volatile rtc_time_t g_time = {0, 0, 0};

/* External flags */
extern volatile uint8_t flag_sd_toggle; 

/* --- System Time (Millis) --- */
volatile uint32_t g_millis = 0;

/* Timer 0 Overflow Interrupt - ticks every 1 ms */
ISR(TIMER0_OVF_vect) {
    g_millis++;
}

/* Get system uptime in milliseconds (Atomic read) */
static uint32_t millis(void) {
    uint32_t t;
    uint8_t sreg = SREG;
    cli(); 
    t = g_millis;
    SREG = sreg; 
    return t;
}

/* Initialize Timer 0 for 1ms system tick */
void timer0_init_system_tick(void) {
    tim0_ovf_1ms();
    tim0_ovf_enable();
}

/* Helper: Reads RTC, converts BCD to Binary, updates global time */
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

/* === Main Application Entry === */
int main(void) {
    
    /* --- 1. Low-Level Initialization --- */
    uart_init(UART_BAUD_SELECT(9600, F_CPU));
    twi_init();
    
    // Initialize RTC
    ds1302_init();
    uart_puts("RTC: Initialized.\r\n");

    /* OPTIONAL: Run once to set time, then comment out. 
       Values taken from your attached file. */
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

    // Initial time read
    sys_update_time();

    /* --- 2. Peripherals Initialization --- */
    // LCD & UI default state
    lcdValue = 0;
    flag_update_lcd = 1;
    logger_display_draw();
    
    // SD Card
    sd_log_init();

    // Timer setup & Interrupts enable
    timer0_init_system_tick();
    sei(); 

    uart_puts("--- System Boot Complete ---\r\n");
    
    // Helper function from utils.h
    i2c_scan();

    /* --- 3. Sensor Initialization --- */
    uart_puts("Sensors: Init BME280...\r\n");
    bme280_init();
    
    uart_puts("Sensors: Init Light Sensor...\r\n");
    // FIXED: Changed from 3 to 0 based on your attached file
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
        logger_encoder_poll();

        // -- TASK 2: Display Update --
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

            // C) Debug Output
            char bufT[10], bufP[10], bufH[10];
            dtostrf(temp, 4, 1, bufT);
            dtostrf(press, 6, 1, bufP);
            dtostrf(hum, 4, 1, bufH);
            
            sprintf(debug_buffer, "DATA: T=%s C, P=%s hPa, H=%s %%, L=%u %%\r\n", 
                    bufT, bufP, bufH, calLight);
            uart_puts(debug_buffer);
            
            // D) Update System Time
            sys_update_time();

            // E) Data Logging to SD
            static uint8_t last_logged_sec = 255;
            if(sd_logging && g_time.ss != last_logged_sec) {
                sd_log_append_line(g_T, g_P, g_H, g_Light);
                last_logged_sec = g_time.ss;
            }

            // F) Request UI Refresh
            flag_update_lcd = 1;
        }

        // -- TASK 4: SD Control Logic --
        if(flag_sd_toggle) {
            flag_sd_toggle = 0;
            
            if(!sd_logging) {
                uart_puts("CMD: SD Log START\r\n");
                sd_log_start();
            } else {
                uart_puts("CMD: SD Log STOP\r\n");
                sd_log_stop();
            }
            flag_update_lcd = 1;
        }
    }

    return 0; 
}