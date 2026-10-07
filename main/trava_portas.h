#ifndef TRAVA_PORTAS_H
#define TRAVA_PORTAS_H
#include <stdbool.h>

void trava_init(void);
void trava_toggle(void);
void trava_set(bool travada);
bool trava_get(void);          /* true = travada */
const char *trava_get_str(void);
#endif