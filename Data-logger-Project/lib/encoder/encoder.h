#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <avr/io.h>

/*
 * Encoder + minimal LCD driver module.
 *
 * Public API:
 *   encoder_init()            - initialize LCD (minimal) and encoder pins
 *   encoder_timer_init()      - configure TIMER2 for ~1 s display refresh
 *   encoder_poll()            - poll the rotary encoder (call frequently)
 *   encoder_set_values(T,P,H) - update measured values for the display
 *   encoder_request_redraw()  - request immediate screen refresh
 *   encoder_draw_if_needed()  - redraw LCD only if a refresh was requested
 *   encoder_get_selected()    - returns selected display index (0–2)
 *
 * Pin configuration (can be overridden before include):
 *   ENC_SW, ENC_DT, ENC_CLK   - bit numbers of encoder pins
 *   ENC_PORT_REG, ENC_DDR_REG, ENC_PIN_REG - encoder port registers
 *
 * This module expects the main program to define:
 *   extern volatile uint32_t g_millis;
 * which must be incremented by Timer0 every ~1 ms.
 */

#ifndef ENC_SW
#define ENC_SW   PC0
#endif
#ifndef ENC_DT
#define ENC_DT   PC1
#endif
#ifndef ENC_CLK
#define ENC_CLK  PC2
#endif

#ifndef ENC_PORT_REG
#define ENC_PORT_REG  PORTB
#endif
#ifndef ENC_DDR_REG
#define ENC_DDR_REG   DDRB
#endif
#ifndef ENC_PIN_REG
#define ENC_PIN_REG   PINB
#endif

#ifdef __cplusplus
extern "C" {
#endif

void encoder_init(void);
void encoder_timer_init(void);
void encoder_poll(void);
void encoder_set_values(float T, float P, float H);
void encoder_request_redraw(void);
void encoder_draw_if_needed(void);
uint8_t encoder_get_selected(void);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_H */
