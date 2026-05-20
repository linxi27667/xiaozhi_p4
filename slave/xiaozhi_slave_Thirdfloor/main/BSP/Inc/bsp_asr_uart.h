/**
 * @file    bsp_asr_uart.h
 * @brief   ASR PRO 语音模块 BSP 抽象层
 *
 * 架构原则：
 * - 纯逻辑抽象，硬件无关
 * - 通过函数指针调用底层硬件操作
 * - 对外只暴露极简 API
 */
#ifndef BSP_ASR_UART_H
#define BSP_ASR_UART_H

#include <stdint.h>
#include <stdbool.h>

typedef struct asr_uart_dev {
    uint32_t baud_rate;
    uint8_t  tx_pin;
    bool     is_init;

    bool (*Hw_Init)(struct asr_uart_dev *dev);
    bool (*Hw_Send_Command)(struct asr_uart_dev *dev, uint8_t cmd);
} asr_uart_t;

void BSP_ASR_UART_Init(asr_uart_t *dev);
void BSP_ASR_Send_Command(asr_uart_t *dev, uint8_t cmd);

#endif
