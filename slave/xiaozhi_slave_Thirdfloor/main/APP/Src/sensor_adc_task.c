/**
 * @file    sensor_adc_task.c
 * @brief   三楼从机统一传感器任务
 *
 * 职责：
 * - 烟雾传感器检测（ADC）
 * - 雨滴传感器检测（ADC）
 * - 求助按键检测（GPIO）
 * - 雨天自动收衣服逻辑触发
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
#include "app_asr_uart.h"
#include "mqtt_receive.h"
#include "mqtt_iot_protocol.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "SENSOR_ADC";

static uint8_t g_smoke_exceed_count = 0;
static uint8_t g_rain_exceed_count = 0;
static uint8_t g_last_help_btn = 0;
static uint8_t g_fire_alert_countdown = 0;
static uint8_t g_help_alert_sent = 0;
static volatile uint16_t g_last_smoke_mv = 0;
static volatile uint16_t g_last_rain_mv = 0;

void Sensor_ADC_Get_Snapshot(uint16_t *smoke_mv, uint16_t *rain_mv,
                             uint8_t *fire_status, uint8_t *rain_status,
                             uint8_t *help_status) {
    if (smoke_mv) *smoke_mv = g_last_smoke_mv;
    if (rain_mv) *rain_mv = g_last_rain_mv;
    if (fire_status) *fire_status = (uint8_t)g_device_flags.fire_status;
    if (rain_status) *rain_status = (uint8_t)g_device_flags.rain_status;
    if (help_status) *help_status = (uint8_t)g_device_flags.help_status;
}

static void HW_Help_Button_Init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << HELP_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
    ESP_LOGI(TAG, "Help button initialized on GPIO%d (PULLDOWN)", HELP_BUTTON_GPIO);
}

static uint8_t HW_Help_Button_Is_Pressed(void) {
    return gpio_get_level(HELP_BUTTON_GPIO) == 1;
}

static void HW_ADC_Init(void) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(SMOKE_ADC_CHANNEL, SMOKE_ADC_ATTEN);
    adc1_config_channel_atten(RAIN_ADC_CHANNEL, RAIN_ADC_ATTEN);
    ESP_LOGI(TAG, "ADC initialized: Smoke=CH0(GPIO1), Rain=CH2(GPIO3)");
    ESP_LOGI(TAG, "Thresholds: Smoke>=%dmV, Rain<%dmV (trigger)",
             SMOKE_THRESHOLD_FIRE, RAIN_THRESHOLD_LIGHT);
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

    HW_Help_Button_Init();
    HW_ADC_Init();

    while (1) {
        int32_t smoke_mv = ADC_Get_Voltage_MV(SMOKE_ADC_CHANNEL);
        int32_t rain_mv = ADC_Get_Voltage_MV(RAIN_ADC_CHANNEL);
        uint8_t help_btn = HW_Help_Button_Is_Pressed();
        g_last_smoke_mv = (uint16_t)smoke_mv;
        g_last_rain_mv = (uint16_t)rain_mv;

        if (smoke_mv >= SMOKE_THRESHOLD_FIRE) {
            if (++g_smoke_exceed_count >= SMOKE_TRIGGER_COUNT) {
                if (g_device_flags.fire_status != FIRE_STATUS_CONFIRMED) {
                    g_device_flags.fire_status = FIRE_STATUS_CONFIRMED;
                    g_fire_alert_countdown = 1;
                    ESP_LOGW(TAG, "[FIRE] Smoke detected! %ld mV >= %d mV",
                             smoke_mv, SMOKE_THRESHOLD_FIRE);
                }
                if (g_fire_alert_countdown == 0) {
                    App_ASR_Send_Fire_Alert();
                    g_fire_alert_countdown = FIRE_ALERT_INTERVAL_MS / ADC_SAMPLE_INTERVAL_MS;
                    ESP_LOGW(TAG, "[FIRE] Alert sent, next in %d ms", FIRE_ALERT_INTERVAL_MS);
                } else {
                    g_fire_alert_countdown--;
                }
            }
        } else {
            g_smoke_exceed_count = 0;
            if (g_device_flags.fire_status != FIRE_STATUS_NORMAL) {
                g_fire_alert_countdown = 0;
                g_device_flags.fire_status = FIRE_STATUS_NORMAL;
                ESP_LOGI(TAG, "[FIRE] Smoke normal %ld mV < %d mV",
                         smoke_mv, SMOKE_THRESHOLD_FIRE);
            }
        }

        if (rain_mv <= RAIN_THRESHOLD_LIGHT) {
            if (++g_rain_exceed_count >= RAIN_TRIGGER_COUNT) {
                if (g_device_flags.rain_status != RAIN_STATUS_RAINING) {
                    g_device_flags.rain_status = RAIN_STATUS_RAINING;
                    ESP_LOGW(TAG, "[RAIN] Rain detected! %ld mV <= %d mV",
                             rain_mv, RAIN_THRESHOLD_LIGHT);
                    ESP_LOGW(TAG, "[RAIN] Auto collecting clothes - servo to 0 deg");
                    g_device_flags.servo[2] = SERVO_0;
                }
            }
        } else {
            if (g_device_flags.rain_status == RAIN_STATUS_RAINING) {
                g_rain_exceed_count = 0;
                g_device_flags.rain_status = RAIN_STATUS_DRY;
                ESP_LOGI(TAG, "[RAIN] Dry %ld mV > %d mV - extending clothes rack",
                         rain_mv, RAIN_THRESHOLD_LIGHT);
                g_device_flags.servo[2] = SERVO_180;
            }
        }

        if (g_last_help_btn == 0 && help_btn == 1) {
            g_device_flags.help_status = HELP_STATUS_ACTIVE;
            ESP_LOGI(TAG, "[HELP] Button pressed!");
            if (!g_help_alert_sent) {
                App_ASR_Send_Help_Alert();
                g_help_alert_sent = 1;
            }
        }
        if (help_btn == 0) {
            g_help_alert_sent = 0;
        }
        g_last_help_btn = help_btn;

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
                    .device_id = 3,
                    .gpio_index = IOT_SENSOR_SMOKE_MV,
                    .value = (uint8_t)(smoke_mv & 0xFF),
                    .reserved = {(uint8_t)((smoke_mv >> 8) & 0xFF), 0, 0, 0}
                };
                esp_mqtt_client_publish(client, topic, (const char*)&pkt, sizeof(pkt), 0, 0);

                pkt.gpio_index = IOT_SENSOR_RAIN_MV;
                pkt.value = (uint8_t)(rain_mv & 0xFF);
                pkt.reserved[0] = (uint8_t)((rain_mv >> 8) & 0xFF);
                esp_mqtt_client_publish(client, topic, (const char*)&pkt, sizeof(pkt), 0, 0);

                pkt.gpio_index = IOT_SENSOR_FIRE_STATUS;
                pkt.value = (uint8_t)g_device_flags.fire_status;
                pkt.reserved[0] = 0;
                esp_mqtt_client_publish(client, topic, (const char*)&pkt, sizeof(pkt), 0, 0);

                pkt.gpio_index = IOT_SENSOR_RAIN_STATUS;
                pkt.value = (uint8_t)g_device_flags.rain_status;
                esp_mqtt_client_publish(client, topic, (const char*)&pkt, sizeof(pkt), 0, 0);

                pkt.gpio_index = IOT_SENSOR_HELP_STATUS;
                pkt.value = (uint8_t)g_device_flags.help_status;
                esp_mqtt_client_publish(client, topic, (const char*)&pkt, sizeof(pkt), 0, 0);
            }
#if DEBUG_MODE == 1
            ESP_LOGI(TAG, "[DEBUG] Smoke:%ldmV Rain:%ldmV Help:%d Fire:%d RainSt:%d HelpSt:%d",
                     smoke_mv, rain_mv, help_btn,
                     g_device_flags.fire_status,
                     g_device_flags.rain_status,
                     g_device_flags.help_status);
#endif
            last_report = now;
        }

        vTaskDelay(pdMS_TO_TICKS(ADC_SAMPLE_INTERVAL_MS));
    }
}

void Sensor_ADC_Task_Init(void) {
    BaseType_t ret = xTaskCreatePinnedToCore(
        Sensor_ADC_Task,
        "Sensor_ADC",
        4096,
        NULL,
        4,
        NULL,
        1
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor ADC task");
    } else {
        ESP_LOGI(TAG, "Sensor ADC task initialized (interval: %d ms)", ADC_SAMPLE_INTERVAL_MS);
    }
}
