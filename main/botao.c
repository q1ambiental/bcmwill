#include "botao.h"
#include "bcm_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"

typedef struct {
    gpio_num_t pin;
    int  last;
    int64_t t_debounce;
    bool click;
} btn_t;

static btn_t b_esq   = { BTN_SETA_ESQ_GPIO, 1, 0, false };
static btn_t b_dir   = { BTN_SETA_DIR_GPIO, 1, 0, false };
static btn_t b_farol = { BTN_FAROL_GPIO,    1, 0, false };
static btn_t b_trava = { BTN_TRAVA_GPIO,    1, 0, false };

#define DEBOUNCE_MS 30

void botao_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL<<BTN_SETA_ESQ_GPIO)|(1ULL<<BTN_SETA_DIR_GPIO)|
                        (1ULL<<BTN_FAROL_GPIO)   |(1ULL<<BTN_TRAVA_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
}

static void update_btn(btn_t *b)
{
    int v = gpio_get_level(b->pin);
    int64_t now = esp_timer_get_time() / 1000;
    if (v != b->last && (now - b->t_debounce) > DEBOUNCE_MS) {
        b->t_debounce = now;
        if (b->last == 1 && v == 0) b->click = true;  /* falling edge */
        b->last = v;
    }
}

void botao_update(void)
{
    update_btn(&b_esq);
    update_btn(&b_dir);
    update_btn(&b_farol);
    update_btn(&b_trava);
}

static bool consume(btn_t *b) { bool v = b->click; b->click = false; return v; }

bool botao_get_click_esq(void)   { return consume(&b_esq); }
bool botao_get_click_dir(void)   { return consume(&b_dir); }
bool botao_get_click_farol(void) { return consume(&b_farol); }
bool botao_get_click_trava(void) { return consume(&b_trava); }