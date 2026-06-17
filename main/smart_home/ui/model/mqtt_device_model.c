#include "mqtt_device_model.h"
#include "../../services/mqtt_iot_protocol.h"
#include "../../services/xiaozhi_mqtt.h"
#include "ui_events.h"
#include "../services/ui_icons.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "DEV_MODEL";

static EXT_RAM_BSS_ATTR mqtt_device_model_t s_model;
static bool s_model_initialized;

void smart_home_alarm_on_fire_status(uint8_t floor_id, bool active) __attribute__((weak));
void smart_home_alarm_on_fire_status(uint8_t floor_id, bool active)
{
    (void)floor_id;
    (void)active;
}

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
        case RC_DEVICE_RGB_LIGHT:
            d->value = d->power_on ? d->brightness : 0;
            lv_snprintf(d->value_text, sizeof(d->value_text),
                d->power_on ? "RGB %u%%" : "已关闭", (unsigned)d->brightness);
            break;
        case RC_DEVICE_FAN:
            d->value = d->power_on ? 2 : 0;
            lv_snprintf(d->value_text, sizeof(d->value_text),
                d->power_on ? "风速 %u档" : "已关闭", (unsigned)d->value);
            break;
        case RC_DEVICE_DOOR:
            lv_snprintf(d->value_text, sizeof(d->value_text),
                d->power_on ? "已打开" : "已关闭");
            break;
        case RC_DEVICE_WINDOW:
            lv_snprintf(d->value_text, sizeof(d->value_text),
                d->power_on ? "角度 %u°" : "已关闭", (unsigned)d->value);
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
    snprintf(s_model.weather_location, sizeof(s_model.weather_location), "%s", "广州");

    /*                    floor   type            id                      name       ctrl  floor_id  cmd_type           gpio_idx */
    add_device(RC_FLOOR_1, RC_DEVICE_DOOR,   "floor1_gate",         "\xE5\xA4\xA7\xE9\x97\xA8",     true, 1, IOT_CMD_SET_SERVO, 6);   /* 大门 */
    add_device(RC_FLOOR_1, RC_DEVICE_LIGHT,  "floor1_hall_light",   "\xE5\xA4\xA7\xE5\x8E\x85\xE7\x81\xAF", true, 1, IOT_CMD_SET_LIGHT, 0); /* 大厅灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_RGB_LIGHT, "floor2_master_light", "\xE4\xB8\xBB\xE5\x8D\xA7\xE7\x81\xAF", true, 2, IOT_CMD_SET_LIGHT, 0); /* 主卧灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_LIGHT,  "floor2_living_light", "\xE5\xAE\xA2\xE5\x8E\x85\xE7\x81\xAF", true, 2, IOT_CMD_SET_LIGHT, 1); /* 客厅灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_LIGHT,  "floor2_toilet_light", "\xE5\x8E\x95\xE6\x89\x80\xE7\x81\xAF", true, 2, IOT_CMD_SET_LIGHT, 2); /* 厕所灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_FAN,    "floor2_fan",          "\xE9\xA3\x8E\xE6\x89\x87",     true, 2, IOT_CMD_SET_RELAY, 0);   /* 风扇 */
    add_device(RC_FLOOR_2, RC_DEVICE_WINDOW, "floor2_hanger",       "\xE4\xBA\x8C\xE6\xA5\xBC\xE6\x99\xBE\xE8\xA1\xA3\xE6\x9D\x86", true, 2, IOT_CMD_SET_SERVO, 6); /* 二楼晾衣杆 */
    add_device(RC_FLOOR_3, RC_DEVICE_LIGHT,  "floor3_balcony_light","\xE9\x98\xB3\xE5\x8F\xB0\xE7\x81\xAF", true, 3, IOT_CMD_SET_LIGHT, 0); /* 阳台灯 */
    add_device(RC_FLOOR_3, RC_DEVICE_WINDOW, "floor3_left_skylight","\xE5\xB7\xA6\xE5\xA4\xA9\xE7\xAA\x97", true, 3, IOT_CMD_SET_SERVO, 6); /* 左天窗 */
    add_device(RC_FLOOR_3, RC_DEVICE_WINDOW, "floor3_right_skylight","\xE5\x8F\xB3\xE5\xA4\xA9\xE7\xAA\x97", true, 3, IOT_CMD_SET_SERVO, 7); /* 右天窗 */
    add_device(RC_FLOOR_3, RC_DEVICE_WINDOW, "floor3_hanger",       "\xE4\xB8\x89\xE6\xA5\xBC\xE6\x99\xBE\xE8\xA1\xA3\xE6\x9D\x86", true, 3, IOT_CMD_SET_SERVO, 8); /* 三楼晾衣杆 */

    s_model.current_scene = IOT_SCENE_NONE;
    lv_snprintf(s_model.scene_name, sizeof(s_model.scene_name), "%s", device_model_scene_name(IOT_SCENE_NONE));

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
        case RC_DEVICE_RGB_LIGHT: return "氛围灯";
        case RC_DEVICE_FAN:   return "通风";
        case RC_DEVICE_DOOR:  return "门禁";
        case RC_DEVICE_WINDOW:return "窗控";
        default: return "设备";
    }
}

const char *device_model_type_icon(rc_device_type_t type)
{
    switch (type) {
        case RC_DEVICE_LIGHT: return ICON_LIGHTBULB;
        case RC_DEVICE_RGB_LIGHT: return ICON_STAR;
        case RC_DEVICE_FAN:   return ICON_FAN;
        case RC_DEVICE_DOOR:  return ICON_HOME;
        case RC_DEVICE_WINDOW:return ICON_WINDOW;
        default: return ICON_POWER;
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
            value = on ? 180 : 0;
        } else {
            value = on ? 1 : 0;  /* light/relay: on=1, off=0 */
        }
        mqtt_send_command(d->floor_id, d->cmd_type, d->gpio_index, value);
        return;
    }

    d->power_on = on;
    d->connected = true;
    if (d->type == RC_DEVICE_RGB_LIGHT) {
        d->brightness = on ? (d->brightness ? d->brightness : 60) : 0;
        if (on && d->red == 0 && d->green == 0 && d->blue == 0) {
            d->red = 255;
            d->green = 160;
            d->blue = 80;
        }
    }
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

void device_model_apply_command_ack(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value)
{
    bool changed = false;
    if (floor_id >= 1 && floor_id <= 3) {
        s_model.controller_online[floor_id - 1] = true;
    }

    for (uint16_t i = 0; i < s_model.device_count; i++) {
        rc_device_t *d = &s_model.devices[i];
        if (d->floor_id != floor_id || d->cmd_type != cmd_type || d->gpio_index != gpio_index) {
            continue;
        }
        d->connected = true;
        d->power_on = value != 0;
        d->value = value;
        if (d->type == RC_DEVICE_RGB_LIGHT) {
            d->brightness = value ? (d->brightness ? d->brightness : 60) : 0;
            if (value && d->red == 0 && d->green == 0 && d->blue == 0) {
                d->red = 255;
                d->green = 160;
                d->blue = 80;
            }
        }
        refresh_device_value(d);
        changed = true;
        break;
    }

    if (changed) {
        publish_update();
    }
}

void device_model_apply_rgb_ack(uint8_t floor_id, uint8_t index, uint8_t red, uint8_t green,
                                uint8_t blue, uint8_t brightness, uint8_t effect)
{
    bool changed = false;
    if (floor_id >= 1 && floor_id <= 3) {
        s_model.controller_online[floor_id - 1] = true;
    }

    for (uint16_t i = 0; i < s_model.device_count; i++) {
        rc_device_t *d = &s_model.devices[i];
        if (d->floor_id != floor_id || d->type != RC_DEVICE_RGB_LIGHT || d->gpio_index != index) {
            continue;
        }
        d->connected = true;
        d->power_on = brightness != 0;
        d->red = red;
        d->green = green;
        d->blue = blue;
        d->brightness = brightness;
        d->effect = effect;
        refresh_device_value(d);
        changed = true;
        break;
    }

    if (changed) {
        publish_update();
    }
}

void device_model_apply_heartbeat(const iot_heartbeat_v2_packet_t *heartbeat)
{
    if (!heartbeat || heartbeat->device_id < 1 || heartbeat->device_id > 3) {
        return;
    }

    uint8_t floor_id = heartbeat->device_id;
    uint8_t floor_idx = floor_id - 1;
    s_model.controller_online[floor_idx] = heartbeat->device_status != 0;
    s_model.gateway_connected = true;

    for (uint16_t i = 0; i < s_model.device_count; i++) {
        rc_device_t *d = &s_model.devices[i];
        if (d->floor_id != floor_id) {
            continue;
        }

        bool known = false;
        uint8_t value = 0;
        if (d->cmd_type == IOT_CMD_SET_LIGHT && d->gpio_index < heartbeat->light_count &&
            d->gpio_index < IOT_MAX_LIGHTS) {
            value = heartbeat->lights[d->gpio_index];
            known = true;
        } else if (d->cmd_type == IOT_CMD_SET_RELAY && d->gpio_index < heartbeat->relay_count &&
                   d->gpio_index < IOT_MAX_RELAYS) {
            value = heartbeat->relays[d->gpio_index];
            known = true;
        } else if (d->cmd_type == IOT_CMD_SET_SERVO && d->gpio_index >= 6) {
            uint8_t servo_index = d->gpio_index - 6;
            if (servo_index < heartbeat->servo_count && servo_index < IOT_MAX_SERVOS) {
                value = heartbeat->servos[servo_index];
                known = true;
            }
        }

        if (known) {
            d->connected = true;
            d->power_on = value != 0;
            d->value = value;
            if (d->type == RC_DEVICE_RGB_LIGHT) {
                d->brightness = value ? (d->brightness ? d->brightness : 60) : 0;
                if (value && d->red == 0 && d->green == 0 && d->blue == 0) {
                    d->red = 255;
                    d->green = 160;
                    d->blue = 80;
                }
            }
            refresh_device_value(d);
        }
    }

    if (heartbeat->sensor_count > 0) {
        s_model.rain_floor_value[floor_idx] = heartbeat->rain_mv;
        s_model.rain_floor_valid[floor_idx] = true;
        s_model.rain_floor_status[floor_idx] = heartbeat->rain_status;
        s_model.rain_status_floor_valid[floor_idx] = true;
    }
    if (heartbeat->sensor_count >= 3 || heartbeat->smoke_mv != 0 || heartbeat->fire_status != 0) {
        s_model.smoke_value = heartbeat->smoke_mv;
        s_model.smoke_valid = true;
        s_model.smoke_floor_value[floor_idx] = heartbeat->smoke_mv;
        s_model.smoke_floor_valid[floor_idx] = true;
        s_model.fire_floor_status[floor_idx] = heartbeat->fire_status;
        s_model.fire_floor_valid[floor_idx] = true;
        s_model.flame_value = heartbeat->fire_status;
        s_model.flame_valid = true;
        smart_home_alarm_on_fire_status(floor_id, heartbeat->fire_status >= 2);
    }
    if (heartbeat->sensor_count >= 3 || heartbeat->help_status != 0) {
        s_model.help_floor_status[floor_idx] = heartbeat->help_status;
        s_model.help_floor_valid[floor_idx] = true;
    }

    s_model.sensor_rx_count++;
    publish_update();
}

void device_model_apply_heartbeat_v3(const iot_heartbeat_v3_packet_t *heartbeat)
{
    if (!heartbeat) {
        return;
    }

    device_model_apply_heartbeat(&heartbeat->v2);
    uint8_t floor_id = heartbeat->v2.device_id;
    if (floor_id < 1 || floor_id > 3) {
        return;
    }

    uint8_t count = heartbeat->rgb_count;
    if (count > IOT_MAX_RGB_LIGHTS) {
        count = IOT_MAX_RGB_LIGHTS;
    }
    for (uint8_t i = 0; i < count; i++) {
        device_model_apply_rgb_ack(floor_id, i,
            heartbeat->rgb_red[i],
            heartbeat->rgb_green[i],
            heartbeat->rgb_blue[i],
            heartbeat->rgb_on[i] ? heartbeat->rgb_brightness[i] : 0,
            heartbeat->rgb_effect[i]);
    }

    if (heartbeat->current_scene != IOT_SCENE_NONE) {
        device_model_set_scene(heartbeat->current_scene);
    }
}

const char *device_model_scene_name(uint8_t scene_id)
{
    switch (scene_id) {
        case IOT_SCENE_SLEEP: return "睡眠";
        case IOT_SCENE_MOVIE: return "观影";
        case IOT_SCENE_NIGHT: return "起夜";
        case IOT_SCENE_FIRE:  return "火警";
        case IOT_SCENE_RAIN:  return "雨天收衣";
        case IOT_SCENE_AWAY:  return "离家";
        case IOT_SCENE_HOME:  return "回家";
        default: return "未启用";
    }
}

void device_model_set_scene(uint8_t scene_id)
{
    s_model.current_scene = scene_id;
    s_model.scene_seq++;
    lv_snprintf(s_model.scene_name, sizeof(s_model.scene_name), "%s", device_model_scene_name(scene_id));
    publish_update();
    ui_event_publish(UI_EVENT_SCENE_CHANGED);
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

void device_model_update_weather(const char *location, float temp, uint8_t humidity,
                                  float precipitation_mm, float wind_speed,
                                  uint16_t weather_code, float pm25, uint16_t aqi,
                                  bool air_quality_valid)
{
    if (location && location[0] != '\0') {
        strncpy(s_model.weather_location, location, sizeof(s_model.weather_location) - 1);
        s_model.weather_location[sizeof(s_model.weather_location) - 1] = '\0';
    }
    s_model.outdoor_temp = temp;
    s_model.outdoor_humidity = humidity;
    s_model.precipitation_mm = precipitation_mm;
    s_model.wind_speed = wind_speed;
    s_model.weather_code = weather_code;
    s_model.pm25_outdoor = pm25;
    s_model.aqi = aqi;
    s_model.weather_valid = true;
    s_model.air_quality_valid = air_quality_valid;
    s_model.weather_last_update_s = (uint32_t)(lv_tick_get() / 1000U);
    publish_update();
}

void device_model_update_sensor_value(uint8_t floor_id, uint8_t sensor_type, uint16_t value)
{
    uint8_t idx = 0;
    if (floor_id >= 1 && floor_id <= 3) {
        idx = floor_id - 1;
        s_model.controller_online[idx] = true;
    }

    switch (sensor_type) {
        case IOT_SENSOR_SMOKE_MV:
            s_model.smoke_value = value;
            s_model.smoke_valid = true;
            if (floor_id >= 1 && floor_id <= 3) {
                s_model.smoke_floor_value[idx] = value;
                s_model.smoke_floor_valid[idx] = true;
            }
            break;
        case IOT_SENSOR_RAIN_MV:
            s_model.rain_value = value;
            s_model.rain_valid = true;
            if (floor_id >= 1 && floor_id <= 3) {
                s_model.rain_floor_value[idx] = value;
                s_model.rain_floor_valid[idx] = true;
            }
            break;
        case IOT_SENSOR_RAIN_STATUS:
            if (floor_id >= 1 && floor_id <= 3) {
                s_model.rain_floor_status[idx] = (uint8_t)value;
                s_model.rain_status_floor_valid[idx] = true;
            }
            break;
        case IOT_SENSOR_FIRE_STATUS:
            s_model.flame_value = value;
            s_model.flame_valid = true;
            if (floor_id >= 1 && floor_id <= 3) {
                s_model.fire_floor_status[idx] = (uint8_t)value;
                s_model.fire_floor_valid[idx] = true;
                s_model.flame_floor_value[idx] = value;
                s_model.flame_floor_valid[idx] = true;
                smart_home_alarm_on_fire_status(floor_id, value >= 2);
            }
            break;
        case IOT_SENSOR_HELP_STATUS:
            if (floor_id >= 1 && floor_id <= 3) {
                s_model.help_floor_status[idx] = (uint8_t)value;
                s_model.help_floor_valid[idx] = true;
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

bool device_model_get_smoke_value(uint8_t floor_id, uint16_t *value)
{
    if (floor_id < 1 || floor_id > 3) return false;
    uint8_t idx = floor_id - 1;
    if (!s_model.smoke_floor_valid[idx]) return false;
    if (value) *value = s_model.smoke_floor_value[idx];
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

bool device_model_get_fire_status(uint8_t floor_id, uint8_t *status)
{
    if (floor_id < 1 || floor_id > 3) return false;
    uint8_t idx = floor_id - 1;
    if (!s_model.fire_floor_valid[idx]) return false;
    if (status) *status = s_model.fire_floor_status[idx];
    return true;
}

bool device_model_get_rain_status(uint8_t floor_id, uint8_t *status)
{
    if (floor_id < 1 || floor_id > 3) return false;
    uint8_t idx = floor_id - 1;
    if (!s_model.rain_status_floor_valid[idx]) return false;
    if (status) *status = s_model.rain_floor_status[idx];
    return true;
}

bool device_model_get_help_status(uint8_t floor_id, uint8_t *status)
{
    if (floor_id < 1 || floor_id > 3) return false;
    uint8_t idx = floor_id - 1;
    if (!s_model.help_floor_valid[idx]) return false;
    if (status) *status = s_model.help_floor_status[idx];
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
        if (s_model.fire_floor_valid[i]) count++;
        if (s_model.help_floor_valid[i]) count++;
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
    uint8_t idx = floor_id - 1;
    s_model.controller_online[idx] = false;
    s_model.rain_floor_valid[idx] = false;
    s_model.smoke_floor_valid[idx] = false;
    s_model.flame_floor_valid[idx] = false;
    s_model.fire_floor_valid[idx] = false;
    s_model.rain_status_floor_valid[idx] = false;
    s_model.help_floor_valid[idx] = false;
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
        uint8_t idx = floor_id - 1;
        s_model.controller_online[idx] = false;
        s_model.rain_floor_valid[idx] = false;
        s_model.smoke_floor_valid[idx] = false;
        s_model.flame_floor_valid[idx] = false;
        s_model.fire_floor_valid[idx] = false;
        s_model.rain_status_floor_valid[idx] = false;
        s_model.help_floor_valid[idx] = false;
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
