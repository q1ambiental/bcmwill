#ifndef SETA_H
#define SETA_H
#include <stdbool.h>
#include <stdint.h>

void    seta_init(void);
void    seta_toggle_esquerda(void);
void    seta_toggle_direita(void);
void    seta_set_esquerda(bool on);
void    seta_set_direita(bool on);
bool    seta_get_esquerda(void);
bool    seta_get_direita(void);
bool    seta_get_blink_phase(void);
void    seta_update(void);          /* chamar a cada ~10ms */
#endif