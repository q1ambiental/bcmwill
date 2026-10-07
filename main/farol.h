#ifndef FAROL_H
#define FAROL_H
#include <stdint.h>

typedef enum {
    FAROL_OFF      = 0,
    FAROL_LANTERNA = 1,
    FAROL_BAIXO    = 2,
    FAROL_ALTO     = 3
} farol_state_t;

void         farol_init(void);
void         farol_next(void);
void         farol_set(farol_state_t s);
farol_state_t farol_get(void);
const char  *farol_get_str(void);
#endif