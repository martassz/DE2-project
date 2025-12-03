/**
 * @file loggerControl.h
 * @brief Controller interface for User Interface and System State.
 *
 * This module acts as the central hub for the application state. It manages:
 * - The shared system time (RTC).
 * - The shared sensor values (Temperature, Pressure, Humidity, Light).
 * - The LCD display and Rotary Encoder inputs.
 * @defgroup app_logic Application Logic
 * @brief High-level application control and UI handling.
 * @{
 */

#ifndef LOGGER_CONTROL_H
#define LOGGER_CONTROL_H

#include <stdint.h>
#include "ds1302.h"   /* DS1302 time type (ds1302_time_t) and API */

/* --- Global Variables (Shared State) --- */

/** @brief Global shared temperature value in degrees Celsius. */
extern volatile float g_T;

/** @brief Global shared pressure value in hPa. */
extern volatile float g_P;

/** @brief Global shared humidity value in %. */
extern volatile float g_H;

/** @brief Global shared light intensity (raw or percentage). */
extern volatile uint16_t g_Light;

/**
 * @brief Simplified structure for holding system time (HH:MM:SS).
 * Used for display and logging purposes to save RAM compared to full RTC struct.
 */
typedef struct {
    uint8_t hh; /**< Hours (0-23) */
    uint8_t mm; /**< Minutes (0-59) */
    uint8_t ss; /**< Seconds (0-59) */
} rtc_time_t;

/** @brief Global shared system time. Updated periodically from RTC. */
extern volatile rtc_time_t g_time;

/* --- UI Control Variables --- */

/**
 * @brief Current value index displayed on LCD.
 * 0 = Temperature, 1 = Pressure, 2 = Humidity, 3 = Light.
 */
extern volatile uint8_t lcdValue;

/**
 * @brief Flag indicating a request to redraw the LCD.
 * Set to 1 by the encoder ISR or periodic timer when data changes.
 */
extern volatile uint8_t flag_update_lcd;

/* --- Function Prototypes --- */

/**
 * @brief Initialize the LCD display and show the welcome screen.
 */
void logger_display_init(void);

/**
 * @brief Render the current screen content to the I2C LCD.
 *
 * Reads the `g_time` and the sensor value selected by `lcdValue`.
 * Also displays the recording status icon.
 */
void logger_display_draw(void);

/**
 * @brief Initialize rotary encoder GPIO pins and internal pull-ups.
 */
void logger_encoder_init(void);

/**
 * @brief Poll the rotary encoder status.
 *
 * This function handles the state machine for rotation (debouncing)
 * and checks the button press state. It updates `lcdValue` and `flag_update_lcd`.
 * Should be called frequently from the main loop.
 */
void logger_encoder_poll(void);

/**
 * @brief Read current time from DS1302 RTC and update the global `g_time` structure.
 * Uses I2C/TWI communication (note: standard DS1302 is SPI-like, ensuring driver match).
 */
void logger_rtc_read_time(void);

#endif /* LOGGER_CONTROL_H */

/** @} */