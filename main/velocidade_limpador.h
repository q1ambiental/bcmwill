#ifndef VELOCIDADE_LIMPADOR_H
#define VELOCIDADE_LIMPADOR_H
#include <stdint.h>
#include "esp_err.h"

esp_err_t velocidade_limpador_init(void);
uint8_t   velocidade_limpador_get_raw(void);
uint8_t   velocidade_limpador_get_percent(void);
#endif