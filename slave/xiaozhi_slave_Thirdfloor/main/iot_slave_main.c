/**
 * @file    iot_slave_main.c
 * @brief   IoT 从机设备主程序
 *
 * 功能：
 * - 初始化 WiFi
 * - 初始化 MQTT 通信
 * - 创建设备控制任务
 *
 * 架构：
 * - mqtt_receive.c   : MQTT 接收 + 命令解析
 * - mqtt_heartbeat.c : 心跳包发送
 * - iot_control_task.c  : 设备标志位刷新 + GPIO 控制
 * - sensor_adc_task.c   : 烟雾/雨滴/求助传感器检测
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_mac.h"

#include "app_wifi.h"
#include "mqtt_receive.h"
#include "mqtt_heartbeat.h"
#include "iot_control_task.h"
#include "sensor_adc_task.h"
#include "app_asr_uart.h"

static const char* TAG = "MAIN";

/* ================= 主入口 ================= */
void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   IoT Slave Device Starting...");
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Connecting to WiFi...");
    App_WiFi_System_Init();

    ESP_LOGI(TAG, "Initializing MQTT...");
    MQTT_Receive_Init();

    ESP_LOGI(TAG, "Initializing Device Control...");
    Iot_Control_Task_Init();

    ESP_LOGI(TAG, "Initializing Heartbeat...");
    MQTT_Heartbeat_Init();

    ESP_LOGI(TAG, "Initializing ASR UART...");
    App_ASR_UART_Init();

    ESP_LOGI(TAG, "Initializing Sensor ADC...");
    Sensor_ADC_Task_Init();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   IoT Slave Ready!");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Waiting for commands...");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
