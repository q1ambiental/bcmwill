#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>

/* Substitutos de hardware/RTOS para exercitar o código de produção no host.
 * A compatibilidade com a API real é validada separadamente pelo build IDF. */
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_TIMEOUT 3
#define IRAM_ATTR
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
typedef int gpio_num_t;
#define GPIO_NUM_NC -1
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(x) (x)
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
typedef struct { int token; } *SemaphoreHandle_t;
typedef struct { size_t size; int count; unsigned char item[64]; } *QueueHandle_t;
static int allocations;
static jmp_buf task_exit;
static bool run_task;
static SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    SemaphoreHandle_t s = calloc(1, sizeof(*s)); allocations++; return s;
}
static SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    SemaphoreHandle_t s = xSemaphoreCreateBinary(); s->token = 1; return s;
}
static int xSemaphoreGive(SemaphoreHandle_t s) { s->token = 1; return pdTRUE; }
static int xSemaphoreTake(SemaphoreHandle_t s, int timeout) {
    (void)timeout; if (!s->token) return pdFALSE; s->token = 0; return pdTRUE;
}
static int xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t *woken) {
    (void)woken; return xSemaphoreGive(s);
}
static void vSemaphoreDelete(SemaphoreHandle_t s) { free(s); allocations--; }
static QueueHandle_t xQueueCreate(int depth, size_t size) {
    (void)depth; QueueHandle_t q = calloc(1, sizeof(*q));
    q->size = size; allocations++; return q;
}
static void vQueueDelete(QueueHandle_t q) { free(q); allocations--; }
static int xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t *woken) {
    (void)woken; if (q->count) return pdFALSE;
    memcpy(q->item, item, q->size); q->count = 1; return pdTRUE;
}
static int xQueueReceive(QueueHandle_t q, void *item, int timeout) {
    (void)timeout;
    if (q->count) { memcpy(item, q->item, q->size); q->count = 0; return pdTRUE; }
    if (run_task) longjmp(task_exit, 1);
    return pdFALSE;
}
static void vTaskDelete(void *task) { (void)task; }
static int64_t esp_timer_get_time(void) { return 1000000; }

typedef void *twai_node_handle_t;
typedef struct { uint32_t id; uint16_t dlc; bool ide, rtr, fdf; } twai_frame_header_t;
typedef struct { twai_frame_header_t header; uint8_t *buffer; size_t buffer_len; } twai_frame_t;
typedef struct { bool is_tx_success; const twai_frame_t *done_tx_frame; } twai_tx_done_event_data_t;
typedef struct { int unused; } twai_rx_done_event_data_t;
typedef union { struct { uint32_t arb_lost:1, bit_err:1, form_err:1, stuff_err:1, ack_err:1; }; uint32_t val; } twai_error_flags_t;
typedef struct { twai_error_flags_t err_flags; } twai_error_event_data_t;
typedef struct {
    bool (*on_rx_done)(twai_node_handle_t, const twai_rx_done_event_data_t *, void *);
    bool (*on_tx_done)(twai_node_handle_t, const twai_tx_done_event_data_t *, void *);
    bool (*on_error)(twai_node_handle_t, const twai_error_event_data_t *, void *);
} twai_event_callbacks_t;
typedef struct {
    struct { int tx, rx, quanta_clk_out, bus_off_indicator; } io_cfg;
    struct { uint32_t bitrate; } bit_timing;
    int fail_retry_cnt, tx_queue_depth;
    struct { bool enable_loopback, no_receive_rtr; } flags;
} twai_onchip_node_config_t;
typedef struct { uint32_t id, mask; bool is_ext; } twai_mask_filter_config_t;
enum { TWAI_ERROR_ACTIVE, TWAI_ERROR_WARNING, TWAI_ERROR_PASSIVE, TWAI_ERROR_BUS_OFF };
typedef struct { int state; unsigned tx_error_count, rx_error_count; } twai_node_status_t;
typedef struct { uint32_t bus_err_num; } twai_node_record_t;
static twai_onchip_node_config_t saved_cfg;
static twai_event_callbacks_t callbacks;
static const twai_frame_t *pending_frame;
static twai_frame_header_t incoming;
static int init_error_stage, init_stage, submit_error, node_state, recover_calls;
static esp_err_t stage_result(void) { return ++init_stage == init_error_stage ? ESP_FAIL : ESP_OK; }
static esp_err_t twai_new_node_onchip(const twai_onchip_node_config_t *cfg, twai_node_handle_t *node) {
    saved_cfg = *cfg; esp_err_t err = stage_result(); if (!err) *node = (void *)1; return err;
}
static esp_err_t twai_node_register_event_callbacks(twai_node_handle_t node, const twai_event_callbacks_t *cbs, void *ctx) {
    (void)node; (void)ctx; callbacks = *cbs; return stage_result();
}
static esp_err_t twai_node_config_mask_filter(twai_node_handle_t node, int index, const twai_mask_filter_config_t *cfg) {
    (void)node; assert(index == 0 && cfg->id == 0x100 && cfg->mask == 0x7ff && !cfg->is_ext); return stage_result();
}
static esp_err_t twai_node_enable(twai_node_handle_t node) { (void)node; return stage_result(); }
static esp_err_t twai_node_delete(twai_node_handle_t node) { (void)node; return ESP_OK; }
static esp_err_t twai_node_transmit(twai_node_handle_t node, const twai_frame_t *frame, int timeout) {
    (void)node; (void)timeout; if (!submit_error) pending_frame = frame; return submit_error;
}
static esp_err_t twai_node_receive_from_isr(twai_node_handle_t node, twai_frame_t *frame) {
    (void)node; frame->header = incoming; memset(frame->buffer, 42, frame->buffer_len); return ESP_OK;
}
static esp_err_t twai_node_get_info(twai_node_handle_t node, twai_node_status_t *status, twai_node_record_t *record) {
    (void)node; *status = (twai_node_status_t){ .state = node_state }; *record = (twai_node_record_t){0}; return ESP_OK;
}
static esp_err_t twai_node_recover(twai_node_handle_t node) {
    (void)node; recover_calls++; node_state = TWAI_ERROR_ACTIVE; return ESP_OK;
}
