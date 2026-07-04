/**
 * @file    sensor_adc_task.c
 * @brief   三楼从机火焰/雨滴传感器任务
 */
#include "sensor_adc_task.h"
#include "iot_control_task.h"
#include "sensor_config.h"
#include "app_asr_uart.h"
#include "mqtt_receive.h"
#include "mqtt_iot_protocol.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static const char* TAG = "SENSOR_ADC";

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_flame_cali = NULL;
static adc_cali_handle_t s_rain_cali = NULL;
static bool s_flame_cali_ok = false;
static bool s_rain_cali_ok = false;

static uint8_t g_flame_exceed_count = 0;
static uint8_t g_rain_exceed_count = 0;
static uint8_t g_fire_alert_sent = 0;
static volatile uint16_t g_last_flame_mv = 0;
static volatile uint16_t g_last_rain_mv = 0;

static int32_t Fire_Voltage_To_Percent(int32_t fire_mv) {
    int32_t percent = 100 - (fire_mv * 100) / SENSOR_ADC_FULL_SCALE_MV;
    if (percent > 100) percent = 100;
    if (percent < 0) percent = 0;
    return percent;
}

void Sensor_ADC_Get_Snapshot(uint16_t *flame_mv, uint16_t *rain_mv,
                             uint8_t *fire_status, uint8_t *rain_status) {
    if (flame_mv) *flame_mv = g_last_flame_mv;
    if (rain_mv) *rain_mv = g_last_rain_mv;
    if (fire_status) *fire_status = (uint8_t)g_device_flags.fire_status;
    if (rain_status) *rain_status = (uint8_t)g_device_flags.rain_status;
}

static bool ADC_Calibration_Init(adc_channel_t channel, adc_atten_t atten,
                                 adc_cali_handle_t *out_handle) {
    if (!out_handle) {
        return false;
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = SENSOR_ADC_UNIT,
        .chan = channel,
        .atten = atten,
        .bitwidth = SENSOR_ADC_BITWIDTH,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, out_handle);
    if (ret == ESP_OK) {
        return true;
    }
    ESP_LOGW(TAG, "ADC calibration unavailable for channel %d: %s",
             channel, esp_err_to_name(ret));
#else
    (void)channel;
    (void)atten;
#endif
    *out_handle = NULL;
    return false;
}

static void HW_ADC_Init(void) {
    gpio_reset_pin(FLAME_SENSOR_GPIO);
    gpio_reset_pin(RAIN_SENSOR_GPIO);

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = SENSOR_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t flame_cfg = {
        .atten = FLAME_ADC_ATTEN,
        .bitwidth = SENSOR_ADC_BITWIDTH,
    };
    adc_oneshot_chan_cfg_t rain_cfg = {
        .atten = RAIN_ADC_ATTEN,
        .bitwidth = SENSOR_ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, FLAME_ADC_CHANNEL, &flame_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, RAIN_ADC_CHANNEL, &rain_cfg));

    s_flame_cali_ok = ADC_Calibration_Init(FLAME_ADC_CHANNEL, FLAME_ADC_ATTEN, &s_flame_cali);
    s_rain_cali_ok = ADC_Calibration_Init(RAIN_ADC_CHANNEL, RAIN_ADC_ATTEN, &s_rain_cali);

    ESP_LOGI(TAG, "ADC initialized: Flame GPIO%d=ADC1_CH1, Rain GPIO%d=ADC1_CH0",
             FLAME_SENSOR_GPIO, RAIN_SENSOR_GPIO);
    ESP_LOGI(TAG, "Thresholds: Fire>%d%%, Rain<%dmV (trigger)",
             FIRE_THRESHOLD_PERCENT, RAIN_THRESHOLD_LIGHT);
    ESP_LOGI(TAG, "Calibration: flame=%s, rain=%s",
             s_flame_cali_ok ? "enabled" : "fallback",
             s_rain_cali_ok ? "enabled" : "fallback");
}

static esp_err_t ADC_Read_Raw_Average(adc_channel_t channel, int32_t *out_raw) {
    if (!s_adc_handle || !out_raw) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t sum = 0;
    for (int i = 0; i < 16; i++) {
        int raw = 0;
        esp_err_t ret = adc_oneshot_read(s_adc_handle, channel, &raw);
        if (ret != ESP_OK) {
            return ret;
        }
        sum += raw;
    }
    *out_raw = (int32_t)(sum / 16);
    return ESP_OK;
}

static int32_t ADC_Raw_To_MV(adc_channel_t channel, int32_t raw) {
    adc_cali_handle_t cali = NULL;
    bool cali_ok = false;
    if (channel == FLAME_ADC_CHANNEL) {
        cali = s_flame_cali;
        cali_ok = s_flame_cali_ok;
    } else if (channel == RAIN_ADC_CHANNEL) {
        cali = s_rain_cali;
        cali_ok = s_rain_cali_ok;
    }

    int voltage = 0;
    if (cali_ok && cali && adc_cali_raw_to_voltage(cali, raw, &voltage) == ESP_OK) {
        return voltage;
    }
    return (raw * SENSOR_ADC_FULL_SCALE_MV) / 4095;
}

static esp_err_t ADC_Read_MV(adc_channel_t channel, int32_t *out_raw, int32_t *out_mv) {
    int32_t raw = 0;
    esp_err_t ret = ADC_Read_Raw_Average(channel, &raw);
    if (ret != ESP_OK) {
        return ret;
    }
    if (out_raw) *out_raw = raw;
    if (out_mv) *out_mv = ADC_Raw_To_MV(channel, raw);
    return ESP_OK;
}

static void Sensor_Publish_Report(int32_t flame_mv, int32_t rain_mv) {
    esp_mqtt_client_handle_t client = MQTT_Get_Client();
    const char* mac = MQTT_Get_MAC_Str();
    if (!client || !mac) {
        return;
    }

    char topic[64];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_SENSOR_PREFIX "%s", mac);

    iot_command_packet_t pkt = {
        .command = IOT_CMD_SENSOR_REPORT,
        .device_id = 3,
        .gpio_index = IOT_SENSOR_FLAME_MV,
        .value = (uint8_t)(flame_mv & 0xFF),
        .reserved = {(uint8_t)((flame_mv >> 8) & 0xFF), 0, 0, 0}
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
}

static void Sensor_ADC_Task(void* arg) {
    (void)arg;
    ESP_LOGI(TAG, "Sensor ADC task started");

    HW_ADC_Init();
    uint32_t start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    while (1) {
        int32_t flame_raw = 0;
        int32_t rain_raw = 0;
        int32_t flame_mv = 0;
        int32_t rain_mv = 0;

        esp_err_t flame_ret = ADC_Read_MV(FLAME_ADC_CHANNEL, &flame_raw, &flame_mv);
        esp_err_t rain_ret = ADC_Read_MV(RAIN_ADC_CHANNEL, &rain_raw, &rain_mv);
        if (flame_ret != ESP_OK || rain_ret != ESP_OK) {
            ESP_LOGW(TAG, "ADC read failed: flame=%s rain=%s",
                     esp_err_to_name(flame_ret), esp_err_to_name(rain_ret));
            vTaskDelay(pdMS_TO_TICKS(ADC_SAMPLE_INTERVAL_MS));
            continue;
        }

        int32_t fire_percent = Fire_Voltage_To_Percent(flame_mv);
        g_last_flame_mv = (uint16_t)flame_mv;
        g_last_rain_mv = (uint16_t)rain_mv;

        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        bool fire_input_ready = (now - start_ms) >= FIRE_STARTUP_IGNORE_MS;

        if (!fire_input_ready) {
            g_flame_exceed_count = 0;
            g_device_flags.fire_status = FIRE_STATUS_NORMAL;
        } else if (fire_percent > FIRE_THRESHOLD_PERCENT) {
            if (++g_flame_exceed_count >= FIRE_TRIGGER_COUNT) {
                if (g_device_flags.fire_status != FIRE_STATUS_CONFIRMED) {
                    g_device_flags.fire_status = FIRE_STATUS_CONFIRMED;
                    ESP_LOGW(TAG, "[FIRE] Fire detected! %ld mV (%ld%%) > %d%%",
                             flame_mv, fire_percent, FIRE_THRESHOLD_PERCENT);
                }
                if (!g_fire_alert_sent) {
                    App_ASR_Send_Emergency_Alert();
                    g_fire_alert_sent = 1;
                    ESP_LOGW(TAG, "[FIRE] Tianwen51 alert sent once: 0x03");
                }
            }
        } else {
            g_flame_exceed_count = 0;
            if (g_device_flags.fire_status != FIRE_STATUS_NORMAL) {
                g_device_flags.fire_status = FIRE_STATUS_NORMAL;
                g_fire_alert_sent = 0;
                ESP_LOGI(TAG, "[FIRE] Fire normal %ld mV (%ld%%) <= %d%%",
                         flame_mv, fire_percent, FIRE_THRESHOLD_PERCENT);
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
            g_rain_exceed_count = 0;
            if (g_device_flags.rain_status == RAIN_STATUS_RAINING) {
                g_device_flags.rain_status = RAIN_STATUS_DRY;
                ESP_LOGI(TAG, "[RAIN] Dry %ld mV > %d mV - extending clothes rack",
                         rain_mv, RAIN_THRESHOLD_LIGHT);
                g_device_flags.servo[2] = SERVO_180;
            }
        }

        static uint32_t last_report = 0;
        if (now - last_report >= 5000) {
            Sensor_Publish_Report(flame_mv, rain_mv);
            ESP_LOGI(TAG, "[ADC] Flame GPIO%d raw=%ld mv=%ld percent=%ld fire=%d | Rain GPIO%d raw=%ld mv=%ld rain=%d",
                     FLAME_SENSOR_GPIO, flame_raw, flame_mv, fire_percent,
                     g_device_flags.fire_status,
                     RAIN_SENSOR_GPIO, rain_raw, rain_mv,
                     g_device_flags.rain_status);
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
