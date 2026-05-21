/**
 * @file    sensor_adc_task.c
 * @brief   二楼从机统一传感器任务
 *
 * 职责：
 * - 雨滴传感器检测（ADC on GPIO1）
 * - 雨天自动收衣服逻辑触发
 * - 干燥自动晾衣服逻辑触发
 *
 * 架构原则：
 * - 统一采样周期 500ms
 * - 软件多次采样平均（简单稳定）
 * - 防抖处理，避免误触发
 * - 只更新标志位，不操作硬件
 * - 硬件操作由 Device_Refresh_Task 执行
 */
#include "sensor_adc_task.h"
#include "iot_control_task.h"
#include "sensor_config.h"
#include "mqtt_receive.h"
#include "mqtt_iot_protocol.h"

#include "esp_log.h"
#include "driver/adc.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "SENSOR_ADC";

static uint8_t g_rain_exceed_count = 0;
static volatile uint16_t g_last_rain_mv = 0;

typedef enum {
    RAIN_STATUS_DRY = 0,
    RAIN_STATUS_RAINING = 1
} rain_status_enum_t;

static volatile rain_status_enum_t g_rain_status = RAIN_STATUS_DRY;

void Sensor_ADC_Get_Snapshot(uint16_t *rain_mv, uint8_t *rain_status) {
    if (rain_mv) *rain_mv = g_last_rain_mv;
    if (rain_status) *rain_status = (uint8_t)g_rain_status;
}

static void HW_ADC_Init(void) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(RAIN_ADC_CHANNEL, RAIN_ADC_ATTEN);
    ESP_LOGI(TAG, "ADC initialized: Rain=CH0(GPIO1)");
    ESP_LOGI(TAG, "Thresholds: Rain < %dmV (trigger)", RAIN_THRESHOLD_LIGHT);
#if DEBUG_MODE == 1
    ESP_LOGI(TAG, "DEBUG MODE: 5s interval logging enabled");
#endif
}

static int32_t ADC_Get_Voltage_MV(adc1_channel_t channel) {
    int sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += adc1_get_raw(channel);
    }
    return (sum * 3100) / (4095 * 16);
}

static void Sensor_ADC_Task(void* arg) {
    ESP_LOGI(TAG, "Sensor ADC task started");

    HW_ADC_Init();

    while (1) {
        int32_t rain_mv = ADC_Get_Voltage_MV(RAIN_ADC_CHANNEL);
        g_last_rain_mv = (uint16_t)rain_mv;

        if (rain_mv <= RAIN_THRESHOLD_LIGHT) {
            if (++g_rain_exceed_count >= RAIN_TRIGGER_COUNT) {
                if (g_rain_status != RAIN_STATUS_RAINING) {
                    g_rain_status = RAIN_STATUS_RAINING;
                    ESP_LOGW(TAG, "[RAIN] Rain detected! %ld mV <= %d mV",
                             rain_mv, RAIN_THRESHOLD_LIGHT);
                    ESP_LOGW(TAG, "[RAIN] Auto collecting clothes - servo to 0 deg");
                    g_device_flags.servo[0] = SERVO_0;
                }
            }
        } else {
            if (g_rain_status == RAIN_STATUS_RAINING) {
                g_rain_exceed_count = 0;
                g_rain_status = RAIN_STATUS_DRY;
                ESP_LOGI(TAG, "[RAIN] Dry %ld mV > %d mV - extending clothes rack",
                         rain_mv, RAIN_THRESHOLD_LIGHT);
                g_device_flags.servo[0] = SERVO_180;
            }
        }

        static uint32_t last_report = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_report >= 5000) {
            esp_mqtt_client_handle_t client = MQTT_Get_Client();
            const char* mac = MQTT_Get_MAC_Str();
            if (client && mac) {
                char topic[64];
                snprintf(topic, sizeof(topic), MQTT_TOPIC_SENSOR_PREFIX "%s", mac);
                iot_command_packet_t pkt = {
                    .command = IOT_CMD_SENSOR_REPORT,
                    .device_id = 2,
                    .gpio_index = IOT_SENSOR_RAIN_MV,
                    .value = (uint8_t)(rain_mv & 0xFF),
                    .reserved = {(uint8_t)((rain_mv >> 8) & 0xFF), 0, 0, 0}
                };
                esp_mqtt_client_publish(client, topic, (const char*)&pkt, sizeof(pkt), 0, 0);

                pkt.gpio_index = IOT_SENSOR_RAIN_STATUS;
                pkt.value = (uint8_t)g_rain_status;
                pkt.reserved[0] = 0;
                esp_mqtt_client_publish(client, topic, (const char*)&pkt, sizeof(pkt), 0, 0);
            }
#if DEBUG_MODE == 1
            ESP_LOGI(TAG, "[DEBUG] Rain: %ld mV, Status: %d, Servo[0]: %d",
                     rain_mv, g_rain_status, g_device_flags.servo[0]);
#endif
            last_report = now;
        }

        vTaskDelay(pdMS_TO_TICKS(ADC_SAMPLE_INTERVAL_MS));
    }
}

void Sensor_ADC_Task_Init(void) {
    xTaskCreate(Sensor_ADC_Task, "sensor_adc", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Sensor ADC task created");
}
