/*
 * sdlog_pff.c
 * CSV logging to SD using Petit FatFs (pff)
 */

#include "sdlog.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <uart.h>
#include "ds1302.h"
#include "loggerControl.h"
#include "pff.h"    // Petit FatFs header

/* --- Module state --- */
volatile uint8_t flag_sd_toggle = 0;
volatile bool sd_logging = false;

static char sd_buffer[SD_BUFFER_SIZE];
static size_t sd_buf_pos = 0;

/* Petit FatFs structures */
static FATFS fs;
static bool file_opened = false;

/* Filename for current log */
static char log_fname[13];   // 8.3 format

/* --- Helper functions --- */
static void dbg_print(const char *s) { uart_puts(s); uart_puts("\r\n"); }

static void format_timestamp(char *buf, size_t len)
{
    snprintf(buf, len, "%02u:%02u:%02u",
             (unsigned)g_time.hh, (unsigned)g_time.mm, (unsigned)g_time.ss);
}

/* --- SD Log API --- */
int sd_log_init(void)
{
    sd_buf_pos = 0;
    flag_sd_toggle = 0;
    sd_logging = false;
    file_opened = false;
    return 0;
}

void sd_log_append_line(float T, float P, float H, uint16_t L)
{
    char line[64];
    char ts[24];
    format_timestamp(ts, sizeof(ts));

    int n = snprintf(line, sizeof(line), "%s,%.2f,%.2f,%.2f,%u\n", ts, T, P, H, L);
    if(n <= 0) return;
    size_t ln = (size_t)n;

    if(ln >= SD_BUFFER_SIZE) {
        if(sd_logging && file_opened) {
            UINT bw;
            pf_write(line, ln, &bw);
        }
        return;
    }

    if(sd_buf_pos + ln >= SD_BUFFER_SIZE) {
        if(sd_logging && file_opened && sd_buf_pos > 0) {
            UINT bw;
            pf_write(sd_buffer, sd_buf_pos, &bw);
        }
        sd_buf_pos = 0;
    }

    memcpy(&sd_buffer[sd_buf_pos], line, ln);
    sd_buf_pos += ln;

    if(sd_logging && file_opened && sd_buf_pos >= SD_FLUSH_THRESHOLD) {
        UINT bw;
        pf_write(sd_buffer, sd_buf_pos, &bw);
        sd_buf_pos = 0;
    }
}

int sd_log_start(void)
{
    if(sd_logging) return 0;

    /* Mount filesystem */
    FRESULT fr = pf_mount(&fs);
    if(fr != FR_OK) {
        dbg_print("SD mount failed");
        return -1;
    }

    /* Prepare filename: 8.3 format */
    snprintf(log_fname, sizeof(log_fname), "%02u%02uLOG.TXT", g_time.hh, g_time.mm);

    /* Open file for append */
    fr = pf_open(log_fname);
    if(fr != FR_OK) {
        dbg_print("SD open failed");
        return -2;
    }
    file_opened = true;

    /* Write CSV header */
    const char *hdr = "time,temperature,pressure,humidity,light\n";
    UINT bw;
    pf_write(hdr, strlen(hdr), &bw);

    sd_buf_pos = 0;
    sd_logging = true;
    dbg_print("SD logging started");
    return 0;
}

void sd_log_stop(void)
{
    if(!sd_logging) return;

    if(file_opened && sd_buf_pos > 0) {
        UINT bw;
        pf_write(sd_buffer, sd_buf_pos, &bw);
        sd_buf_pos = 0;
    }

    /* Petit FatFs doesn't require explicit close */
    file_opened = false;
    sd_logging = false;
    dbg_print("SD logging stopped");
}
