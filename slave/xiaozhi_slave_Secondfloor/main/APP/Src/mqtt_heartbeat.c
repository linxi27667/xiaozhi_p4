#include "mqtt_heartbeat.h"
#include "mqtt_receive.h"
#include "iot_control_task.h"
#include "app_ambient_light.h"
#include "mqtt_iot_protocol.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char* TAG = "MQTT_HB";

#define HEARTBEAT_INTERVAL_MS   30000
#define DEVICE_ID_NUM           2
#define DEVICE_NAME             "2F_Device"

void MQTT_Heartbeat_Publish_Now(void) {
    esp_mqtt_client_handle_t client = MQTT_Get_Client();
    const char* mac_str = MQTT_Get_MAC_Str();

    if (client == NULL || mac_str == NULL) {
        return;
    }

    iot_heartbeat_v3_packet_t heartbeat = {0};
    heartbeat.v2.command = IOT_CMD_HEARTBEAT;
    heartbeat.v2.protocol_version = IOT_PROTOCOL_VERSION_V3;
    heartbeat.v2.device_id = DEVICE_ID_NUM;
    heartbeat.v2.device_status = 1;
    heartbeat.v2.light_count = LIGHT_COUNT;
    heartbeat.v2.relay_count = RELAY_COUNT;
    heartbeat.v2.servo_count = SERVO_COUNT;
    heartbeat.v2.sensor_count = 0;
    heartbeat.v2.rain_mv = 0;
    heartbeat.v2.rain_status = 0;
    heartbeat.v2.uptime_s = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
    heartbeat.v2.main_power_status = g_device_flags.main_power;

    snprintf(heartbeat.v2.device_name, sizeof(heartbeat.v2.device_name), "%s", DEVICE_NAME);
    snprintf(heartbeat.v2.mac_str, sizeof(heartbeat.v2.mac_str), "%s", mac_str);
    for (int i = 0; i < LIGHT_COUNT && i < IOT_MAX_LIGHTS; i++) heartbeat.v2.lights[i] = g_device_flags.light[i];
    for (int i = 0; i < RELAY_COUNT && i < IOT_MAX_RELAYS; i++) heartbeat.v2.relays[i] = g_device_flags.relay[i];
    for (int i = 0; i < SERVO_COUNT && i < IOT_MAX_SERVOS; i++) heartbeat.v2.servos[i] = g_device_flags.servo[i];

    heartbeat.rgb_count = AMBIENT_STRIP_COUNT;
    heartbeat.rgb_on[0] = App_Ambient_Light_Is_On(AMBIENT_BEDROOM_INDEX) ? 1 : 0;
    heartbeat.rgb_red[0] = App_Ambient_Light_Get_Red(AMBIENT_BEDROOM_INDEX);
    heartbeat.rgb_green[0] = App_Ambient_Light_Get_Green(AMBIENT_BEDROOM_INDEX);
    heartbeat.rgb_blue[0] = App_Ambient_Light_Get_Blue(AMBIENT_BEDROOM_INDEX);
    heartbeat.rgb_brightness[0] = App_Ambient_Light_Get_Brightness(AMBIENT_BEDROOM_INDEX);
    heartbeat.rgb_effect[0] = App_Ambient_Light_Get_Effect(AMBIENT_BEDROOM_INDEX);

    char topic[64];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_HEARTBEAT_PREFIX "%s", mac_str);
    esp_mqtt_client_publish(client, topic, (const char*)&heartbeat, sizeof(heartbeat), 0, 0);
    ESP_LOGD(TAG, "Heartbeat V3 sent");
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
