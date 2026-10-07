#ifndef ADC_SHARED_H
#define ADC_SHARED_H

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

/* Cria (uma vez) a unidade ADC1 e devolve o handle. Idempotente. */
esp_err_t adc_shared_init(void);

/* Handle global do ADC1 — use nos outros módulos */
adc_oneshot_unit_handle_t adc_shared_get(void);

#endif