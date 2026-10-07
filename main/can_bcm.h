#ifndef CAN_BCM_H
#define CAN_BCM_H

#include <stdint.h>
#include "esp_err.h"

/* Layout Message 0x200 (BCM_STATUS) - DLC = 8
 *  Byte 0 : Luminosidade       (0..255 -> 0..100 %  factor 0.3922)
 *  Byte 1 : VelocidadeLimpador (0..255 -> 0..100 %  factor 0.3922)
 *  Byte 2 : DimmerInterno      (0..255 -> 0..100 %  factor 0.3922)
 *  Byte 3 : bit0 SetaEsquerda | bit1 SetaDireita
 *           bit2 TravaPortas   | bit3 PiscaAlerta
 *  Byte 4 : bits0..1 Farol (0=Off,1=Lanterna,2=Baixo,3=Alto)
 *  Bytes 5..7: reservado 0x00
 */
esp_err_t can_bcm_init(void);
esp_err_t can_bcm_send_status(void);

/* Task de RX (criada com xTaskCreate no main.c) */
void can_bcm_rx_task(void *arg);

#endif