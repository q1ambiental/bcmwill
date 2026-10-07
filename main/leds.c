#include "leds.h"
#include "bcm_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static const gpio_num_t s_pins[LED_BAR_QTY] = {
    LED_BAR_GPIO_0, LED_BAR_GPIO_1, LED_BAR_GPIO_2, LED_BAR_GPIO_3,
    LED_BAR_GPIO_4, LED_BAR_GPIO_5, LED_BAR_GPIO_6, LED_BAR_GPIO_7
};

static led_mode_t s_mode = LED_MODE_OFF;
static uint8_t    s_dimmer = 0;
static int        s_step = 0;
static int64_t    s_last_ms = 0;

void leds_init(void)
{
    for (int i = 0; i < LED_BAR_QTY; i++) {
        gpio_config_t io = { .pin_bit_mask = 1ULL << s_pins[i],
                             .mode = GPIO_MODE_OUTPUT };
        gpio_config(&io);
        gpio_set_level(s_pins[i], 0);
    }
    s_last_ms = esp_timer_get_time() / 1000;
}

void leds_set_mode(led_mode_t m)
{
    if (m != s_mode) { s_mode = m; s_step = 0; }
}

void leds_set_dimmer_level(uint8_t level_0_8)
{
    if (level_0_8 > 8) level_0_8 = 8;
    s_dimmer = level_0_8;
}

static void all_off(void)
{
    for (int i = 0; i < LED_BAR_QTY; i++) gpio_set_level(s_pins[i], 0);
}

static void show_dimmer(void)
{
    for (int i = 0; i < LED_BAR_QTY; i++)
        gpio_set_level(s_pins[i], (i < s_dimmer) ? 1 : 0);
}

static void show_sequential(bool left_side)
{
    /* setas sequenciais: passo 0..7 acende LEDs da extremidade ao centro */
    all_off();
    int idx = s_step % LED_BAR_QTY;
    int led = left_side ? idx : (LED_BAR_QTY - 1 - idx);
    gpio_set_level(s_pins[led], 1);
}

static void show_pisca_alerta(bool phase)
{
    for (int i = 0; i < LED_BAR_QTY; i++)
        gpio_set_level(s_pins[i], phase ? 1 : 0);
}

void leds_update(void)
{
    int64_t now = esp_timer_get_time() / 1000;
    if (now - s_last_ms >= 120) { s_step++; s_last_ms = now; }

    switch (s_mode) {
        case LED_MODE_OFF:          all_off();              break;
        case LED_MODE_DIMMER:       show_dimmer();          break;
        case LED_MODE_SETA_ESQ:     show_sequential(true);  break;
        case LED_MODE_SETA_DIR:     show_sequential(false); break;
        case LED_MODE_PISCA_ALERTA: show_pisca_alerta(s_step & 1); break;
    }
}