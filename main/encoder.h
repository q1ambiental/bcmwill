#ifndef ENCODER_H
#define ENCODER_H

typedef enum {
    ENC_EVT_NONE = 0,
    ENC_EVT_CW,       /* girou no sentido horário */
    ENC_EVT_CCW,      /* anti-horário            */
    ENC_EVT_CLICK     /* botão do encoder        */
} enc_event_t;

void        encoder_init(void);
void        encoder_update(void);        /* a cada ~2 ms */
enc_event_t encoder_get_event(void);     /* consumível */
#endif