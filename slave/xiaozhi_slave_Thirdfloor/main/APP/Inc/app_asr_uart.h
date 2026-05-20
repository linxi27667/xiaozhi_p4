/**
 * @file    app_asr_uart.h
 * @brief   ASR PRO 语音模块 APP 硬件绑定层
 *
 * 【解禁区】此文件可以包含 ESP32 底层硬件头文件
 * 负责实例化 BSP 对象并绑定具体硬件操作函数
 */
#ifndef APP_ASR_UART_H
#define APP_ASR_UART_H

#include "bsp_asr_uart.h"

#define ASR_CMD_FIRE_ALERT    0x01
#define ASR_CMD_HELP_ALERT    0x02
#define ASR_CMD_EMERGENCY     0x03

extern asr_uart_t g_asr_uart_dev;

void App_ASR_UART_Init(void);
void App_ASR_Send_Fire_Alert(void);
void App_ASR_Send_Help_Alert(void);
void App_ASR_Send_Emergency_Alert(void);

#endif
