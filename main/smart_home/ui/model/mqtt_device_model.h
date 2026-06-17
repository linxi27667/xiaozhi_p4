#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "../../services/mqtt_iot_protocol.h"

#define RC_DEVICE_MAX  32
#define RC_NAME_MAX    20
#define RC_VALUE_MAX   24
#define RC_ID_MAX      24
#define MQTT_TOPIC_MAX 64
#define WIFI_SSID_MAX  33
#define WIFI_AP_MAX    16

typedef enum {
    RC_FLOOR_1 = 1,
    RC_FLOOR_2 = 2,
    RC_FLOOR_3 = 3,
} rc_floor_t;

typedef enum {
    RC_DEVICE_LIGHT,
    RC_DEVICE_RGB_LIGHT,
    RC_DEVICE_FAN,
    RC_DEVICE_DOOR,
    RC_DEVICE_WINDOW,
} rc_device_type_t;

typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_FAILED,
} mqtt_state_t;

typedef enum {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_SCANNING,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED,
} wifi_state_t;

typedef struct {
    char id[RC_ID_MAX];
    rc_floor_t floor;
    rc_device_type_t type;
    char name[RC_NAME_MAX];
    bool connected;
    bool controllable;
    bool power_on;
    uint16_t value;
    char value_text[RC_VALUE_MAX];
    uint8_t floor_id;
    uint8_t cmd_type;
    uint8_t gpio_index;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t brightness;
    uint8_t effect;
} rc_device_t;

typedef struct {
    char ssid[WIFI_SSID_MAX];
    int8_t rssi;
    bool encrypted;
} wifi_ap_t;

typedef struct {
    rc_device_t devices[RC_DEVICE_MAX];
    uint16_t device_count;
    bool gateway_connected;
    uint32_t refresh_seq;
    mqtt_state_t mqtt_state;

    /* WiFi state */
    wifi_state_t wifi_state;
    char wifi_ssid[WIFI_SSID_MAX];
    wifi_ap_t wifi_aps[WIFI_AP_MAX];
    int wifi_ap_count;

    /* Environment sensors */
    float temperature;
    float humidity;
    uint16_t pm25;
    uint16_t rain_value;
    uint16_t flame_value;
    uint16_t smoke_value;
    bool temperature_valid;
    bool humidity_valid;
    bool pm25_valid;
    bool rain_valid;
    bool flame_valid;
    bool smoke_valid;
    uint16_t rain_floor_value[3];
    uint16_t smoke_floor_value[3];
    uint16_t flame_floor_value[3];
    uint8_t fire_floor_status[3];
    uint8_t rain_floor_status[3];
    uint8_t help_floor_status[3];
    bool rain_floor_valid[3];
    bool smoke_floor_valid[3];
    bool flame_floor_valid[3];
    bool fire_floor_valid[3];
    bool rain_status_floor_valid[3];
    bool help_floor_valid[3];
    uint32_t sensor_rx_count;

    /* Outdoor weather service */
    bool weather_valid;
    bool air_quality_valid;
    char weather_location[RC_NAME_MAX];
    float outdoor_temp;
    uint8_t outdoor_humidity;
    float precipitation_mm;
    float wind_speed;
    uint16_t weather_code;
    float pm25_outdoor;
    uint16_t aqi;
    uint32_t weather_last_update_s;

    /* MQTT stats */
    char mqtt_broker[40];
    uint32_t mqtt_rx_count;
    uint32_t mqtt_last_seen_sec;

    /* Controller online status (per floor) */
    bool controller_online[3];

    uint8_t current_scene;
    uint32_t scene_seq;
    char scene_name[RC_NAME_MAX];
} mqtt_device_model_t;

void device_model_init(void);
const mqtt_device_model_t *device_model_get(void);

uint16_t device_model_count(void);
uint16_t device_model_connected_count(void);
const rc_device_t *device_model_at(uint16_t index);

const char *device_model_floor_name(rc_floor_t floor);
const char *device_model_type_name(rc_device_type_t type);
const char *device_model_type_icon(rc_device_type_t type);

void device_model_toggle_device(uint16_t index);
void device_model_set_power(uint16_t index, bool on);
void device_model_apply_power(uint16_t index, bool on);

void device_model_update_from_mqtt(const char *device_id, bool power_on, uint16_t value);
void device_model_set_mqtt_state(mqtt_state_t state);
void device_model_set_device_online(const char *device_id, bool online);

/* Extended updates for WiFi and sensors */
void device_model_update_wifi_state(wifi_state_t state, const char *ssid);
void device_model_update_wifi_aps(const wifi_ap_t *aps, uint16_t count);
void device_model_update_sensors(float temp, float humi, uint16_t pm25,
                                  uint16_t rain, uint16_t flame, uint16_t smoke);
void device_model_update_weather(const char *location, float temp, uint8_t humidity,
                                  float precipitation_mm, float wind_speed,
                                  uint16_t weather_code, float pm25, uint16_t aqi,
                                  bool air_quality_valid);
void device_model_update_sensor_value(uint8_t floor_id, uint8_t sensor_type, uint16_t value);
void device_model_apply_command_ack(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value);
void device_model_apply_heartbeat(const iot_heartbeat_v2_packet_t *heartbeat);
void device_model_apply_heartbeat_v3(const iot_heartbeat_v3_packet_t *heartbeat);
void device_model_apply_rgb_ack(uint8_t floor_id, uint8_t index, uint8_t red, uint8_t green,
                                uint8_t blue, uint8_t brightness, uint8_t effect);
void device_model_set_scene(uint8_t scene_id);
const char *device_model_scene_name(uint8_t scene_id);
bool device_model_get_smoke_value(uint8_t floor_id, uint16_t *value);
bool device_model_get_rain_value(uint8_t floor_id, uint16_t *value);
bool device_model_get_flame_value(uint8_t floor_id, uint16_t *value);
bool device_model_get_fire_status(uint8_t floor_id, uint8_t *status);
bool device_model_get_rain_status(uint8_t floor_id, uint8_t *status);
bool device_model_get_help_status(uint8_t floor_id, uint8_t *status);
uint16_t device_model_sensor_online_count(void);
void device_model_set_controller_online(uint8_t floor_id, bool online);
void device_model_set_mqtt_stats(uint32_t rx_count, uint32_t last_seen_sec);
void device_model_set_floor_runtime_offline(uint8_t floor_id);
void device_model_reset_runtime_data(void);

/* Convenience aliases for ported lvgl_sim code */
static inline const mqtt_device_model_t *garden_model_get(void) { return device_model_get(); }
static inline uint16_t garden_model_device_count(void) { return device_model_count(); }
static inline const rc_device_t *garden_model_device_at(uint16_t i) { return device_model_at(i); }
static inline const char *garden_model_floor_name(rc_floor_t f) { return device_model_floor_name(f); }
static inline const char *garden_model_type_name(rc_device_type_t t) { return device_model_type_name(t); }
static inline const char *garden_model_type_icon(rc_device_type_t t) { return device_model_type_icon(t); }
static inline void garden_model_toggle_device(uint16_t i) { device_model_toggle_device(i); }
static inline void garden_model_set_device_power(uint16_t i, bool on) { device_model_set_power(i, on); }
static inline uint16_t garden_model_connected_count(void) { return device_model_connected_count(); }

#ifdef __cplusplus
}
#endif
