#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t oled_init(void);

void oled_clear(void);
void oled_update(void);

void oled_draw_pixel(int x, int y, int color);
void oled_draw_text(int x, int y, const char *str, int color);
void oled_draw_hline(int x, int y, int w, int color);
void oled_draw_vline(int x, int y, int h, int color);
void oled_draw_rect(int x, int y, int w, int h, int color);
void oled_draw_fill_rect(int x, int y, int w, int h, int color);
#endif