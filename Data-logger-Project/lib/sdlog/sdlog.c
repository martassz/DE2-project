/*
 * sdlog.c
 *
 * Buffered CSV logging to SD card.
 * - Uses weak low-level wrappers: sdcard_init/open_append/write/close
 * - Uses g_millis (extern) as fallback timestamp source (seconds since power-on).
 *
 * Comments and identifiers are in English.
 */

#include "sdlog.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <uart.h>   /* for debug prints (uart_puts) */

extern volatile uint32_t g_millis; /* defined in main.c */

/* Module state */
volatile uint8_t flag_sd_toggle = 0;       /* set by encoder code to request start/stop */
volatile bool sd_logging = false;          /* true while logging is active */

static char sd_buffer[SD_BUFFER_SIZE];
static size_t sd_buf_pos = 0;

/* Forward declarations of low-level wrappers (weak defaults below) */
int sdcard_init(void);
int sdcard_open_append(const char *filename);
int sdcard_write(const void *buf, size_t len);
void sdcard_close(void);

/* Helper: basic UART debug print (safe, simple) */
static void dbg_print(const char *s)
{
    /* minimal newline */
    uart_puts(s);
    uart_puts("\r\n");
}

/* Timestamp helper — fallback to seconds since power-on using g_millis */
static void format_timestamp(char *buf, size_t len)
{
    uint32_t s = g_millis / 1000UL;
    snprintf(buf, len, "%lu", (unsigned long)s);
}

/* Initialize module (optional) */
int sd_log_init(void)
{
    sd_buf_pos = 0;
    flag_sd_toggle = 0;
    /* sd_logging left unchanged (stopped by default) */
    return 0;
}

/* Append CSV line to RAM buffer and flush to SD if conditions met */
void sd_log_append_line(float T, float P, float H)
{
    char line[64];
    char ts[24];

    format_timestamp(ts, sizeof(ts));
    int n = snprintf(line, sizeof(line), "%s,%.2f,%.2f,%.2f\n", ts, T, P, H);
    if (n <= 0) return;

    size_t ln = (size_t)n;

    /* If single line is larger than buffer capacity, write directly */
    if (ln >= SD_BUFFER_SIZE)
    {
        if (sd_logging)
        {
            int w = sdcard_write(line, ln);
            (void)w;
        }
        return;
    }

    /* If not enough space in buffer, flush it first */
    if (sd_buf_pos + ln >= SD_BUFFER_SIZE)
    {
        if (sd_logging && sd_buf_pos > 0)
        {
            sdcard_write(sd_buffer, sd_buf_pos);
        }
        sd_buf_pos = 0;
    }

    /* Copy line into buffer (interrupts briefly disabled during pointer update) */
    memcpy(&sd_buffer[sd_buf_pos], line, ln);
    sd_buf_pos += ln;

    /* Flush if threshold reached */
    if (sd_logging && sd_buf_pos >= SD_FLUSH_THRESHOLD)
    {
        sdcard_write(sd_buffer, sd_buf_pos);
        sd_buf_pos = 0;
    }
}

/* Start logging: init SD, open file and write header */
int sd_log_start(void)
{
    if (sd_logging) return 0;

    if (sdcard_init() != 0)
    {
        dbg_print("SD init failed");
        return -1;
    }

    if (sdcard_open_append(SD_FILENAME) != 0)
    {
        dbg_print("SD open failed");
        return -2;
    }

    const char *hdr = "time,temperature,pressure,humidity\n";
    (void)sdcard_write(hdr, strlen(hdr));

    sd_buf_pos = 0;
    sd_logging = true;
    dbg_print("SD logging started");
    return 0;
}

/* Stop logging: flush and close */
void sd_log_stop(void)
{
    if (!sd_logging) return;

    if (sd_buf_pos > 0)
    {
        sdcard_write(sd_buffer, sd_buf_pos);
        sd_buf_pos = 0;
    }

    sdcard_close();
    sd_logging = false;
    dbg_print("SD logging stopped");
}

/* -----------------------------------------------------------
 * Weak default implementations of the low-level SD wrappers.
 * Replace these by providing non-weak functions in your SD module
 * (for example in sdcard_sdfat.c that uses the SdFat library).
 *
 * Signatures:
 *   int sdcard_init(void);                    // 0 == OK
 *   int sdcard_open_append(const char *fn);   // 0 == OK
 *   int sdcard_write(const void *buf, size_t len); // >=0 bytes written, <0 error
 *   void sdcard_close(void);
 *
 * The weak default returns errors so build/link works but runtime
 * will not actually write to SD until you implement real wrappers.
 * ----------------------------------------------------------- */

int __attribute__((weak)) sdcard_init(void)
{
    (void)0;
    return -1;
}

int __attribute__((weak)) sdcard_open_append(const char *filename)
{
    (void)filename;
    return -1;
}

int __attribute__((weak)) sdcard_write(const void *buf, size_t len)
{
    (void)buf;
    (void)len;
    return -1;
}

void __attribute__((weak)) sdcard_close(void)
{
    /* noop */
}
