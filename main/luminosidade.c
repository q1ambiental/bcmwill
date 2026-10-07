#include "luminosidade.h"
#include "bcm_config.h"
#include "adc_shared.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "LUMINOSIDADE";

esp_err_t luminosidade_init(void)
{
    esp_err_t err = adc_shared_init();
    if (err != ESP_OK) return err;

    adc_oneshot_chan_cfg_t ch = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_oneshot_config_channel(adc_shared_get(),
                                     POT_LUMINOSIDADE_ADC_CH, &ch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config channel falhou: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "init ok (GPIO34 / ADC1_CH6)");
    return ESP_OK;
}

static int read_avg(void)
{
    int v = 0, sum = 0;
    for (int i = 0; i < 8; i++) {
        adc_oneshot_read(adc_shared_get(), POT_LUMINOSIDADE_ADC_CH, &v);
        sum += v;
    }
    return sum / 8;
}

uint8_t luminosidade_get_raw(void)
{
    return (uint8_t)((read_avg() * 255) / 4095);
}

uint8_t luminosidade_get_percent(void)
{
    return (uint8_t)(((uint32_t)luminosidade_get_raw() * 100) / 255);
}