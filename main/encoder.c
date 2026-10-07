#include "encoder.h"
#include "bcm_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static int s_last_clk = 1;
static int s_last_sw  = 1;
static int64_t s_t_sw = 0;
static enc_event_t s_evt = ENC_EVT_NONE;

void encoder_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL<<ENC_CLK_GPIO)|(1ULL<<ENC_DT_GPIO)|(1ULL<<ENC_SW_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    s_last_clk = gpio_get_level(ENC_CLK_GPIO);
    s_last_sw  = gpio_get_level(ENC_SW_GPIO);
}

void encoder_update(void)
{
    int clk = gpio_get_level(ENC_CLK_GPIO);
    int dt  = gpio_get_level(ENC_DT_GPIO);
    int sw  = gpio_get_level(ENC_SW_GPIO);

    /* detecção de rotação: transição de descida em CLK */
    if (s_last_clk == 1 && clk == 0) {
        s_evt = (dt == 0) ? ENC_EVT_CW : ENC_EVT_CCW;
    }
    s_last_clk = clk;

    /* clique do botão */
    int64_t now = esp_timer_get_time() / 1000;
    if (s_last_sw == 1 && sw == 0 && (now - s_t_sw) > 50) {
        s_evt = ENC_EVT_CLICK;
        s_t_sw = now;
    }
    s_last_sw = sw;
}

enc_event_t encoder_get_event(void)
{
    enc_event_t e = s_evt;
    s_evt = ENC_EVT_NONE;
    return e;
}