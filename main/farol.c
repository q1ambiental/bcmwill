#include "farol.h"

static farol_state_t s_state = FAROL_OFF;

void farol_init(void) { s_state = FAROL_OFF; }

void farol_next(void) { s_state = (farol_state_t)((s_state + 1) & 0x03); }

void farol_set(farol_state_t s) { s_state = (farol_state_t)(s & 0x03); }

farol_state_t farol_get(void) { return s_state; }

const char *farol_get_str(void)
{
    switch (s_state) {
        case FAROL_OFF:      return "OFF";
        case FAROL_LANTERNA: return "LANTERNA";
        case FAROL_BAIXO:    return "BAIXO";
        case FAROL_ALTO:     return "ALTO";
    }
    return "?";
}