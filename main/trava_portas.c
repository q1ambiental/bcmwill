#include "trava_portas.h"

static bool s_travada = false;

void trava_init(void)      { s_travada = false; }
void trava_toggle(void)    { s_travada = !s_travada; }
void trava_set(bool t)     { s_travada = t; }
bool trava_get(void)       { return s_travada; }
const char *trava_get_str(void) { return s_travada ? "TRAVADA" : "ABERTA"; }