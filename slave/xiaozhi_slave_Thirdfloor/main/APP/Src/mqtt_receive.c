/**
 * @file    mqtt_receive.c
 * @brief   MQTT 接收任务 (三楼设备)
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
#define DEVICE_NAME "3F_Device"
#define DEVICE_NAME_CN "三楼设备"
#define TOTAL_GPIO_COUNT 16
#define DEVICE_ID_NUM 3

/* ================= MQTT配置 ================= */
#define MQTT_BROKER_URI     "mqtt://8.134.167.240"  // 阿里云服务器 IP
#define MQTT_BROKER_PORT    1883
#define MQTT_CLIENT_ID      "xiaozhi_slave_3"

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

static void Send_Announce(void) {
    if (s_mqtt_client == NULL) return;

    iot_announce_v2_packet_t resp = {
        .command = IOT_CMD_ANNOUNCE_V2,
        .device_id = 3,
        .gpio_index = TOTAL_GPIO_COUNT,
        .value = 1,
        .device_name = DEVICE_NAME
    };

    char topic[64];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_ANNOUNCE_PREFIX "%s", s_mac_str);
    esp_mqtt_client_publish(s_mqtt_client, topic, (const char*)&resp, sizeof(resp), 1, 0);
    ESP_LOGI(TAG, "Announce V2 sent: %s (%s)", DEVICE_NAME, DEVICE_NAME_CN);
}

/* ================= 场景命令本地处理 ================= */
static void Apply_Local_Scene(uint8_t scene_id) {
    switch (scene_id) {
        case IOT_SCENE_SLEEP:
            /* 睡眠: 关阳台灯, 关天窗, 收衣 */
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = OFF;
            g_device_flags.servo[0] = SERVO_0;
            g_device_flags.servo[1] = SERVO_0;
            g_device_flags.servo[2] = SERVO_0;
            break;
        case IOT_SCENE_MOVIE:
            /* 观影: 关阳台灯, 关天窗 */
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = OFF;
            g_device_flags.servo[0] = SERVO_0;
            g_device_flags.servo[1] = SERVO_0;
            break;
        case IOT_SCENE_NIGHT:
            /* 起夜: 开阳台灯(弱光) */
            g_device_flags.light[0] = ON;
            break;
        case IOT_SCENE_FIRE:
            /* 火警: 开阳台灯, 开天窗(排烟), 收衣 */
            g_device_flags.light[0] = ON;
            g_device_flags.servo[0] = SERVO_135;
            g_device_flags.servo[1] = SERVO_135;
            g_device_flags.servo[2] = SERVO_0;
            break;
        case IOT_SCENE_RAIN:
            /* 雨天: 收衣, 关天窗 */
            g_device_flags.servo[0] = SERVO_0;
            g_device_flags.servo[1] = SERVO_0;
            g_device_flags.servo[2] = SERVO_0;
            break;
        case IOT_SCENE_AWAY:
            /* 离家: 全关 */
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = OFF;
            for (int i = 0; i < RELAY_COUNT; i++) g_device_flags.relay[i] = OFF;
            for (int i = 0; i < SERVO_COUNT; i++) g_device_flags.servo[i] = SERVO_0;
            break;
        case IOT_SCENE_HOME:
            /* 回家: 开阳台灯 */
            g_device_flags.light[0] = ON;
            break;
        case IOT_SCENE_BRIGHT:
            /* 明亮: 全开 */
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = ON;
            break;
        default:
            break;
    }
}

static void Process_Scene_Command(const iot_scene_packet_t* cmd) {
    if (!cmd) return;
    Apply_Local_Scene(cmd->scene_id);
    ESP_LOGI(TAG, "Scene applied: %u", cmd->scene_id);
    MQTT_Heartbeat_Publish_Now();
}

/* ================= 命令处理（只更新标志位） ================= */
static void Process_Command(const iot_command_packet_t* cmd) {
    ESP_LOGI(TAG, "Command: cmd=%d, gpio=%d, value=%d",
             cmd->command, cmd->gpio_index, cmd->value);

    switch (cmd->command) {
        case IOT_CMD_SET_GPIO:
            if (cmd->gpio_index < LIGHT_COUNT) {
                g_device_flags.light[cmd->gpio_index] = (cmd->value == 1) ? ON : OFF;
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
            if (cmd->gpio_index >= 6 && cmd->gpio_index < 6 + SERVO_COUNT) {
                uint8_t servo_local_index = cmd->gpio_index - 6;
                bool valid = false;
                if (servo_local_index < 2) {
                    if (cmd->value == 0 || cmd->value == 25 || cmd->value == 70 || cmd->value == 135) {
                        g_device_flags.servo[servo_local_index] = (servo_angle_enum_t)(cmd->value == 0 ? 0 : cmd->value == 25 ? 1 : cmd->value == 70 ? 2 : 3);
                        valid = true;
                    }
                } else {
                    if (cmd->value == 0 || cmd->value == 180) {
                        g_device_flags.servo[servo_local_index] = (servo_angle_enum_t)(cmd->value == 0 ? 0 : 4);
                        valid = true;
                    }
                }
                if (valid) {
                    ESP_LOGI(TAG, "Servo[%d] (GPIO%d) = %d deg", servo_local_index, cmd->gpio_index, cmd->value);
                    Send_Response(cmd->command, cmd->gpio_index, cmd->value);
                    MQTT_Heartbeat_Publish_Now();
                } else {
                    ESP_LOGW(TAG, "Invalid servo angle: %d for servo[%d]", cmd->value, servo_local_index);
                }
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
            ESP_LOGI(TAG, "All lights OFF");
            MQTT_Heartbeat_Publish_Now();
            break;

        case IOT_CMD_BROADCAST_LIGHTS_ON:
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = ON;
            ESP_LOGI(TAG, "All lights ON");
            MQTT_Heartbeat_Publish_Now();
            break;

        case IOT_CMD_EMERGENCY:
            ESP_LOGW(TAG, "[EMERGENCY] Received emergency command from master!");
            g_device_flags.help_status = HELP_STATUS_ACTIVE;
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
            esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_CMD_PREFIX DEVICE_ID_THIRDFLOOR, 1);
            Send_Announce();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            break;

        case MQTT_EVENT_DATA:
            if (event->data_len >= (int)sizeof(iot_scene_packet_t) &&
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
