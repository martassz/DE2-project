/**
 * @file sdlog.h
 * @brief High-level interface for Data Logging to SD Card.
 *
 * Provides functions to initialize, start, stop, and append data to the log file.
 * Uses the Petit FatFs library for the underlying filesystem operations.
 *
 * @author Team DE2-Project
 * @date 2025
 */

#ifndef SDLOG_H
#define SDLOG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Flag set by the UI button to request a toggle of logging state.
 * 1 = Toggle requested, 0 = No request.
 */
extern volatile uint8_t flag_sd_toggle;

/**
 * @brief Current logging state.
 * 1 = Logging active, 0 = Logging stopped.
 */
extern volatile uint8_t sd_logging;

/**
 * @brief Initialize internal logging flags.
 * Does not mount the card yet.
 */
void sd_log_init(void);

/**
 * @brief Start the logging process.
 *
 * Mounts the file system, opens the "DATA.TXT" file, and seeks to the beginning (or end).
 * @return 0 on success, error code otherwise.
 */
int sd_log_start(void);

/**
 * @brief Stop the logging process.
 *
 * Finalizes the write operation (flushes buffer) and unmounts the file system.
 */
void sd_log_stop(void);

/**
 * @brief Append a formatted data line to the open log file.
 *
 * Formats the timestamp and sensor values into a CSV-like string and writes it to the SD card.
 *
 * @param T Temperature value.
 * @param P Pressure value.
 * @param H Humidity value.
 * @param L Light intensity value.
 */
void sd_log_append_line(float T, float P, float H, uint16_t L);

#endif /* SDLOG_H */