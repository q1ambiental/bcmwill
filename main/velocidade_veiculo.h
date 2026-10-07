#ifndef VELOCIDADE_VEICULO_H
#define VELOCIDADE_VEICULO_H
#include <stdint.h>

void     velocidade_veiculo_init(void);
void     velocidade_veiculo_set(uint8_t kmh);
uint8_t  velocidade_veiculo_get(void);
#endif