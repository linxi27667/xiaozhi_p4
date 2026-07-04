#ifndef __MQTT_IOT_PROTOCOL_H__
#define __MQTT_IOT_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* ================= MQTT Topic definitions ================= */
#define MQTT_TOPIC_CMD_BROADCAST    "xiaozhi/iot/cmd/broadcast"
#define MQTT_TOPIC_CMD_PREFIX       "xiaozhi/iot/cmd/"
#define MQTT_TOPIC_RESP_PREFIX      "xiaozhi/iot/resp/"
#define MQTT_TOPIC_HEARTBEAT_PREFIX "xiaozhi/iot/heartbeat/"
#define MQTT_TOPIC_ANNOUNCE         "xiaozhi/iot/announce"
#define MQTT_TOPIC_ANNOUNCE_PREFIX  "xiaozhi/iot/announce/"
#define MQTT_TOPIC_SENSOR_PREFIX    "xiaozhi/iot/sensor/"
#define MQTT_TOPIC_POWER_BROADCAST  "xiaozhi/iot/power/broadcast"
#define MQTT_TOPIC_POWER_PREFIX     "xiaozhi/iot/power/"

/* ================= Command codes ================= */
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
    IOT_CMD_SET_MAIN_POWER = 0x14,
    IOT_CMD_BROADCAST_ALL_OFF = 0x30,
    IOT_CMD_BROADCAST_ALL_ON = 0x31,
    IOT_CMD_BROADCAST_LIGHTS_OFF = 0x32,
    IOT_CMD_BROADCAST_LIGHTS_ON = 0x33,
    IOT_CMD_EMERGENCY = 0x34,
    IOT_CMD_SET_RGB_LIGHT = 0x40,
    IOT_CMD_SET_SCENE = 0x41,
    IOT_CMD_SET_AMBIENT_SCENE = 0x42
} iot_command_t;

/* Source identifiers for scene/command origin */
typedef enum {
    IOT_SOURCE_USER = 0,
    IOT_SOURCE_RULE = 1,
    IOT_SOURCE_VOICE = 2
} iot_source_t;

/* Ambient scene presets for RGB lights (host-side only, not sent to slave) */
typedef enum {
    IOT_AMBIENT_SCENE_OFF = 0,
    IOT_AMBIENT_SCENE_SLEEP = 1,
    IOT_AMBIENT_SCENE_RAIN = 2,
    IOT_AMBIENT_SCENE_WARNING = 3,
    IOT_AMBIENT_SCENE_WARM_HOME = 4
} iot_ambient_scene_t;

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

#define IOT_PROTOCOL_VERSION 2
#define IOT_PROTOCOL_VERSION_V3 3
#define IOT_MAX_LIGHTS      3
#define IOT_MAX_RELAYS      3
#define IOT_MAX_SERVOS      3
#define IOT_MAX_RGB_LIGHTS  2

typedef enum {
    IOT_LIGHT_EFFECT_STATIC = 0,
    IOT_LIGHT_EFFECT_BREATHE = 1,
    IOT_LIGHT_EFFECT_RAINBOW = 2,
    IOT_LIGHT_EFFECT_WARNING = 3,
} iot_light_effect_t;

typedef enum {
    IOT_SCENE_NONE = 0,
    IOT_SCENE_SLEEP = 1,
    IOT_SCENE_MOVIE = 2,
    IOT_SCENE_NIGHT = 3,
    IOT_SCENE_FIRE = 4,
    IOT_SCENE_RAIN = 5,
    IOT_SCENE_AWAY = 6,
    IOT_SCENE_HOME = 7,
    IOT_SCENE_BRIGHT = 8,
} iot_scene_id_t;

typedef struct {
    uint8_t command;
    uint8_t protocol_version;
    uint8_t device_id;
    uint8_t index;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t brightness;
    uint8_t effect;
    uint8_t speed;
    uint8_t reserved[6];
} __attribute__((packed)) iot_rgb_light_packet_t;

typedef struct {
    uint8_t command;
    uint8_t protocol_version;
    uint8_t scene_id;
    uint8_t source;
    uint8_t reserved[8];
} __attribute__((packed)) iot_scene_packet_t;

typedef struct {
    uint8_t command;
    uint8_t protocol_version;
    uint8_t device_id;
    uint8_t device_status;
    char device_name[24];
    char mac_str[13];
    uint8_t light_count;
    uint8_t relay_count;
    uint8_t servo_count;
    uint8_t sensor_count;
    uint8_t lights[IOT_MAX_LIGHTS];
    uint8_t relays[IOT_MAX_RELAYS];
    uint8_t servos[IOT_MAX_SERVOS];
    uint16_t smoke_mv;  /* legacy field name: carries 3F flame sensor voltage */
    uint16_t rain_mv;
    uint8_t fire_status;
    uint8_t rain_status;
    uint8_t help_status;
    uint32_t uptime_s;
    uint8_t main_power_status;
    uint8_t reserved[7];
} __attribute__((packed)) iot_heartbeat_v2_packet_t;

typedef struct {
    iot_heartbeat_v2_packet_t v2;
    uint8_t rgb_count;
    uint8_t rgb_on[IOT_MAX_RGB_LIGHTS];
    uint8_t rgb_red[IOT_MAX_RGB_LIGHTS];
    uint8_t rgb_green[IOT_MAX_RGB_LIGHTS];
    uint8_t rgb_blue[IOT_MAX_RGB_LIGHTS];
    uint8_t rgb_brightness[IOT_MAX_RGB_LIGHTS];
    uint8_t rgb_effect[IOT_MAX_RGB_LIGHTS];
    uint8_t current_scene;
    uint8_t reserved_v3[8];
} __attribute__((packed)) iot_heartbeat_v3_packet_t;

typedef enum {
    IOT_SENSOR_SMOKE_MV    = 0,  /* legacy name kept for wire compatibility */
    IOT_SENSOR_FLAME_MV    = IOT_SENSOR_SMOKE_MV,
    IOT_SENSOR_RAIN_MV     = 1,
    IOT_SENSOR_FIRE_STATUS = 2,
    IOT_SENSOR_RAIN_STATUS = 3,
    IOT_SENSOR_HELP_STATUS = 4
} iot_sensor_type_t;

#define DEVICE_ID_FIRSTFLOOR    "1"
#define DEVICE_ID_SECONDFLOOR   "2"
#define DEVICE_ID_THIRDFLOOR    "3"

static inline void mac_to_string(const uint8_t* mac, char* str, size_t len) {
    snprintf(str, len, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

#endif
