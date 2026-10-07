#include "dimmer_interno.h"

static uint8_t s_raw = 128;         /* começa em ~50 % */

void dimmer_init(void) { s_raw = 128; }

void dimmer_inc(void)
{
    if (s_raw <= 255 - 8) s_raw += 8;
    else s_raw = 255;
}
void dimmer_dec(void)
{
    if (s_raw >= 8) s_raw -= 8;
    else s_raw = 0;
}
uint8_t dimmer_get_raw(void)     { return s_raw; }
uint8_t dimmer_get_percent(void) { return (uint8_t)(((uint32_t)s_raw * 100) / 255); }
uint8_t dimmer_get_level_8(void) { return (uint8_t)(((uint32_t)s_raw * 8 + 127) / 255); }