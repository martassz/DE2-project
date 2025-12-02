/*
 * Buffered CSV logging to SD card.
 * - Uses low-level SPI for SD initialization
 * - Other SD write/open/close functions remain weak
 */

#include "sdlog.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <uart.h>
#include "loggerControl.h"
#include "ds1302.h"
#include <stdbool.h>

/* Module state */
volatile uint8_t flag_sd_toggle = 0;       /* set by encoder code to request start/stop */
volatile bool sd_logging = false;          /* true while logging is active */

static char sd_buffer[SD_BUFFER_SIZE];
static size_t sd_buf_pos = 0;

/* --- SD SPI Pins --- */
#define CS_PORT PORTD
#define CS_DDR  DDRD
#define CS_PIN  (1<<4)   // PD4
#define SPI_DDR DDRB
#define SPI_PORT PORTB
#define MOSI_PIN (1<<3)  // PB3
#define MISO_PIN (1<<4)  // PB4
#define SCK_PIN  (1<<5)  // PB5
#define CS_HIGH() (CS_PORT|=CS_PIN)
#define CS_LOW()  (CS_PORT&=~CS_PIN)

/* --- SPI helpers --- */
static void spi_init(void)
{
    SPI_DDR |= MOSI_PIN | SCK_PIN | (1<<PB2); // SS must be output
    SPI_DDR &= ~MISO_PIN;
    CS_DDR |= CS_PIN; CS_HIGH();

    SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR1) | (1<<SPR0); // f/128
    SPSR &= ~(1<<SPI2X);
}

static uint8_t spi_xfer(uint8_t b)
{
    SPDR = b; while(!(SPSR & (1<<SPIF))); return SPDR;
}

static void send_dummy_clocks(void)
{
    CS_HIGH();
    for(uint8_t i=0;i<10;i++) spi_xfer(0xFF);
}

static uint8_t send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t r;
    CS_LOW(); spi_xfer(0xFF);
    spi_xfer(0x40 | cmd);
    spi_xfer(arg>>24); spi_xfer(arg>>16); spi_xfer(arg>>8); spi_xfer(arg);
    spi_xfer(crc);
    for(uint8_t i=0;i<20;i++) { r=spi_xfer(0xFF); if(!(r&0x80)) break; }
    CS_HIGH(); spi_xfer(0xFF);
    return r;
}

/* SD card init sequence */
int __attribute__((weak)) sdcard_init(void)
{
    spi_init();
    send_dummy_clocks();

    uint8_t r;
    // CMD0 - reset card
    r = send_cmd(0,0,0x95);
    if(r != 1) { uart_puts("SD: CMD0 failed\r\n"); return -1; }

    // CMD8 - check SDv2
    r = send_cmd(8,0x1AA,0x87);
    if(r != 1) uart_puts("SD: SDv1 or MMC detected\r\n");
    else uart_puts("SD: SDv2 detected\r\n");

    // ACMD41 - wait until ready
    for(uint16_t i=0;i<5000;i++)
    {
        r = send_cmd(55,0,0x65);
        if(r>1) continue;
        r = send_cmd(41,1UL<<30,0x77);
        if(r==0) break;
    }
    if(r != 0) { uart_puts("SD: ACMD41 timeout\r\n"); return -2; }

    // CMD58 - read OCR
    r = send_cmd(58,0,0x00);
    if(r != 0) { uart_puts("SD: CMD58 failed\r\n"); return -3; }

    uart_puts("SD: Initialization OK\r\n");
    return 0;
}

/* --- Buffer management and CSV logging --- */
static void dbg_print(const char *s) { uart_puts(s); uart_puts("\r\n"); }

static void format_timestamp(char *buf, size_t len)
{
    snprintf(buf,len,"%02u:%02u:%02u",
             (unsigned)g_time.hh,(unsigned)g_time.mm,(unsigned)g_time.ss);
}

int sd_log_init(void)
{
    sd_buf_pos = 0;
    flag_sd_toggle = 0;
    return 0;
}

void sd_log_append_line(float T, float P, float H, uint16_t L)
{
    char line[64]; char ts[24];
    format_timestamp(ts,sizeof(ts));
    int n = snprintf(line,sizeof(line),"%s,%.2f,%.2f,%.2f,%u\n",ts,T,P,H,L);
    if(n<=0) return;
    size_t ln = (size_t)n;
    if(ln>=SD_BUFFER_SIZE) { if(sd_logging) sdcard_write(line,ln); return; }
    if(sd_buf_pos + ln >= SD_BUFFER_SIZE) { if(sd_logging && sd_buf_pos>0) sdcard_write(sd_buffer,sd_buf_pos); sd_buf_pos=0; }
    memcpy(&sd_buffer[sd_buf_pos],line,ln); sd_buf_pos+=ln;
    if(sd_logging && sd_buf_pos>=SD_FLUSH_THRESHOLD) { sdcard_write(sd_buffer,sd_buf_pos); sd_buf_pos=0; }
}

int sd_log_start(void)
{
    if(sd_logging) return 0;
    if(sdcard_init()!=0) { dbg_print("SD init failed"); return -1; }

    char fname[20];
    snprintf(fname,sizeof(fname),"%02u%02u_LOG.TXT",(unsigned)g_time.hh,(unsigned)g_time.mm);

    if(sdcard_open_append(fname)!=0) { dbg_print("SD open failed"); return -2; }

    const char *hdr = "time,temperature,pressure,humidity,light\n";
    sdcard_write(hdr,strlen(hdr));

    sd_buf_pos = 0; sd_logging = true;
    dbg_print("SD logging started");
    return 0;
}

void sd_log_stop(void)
{
    if(!sd_logging) return;
    if(sd_buf_pos>0) { sdcard_write(sd_buffer,sd_buf_pos); sd_buf_pos=0; }
    sdcard_close(); sd_logging=false;
    dbg_print("SD logging stopped");
}
