/**
 * @file    app_asr_uart.c
 * @brief   ASR PRO 语音模块 APP 硬件绑定层实现
 */
#include "app_asr_uart.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static const char* TAG = "APP_ASR_UART";

#define ASR_UART_NUM       UART_NUM_1
#define ASR_UART_TX_PIN    GPIO_NUM_16
#define ASR_UART_RX_PIN    GPIO_NUM_17
#define ASR_UART_BAUD      115200
#define ASR_UART_BUF_SIZE  256

static bool HW_ASR_UART_Init(asr_uart_t *dev) {
    uart_config_t uart_cfg = {
        .baud_rate = dev->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (uart_param_config(ASR_UART_NUM, &uart_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "UART config failed");
        return false;
    }

    if (uart_set_pin(ASR_UART_NUM, dev->tx_pin, dev->rx_pin,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "UART pin set failed");
        return false;
    }

    if (uart_driver_install(ASR_UART_NUM, ASR_UART_BUF_SIZE, 0, 0, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed");
        return false;
    }

    ESP_LOGI(TAG, "ASR UART initialized: TX=%d, RX=%d, Baud=%d",
             dev->tx_pin, dev->rx_pin, dev->baud_rate);
    return true;
}

static bool HW_ASR_Send_Command(asr_uart_t *dev, uint8_t cmd) {
    uint8_t data[1] = {cmd};
    int len = uart_write_bytes(ASR_UART_NUM, data, 1);
    return (len == 1);
}

asr_uart_t g_asr_uart_dev = {
    .baud_rate = ASR_UART_BAUD,
    .tx_pin = ASR_UART_TX_PIN,
    .rx_pin = ASR_UART_RX_PIN,
    .is_init = false,
    .Hw_Init = HW_ASR_UART_Init,
    .Hw_Send_Command = HW_ASR_Send_Command,
};

void App_ASR_UART_Init(void) {
    BSP_ASR_UART_Init(&g_asr_uart_dev);
    if (g_asr_uart_dev.is_init) {
        ESP_LOGI(TAG, "ASR UART device ready");
    }
}

void App_ASR_Send_Fire_Alert(void) {
    if (g_asr_uart_dev.is_init) {
        BSP_ASR_Send_Command(&g_asr_uart_dev, ASR_CMD_FIRE_ALERT);
        ESP_LOGI(TAG, "[ASR] Fire alert: 0x%02X", ASR_CMD_FIRE_ALERT);
    }
}

void App_ASR_Send_Help_Alert(void) {
    if (g_asr_uart_dev.is_init) {
        BSP_ASR_Send_Command(&g_asr_uart_dev, ASR_CMD_HELP_ALERT);
        ESP_LOGI(TAG, "[ASR] Help alert: 0x%02X", ASR_CMD_HELP_ALERT);
    }
}

void App_ASR_Send_Emergency_Alert(void) {
    if (g_asr_uart_dev.is_init) {
        BSP_ASR_Send_Command(&g_asr_uart_dev, ASR_CMD_EMERGENCY);
        ESP_LOGW(TAG, "[ASR] Emergency alert triggered: 0x%02X", ASR_CMD_EMERGENCY);
    }
}
