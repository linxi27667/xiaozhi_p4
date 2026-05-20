#ifndef __MQTT_IOT_PROTOCOL_H__
#define __MQTT_IOT_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* ================= MQTT Topic 定义 ================= */
#define MQTT_TOPIC_CMD_BROADCAST    "xiaozhi/iot/cmd/broadcast"
#define MQTT_TOPIC_CMD_PREFIX       "xiaozhi/iot/cmd/"
#define MQTT_TOPIC_RESP_PREFIX      "xiaozhi/iot/resp/"
#define MQTT_TOPIC_HEARTBEAT_PREFIX "xiaozhi/iot/heartbeat/"
#define MQTT_TOPIC_ANNOUNCE         "xiaozhi/iot/announce"
#define MQTT_TOPIC_ANNOUNCE_PREFIX  "xiaozhi/iot/announce/"
#define MQTT_TOPIC_SENSOR_PREFIX    "xiaozhi/iot/sensor/"

/* ================= 命令码定义 (与原ESP-NOW一致) ================= */
typedef enum {
    IOT_CMD_SET_GPIO = 0x01,
    IOT_CMD_GET_GPIO = 0x02,
    IOT_CMD_GET_ALL_GPIO = 0x03,
    IOT_CMD_HEARTBEAT = 0x04,
    IOT_CMD_RESPONSE = 0x05,
    IOT_CMD_DISCOVER = 0x06,
    IOT_CMD_ANNOUNCE = 0x07,
    IOT_CMD_ANNOUNCE_V2 = 0x08,
    IOT_CMD_SET_SERVO = 0x10,
    IOT_CMD_SET_LIGHT = 0x11,
    IOT_CMD_SET_RELAY = 0x12,
    IOT_CMD_SENSOR_REPORT = 0x13,
    IOT_CMD_BROADCAST_ALL_OFF = 0x30,
    IOT_CMD_BROADCAST_ALL_ON = 0x31,
    IOT_CMD_BROADCAST_LIGHTS_OFF = 0x32,
    IOT_CMD_BROADCAST_LIGHTS_ON = 0x33,
    IOT_CMD_EMERGENCY = 0x34
} iot_command_t;

/* ================= 数据包结构 (与原ESP-NOW一致) ================= */
typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    uint8_t reserved[4];
} __attribute__((packed)) iot_command_packet_t;

typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    char device_name[24];
} __attribute__((packed)) iot_announce_v2_packet_t;

typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    char mac_str[13];
} __attribute__((packed)) iot_heartbeat_packet_t;

/* ================= 传感器类型定义 ================= */
typedef enum {
    IOT_SENSOR_SMOKE_MV   = 0,
    IOT_SENSOR_RAIN_MV    = 1,
    IOT_SENSOR_FIRE_STATUS = 2,
    IOT_SENSOR_RAIN_STATUS = 3,
    IOT_SENSOR_HELP_STATUS = 4
} iot_sensor_type_t;

/* ================= 设备ID定义 ================= */
#define DEVICE_ID_FIRSTFLOOR    "1"
#define DEVICE_ID_SECONDFLOOR   "2"
#define DEVICE_ID_THIRDFLOOR    "3"

/* ================= 工具函数 ================= */
static inline void mac_to_string(const uint8_t* mac, char* str, size_t len) {
    snprintf(str, len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

#endif
