#ifndef BUZZER_H
#define BUZZER_H

void buzzer_init(void);
void buzzer_on(void);
void buzzer_off(void);
void buzzer_beep_ms(int ms);       /* beep curto (bloqueante) */
void buzzer_click(void);           /* pulso curtinho p/ pisca */
#endif