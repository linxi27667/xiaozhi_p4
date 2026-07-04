#include "mqtt_heartbeat.h"
#include "mqtt_receive.h"
#include "mqtt_iot_protocol.h"
#include "iot_control_task.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char* TAG = "MQTT_HB";

#define HEARTBEAT_INTERVAL_MS   30000
#define DEVICE_ID_NUM           1
#define DEVICE_NAME             "1F_Device"

void MQTT_Heartbeat_Publish_Now(void) {
    esp_mqtt_client_handle_t client = MQTT_Get_Client();
    const char* mac_str = MQTT_Get_MAC_Str();

    if (client == NULL || mac_str == NULL) {
        return;
    }

    iot_heartbeat_v2_packet_t heartbeat = {
        .command = IOT_CMD_HEARTBEAT,
        .protocol_version = IOT_PROTOCOL_VERSION,
        .device_id = DEVICE_ID_NUM,
        .device_status = 1,
        .light_count = LIGHT_COUNT,
        .relay_count = RELAY_COUNT,
        .servo_count = SERVO_COUNT,
        .sensor_count = 0,
        .uptime_s = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000),
        .main_power_status = g_device_flags.main_power
    };

    snprintf(heartbeat.device_name, sizeof(heartbeat.device_name), "%s", DEVICE_NAME);
    snprintf(heartbeat.mac_str, sizeof(heartbeat.mac_str), "%s", mac_str);
    for (int i = 0; i < LIGHT_COUNT && i < IOT_MAX_LIGHTS; i++) heartbeat.lights[i] = g_device_flags.light[i];
    for (int i = 0; i < RELAY_COUNT && i < IOT_MAX_RELAYS; i++) heartbeat.relays[i] = g_device_flags.relay[i];
    for (int i = 0; i < SERVO_COUNT && i < IOT_MAX_SERVOS; i++) heartbeat.servos[i] = g_device_flags.servo[i];

    char topic[64];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_HEARTBEAT_PREFIX "%s", mac_str);
    esp_mqtt_client_publish(client, topic, (const char*)&heartbeat, sizeof(heartbeat), 0, 0);
    ESP_LOGD(TAG, "Heartbeat V2 sent");
}

void Heartbeat_Task(void* arg) {
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1) {
        MQTT_Heartbeat_Publish_Now();
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
    }
}

void MQTT_Heartbeat_Init(void) {
    BaseType_t ret = xTaskCreate(
        Heartbeat_Task,
        "mqtt_hb",
        2048,
        NULL,
        4,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create heartbeat task");
    } else {
        ESP_LOGI(TAG, "Heartbeat task started (interval: %d ms)", HEARTBEAT_INTERVAL_MS);
    }
}
