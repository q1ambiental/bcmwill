#ifndef PISCA_ALERTA_H
#define PISCA_ALERTA_H
#include <stdbool.h>

void pisca_alerta_init(void);
void pisca_alerta_toggle(void);
void pisca_alerta_set(bool on);
bool pisca_alerta_get(void);
#endif