#include "adc_shared.h"
#include "esp_log.h"

static const char *TAG = "ADC_SHARED";
static adc_oneshot_unit_handle_t s_adc = NULL;
static bool s_initialized = false;

esp_err_t adc_shared_init(void)
{
    if (s_initialized) return ESP_OK;

    adc_oneshot_unit_init_cfg_t unit = { .unit_id = ADC_UNIT_1 };
    esp_err_t err = adc_oneshot_new_unit(&unit, &s_adc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit falhou: %s", esp_err_to_name(err));
        return err;
    }
    s_initialized = true;
    ESP_LOGI(TAG, "ADC1 criado com sucesso");
    return ESP_OK;
}

adc_oneshot_unit_handle_t adc_shared_get(void)
{
    return s_adc;
}