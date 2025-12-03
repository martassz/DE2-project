/**
 * @file sdlog.c
 * @brief SD Card Logging Implementation.
 *
 * Uses the Petit FatFs library to write data to a file named DATA.TXT
 * on the SD card.
 */

// Includes must be in this order
#include "ds1302.h"        // Time types
#include "loggerControl.h" // Access to g_time

#include "sdlog.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pff.h"
#include "diskio.h"
#include "uart.h"

/** @brief Name of the log file on the SD card. */
#define LOG_FILENAME "DATA.TXT"

/* --- Global Variables --- */
FATFS fs;                   /**< File system object */
volatile uint8_t flag_sd_toggle = 0; /**< Flag to request logging start/stop */
volatile uint8_t sd_logging = 0;     /**< Current logging state (1=active) */

void sd_log_init(void)
{
    sd_logging = 0;
    flag_sd_toggle = 0;
}

int sd_log_start(void)
{
    if (sd_logging) return 0;

    FRESULT res;

    uart_puts("SD: Mounting...\r\n");
    res = pf_mount(&fs);
    if (res != FR_OK) {
        uart_puts("SD: Mount Error!\r\n");
        return res;
    }

    uart_puts("SD: Opening file...\r\n");
    // File must exist and have pre-allocated size!
    res = pf_open(LOG_FILENAME);
    if (res != FR_OK) {
        uart_puts("SD: Open Error! (Check DATA.TXT)\r\n");
        return res;
    }

    // Rewind to beginning (Overwriting mode)
    res = pf_lseek(0);
    if (res != FR_OK) {
        uart_puts("SD: Seek Error!\r\n");
        return res;
    }

    sd_logging = 1;
    uart_puts("SD: Logging started.\r\n");
    return 0;
}

void sd_log_stop(void)
{
    if (!sd_logging) return;

    UINT bw;
    // Finalize write (flush incomplete sector)
    pf_write(0, 0, &bw);

    pf_mount(NULL); // Unmount

    sd_logging = 0;
    uart_puts("SD: Logging stopped.\r\n");
}

void sd_log_append_line(float T, float P, float H, uint16_t L)
{
    if (!sd_logging) return;

    char buffer[64];
    UINT bw;
    FRESULT res;

    // Convert float to int/dec parts manually to avoid heavy printf float support
    int t_int = (int)T;
    int t_dec = abs((int)((T - t_int) * 100));

    int p_int = (int)P;
    int p_dec = abs((int)((P - p_int) * 100));

    int h_int = (int)H;
    int h_dec = abs((int)((H - h_int) * 100));

    // Format: HH:MM:SS, Temp, Press, Hum, Light
    sprintf(buffer, "%02d:%02d:%02d, %d.%02d, %d.%02d, %d.%02d, %u\r\n",
            g_time.hh, g_time.mm, g_time.ss,
            t_int, t_dec,
            p_int, p_dec,
            h_int, h_dec,
            L);

    // Write to file
    res = pf_write(buffer, strlen(buffer), &bw);

    if (res != FR_OK) {
        uart_puts("SD: Write Error!\r\n");
    } else if (bw < strlen(buffer)) {
        uart_puts("SD: Disk Full or Error!\r\n");
        sd_log_stop();
    } else {
        uart_puts("LOG: ");
        uart_puts(buffer);
    }
}