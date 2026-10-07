#include "pisca_alerta.h"

static bool s_on = false;

void pisca_alerta_init(void)   { s_on = false; }
void pisca_alerta_toggle(void) { s_on = !s_on; }
void pisca_alerta_set(bool v)  { s_on = v; }
bool pisca_alerta_get(void)    { return s_on; }