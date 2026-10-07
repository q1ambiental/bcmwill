#ifndef BCM_CONFIG_H
#define BCM_CONFIG_H

/* ================= I2C / OLED ================= */
#define BCM_I2C_PORT            I2C_NUM_0
#define BCM_I2C_SDA_GPIO        21
#define BCM_I2C_SCL_GPIO        22
#define BCM_I2C_FREQ_HZ         400000
#define OLED_I2C_ADDR           0x3C
#define OLED_WIDTH              128
#define OLED_HEIGHT             64

/* ================= Potenciômetros (ADC1) ================= */
#define POT_LUMINOSIDADE_ADC_CH ADC_CHANNEL_6   /* GPIO34 */
#define POT_LIMPADOR_ADC_CH     ADC_CHANNEL_7   /* GPIO35 */

/* ================= Barra de LEDs (8) ================= */
#define LED_BAR_QTY             8
#define LED_BAR_GPIO_0          12
#define LED_BAR_GPIO_1          13
#define LED_BAR_GPIO_2          14
#define LED_BAR_GPIO_3          25
#define LED_BAR_GPIO_4          26
#define LED_BAR_GPIO_5          27
#define LED_BAR_GPIO_6          32
#define LED_BAR_GPIO_7          33

/* ================= Push-buttons ================= */
#define BTN_SETA_ESQ_GPIO       4
#define BTN_SETA_DIR_GPIO       5
#define BTN_FAROL_GPIO          18
#define BTN_TRAVA_GPIO          19

/* ================= Encoder KY-040 ================= */
#define ENC_CLK_GPIO            16
#define ENC_DT_GPIO             17
#define ENC_SW_GPIO             23

/* ================= Buzzer ================= */
#define BUZZER_GPIO             2

/* ================= CAN (TJA1050) ================= */
#define CAN_TX_GPIO             15
#define CAN_RX_GPIO             36

/* ================= IDs CAN ================= */
#define CAN_ID_BCM_STATUS       0x200   /* Tx BCM -> barramento  */
#define CAN_ID_VEHICLE_SPEED    0x100   /* Rx PCM -> barramento  */

/* ================= Parâmetros BCM ================= */
#define BCM_TX_PERIOD_MS        100
#define BCM_AUTOLOCK_SPEED_KMH  20

#endif /* BCM_CONFIG_H */