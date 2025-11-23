#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>

#include "loggerControl.h"
#include "lcd.h"
#include "twi.h"

/* Encoder Pins */
#define ENC_SW   PC0
#define ENC_DT   PC1
#define ENC_CLK  PC2

#define ENC_PORT_REG  PORTB
#define ENC_DDR_REG   DDRB
#define ENC_PIN_REG   PINB

/* Local encoder state */
static uint8_t lastStateCLK = 0;

/* Forward */
static uint8_t bcd2dec(uint8_t v) { return ((v>>4)*10 + (v & 0x0F)); }

/* LCD INIT  */
void logger_display_init(void)
{
    lcd_init(LCD_DISP_ON);
    lcd_clrscr();
    lcd_gotoxy(0,0);
    lcd_puts("Env. logger");
    lcd_gotoxy(0,1);
    lcd_puts("Starting...");
}

/* ENCODER INIT */
void logger_encoder_init(void)
{
    ENC_DDR_REG  &= ~((1 << ENC_CLK) | (1 << ENC_DT) | (1 << ENC_SW));
    ENC_PORT_REG |=  ((1 << ENC_CLK) | (1 << ENC_DT) | (1 << ENC_SW));

    lastStateCLK = (ENC_PIN_REG & (1 << ENC_CLK)) ? 1 : 0;
}

/* ENCODER POLL */
void logger_encoder_poll(void)
{
    uint8_t currentStateCLK = (ENC_PIN_REG & (1 << ENC_CLK)) ? 1 : 0;

    if (currentStateCLK != lastStateCLK)
    {
        uint8_t dt = (ENC_PIN_REG & (1 << ENC_DT)) ? 1 : 0;
        if (dt != currentStateCLK)
        {
            if (lcdValue == 0) lcdValue = 2;
            else lcdValue--;
        }
        else
        {
            lcdValue++;
            if (lcdValue > 2) lcdValue = 0;
        }
        flag_update_lcd = 1;  // extern in main
    }
    lastStateCLK = currentStateCLK;

    /* Button handled in main if needed */
}

/* RTC READ TIME */
#define RTC_ADR     0x68
#define RTC_SEC_MEM 0x00

void logger_rtc_read_time(void)
{
    uint8_t buf[3];
    twi_readfrom_mem_into(RTC_ADR, RTC_SEC_MEM, buf, 3);

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

/* DISPLAY DRAW */
void logger_display_draw(void)
{
    char buf[16];
    char line[17];

    const char *label;
    switch (lcdValue)
    {
        case 0: label = "Temp"; break;
        case 1: label = "Pressure"; break;
        case 2: label = "Humidity"; break;
        default: label = "Value"; break;
    }

    snprintf(line, sizeof(line), "%-11s%02u:%02u",
             label,
             (unsigned)g_time.h,
             (unsigned)g_time.m);

    line[16] = '\0';

    lcd_gotoxy(0,0);
    lcd_puts(line);

    lcd_gotoxy(0,1);
    for(uint8_t i=0;i<16;i++) lcd_putc(' ');
    lcd_gotoxy(0,1);

    switch (lcdValue)
    {
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
