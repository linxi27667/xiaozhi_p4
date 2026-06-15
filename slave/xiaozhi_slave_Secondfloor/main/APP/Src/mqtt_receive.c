/**
 * @file    mqtt_receive.c
 * @brief   MQTT 接收任务 (二楼设备)
 *
 * 职责：
 * - 连接MQTT Broker并订阅命令Topic
 * - 解析命令并更新设备标志位
 *
 * 架构原则：
 * - 只负责接收和解析，不直接操作硬件
 * - 通过更新全局标志位，由 iot_control_task 刷新 GPIO
 */
#include "mqtt_receive.h"
#include "iot_control_task.h"
#include "mqtt_heartbeat.h"
#include "app_ambient_light.h"
#include "mqtt_iot_protocol.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_netif.h"

static const char* TAG = "MQTT_RECV";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static char s_mac_str[13] = {0};

/* ================= 设备名称定义 ================= */
#define DEVICE_NAME "2F_Device"
#define DEVICE_NAME_CN "二楼设备"
#define TOTAL_GPIO_COUNT 16
#define DEVICE_ID_NUM 2

/* ================= MQTT配置 ================= */
#define MQTT_BROKER_URI     "mqtt://8.134.167.240"  // 阿里云服务器 IP
#define MQTT_BROKER_PORT    1883
#define MQTT_CLIENT_ID      "xiaozhi_slave_2"

/* ================= 响应发送 ================= */
static void Send_Response(uint8_t cmd, uint8_t gpio_index, uint8_t value) {
    if (s_mqtt_client == NULL) return;

    iot_command_packet_t resp = {
        .command = cmd,
        .device_id = DEVICE_ID_NUM,
        .gpio_index = gpio_index,
        .value = value,
        .reserved = {0}
    };

    char topic[64];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_RESP_PREFIX "%s", s_mac_str);
    
    esp_mqtt_client_publish(s_mqtt_client, topic, (const char*)&resp, sizeof(resp), 0, 0);
    ESP_LOGD(TAG, "Response sent: cmd=%d, gpio=%d, value=%d", cmd, gpio_index, value);
}

static void Send_RGB_Response(const iot_rgb_light_packet_t* cmd) {
    if (s_mqtt_client == NULL || cmd == NULL) return;

    iot_rgb_light_packet_t resp = *cmd;
    resp.device_id = 2;

    char topic[64];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_RESP_PREFIX "%s", s_mac_str);
    esp_mqtt_client_publish(s_mqtt_client, topic, (const char*)&resp, sizeof(resp), 0, 0);
}

static void Apply_Local_Scene(uint8_t scene_id) {
    switch (scene_id) {
        case IOT_SCENE_SLEEP:
            g_device_flags.light[0] = ON;
            g_device_flags.light[1] = OFF;
            g_device_flags.light[2] = OFF;
            g_device_flags.relay[0] = OFF;
            App_Ambient_Light_Set_RGB(AMBIENT_BEDROOM_INDEX, 24, 30, 96, 18, IOT_LIGHT_EFFECT_STATIC);
            break;
        case IOT_SCENE_MOVIE:
            g_device_flags.light[0] = ON;
            g_device_flags.light[1] = OFF;
            App_Ambient_Light_Set_RGB(AMBIENT_BEDROOM_INDEX, 52, 68, 210, 24, IOT_LIGHT_EFFECT_STATIC);
            break;
        case IOT_SCENE_NIGHT:
            g_device_flags.light[0] = ON;
            g_device_flags.light[2] = ON;
            App_Ambient_Light_Set_RGB(AMBIENT_BEDROOM_INDEX, 255, 154, 68, 8, IOT_LIGHT_EFFECT_STATIC);
            break;
        case IOT_SCENE_FIRE:
            g_device_flags.light[0] = ON;
            App_Ambient_Light_Set_RGB(AMBIENT_BEDROOM_INDEX, 255, 0, 0, 80, IOT_LIGHT_EFFECT_WARNING);
            break;
        case IOT_SCENE_RAIN:
            g_device_flags.servo[0] = SERVO_0;
            App_Ambient_Light_Set_RGB(AMBIENT_BEDROOM_INDEX, 0, 120, 255, 35, IOT_LIGHT_EFFECT_BREATHE);
            break;
        case IOT_SCENE_AWAY:
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = OFF;
            for (int i = 0; i < RELAY_COUNT; i++) g_device_flags.relay[i] = OFF;
            for (int i = 0; i < SERVO_COUNT; i++) g_device_flags.servo[i] = SERVO_0;
            App_Ambient_Light_Set_RGB(AMBIENT_BEDROOM_INDEX, 0, 0, 0, 0, IOT_LIGHT_EFFECT_STATIC);
            break;
        case IOT_SCENE_HOME:
            g_device_flags.light[0] = ON;
            App_Ambient_Light_Set_RGB(AMBIENT_BEDROOM_INDEX, 255, 166, 82, 45, IOT_LIGHT_EFFECT_BREATHE);
            break;
        default:
            break;
    }
}

static void Process_RGB_Command(const iot_rgb_light_packet_t* cmd) {
    if (!cmd || cmd->device_id != 2 || cmd->index >= AMBIENT_STRIP_COUNT) return;
    uint8_t brightness = cmd->brightness > 100 ? 100 : cmd->brightness;
    App_Ambient_Light_Set_RGB(cmd->index, cmd->red, cmd->green, cmd->blue, brightness, cmd->effect);
    g_device_flags.light[cmd->index] = brightness ? ON : OFF;
    ESP_LOGI(TAG, "RGB[%u] = (%u,%u,%u) br=%u effect=%u",
             cmd->index, cmd->red, cmd->green, cmd->blue, brightness, cmd->effect);
    Send_RGB_Response(cmd);
    MQTT_Heartbeat_Publish_Now();
}

static void Process_Scene_Command(const iot_scene_packet_t* cmd) {
    if (!cmd) return;
    Apply_Local_Scene(cmd->scene_id);
    ESP_LOGI(TAG, "Scene applied: %u", cmd->scene_id);
    MQTT_Heartbeat_Publish_Now();
}

static void Send_Announce(void) {
    if (s_mqtt_client == NULL) return;

    iot_announce_v2_packet_t resp = {
        .command = IOT_CMD_ANNOUNCE_V2,
        .device_id = 2,
        .gpio_index = TOTAL_GPIO_COUNT,
        .value = 1,
        .device_name = DEVICE_NAME
    };

    char topic[64];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_ANNOUNCE_PREFIX "%s", s_mac_str);
    esp_mqtt_client_publish(s_mqtt_client, topic, (const char*)&resp, sizeof(resp), 1, 0);
    ESP_LOGI(TAG, "Announce V2 sent: %s (%s)", DEVICE_NAME, DEVICE_NAME_CN);
}

/* ================= 命令处理（只更新标志位） ================= */
static void Process_Command(const iot_command_packet_t* cmd) {
    ESP_LOGI(TAG, "Command: cmd=%d, gpio=%d, value=%d",
             cmd->command, cmd->gpio_index, cmd->value);

    switch (cmd->command) {
        case IOT_CMD_SET_GPIO:
            if (cmd->gpio_index < LIGHT_COUNT) {
                g_device_flags.light[cmd->gpio_index] = (cmd->value == 1) ? ON : OFF;
                if (cmd->gpio_index == AMBIENT_BEDROOM_INDEX) {
                    App_Ambient_Light_Set_Power(AMBIENT_BEDROOM_INDEX, cmd->value == 1);
                }
                ESP_LOGI(TAG, "Light[%d] = %s", cmd->gpio_index, cmd->value ? "ON" : "OFF");
                Send_Response(cmd->command, cmd->gpio_index, cmd->value);
                MQTT_Heartbeat_Publish_Now();
            } else if (cmd->gpio_index < LIGHT_COUNT + RELAY_COUNT) {
                g_device_flags.relay[cmd->gpio_index - LIGHT_COUNT] = (cmd->value == 1) ? ON : OFF;
                ESP_LOGI(TAG, "Relay[%d] = %s", cmd->gpio_index - LIGHT_COUNT, cmd->value ? "ON" : "OFF");
                Send_Response(cmd->command, cmd->gpio_index, cmd->value);
                MQTT_Heartbeat_Publish_Now();
            }
            break;

        case IOT_CMD_SET_LIGHT:
            if (cmd->gpio_index < LIGHT_COUNT) {
                g_device_flags.light[cmd->gpio_index] = (cmd->value == 1) ? ON : OFF;
                if (cmd->gpio_index == AMBIENT_BEDROOM_INDEX) {
                    App_Ambient_Light_Set_Power(AMBIENT_BEDROOM_INDEX, cmd->value == 1);
                }
                ESP_LOGI(TAG, "Light[%d] = %s", cmd->gpio_index, cmd->value ? "ON" : "OFF");
                Send_Response(cmd->command, cmd->gpio_index, cmd->value);
                MQTT_Heartbeat_Publish_Now();
            }
            break;

        case IOT_CMD_SET_RELAY:
            if (cmd->gpio_index < RELAY_COUNT) {
                g_device_flags.relay[cmd->gpio_index] = (cmd->value == 1) ? ON : OFF;
                ESP_LOGI(TAG, "Relay[%d] = %s", cmd->gpio_index, cmd->value ? "ON" : "OFF");
                Send_Response(cmd->command, cmd->gpio_index, cmd->value);
                MQTT_Heartbeat_Publish_Now();
            }
            break;

        case IOT_CMD_SET_SERVO:
            if (cmd->gpio_index >= 6 && cmd->gpio_index < 6 + SERVO_COUNT && cmd->value <= 180) {
                uint8_t servo_local_index = cmd->gpio_index - 6;
                uint8_t step = cmd->value / 45;
                if (step > 4) step = 4;
                g_device_flags.servo[servo_local_index] = (servo_angle_enum_t)step;
                ESP_LOGI(TAG, "Servo[%d] (GPIO%d) = %d deg", servo_local_index, cmd->gpio_index, cmd->value);
                Send_Response(cmd->command, cmd->gpio_index, cmd->value);
                MQTT_Heartbeat_Publish_Now();
            }
            break;

        case IOT_CMD_GET_GPIO:
            ESP_LOGI(TAG, "Get GPIO[%d]", cmd->gpio_index);
            break;

        case IOT_CMD_GET_ALL_GPIO:
            ESP_LOGI(TAG, "Get all GPIOs");
            break;

        case IOT_CMD_HEARTBEAT:
            Send_Response(IOT_CMD_RESPONSE, 0, 1);
            break;

        case IOT_CMD_DISCOVER:
            ESP_LOGI(TAG, "Master discovered via MQTT");
            Send_Announce();
            break;

        case IOT_CMD_BROADCAST_ALL_OFF:
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = OFF;
            for (int i = 0; i < RELAY_COUNT; i++) g_device_flags.relay[i] = OFF;
            for (int i = 0; i < SERVO_COUNT; i++) g_device_flags.servo[i] = SERVO_0;
            App_Ambient_Light_Set_RGB(AMBIENT_BEDROOM_INDEX, 0, 0, 0, 0, IOT_LIGHT_EFFECT_STATIC);
            ESP_LOGI(TAG, "All devices OFF");
            MQTT_Heartbeat_Publish_Now();
            break;

        case IOT_CMD_BROADCAST_ALL_ON:
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = ON;
            for (int i = 0; i < RELAY_COUNT; i++) g_device_flags.relay[i] = ON;
            ESP_LOGI(TAG, "All lights and relays ON");
            MQTT_Heartbeat_Publish_Now();
            break;

        case IOT_CMD_BROADCAST_LIGHTS_OFF:
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = OFF;
            App_Ambient_Light_Set_RGB(AMBIENT_BEDROOM_INDEX, 0, 0, 0, 0, IOT_LIGHT_EFFECT_STATIC);
            ESP_LOGI(TAG, "All lights OFF");
            MQTT_Heartbeat_Publish_Now();
            break;

        case IOT_CMD_BROADCAST_LIGHTS_ON:
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = ON;
            App_Ambient_Light_Set_Power(AMBIENT_BEDROOM_INDEX, true);
            ESP_LOGI(TAG, "All lights ON");
            MQTT_Heartbeat_Publish_Now();
            break;

        default:
            ESP_LOGW(TAG, "Unknown command: %d", cmd->command);
            break;
    }
}

/* ================= MQTT 事件回调 ================= */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_CMD_BROADCAST, 1);
            esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_CMD_PREFIX DEVICE_ID_SECONDFLOOR, 1);
            Send_Announce();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            break;

        case MQTT_EVENT_DATA:
            if (event->data_len >= (int)sizeof(iot_rgb_light_packet_t) &&
                ((const iot_rgb_light_packet_t*)event->data)->command == IOT_CMD_SET_RGB_LIGHT) {
                Process_RGB_Command((const iot_rgb_light_packet_t*)event->data);
            } else if (event->data_len >= (int)sizeof(iot_scene_packet_t) &&
                       ((const iot_scene_packet_t*)event->data)->command == IOT_CMD_SET_SCENE) {
                Process_Scene_Command((const iot_scene_packet_t*)event->data);
            } else if (event->data_len >= (int)sizeof(iot_command_packet_t)) {
                const iot_command_packet_t* cmd = (const iot_command_packet_t*)event->data;
                Process_Command(cmd);
            } else {
                ESP_LOGW(TAG, "Short packet: %d bytes", event->data_len);
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;

        default:
            break;
    }
}

/* ================= 初始化 ================= */
void MQTT_Receive_Init(void) {
    ESP_LOGI(TAG, "Initializing MQTT...");

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    mac_to_string(mac, s_mac_str, sizeof(s_mac_str));

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .broker.address.port = MQTT_BROKER_PORT,
        .credentials.client_id = MQTT_CLIENT_ID,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);

    ESP_LOGI(TAG, "MQTT initialized (client_id: %s)", MQTT_CLIENT_ID);
}

esp_mqtt_client_handle_t MQTT_Get_Client(void) {
    return s_mqtt_client;
}

const char* MQTT_Get_MAC_Str(void) {
    return s_mac_str;
}
