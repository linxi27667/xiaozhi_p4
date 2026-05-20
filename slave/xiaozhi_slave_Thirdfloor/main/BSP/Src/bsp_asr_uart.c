/**
 * @file    bsp_asr_uart.c
 * @brief   ASR PRO 语音模块 BSP 抽象层实现
 */
#include "bsp_asr_uart.h"

void BSP_ASR_UART_Init(asr_uart_t *dev) {
    if (dev && dev->Hw_Init) {
        dev->is_init = dev->Hw_Init(dev);
    }
}

void BSP_ASR_Send_Command(asr_uart_t *dev, uint8_t cmd) {
    if (dev && dev->is_init && dev->Hw_Send_Command) {
        dev->Hw_Send_Command(dev, cmd);
    }
}
