#include "mqtt_device_model.h"
#include "../../services/mqtt_iot_protocol.h"
#include "../../services/xiaozhi_mqtt.h"
#include "ui_events.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "DEV_MODEL";

static EXT_RAM_BSS_ATTR mqtt_device_model_t s_model;
static bool s_model_initialized;

static void publish_update(void)
{
    s_model.refresh_seq++;
    ui_event_publish(UI_EVENT_MODEL_UPDATED);
}

static void refresh_device_value(rc_device_t *d)
{
    if (!d) return;
    switch (d->type) {
        case RC_DEVICE_LIGHT:
            d->value = d->power_on ? 80 : 0;
            lv_snprintf(d->value_text, sizeof(d->value_text),
                d->power_on ? "亮度 %u%%" : "已关闭", (unsigned)d->value);
            break;
        case RC_DEVICE_FAN:
            d->value = d->power_on ? 2 : 0;
            lv_snprintf(d->value_text, sizeof(d->value_text),
                d->power_on ? "风速 %u档" : "已关闭", (unsigned)d->value);
            break;
        case RC_DEVICE_DOOR:
            d->value = d->power_on ? 1 : 0;
            lv_snprintf(d->value_text, sizeof(d->value_text),
                d->power_on ? "已打开" : "已关闭");
            break;
        case RC_DEVICE_WINDOW:
            d->value = d->power_on ? 50 : 0;
            lv_snprintf(d->value_text, sizeof(d->value_text),
                d->power_on ? "开度 %u%%" : "已关闭", (unsigned)d->value);
            break;
    }
}

static void add_device(rc_floor_t floor, rc_device_type_t type,
    const char *id, const char *name, bool controllable,
    uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index)
{
    if (s_model.device_count >= RC_DEVICE_MAX || !id || !name) return;
    rc_device_t *d = &s_model.devices[s_model.device_count++];
    memset(d, 0, sizeof(*d));
    strncpy(d->id, id, RC_ID_MAX - 1);
    d->floor = floor;
    d->type = type;
    d->controllable = controllable;
    strncpy(d->name, name, RC_NAME_MAX - 1);
    d->connected = false;
    d->floor_id = floor_id;
    d->cmd_type = cmd_type;
    d->gpio_index = gpio_index;
    refresh_device_value(d);
}

static rc_device_t *find_device(const char *id)
{
    if (!id) return NULL;
    for (uint16_t i = 0; i < s_model.device_count; i++) {
        if (strcmp(s_model.devices[i].id, id) == 0) {
            return &s_model.devices[i];
        }
    }
    return NULL;
}

void device_model_init(void)
{
    if (s_model_initialized) {
        return;
    }
    s_model_initialized = true;

    memset(&s_model, 0, sizeof(s_model));
    s_model.mqtt_state = MQTT_STATE_DISCONNECTED;
    s_model.wifi_state = WIFI_STATE_IDLE;
    snprintf(s_model.mqtt_broker, sizeof(s_model.mqtt_broker), "mqtt://8.134.167.240");

    /*                    floor   type            id                      name       ctrl  floor_id  cmd_type           gpio_idx */
    add_device(RC_FLOOR_1, RC_DEVICE_DOOR,   "floor1_gate",         "\xE5\xA4\xA7\xE9\x97\xA8",     true, 1, IOT_CMD_SET_SERVO, 6);   /* 大门 */
    add_device(RC_FLOOR_1, RC_DEVICE_LIGHT,  "floor1_hall_light",   "\xE5\xA4\xA7\xE5\x8E\x85\xE7\x81\xAF", true, 1, IOT_CMD_SET_LIGHT, 0); /* 大厅灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_LIGHT,  "floor2_master_light", "\xE4\xB8\xBB\xE5\x8D\xA7\xE7\x81\xAF", true, 2, IOT_CMD_SET_LIGHT, 0); /* 主卧灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_LIGHT,  "floor2_living_light", "\xE5\xAE\xA2\xE5\x8E\x85\xE7\x81\xAF", true, 2, IOT_CMD_SET_LIGHT, 1); /* 客厅灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_LIGHT,  "floor2_toilet_light", "\xE5\x8E\x95\xE6\x89\x80\xE7\x81\xAF", true, 2, IOT_CMD_SET_LIGHT, 2); /* 厕所灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_FAN,    "floor2_fan",          "\xE9\xA3\x8E\xE6\x89\x87",     true, 2, IOT_CMD_SET_RELAY, 0);   /* 风扇 */
    add_device(RC_FLOOR_2, RC_DEVICE_WINDOW, "floor2_hanger",       "\xE4\xBA\x8C\xE6\xA5\xBC\xE6\x99\xBE\xE8\xA1\xA3\xE6\x9D\x86", true, 2, IOT_CMD_SET_SERVO, 7); /* 二楼晾衣杆 */
    add_device(RC_FLOOR_3, RC_DEVICE_LIGHT,  "floor3_balcony_light","\xE9\x98\xB3\xE5\x8F\xB0\xE7\x81\xAF", true, 3, IOT_CMD_SET_LIGHT, 0); /* 阳台灯 */
    add_device(RC_FLOOR_3, RC_DEVICE_WINDOW, "floor3_skylight",     "\xE5\xA4\xA9\xE7\xAA\x97",     true, 3, IOT_CMD_SET_SERVO, 6);   /* 天窗 */
    add_device(RC_FLOOR_3, RC_DEVICE_WINDOW, "floor3_hanger",       "\xE4\xB8\x89\xE6\xA5\xBC\xE6\x99\xBE\xE8\xA1\xA3\xE6\x9D\x86", true, 3, IOT_CMD_SET_SERVO, 7); /* 三楼晾衣杆 */

    ESP_LOGI(TAG, "Model initialized: %d devices", s_model.device_count);
    publish_update();
}

const mqtt_device_model_t *device_model_get(void)
{
    return &s_model;
}

uint16_t device_model_count(void)
{
    return s_model.device_count;
}

uint16_t device_model_connected_count(void)
{
    uint16_t count = 0;
    for (uint16_t i = 0; i < s_model.device_count; i++) {
        if (s_model.devices[i].connected) count++;
    }
    return count;
}

const rc_device_t *device_model_at(uint16_t index)
{
    if (index >= s_model.device_count) return NULL;
    return &s_model.devices[index];
}

const char *device_model_floor_name(rc_floor_t floor)
{
    switch (floor) {
        case RC_FLOOR_1: return "一楼";
        case RC_FLOOR_2: return "二楼";
        case RC_FLOOR_3: return "三楼";
        default: return "未知楼层";
    }
}

const char *device_model_type_name(rc_device_type_t type)
{
    switch (type) {
        case RC_DEVICE_LIGHT: return "照明";
        case RC_DEVICE_FAN:   return "通风";
        case RC_DEVICE_DOOR:  return "门禁";
        case RC_DEVICE_WINDOW:return "窗控";
        default: return "设备";
    }
}

const char *device_model_type_icon(rc_device_type_t type)
{
    switch (type) {
        case RC_DEVICE_LIGHT: return "\xEF\x83\xAB";  /* ICON_LIGHTBULB */
        case RC_DEVICE_FAN:   return "\xEF\xA1\xA3";  /* ICON_FAN */
        case RC_DEVICE_DOOR:  return "\xEF\x80\x95";  /* ICON_HOME */
        case RC_DEVICE_WINDOW:return "\xEF\x8B\x90";  /* ICON_WINDOW */
        default: return "\xEF\x88\xB6";               /* ICON_POWER */
    }
}

void device_model_toggle_device(uint16_t index)
{
    if (index >= s_model.device_count) return;
    rc_device_t *d = &s_model.devices[index];
    if (!d->controllable) return;
    device_model_set_power(index, !d->power_on);
}

static void set_power_internal(uint16_t index, bool on, bool publish_mqtt)
{
    if (index >= s_model.device_count) return;
    rc_device_t *d = &s_model.devices[index];
    if (!d->controllable) return;

    if (publish_mqtt) {
        uint8_t value = 0;
        if (d->cmd_type == IOT_CMD_SET_SERVO) {
            value = on ? 90 : 0; /* servo: open=90, closed=0 */
        } else {
            value = on ? 1 : 0;  /* light/relay: on=1, off=0 */
        }
        mqtt_send_command(d->floor_id, d->cmd_type, d->gpio_index, value);
        return;
    }

    d->power_on = on;
    d->connected = true;
    refresh_device_value(d);
    publish_update();
}

void device_model_set_power(uint16_t index, bool on)
{
    set_power_internal(index, on, true);
}

void device_model_apply_power(uint16_t index, bool on)
{
    set_power_internal(index, on, false);
}

void device_model_update_from_mqtt(const char *device_id, bool power_on, uint16_t value)
{
    rc_device_t *d = find_device(device_id);
    if (!d) return;
    d->power_on = power_on;
    d->value = value;
    d->connected = true;
    s_model.gateway_connected = true;
    refresh_device_value(d);
    publish_update();
}

void device_model_set_mqtt_state(mqtt_state_t state)
{
    s_model.mqtt_state = state;
    if (state == MQTT_STATE_CONNECTED) {
        s_model.gateway_connected = true;
    }
    publish_update();
}

void device_model_set_device_online(const char *device_id, bool online)
{
    rc_device_t *d = find_device(device_id);
    if (!d) return;
    d->connected = online;
    publish_update();
}

void device_model_update_wifi_state(wifi_state_t state, const char *ssid)
{
    s_model.wifi_state = state;
    if (ssid) {
        strncpy(s_model.wifi_ssid, ssid, WIFI_SSID_MAX - 1);
    }
    publish_update();
}

void device_model_update_wifi_aps(const wifi_ap_t *aps, uint16_t count)
{
    if (count > WIFI_AP_MAX) count = WIFI_AP_MAX;
    if (aps && count > 0) {
        memcpy(s_model.wifi_aps, aps, count * sizeof(wifi_ap_t));
    } else {
        memset(s_model.wifi_aps, 0, sizeof(s_model.wifi_aps));
        count = 0;
    }
    s_model.wifi_ap_count = count;
    publish_update();
}

void device_model_update_sensors(float temp, float humi, uint16_t pm25,
                                  uint16_t rain, uint16_t flame, uint16_t smoke)
{
    s_model.temperature = temp;
    s_model.humidity = humi;
    s_model.pm25 = pm25;
    s_model.rain_value = rain;
    s_model.flame_value = flame;
    s_model.smoke_value = smoke;
    s_model.temperature_valid = true;
    s_model.humidity_valid = true;
    s_model.pm25_valid = true;
    s_model.rain_valid = true;
    s_model.flame_valid = true;
    s_model.smoke_valid = true;
    s_model.sensor_rx_count++;
    publish_update();
}

void device_model_update_sensor_value(uint8_t floor_id, uint8_t sensor_type, uint16_t value)
{
    if (floor_id >= 1 && floor_id <= 3) {
        s_model.controller_online[floor_id - 1] = true;
    }

    switch (sensor_type) {
        case IOT_SENSOR_SMOKE_MV:
            s_model.smoke_value = value;
            s_model.smoke_valid = true;
            break;
        case IOT_SENSOR_RAIN_MV:
        case IOT_SENSOR_RAIN_STATUS:
            s_model.rain_value = value;
            s_model.rain_valid = true;
            if (floor_id >= 1 && floor_id <= 3) {
                s_model.rain_floor_value[floor_id - 1] = value;
                s_model.rain_floor_valid[floor_id - 1] = true;
            }
            break;
        case IOT_SENSOR_FIRE_STATUS:
            s_model.flame_value = value;
            s_model.flame_valid = true;
            if (floor_id >= 1 && floor_id <= 3) {
                s_model.flame_floor_value[floor_id - 1] = value;
                s_model.flame_floor_valid[floor_id - 1] = true;
            }
            break;
        default:
            return;
    }

    s_model.sensor_rx_count++;
    publish_update();
}

bool device_model_get_rain_value(uint8_t floor_id, uint16_t *value)
{
    if (floor_id < 1 || floor_id > 3) return false;
    uint8_t idx = floor_id - 1;
    if (!s_model.rain_floor_valid[idx]) return false;
    if (value) *value = s_model.rain_floor_value[idx];
    return true;
}

bool device_model_get_flame_value(uint8_t floor_id, uint16_t *value)
{
    if (floor_id < 1 || floor_id > 3) return false;
    uint8_t idx = floor_id - 1;
    if (!s_model.flame_floor_valid[idx]) return false;
    if (value) *value = s_model.flame_floor_value[idx];
    return true;
}

uint16_t device_model_sensor_online_count(void)
{
    uint16_t count = 0;
    if (s_model.temperature_valid) count++;
    if (s_model.humidity_valid) count++;
    if (s_model.pm25_valid) count++;
    if (s_model.smoke_valid) count++;
    for (uint8_t i = 0; i < 3; i++) {
        if (s_model.rain_floor_valid[i]) count++;
        if (s_model.flame_floor_valid[i]) count++;
    }
    return count;
}

void device_model_set_controller_online(uint8_t floor_id, bool online)
{
    if (floor_id >= 1 && floor_id <= 3) {
        s_model.controller_online[floor_id - 1] = online;
        publish_update();
    }
}

void device_model_set_mqtt_stats(uint32_t rx_count, uint32_t last_seen_sec)
{
    s_model.mqtt_rx_count = rx_count;
    s_model.mqtt_last_seen_sec = last_seen_sec;
    publish_update();
}

void device_model_set_floor_runtime_offline(uint8_t floor_id)
{
    if (floor_id < 1 || floor_id > 3) return;
    s_model.controller_online[floor_id - 1] = false;
    s_model.rain_floor_valid[floor_id - 1] = false;
    s_model.flame_floor_valid[floor_id - 1] = false;
    for (uint16_t i = 0; i < s_model.device_count; i++) {
        rc_device_t *d = &s_model.devices[i];
        if (d->floor_id == floor_id) {
            d->connected = false;
            refresh_device_value(d);
        }
    }
    publish_update();
}

void device_model_reset_runtime_data(void)
{
    for (uint8_t floor_id = 1; floor_id <= 3; floor_id++) {
        s_model.controller_online[floor_id - 1] = false;
        s_model.rain_floor_valid[floor_id - 1] = false;
        s_model.flame_floor_valid[floor_id - 1] = false;
    }
    for (uint16_t i = 0; i < s_model.device_count; i++) {
        s_model.devices[i].connected = false;
        refresh_device_value(&s_model.devices[i]);
    }
    s_model.temperature_valid = false;
    s_model.humidity_valid = false;
    s_model.pm25_valid = false;
    s_model.rain_valid = false;
    s_model.flame_valid = false;
    s_model.smoke_valid = false;
    s_model.sensor_rx_count = 0;
    s_model.mqtt_rx_count = 0;
    s_model.mqtt_last_seen_sec = 0;
    publish_update();
}
