/***********************************************************************
 * Data Logger Main (upravený pro SD SPI init + logging)
 **********************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>

#include "uart.h"
#include "twi.h"
#include "bme280.h"
#include "LightSensor.h"
#include "loggerControl.h"
#include "sdlog.h"
#include "lcd_i2c.h"
#include "ds1302.h"
#include "timer.h"
#include "utils.h"  /* Includes i2c_scan() */

/* --- Configuration --- */
#define SAMPLE_PERIOD_MS 1000UL

/* --- Global Variables --- */
volatile float g_T=0.0f, g_P=0.0f, g_H=0.0f;
volatile uint16_t g_Light=0;
volatile rtc_time_t g_time={0,0,0};

/* External flags */
extern volatile uint8_t flag_sd_toggle; 

/* --- System Millis --- */
volatile uint32_t g_millis=0;
ISR(TIMER0_OVF_vect) { g_millis++; }

static uint32_t millis(void) {
    uint32_t t;
    uint8_t sreg = SREG; cli();
    t=g_millis; SREG=sreg;
    return t;
}

void timer0_init_system_tick(void) {
    tim0_ovf_1ms();
    tim0_ovf_enable();
}

/* --- RTC helper --- */
void sys_update_time(void) {
    ds1302_time_t raw;
    ds1302_read_time(&raw);
    uint8_t hh=((raw.hour>>4)*10)+(raw.hour&0x0F);
    uint8_t mm=((raw.min>>4)*10)+(raw.min&0x0F);
    uint8_t ss=((raw.sec>>4)*10)+(raw.sec&0x0F);
    uint8_t sreg=SREG; cli();
    g_time.hh=hh; g_time.mm=mm; g_time.ss=ss;
    SREG=sreg;
}

/* === Main === */
int main(void) {

    /* 1. Low-Level Init */
    uart_init(UART_BAUD_SELECT(9600,F_CPU));
    twi_init();
    ds1302_init();
    uart_puts("RTC: Initialized.\r\n");

    sys_update_time();

    /* 2. Peripherals Init */
    lcdValue=0; flag_update_lcd=1;
    logger_display_draw();

    sd_log_init();

    timer0_init_system_tick();
    sei();

    uart_puts("--- System Boot Complete ---\r\n");
    i2c_scan();

    /* Sensors */
    uart_puts("Sensors: Init BME280...\r\n");
    bme280_init();
    uart_puts("Sensors: Init Light Sensor...\r\n");
    lightSensor_init(0);
    lightSensor_setCalibration(10,750);

    logger_display_init();
    logger_encoder_init();

    /* Loop vars */
    uint32_t last_sample_time=0;
    char debug_buffer[80];
    float temp, press, hum;
    uint16_t calLight;

    /* Super-loop */
    while(1) {
        uint32_t current_time=millis();

        /* 1. UI Polling */
        logger_encoder_poll();

        /* 2. Display update */
        if(flag_update_lcd) logger_display_draw();

        /* 3. Periodic Sampling */
        if(current_time - last_sample_time >= SAMPLE_PERIOD_MS) {
            last_sample_time=current_time;

            bme280_read(&temp,&press,&hum);
            calLight=lightSensor_readCalibrated();

            /* Update global state */
            uint8_t sreg=SREG; cli();
            g_T=temp; g_P=press; g_H=hum; g_Light=calLight;
            SREG=sreg;

            /* Debug output */
            char bufT[10],bufP[10],bufH[10];
            dtostrf(temp,4,1,bufT);
            dtostrf(press,6,1,bufP);
            dtostrf(hum,4,1,bufH);
            sprintf(debug_buffer,"DATA: T=%s C, P=%s hPa, H=%s %%, L=%u %%\r\n",
                    bufT,bufP,bufH,calLight);
            uart_puts(debug_buffer);

            sys_update_time();

            /* SD logging (append) */
            static uint8_t last_logged_sec=255;
            if(sd_logging && g_time.ss!=last_logged_sec) {
                sd_log_append_line(g_T,g_P,g_H,g_Light);
                last_logged_sec=g_time.ss;
            }

            flag_update_lcd=1;
        }

        /* 4. SD Control (start/stop) */
        if(flag_sd_toggle) {
            flag_sd_toggle=0;
            if(!sd_logging) {
                uart_puts("CMD: SD Log START\r\n");
                sd_log_start();
            } else {
                uart_puts("CMD: SD Log STOP\r\n");
                sd_log_stop();
            }
            flag_update_lcd=1;
        }
    }

    return 0;
}
