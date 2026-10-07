#include "mock.h"
#include "../../main/can_bcm.c"
#include "../../main/velocidade_veiculo.c"

static uint8_t brightness = 10;
uint8_t luminosidade_get_raw(void) { return brightness; }
uint8_t velocidade_limpador_get_raw(void) { return 20; }
uint8_t dimmer_get_raw(void) { return 30; }
bool seta_get_esquerda(void) { return true; }
bool seta_get_direita(void) { return false; }
bool trava_get(void) { return true; }
bool pisca_alerta_get(void) { return false; }
farol_state_t farol_get(void) { return FAROL_ALTO; }

static void finish_tx(bool success) {
    twai_tx_done_event_data_t event = { success, pending_frame };
    callbacks.on_tx_done(s_node_hdl, &event, NULL);
}
static void poll_rx_task(void) {
    run_task = true;
    if (!setjmp(task_exit)) can_bcm_rx_task(NULL);
    run_task = false;
}
int main(void) {
    assert(can_bcm_send_status() == ESP_ERR_INVALID_STATE);
    for (int stage = 1; stage <= 4; stage++) {
        init_stage = 0; init_error_stage = stage;
        assert(can_bcm_init() == ESP_FAIL);
        assert(allocations == 0 && s_node_hdl == NULL);
    }
    init_stage = 0; init_error_stage = 0;
    assert(can_bcm_init() == ESP_OK);
    assert(can_bcm_init() == ESP_ERR_INVALID_STATE);
    assert(saved_cfg.io_cfg.tx == 15 && saved_cfg.io_cfg.rx == 36);
    assert(saved_cfg.io_cfg.quanta_clk_out == -1 && saved_cfg.io_cfg.bus_off_indicator == -1);
    assert(!saved_cfg.flags.enable_loopback && saved_cfg.flags.no_receive_rtr);
    assert(saved_cfg.bit_timing.bitrate == 500000);
    puts("PASS: inicializacao, limpeza de falhas e configuracao CAN");

    assert(can_bcm_send_status() == ESP_OK);
    /* O driver simulado só lê o frame depois que a função retorna. */
    assert(pending_frame == &s_tx_frame && pending_frame->buffer == s_tx_data);
    assert(pending_frame->header.id == 0x200 && pending_frame->header.dlc == 8);
    const uint8_t expected[8] = {10, 20, 30, 5, 3, 0, 0, 0};
    assert(memcmp(pending_frame->buffer, expected, 8) == 0);
    brightness = 99;
    assert(can_bcm_send_status() == ESP_ERR_TIMEOUT);
    assert(memcmp(pending_frame->buffer, expected, 8) == 0);
    finish_tx(true);
    assert(s_tx_ok == 1);
    assert(can_bcm_send_status() == ESP_OK && pending_frame->buffer[0] == 99);
    finish_tx(false);
    assert(s_tx_failed == 1);
    submit_error = ESP_ERR_INVALID_STATE;
    assert(can_bcm_send_status() == ESP_ERR_INVALID_STATE);
    submit_error = ESP_OK;
    assert(can_bcm_send_status() == ESP_OK);
    finish_tx(true);
    puts("PASS: payload, vida util assincrona, frame ocupado e falha de envio");

    incoming = (twai_frame_header_t){ .id = 0x100, .dlc = 1 };
    callbacks.on_rx_done(s_node_hdl, NULL, NULL);
    assert(s_rx_queue->count == 1);
    callbacks.on_rx_done(s_node_hdl, NULL, NULL);
    assert(s_rx_dropped == 1);
    poll_rx_task();
    assert(velocidade_veiculo_get() == 42);
    for (int kind = 0; kind < 6; kind++) {
        incoming = (twai_frame_header_t){ .id = 0x100, .dlc = 1 };
        if (kind == 0) incoming.rtr = true;
        if (kind == 1) incoming.ide = true;
        if (kind == 2) incoming.fdf = true;
        if (kind == 3) incoming.dlc = 0;
        if (kind == 4) incoming.dlc = 9;
        if (kind == 5) incoming.id = 0x200;
        callbacks.on_rx_done(s_node_hdl, NULL, NULL);
        assert(s_rx_queue->count == 0);
    }
    puts("PASS: velocidade RX, fila cheia e rejeicao de RTR/IDE/FD/DLC/ID");
    twai_error_event_data_t error = { .err_flags.ack_err = 1 };
    callbacks.on_error(s_node_hdl, &error, NULL);
    assert(s_ack_errors == 1);
    node_state = TWAI_ERROR_BUS_OFF;
    poll_rx_task();
    assert(recover_calls == 1 && node_state == TWAI_ERROR_ACTIVE);
    puts("PASS: diagnostico ACK e inicio de recuperacao bus-off");
    return 0;
}
