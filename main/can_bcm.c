#include "can_bcm.h"
#include "bcm_config.h"

/* API nova do TWAI no IDF 6.x */
#include "esp_twai.h"
#include "esp_twai_onchip.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>

/* Subsistemas do BCM (todos os getters) */
#include "luminosidade.h"
#include "velocidade_limpador.h"
#include "dimmer_interno.h"
#include "seta.h"
#include "trava_portas.h"
#include "farol.h"
#include "pisca_alerta.h"
#include "velocidade_veiculo.h"

static const char *TAG = "CAN_BCM";

/* Handle global do nó TWAI */
static twai_node_handle_t s_node_hdl = NULL;

/* Fila de RX: mensagens recebidas do barramento */
typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} can_rx_item_t;

static QueueHandle_t s_rx_queue = NULL;

/* -------------------------------------------------------------------- */
/*  RX callback (chamado do contexto de ISR quando chega frame)          */
/* -------------------------------------------------------------------- */
static bool IRAM_ATTR can_rx_done_cb(twai_node_handle_t handle,
                                     const twai_rx_done_event_data_t *edata,
                                     void *user_ctx)
{
    (void)edata; (void)user_ctx;
    BaseType_t hp_woken = pdFALSE;
    can_rx_item_t item = {0};

    twai_frame_t frame;
    uint8_t buf[8];
    frame.buffer = buf;
    frame.buffer_len = sizeof(buf);

    if (twai_node_receive_from_isr(handle, &frame) == ESP_OK) {
        item.id  = frame.header.id;
        item.dlc = frame.header.dlc;
        if (item.dlc > 8) item.dlc = 8;
        memcpy(item.data, buf, item.dlc);
        if (s_rx_queue) xQueueSendFromISR(s_rx_queue, &item, &hp_woken);
    }
    return (hp_woken == pdTRUE);
}

/* -------------------------------------------------------------------- */
/*  Inicialização do nó TWAI na API nova                                */
/* -------------------------------------------------------------------- */
esp_err_t can_bcm_init(void)
{
    s_rx_queue = xQueueCreate(16, sizeof(can_rx_item_t));
    if (s_rx_queue == NULL) {
        ESP_LOGE(TAG, "falha criando fila de RX");
        return ESP_FAIL;
    }

    twai_onchip_node_config_t node_cfg = {
        .io_cfg.tx = (gpio_num_t)CAN_TX_GPIO,
        .io_cfg.rx = (gpio_num_t)CAN_RX_GPIO,
        .bit_timing.bitrate = 500000,   /* 500 kbps */
        .tx_queue_depth     = 10,
        .flags.enable_loopback = 1,
    };

    esp_err_t err = twai_new_node_onchip(&node_cfg, &s_node_hdl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_new_node_onchip: %s", esp_err_to_name(err));
        return err;
    }

    twai_event_callbacks_t cbs = {
        .on_rx_done = can_rx_done_cb,
    };
    err = twai_node_register_event_callbacks(s_node_hdl, &cbs, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register_event_callbacks: %s", esp_err_to_name(err));
        return err;
    }

    err = twai_node_enable(s_node_hdl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_node_enable: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "TWAI pronto 500kbps  TX=IO%d  RX=IO%d",
             CAN_TX_GPIO, CAN_RX_GPIO);
    return ESP_OK;
}

/* -------------------------------------------------------------------- */
/*  TX: monta os 8 bytes do 0x200 e envia via API nova                  */
/* -------------------------------------------------------------------- */
esp_err_t can_bcm_send_status(void)
{
    if (s_node_hdl == NULL) return ESP_ERR_INVALID_STATE;

    uint8_t d[8] = {0};

    d[0] = luminosidade_get_raw();
    d[1] = velocidade_limpador_get_raw();
    d[2] = dimmer_get_raw();

    if (seta_get_esquerda()) d[3] |= (1u << 0);
    if (seta_get_direita())  d[3] |= (1u << 1);
    if (trava_get())         d[3] |= (1u << 2);
    if (pisca_alerta_get())  d[3] |= (1u << 3);

    d[4] = (uint8_t)(farol_get() & 0x03);

    /* Novo formato de frame */
    twai_frame_t frame = {
        .header.id  = CAN_ID_BCM_STATUS,
        .header.dlc = 8,
        .buffer     = d,
        .buffer_len = 8,
    };

    esp_err_t err = twai_node_transmit(s_node_hdl, &frame, 10 /* ms */);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "tx 0x200 falhou: %s", esp_err_to_name(err));
    }
    return err;
}

/* -------------------------------------------------------------------- */
/*  Task de RX: escuta a fila e reage aos IDs que interessam            */
/* -------------------------------------------------------------------- */
void can_bcm_rx_task(void *arg)
{
    (void)arg;
    can_rx_item_t item;

    while (1) {
        if (xQueueReceive(s_rx_queue, &item, portMAX_DELAY) == pdTRUE) {
            if (item.id == CAN_ID_VEHICLE_SPEED && item.dlc >= 1) {
                uint8_t kmh = item.data[0];   /* factor 1, offset 0 */
                velocidade_veiculo_set(kmh);
                ESP_LOGI(TAG, "0x100 VelocidadeVeiculo = %u km/h", kmh);
            }
        }
    }
}