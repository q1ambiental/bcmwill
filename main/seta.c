#include "seta.h"
#include "esp_timer.h"

#define BLINK_PERIOD_MS 500

static bool s_esq = false, s_dir = false;
static bool s_phase = false;
static int64_t s_last_toggle_ms = 0;

void seta_init(void)
{
    s_esq = s_dir = s_phase = false;
    s_last_toggle_ms = esp_timer_get_time() / 1000;
}

void seta_toggle_esquerda(void)
{
    s_esq = !s_esq;
    if (s_esq) s_dir = false;       /* mutuamente exclusivas */
    s_last_toggle_ms = esp_timer_get_time() / 1000;
    s_phase = true;
}
void seta_toggle_direita(void)
{
    s_dir = !s_dir;
    if (s_dir) s_esq = false;
    s_last_toggle_ms = esp_timer_get_time() / 1000;
    s_phase = true;
}
void seta_set_esquerda(bool on) { s_esq = on; if (on) s_dir = false; }
void seta_set_direita(bool on)  { s_dir = on; if (on) s_esq = false; }
bool seta_get_esquerda(void)    { return s_esq; }
bool seta_get_direita(void)     { return s_dir;  }
bool seta_get_blink_phase(void) { return s_phase; }

void seta_update(void)
{
    if (!s_esq && !s_dir) { s_phase = false; return; }
    int64_t now = esp_timer_get_time() / 1000;
    if ((now - s_last_toggle_ms) >= BLINK_PERIOD_MS) {
        s_phase = !s_phase;
        s_last_toggle_ms = now;
    }
}