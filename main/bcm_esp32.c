#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "bcm_config.h"

#include "luminosidade.h"
#include "velocidade_limpador.h"
#include "dimmer_interno.h"
#include "seta.h"
#include "farol.h"
#include "trava_portas.h"
#include "pisca_alerta.h"
#include "velocidade_veiculo.h"
#include "can_bcm.h"
#include "oled_display.h"
#include "buzzer.h"
#include "leds.h"
#include "botao.h"
#include "encoder.h"
#include "adc_shared.h"

static const char *TAG = "BCM";

/* ---------- Desenha o painel do display ---------- */
static void draw_dashboard(void)
{
    char line[24];

    oled_clear();

    /* Barra de título */
    oled_draw_fill_rect(0, 0, OLED_WIDTH, 10, 1);
    oled_draw_text(2, 1, "BCM - Body Control", 0);

    snprintf(line, sizeof(line), "Luz : %u%%", luminosidade_get_percent());
    oled_draw_text(0, 12, line, 1);

    snprintf(line, sizeof(line), "Limp: %u%%", velocidade_limpador_get_percent());
    oled_draw_text(0, 22, line, 1);

    snprintf(line, sizeof(line), "Farol: %s", farol_get_str());
    oled_draw_text(0, 32, line, 1);

    snprintf(line, sizeof(line), "Portas: %s", trava_get_str());
    oled_draw_text(0, 42, line, 1);

    /* Seta esquerda / direita / pisca */
    if (pisca_alerta_get())                 oled_draw_text(0, 52, "SETA << >> (HAZ)", 1);
    else if (seta_get_esquerda())           oled_draw_text(0, 52, "SETA  <<  ", 1);
    else if (seta_get_direita())            oled_draw_text(0, 52, "SETA  >>  ", 1);
    else                                    oled_draw_text(0, 52, "SETA  --  ", 1);

    /* Coluna direita: velocidade do veículo (0x100) e dimmer */
    snprintf(line, sizeof(line), "%3u km/h", velocidade_veiculo_get());
    oled_draw_text(66, 12, line, 1);

    snprintf(line, sizeof(line), "Dim %3u%%", dimmer_get_percent());
    oled_draw_text(66, 32, line, 1);

    /* Indicador AutoLock */
    if (velocidade_veiculo_get() > BCM_AUTOLOCK_SPEED_KMH && trava_get())
        oled_draw_text(66, 42, "AUTOLOCK", 1);

    oled_update();
}

/* ---------- Aplica botões / encoder ao estado ---------- */
static void process_inputs(void)
{
    botao_update();
    encoder_update();

    if (botao_get_click_esq())   { seta_toggle_esquerda(); }
    if (botao_get_click_dir())   { seta_toggle_direita();  }
    if (botao_get_click_farol()) { farol_next();           }
    if (botao_get_click_trava()) { trava_toggle(); buzzer_click(); }

    enc_event_t ev = encoder_get_event();
    switch (ev) {
        case ENC_EVT_CW:    dimmer_inc(); break;
        case ENC_EVT_CCW:   dimmer_dec(); break;
        case ENC_EVT_CLICK: pisca_alerta_toggle(); buzzer_click(); break;
        default: break;
    }
}

/* ---------- Escolhe o modo visual da barra de LEDs ---------- */
static void update_leds_mode(void)
{
    if (pisca_alerta_get())             leds_set_mode(LED_MODE_PISCA_ALERTA);
    else if (seta_get_esquerda())       leds_set_mode(LED_MODE_SETA_ESQ);
    else if (seta_get_direita())        leds_set_mode(LED_MODE_SETA_DIR);
    else                                leds_set_mode(LED_MODE_DIMMER);

    leds_set_dimmer_level(dimmer_get_level_8());
}

/* ---------- Trava automática por velocidade (item 0x100) ---------- */
static void check_autolock(void)
{
    static bool já_travou = false;
    uint8_t v = velocidade_veiculo_get();

    if (v > BCM_AUTOLOCK_SPEED_KMH && !trava_get() && !já_travou) {
        trava_set(true);
        já_travou = true;
        buzzer_beep_ms(80);
        ESP_LOGW(TAG, "AUTOLOCK: velocidade=%u km/h -> portas travadas", v);
    }
    if (v <= 5) já_travou = false;   /* rearma quando para */
}

/* ---------- Buzzer "relay click" sincronizado com pisca ---------- */
static void update_buzzer(void)
{
    static int last_phase = -1;
    int phase = 0;
    if (pisca_alerta_get())      phase = seta_get_blink_phase();
    else if (seta_get_esquerda() || seta_get_direita())
                                 phase = seta_get_blink_phase();
    else                         phase = 0;

    if (phase != last_phase && (pisca_alerta_get() ||
                                seta_get_esquerda() || seta_get_direita())) {
        if (phase) buzzer_click();
        last_phase = phase;
    }
}

/* ---------- Task principal ---------- */
static void bcm_task(void *arg)
{
    (void)arg;
    int64_t last_tx = 0;
    int64_t last_draw = 0;

    while (1) {
        process_inputs();
        seta_update();
        leds_update();
        update_leds_mode();
        update_buzzer();
        check_autolock();

        int64_t now = esp_timer_get_time() / 1000;

        if (now - last_tx >= BCM_TX_PERIOD_MS) {
            last_tx = now;
            can_bcm_send_status();
        }
        if (now - last_draw >= 200) {
            last_draw = now;
            draw_dashboard();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ---------- Entry point ---------- */
void app_main(void)
{
    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "==== BCM iniciando ====");

    /* Inicialização de módulos */
    ESP_ERROR_CHECK(adc_shared_init());          /* cria o ADC uma vez só */
    ESP_ERROR_CHECK(oled_init());
    ESP_ERROR_CHECK(luminosidade_init());
    ESP_ERROR_CHECK(velocidade_limpador_init());

    buzzer_init();
    leds_init();
    botao_init();
    encoder_init();

    seta_init();
    farol_init();
    trava_init();
    dimmer_init();
    pisca_alerta_init();
    velocidade_veiculo_init();

    ESP_ERROR_CHECK(can_bcm_init());

    /* Task RX da CAN (0x100 - VelocidadeVeiculo) */
    ESP_ERROR_CHECK(xTaskCreate(can_bcm_rx_task, "can_rx", 4096, NULL, 5, NULL)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    /* Task principal do BCM */
    ESP_ERROR_CHECK(xTaskCreate(bcm_task, "bcm", 6144, NULL, 5, NULL)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_LOGI(TAG, "==== BCM pronto ====");
}