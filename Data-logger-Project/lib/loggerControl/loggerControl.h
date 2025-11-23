#ifndef LOGGER_CONTROL_H
#define LOGGER_CONTROL_H

#include <stdint.h>

/* Types */

typedef struct {
    uint8_t h;
    uint8_t m;
    uint8_t s;
} rtc_time_t;

/* External shared variables */
extern volatile uint8_t lcdValue;
extern volatile uint8_t flag_update_lcd;

extern volatile float g_T;
extern volatile float g_P;
extern volatile float g_H;

extern volatile rtc_time_t g_time;

/* Public API */

void logger_display_init(void);
void logger_display_draw(void);

void logger_encoder_init(void);
void logger_encoder_poll(void);

void logger_rtc_read_time(void);

#endif
