#ifndef LEDS_H
#define LEDS_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LED_MODE_DIMMER = 0,   /* barra = nível de brilho */
    LED_MODE_SETA_ESQ,
    LED_MODE_SETA_DIR,
    LED_MODE_PISCA_ALERTA,
    LED_MODE_OFF
} led_mode_t;

void leds_init(void);
void leds_set_mode(led_mode_t m);
void leds_set_dimmer_level(uint8_t level_0_8);
void leds_update(void);        /* chamar a cada ~20ms */
#endif