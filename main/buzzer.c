#include "buzzer.h"
#include "bcm_config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void buzzer_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BUZZER_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(BUZZER_GPIO, 0);
}
void buzzer_on(void)  { gpio_set_level(BUZZER_GPIO, 1); }
void buzzer_off(void) { gpio_set_level(BUZZER_GPIO, 0); }
void buzzer_beep_ms(int ms)
{
    buzzer_on();
    vTaskDelay(pdMS_TO_TICKS(ms));
    buzzer_off();
}
void buzzer_click(void)
{
    buzzer_on();
    vTaskDelay(pdMS_TO_TICKS(30));
    buzzer_off();
}