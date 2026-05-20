#include "mqtt_heartbeat.h"
#include "mqtt_receive.h"
#include "mqtt_iot_protocol.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char* TAG = "MQTT_HB";

#define HEARTBEAT_INTERVAL_MS   30000

void Heartbeat_Task(void* arg) {
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1) {
        esp_mqtt_client_handle_t client = MQTT_Get_Client();
        const char* mac_str = MQTT_Get_MAC_Str();
        
        if (client != NULL && mac_str != NULL) {
            iot_heartbeat_packet_t heartbeat = {
                .command = IOT_CMD_HEARTBEAT,
                .device_id = 0,
                .gpio_index = 0,
                .value = 0,
            };
            snprintf(heartbeat.mac_str, sizeof(heartbeat.mac_str), "%s", mac_str);
            
            char topic[64];
            snprintf(topic, sizeof(topic), MQTT_TOPIC_HEARTBEAT_PREFIX "%s", mac_str);
            esp_mqtt_client_publish(client, topic, (const char*)&heartbeat, sizeof(heartbeat), 0, 0);
            ESP_LOGD(TAG, "Heartbeat sent");
        }

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
