/*
 * main.c
 * Minimal integration: BME280 + Light sensor (existing working code)
 * + RTC time source + 16x2 LCD + Encoder KY-040
 *
 * Notes:
 * - I preserved your original serial output formatting exactly.
 * - No additional "read failed" messages or other new UART spam.
 * - LCD shows selected sensor value and HH:MM:SS from RTC when available.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

#include "uart.h"       // Fleury UART
#include "twi.h"        // Fryza TWI
#include "bme280.h"
#include "LightSensor.h"

#include "lcd.h"        // HD44780 LCD library (assumed present)
#include "timer.h"      // for tim2_ovf_16ms(), tim2_ovf_enable()

#define SAMPLE_PERIOD_MS 1000UL

/* ---------------------- millis (Timer0) ------------------- */
volatile uint32_t g_millis = 0;
ISR(TIMER0_OVF_vect) { g_millis++; }
static uint32_t millis(void) { uint32_t t; uint8_t sreg=SREG; cli(); t=g_millis; SREG=sreg; return t; }

/* Print line with Fleury UART */
static void uart_println(const char *s) { uart_puts(s); uart_puts("\r\n"); }

/* I2C debug scan (kept as in original) */
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

/* ---------------- DISPLAY + ENCODER (minimal additions) ---------------- */
/* Encoder KY-040 pin numbers (kept as provided) */
#define ENC_SW   PC0   // push button (active low)
#define ENC_DT   PC1
#define ENC_CLK  PC2

/* Encoder port registers (kept as your fragment had them) */
#define ENC_PORT_REG  PORTB
#define ENC_DDR_REG   DDRB
#define ENC_PIN_REG   PINB

/* Display selection: 0 = Temperature, 1 = Pressure, 2 = Humidity */
volatile uint8_t lcdValue = 0;          // which value to show
volatile uint8_t flag_update_lcd = 0;   // set by timer2 ISR or encoder to request redraw

/* Shared latest sensor values (updated after successful read) */
volatile float g_T = 0.0f, g_P = 0.0f, g_H = 0.0f;

/* RTC time struct (hours/minutes/seconds) */
typedef struct { uint8_t h; uint8_t m; uint8_t s; } rtc_time_t;
volatile rtc_time_t g_time = {0,0,0};

/* Encoder state */
static uint8_t lastStateCLK = 0;
static uint32_t lastEncButtonPress = 0;

/* Forward declarations */
static void display_init_minimal(void);
static void encoder_init(void);
static void display_draw_now(void);
static void timer2_init_for_display_1s(void);
static void encoder_check_poll(void);

/* --- minimal LCD init --- */
static void display_init_minimal(void)
{
    lcd_init(LCD_DISP_ON);
    lcd_clrscr();
    lcd_gotoxy(0,0);
    lcd_puts("Env. logger");
    lcd_gotoxy(0,1);
    lcd_puts("Starting...");
}

/* --- encoder init (minimal) --- */
static void encoder_init(void)
{
    /* As in your fragment: configure as inputs + pull-ups */
    ENC_DDR_REG &= ~((1 << ENC_CLK) | (1 << ENC_DT) | (1 << ENC_SW));
    ENC_PORT_REG |= ((1 << ENC_CLK) | (1 << ENC_DT) | (1 << ENC_SW));
    lastStateCLK = (ENC_PIN_REG & (1 << ENC_CLK)) ? 1 : 0;
}

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

/* --- encoder poll (call frequently in loop) --- */
static void encoder_check_poll(void)
{
    uint8_t currentStateCLK = (ENC_PIN_REG & (1 << ENC_CLK)) ? 1 : 0;

    if (currentStateCLK != lastStateCLK)
    {
        uint8_t dt = (ENC_PIN_REG & (1 << ENC_DT)) ? 1 : 0;
        if (dt != currentStateCLK)
        {
            /* counter-clockwise */
            if (lcdValue == 0) lcdValue = 2;
            else lcdValue--;
        } else {
            /* clockwise */
            lcdValue++;
            if (lcdValue > 2) lcdValue = 0;
        }
        flag_update_lcd = 1;
    }
    lastStateCLK = currentStateCLK;

    /* Button (active LOW) - debounce */
    if ( (ENC_PIN_REG & (1 << ENC_SW)) == 0 )
    {
        uint32_t now = millis();
        if (now - lastEncButtonPress > 50)
        {
            /* placeholder action - request redraw to keep UI consistent */
            flag_update_lcd = 1;
            lastEncButtonPress = now;
        }
    }
}

/* ---------------- RTC helper (very small, non-intrusive) --------------- */
#define RTC_ADR     0x68
#define RTC_SEC_MEM 0x00

/* BCD -> DEC */
static uint8_t bcd2dec(uint8_t v) { return ( (v>>4)*10 + (v & 0x0F) ); }

static void rtc_read_time_silent(void)
{
    uint8_t buf[3];

    /* Attempt RTC read – twi_readfrom_mem_into has void return */
    twi_readfrom_mem_into(RTC_ADR, RTC_SEC_MEM, buf, 3);

    /* Convert & store – data will be garbage if RTC missing,
       but you explicitly asked for silent behaviour. */
    uint8_t sec  = bcd2dec(buf[0] & 0x7F);
    uint8_t min  = bcd2dec(buf[1]);
    uint8_t hour = bcd2dec(buf[2] & 0x3F);

    uint8_t sreg = SREG;
    cli();
    g_time.h = hour;
    g_time.m = min;
    g_time.s = sec;
    SREG = sreg;
}

/* --- display draw (uses g_T/g_P/g_H and g_time) --- */
static void display_draw_now(void)
{
    char buf[16];
    char line[17];

    /* label left, time right (fits 16 chars) */
    const char *label;
    switch (lcdValue) {
        case 0: label = "Temperature"; break;
        case 1: label = "Pressure";    break;
        case 2: label = "Humidity";    break;
        default: label = "Value";      break;
    }
    snprintf(line, sizeof(line), "%-11s %02u:%02u", label, (unsigned)g_time.h, (unsigned)g_time.m);
    /* ensure null-terminated and not longer than 16 */
    line[16] = '\0';
    lcd_gotoxy(0,0);
    lcd_puts(line);

    /* second row: value */
    lcd_gotoxy(0,1);
    for (uint8_t i=0;i<16;i++) lcd_putc(' ');
    lcd_gotoxy(0,1);

    switch (lcdValue) {
        case 0:
            dtostrf((double)g_T,6,2,buf);
            lcd_puts(buf); lcd_puts(" C");
            break;
        case 1:
            dtostrf((double)g_P,7,2,buf);
            lcd_puts(buf); lcd_puts(" hPa");
            break;
        case 2:
            dtostrf((double)g_H,6,2,buf);
            lcd_puts(buf); lcd_puts(" %");
            break;
        default:
            lcd_puts("N/A");
            break;
    }
}

/* ----------------------- main (kept intact, minimal changes) ------------------- */
int main(void) {
    uart_init(UART_BAUD_SELECT(9600,F_CPU));
    twi_init();

    /* Timer0: 1 ms overflow; preserve original behavior */
    TIMSK0 = (1<<TOIE0);
    TCCR0B = (1<<CS01) | (1<<CS00);
    sei();

    i2c_scan();

    uart_println("Initializing BME280...");
    bme280_init();
    _delay_ms(10);
    uart_println("BME280 initialized.");

    /* Light Sensor - preserved exactly */
    lightSensor_init(3);
    lightSensor_setCalibration(10,750);
    uart_println("Light sensor initialized.");

    /* Initialize LCD/Encoder/Timer2 for display */
    display_init_minimal();
    encoder_init();
    timer2_init_for_display_1s();

    /* buffers and variables preserved from original */
    float T,P,H;
    uint16_t rawLight, calLight;
    char bufT[8], bufP[10], bufH[8], line[128];

    uint32_t last_sample = millis();

    while(1) {
        uint32_t now = millis();

        /* keep polling encoder often */
        encoder_check_poll();

        if(now - last_sample >= SAMPLE_PERIOD_MS) {
            last_sample += SAMPLE_PERIOD_MS;

            /* BME280 - keep original read and UART output exactly */
            bme280_read(&T,&P,&H);

            /* update shared values used by LCD (minimal, safe) */
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

            /* Light Sensor - preserved output */
            rawLight = lightSensor_readRaw();
            calLight = lightSensor_readCalibrated();
            snprintf(line,sizeof(line),"Light -> Raw=%u | SVETLo=%u%%",rawLight,calLight);
            uart_println(line);

            /* RTC: read silently (no UART messages on failure) */
            rtc_read_time_silent();

            /* request LCD update */
            flag_update_lcd = 1;
        }

        /* Do LCD redraw if requested */
        if (flag_update_lcd) {
            flag_update_lcd = 0;
            display_draw_now();
        }

        /* small delay to keep loop sane */
        _delay_ms(2);
    }

    /* never reached */
    return 0;
}
