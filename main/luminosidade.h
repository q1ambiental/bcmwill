#ifndef LUMINOSIDADE_H
#define LUMINOSIDADE_H
#include <stdint.h>
#include "esp_err.h"

esp_err_t luminosidade_init(void);
uint8_t   luminosidade_get_raw(void);      /* 0..255 -> CAN      */
uint8_t   luminosidade_get_percent(void);  /* 0..100 %           */
#endif