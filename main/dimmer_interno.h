#ifndef DIMMER_INTERNO_H
#define DIMMER_INTERNO_H
#include <stdint.h>

void    dimmer_init(void);
void    dimmer_inc(void);
void    dimmer_dec(void);
uint8_t dimmer_get_raw(void);       /* 0..255 */
uint8_t dimmer_get_percent(void);   /* 0..100 % */
uint8_t dimmer_get_level_8(void);   /* 0..8 (para a barra) */
#endif