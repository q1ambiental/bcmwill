#ifndef BOTAO_H
#define BOTAO_H
#include <stdbool.h>

void botao_init(void);
void botao_update(void);          /* chamar a cada ~10ms */

/* retorna true UMA vez quando a tecla for solta (click) */
bool botao_get_click_esq(void);
bool botao_get_click_dir(void);
bool botao_get_click_farol(void);
bool botao_get_click_trava(void);
#endif