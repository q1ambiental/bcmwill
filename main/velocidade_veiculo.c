#include "velocidade_veiculo.h"
#include <stdatomic.h>

static _Atomic uint8_t s_kmh = 0;

void velocidade_veiculo_init(void)     { atomic_store(&s_kmh, 0); }
void velocidade_veiculo_set(uint8_t v) { atomic_store(&s_kmh, v); }
uint8_t velocidade_veiculo_get(void)   { return atomic_load(&s_kmh); }
