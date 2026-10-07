#include "velocidade_veiculo.h"

static uint8_t s_kmh = 0;

void velocidade_veiculo_init(void)     { s_kmh = 0; }
void velocidade_veiculo_set(uint8_t v) { s_kmh = v; }
uint8_t velocidade_veiculo_get(void)   { return s_kmh; }