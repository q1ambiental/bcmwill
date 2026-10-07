#include "can_bcm.h"
#include "bcm_config.h"

/* API nova do TWAI no IDF 6.x */
#include "esp_twai.h"
#include "esp_twai_onchip.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <inttypes.h>
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
/* O driver guarda ponteiros: só reutilizar após on_tx_done. */
static uint8_t s_tx_data[8];
static twai_frame_t s_tx_frame = {
    .header.id = CAN_ID_BCM_STATUS,
    .header.dlc = 8,
    .buffer = s_tx_data,
    .buffer_len = sizeof(s_tx_data),
};
static SemaphoreHandle_t s_tx_lock;
static SemaphoreHandle_t s_tx_available;
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_tx_ok, s_tx_failed, s_rx_dropped, s_ack_errors;
static uint32_t s_last_error_flags;

static bool IRAM_ATTR can_tx_done_cb(twai_node_handle_t handle,
                                    const twai_tx_done_event_data_t *edata,
                                    void *user_ctx)
{
    (void)handle; (void)user_ctx;
    BaseType_t hp_woken = pdFALSE;
    portENTER_CRITICAL_ISR(&s_stats_lock);
    if (edata->is_tx_success) s_tx_ok++;
    else s_tx_failed++;
    portEXIT_CRITICAL_ISR(&s_stats_lock);
    xSemaphoreGiveFromISR(s_tx_available, &hp_woken);
    return hp_woken == pdTRUE;
}

static bool IRAM_ATTR can_error_cb(twai_node_handle_t handle,
                                  const twai_error_event_data_t *edata,
                                  void *user_ctx)
{
    (void)handle; (void)user_ctx;
    portENTER_CRITICAL_ISR(&s_stats_lock);
    if (edata->err_flags.ack_err) s_ack_errors++;
    s_last_error_flags = edata->err_flags.val;
    portEXIT_CRITICAL_ISR(&s_stats_lock);
    return false;
}

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

    twai_frame_t frame = {0};
    uint8_t buf[8] = {0};
    frame.buffer = buf;
    frame.buffer_len = sizeof(buf);

    if (twai_node_receive_from_isr(handle, &frame) == ESP_OK) {
        /* RTR não contém velocidade; ID estendido não é o protocolo BCM. */
        if (frame.header.ide || frame.header.rtr || frame.header.fdf ||
            frame.header.id != CAN_ID_VEHICLE_SPEED || frame.header.dlc < 1 ||
            frame.header.dlc > 8) return false;
        item.id  = frame.header.id;
        item.dlc = frame.header.dlc;
        if (item.dlc > 8) item.dlc = 8;
        memcpy(item.data, buf, item.dlc);
        if (xQueueSendFromISR(s_rx_queue, &item, &hp_woken) != pdTRUE) {
            portENTER_CRITICAL_ISR(&s_stats_lock);
            s_rx_dropped++;
            portEXIT_CRITICAL_ISR(&s_stats_lock);
        }
    }
    return (hp_woken == pdTRUE);
}

/* -------------------------------------------------------------------- */
/*  Inicialização do nó TWAI na API nova                                */
/* -------------------------------------------------------------------- */
esp_err_t can_bcm_init(void)
{
    if (s_node_hdl != NULL) return ESP_ERR_INVALID_STATE;
    s_rx_queue = xQueueCreate(16, sizeof(can_rx_item_t));
    s_tx_lock = xSemaphoreCreateMutex();
    s_tx_available = xSemaphoreCreateBinary();
    if (!s_rx_queue || !s_tx_lock || !s_tx_available) {
        if (s_rx_queue) vQueueDelete(s_rx_queue);
        if (s_tx_lock) vSemaphoreDelete(s_tx_lock);
        if (s_tx_available) vSemaphoreDelete(s_tx_available);
        s_rx_queue = NULL;
        s_tx_lock = s_tx_available = NULL;
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(s_tx_available);

    twai_onchip_node_config_t node_cfg = {
        .io_cfg.tx = (gpio_num_t)CAN_TX_GPIO,
        .io_cfg.rx = (gpio_num_t)CAN_RX_GPIO,
        .io_cfg.quanta_clk_out = GPIO_NUM_NC,
        .io_cfg.bus_off_indicator = GPIO_NUM_NC,
        .bit_timing.bitrate = CAN_BITRATE,
        .fail_retry_cnt = 0, /* uma tentativa; o próximo status substitui o antigo */
        .tx_queue_depth     = 1,
        .flags.enable_loopback = 0,
        .flags.no_receive_rtr = 1,
    };

    esp_err_t err = twai_new_node_onchip(&node_cfg, &s_node_hdl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_new_node_onchip: %s", esp_err_to_name(err));
        goto fail;
    }

    twai_event_callbacks_t cbs = {
        .on_rx_done = can_rx_done_cb,
        .on_tx_done = can_tx_done_cb,
        .on_error = can_error_cb,
    };
    err = twai_node_register_event_callbacks(s_node_hdl, &cbs, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register_event_callbacks: %s", esp_err_to_name(err));
        goto fail;
    }

    const twai_mask_filter_config_t filter = {
        .id = CAN_ID_VEHICLE_SPEED,
        .mask = 0x7FF,
        .is_ext = false,
    };
    err = twai_node_config_mask_filter(s_node_hdl, 0, &filter);
    if (err != ESP_OK) goto fail;

    err = twai_node_enable(s_node_hdl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "twai_node_enable: %s", esp_err_to_name(err));
        goto fail;
    }

    ESP_LOGI(TAG, "TWAI pronto %d bps TX=IO%d RX=IO%d; exige ACK de outro no",
             CAN_BITRATE, CAN_TX_GPIO, CAN_RX_GPIO);
    return ESP_OK;

fail:
    if (s_node_hdl) twai_node_delete(s_node_hdl);
    s_node_hdl = NULL;
    vQueueDelete(s_rx_queue);
    vSemaphoreDelete(s_tx_lock);
    vSemaphoreDelete(s_tx_available);
    s_rx_queue = NULL;
    s_tx_lock = s_tx_available = NULL;
    return err;
}

/* -------------------------------------------------------------------- */
/*  TX: monta os 8 bytes do 0x200 e envia via API nova                  */
/* -------------------------------------------------------------------- */
esp_err_t can_bcm_send_status(void)
{
    if (s_node_hdl == NULL) return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(s_tx_lock, 0) != pdTRUE) return ESP_ERR_TIMEOUT;
    if (xSemaphoreTake(s_tx_available, 0) != pdTRUE) {
        xSemaphoreGive(s_tx_lock);
        return ESP_ERR_TIMEOUT;
    }
    uint8_t *d = s_tx_data;
    memset(d, 0, sizeof(s_tx_data));

    d[0] = luminosidade_get_raw();
    d[1] = velocidade_limpador_get_raw();
    d[2] = dimmer_get_raw();

    if (seta_get_esquerda()) d[3] |= (1u << 0);
    if (seta_get_direita())  d[3] |= (1u << 1);
    if (trava_get())         d[3] |= (1u << 2);
    if (pisca_alerta_get())  d[3] |= (1u << 3);

    d[4] = (uint8_t)(farol_get() & 0x03);

    esp_err_t err = twai_node_transmit(s_node_hdl, &s_tx_frame, 0);
    if (err != ESP_OK) xSemaphoreGive(s_tx_available);
    xSemaphoreGive(s_tx_lock);
    /* ESP_OK significa enfileirado, não ACK; on_tx_done registra o resultado. */
    return err;
}

/* -------------------------------------------------------------------- */
/*  Task de RX: escuta a fila e reage aos IDs que interessam            */
/* -------------------------------------------------------------------- */
void can_bcm_rx_task(void *arg)
{
    (void)arg;
    can_rx_item_t item;
    bool recovering = false;
    int64_t last_diag_ms = 0;
    if (!s_node_hdl || !s_rx_queue) { vTaskDelete(NULL); return; }

    while (1) {
        twai_node_status_t status;
        twai_node_record_t record;
        if (twai_node_get_info(s_node_hdl, &status, &record) == ESP_OK) {
            if (status.state == TWAI_ERROR_BUS_OFF && !recovering) {
                ESP_LOGE(TAG, "BUS-OFF: verificar ACK, bitrate, alimentacao e ligacao CAN");
                esp_err_t err = twai_node_recover(s_node_hdl);
                recovering = (err == ESP_OK);
                if (err != ESP_OK) ESP_LOGE(TAG, "recover: %s", esp_err_to_name(err));
            } else if (recovering && status.state == TWAI_ERROR_ACTIVE) {
                recovering = false;
                ESP_LOGI(TAG, "CAN recuperada");
            }
            int64_t now_ms = esp_timer_get_time() / 1000;
            if (now_ms - last_diag_ms >= 1000) {
                uint32_t ok, failed, dropped, ack, flags;
                portENTER_CRITICAL(&s_stats_lock);
                ok = s_tx_ok; failed = s_tx_failed; dropped = s_rx_dropped;
                ack = s_ack_errors; flags = s_last_error_flags;
                portEXIT_CRITICAL(&s_stats_lock);
                ESP_LOGI(TAG, "CAN estado=%d TEC=%u REC=%u erros=%" PRIu32
                         " TX_ACK=%" PRIu32 " TX_falha=%" PRIu32 " RX_perdidos=%" PRIu32
                         " ACK_erros=%" PRIu32 " ultimo_erro=0x%" PRIx32,
                         status.state, status.tx_error_count, status.rx_error_count,
                         record.bus_err_num, ok, failed, dropped, ack, flags);
                last_diag_ms = now_ms;
            }
        }
        if (xQueueReceive(s_rx_queue, &item, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (item.id == CAN_ID_VEHICLE_SPEED && item.dlc >= 1) {
                uint8_t kmh = item.data[0];   /* factor 1, offset 0 */
                velocidade_veiculo_set(kmh);
                ESP_LOGI(TAG, "0x100 VelocidadeVeiculo = %u km/h", kmh);
            }
        }
    }
}
