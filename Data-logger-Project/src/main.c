#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

#include "uart.h"       // Fleury UART
#include "twi.h"        // Fryza TWI
#include "bme280.h"
#include "LightSensor.h"
#include "loggerControl.h"
#include "sdlog.h"

#include "lcd.h"        // HD44780 LCD library (assumed present)
#include "timer.h"      // for tim2_ovf_16ms(), tim2_ovf_enable()

#define SAMPLE_PERIOD_MS 1000UL

/* ---------------------- millis (Timer0) ------------------- */
volatile uint32_t g_millis = 0;
ISR(TIMER0_OVF_vect) { g_millis++; }

static uint32_t millis(void) { 
    uint32_t t; 
    uint8_t sreg=SREG; 
    cli(); 
    t=g_millis; 
    SREG=sreg; 
    return t; 
}

/* Print line with Fleury UART */
static void uart_println(const char *s) { 
    uart_puts(s); 
    uart_puts("\r\n"); 
}

/* I2C debug scan */
void i2c_scan(void) {
    uart_println("Scanning I2C...");
    for(uint8_t addr=1; addr<127; addr++) {
        if(twi_test_address(addr)==0) {
            char buf[32];
            snprintf(buf,sizeof(buf),"Found: 0x%02X", addr);
            uart_println(buf);
        }
    }
    uart_println("Scan done!");
}

/* ---------------- DISPLAY STATE AND SHARED DATA ---------------- */

/* Display selection */
volatile uint8_t lcdValue = 0;
volatile uint8_t flag_update_lcd = 0;

/* Shared sensor values */
volatile float g_T = 0.0f, g_P = 0.0f, g_H = 0.0f;

/* Time struct (shared with loggerControl) */
volatile rtc_time_t g_time = {0,0,0};

/* --- Timer2 ISR: accumulate ~1s and set flag_update_lcd --- */
ISR(TIMER2_OVF_vect)
{
    static uint8_t n_ovfs = 0;
    n_ovfs++;
    if (n_ovfs >= 62) { /* 62 * ~16 ms == ~1 s */
        n_ovfs = 0;
        flag_update_lcd = 1;
    }
}

static void timer2_init_for_display_1s(void)
{
    tim2_ovf_16ms();
    tim2_ovf_enable();
}

/* ---------------- SD log ---------------- */
extern volatile uint8_t flag_sd_toggle;

static uint8_t last_s = 0;    // compare with g_time in loop

/* === Main function === */
int main(void) {
    uart_init(UART_BAUD_SELECT(9600,F_CPU));
    twi_init();
    sd_log_init();

    /* Timer0: 1 ms overflow */
    TIMSK0 = (1<<TOIE0);
    TCCR0B = (1<<CS01) | (1<<CS00);
    sei();

    i2c_scan();

    uart_println("Initializing BME280...");
    bme280_init();
    _delay_ms(10);
    uart_println("BME280 initialized.");

    /* Light Sensor */
    lightSensor_init(3);
    lightSensor_setCalibration(10,750);
    uart_println("Light sensor initialized.");

    /* Initialize LCD/Encoder/Timer2 for display */
    logger_display_init();
    logger_encoder_init();
    timer2_init_for_display_1s();

    /* Buffers */
    float T,P,H;
    uint16_t rawLight, calLight;
    char bufT[8], bufP[10], bufH[8], line[128];

    uint32_t last_sample = millis();

    while(1) {
        uint32_t now = millis();

        /* keep polling encoder often */
        logger_encoder_poll();

        if(now - last_sample >= SAMPLE_PERIOD_MS) {
            last_sample += SAMPLE_PERIOD_MS;

            /* BME280 - keep original read and UART output exactly */
            bme280_read(&T,&P,&H);

            /* update shared values used by LCD */
            {
                uint8_t sreg = SREG;
                cli();
                g_T = T;
                g_P = P;
                g_H = H;
                SREG = sreg;
            }

            dtostrf(T,6,2,bufT);
            dtostrf(P,7,2,bufP);
            dtostrf(H,6,2,bufH);
            snprintf(line,sizeof(line),"BME280 -> T=%s C, P=%s hPa, H=%s %%",bufT,bufP,bufH);
            uart_println(line);

            /* Light Sensor */
            rawLight = lightSensor_readRaw();
            calLight = lightSensor_readCalibrated();
            snprintf(line,sizeof(line),"Light -> Raw=%u | SVETLo=%u%%",rawLight,calLight);
            uart_println(line);

            logger_rtc_read_time();

            /* request LCD update */
            flag_update_lcd = 1;
        }

        /* Do LCD redraw if requested */
        if (flag_update_lcd) {
            flag_update_lcd = 0;
            logger_display_draw();
        }

        /* SD measurements logging */
        if(flag_sd_toggle) {
            flag_sd_toggle = 0;

            if(!sd_logging) {
                sd_log_start();
            } else {
                sd_log_stop();
            }
        }

        if (g_time.s != last_s) {
            last_s = g_time.s;

            if(sd_logging) {
                sd_log_append_line(g_T, g_P, g_H);
            }
        }

        /* small delay to keep loop sane */
        _delay_ms(2);
    }

    /* never reached */
    return 0;
}
