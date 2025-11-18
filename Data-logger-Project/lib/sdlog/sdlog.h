#ifndef SDLOG_H
#define SDLOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * SD logging (CSV) public API
 *
 * - Buffered CSV logging: timestamp,temperature,pressure,humidity\n
 * - Encoder short-press toggles logging via a shared flag: flag_sd_toggle
 *
 * Configuration macros (override before include if needed):
 *   SD_BUFFER_SIZE      - RAM buffer size in bytes (default 256)
 *   SD_FLUSH_THRESHOLD  - flush when buffer >= this many bytes (default 128)
 *   SD_FILENAME         - file name to append to (default "LOG.TXT")
 */

#ifndef SD_BUFFER_SIZE
#define SD_BUFFER_SIZE      256
#endif

#ifndef SD_FLUSH_THRESHOLD
#define SD_FLUSH_THRESHOLD  128
#endif

#ifndef SD_FILENAME
#define SD_FILENAME         "LOG.TXT"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Shared toggle flag set by encoder ISR/poll (short press) */
extern volatile uint8_t flag_sd_toggle;

/* Public state: true while logging is active */
extern volatile bool sd_logging;

/* Initialize logging internals (optionally called implicitly by sd_log_start) */
int sd_log_init(void);

/* Start logging: init SD, open file for append and write header. Returns 0 on success. */
int sd_log_start(void);

/* Stop logging: flush buffer and close file */
void sd_log_stop(void);

/* Append a single measurement line (buffered). Safe to call from main context only. */
void sd_log_append_line(float T, float P, float H);

/* --- Low-level SD wrapper prototypes (implement with SdFat, FatFs, ...) --- */
/* These have weak implementations in sdlog.c — override them in your SD module. */
int sdcard_init(void);
int sdcard_open_append(const char *filename);
int sdcard_write(const void *buf, size_t len);
void sdcard_close(void);

#ifdef __cplusplus
}
#endif

#endif /* SDLOG_H */
