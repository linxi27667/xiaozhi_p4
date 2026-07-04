#include "mqtt_device_model.h"
#include "../../services/mqtt_iot_protocol.h"
#include "../../services/xiaozhi_mqtt.h"
#include "ui_events.h"
#include "../services/ui_icons.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "DEV_MODEL";

static EXT_RAM_BSS_ATTR mqtt_device_model_t s_model;
static bool s_model_initialized;
/* 互斥锁: 保护 s_model 读写,防止 MQTT 任务(Core1)与 LVGL 任务并发访问导致数据错乱 */
static SemaphoreHandle_t s_model_mutex = NULL;
static StaticSemaphore_t s_model_mutex_buffer;

/* 临界区辅助宏: 保护写操作,读操作(device_model_get 返回 const 指针)依赖最终一致性 */
static inline void model_lock(void) {
    if (s_model_mutex) xSemaphoreTake(s_model_mutex, portMAX_DELAY);
}
static inline void model_unlock(void) {
    if (s_model_mutex) xSemaphoreGive(s_model_mutex);
}

void smart_home_alarm_on_fire_status(uint8_t floor_id, bool active) __attribute__((weak));
void smart_home_alarm_on_fire_status(uint8_t floor_id, bool active)
{
    (void)floor_id;
    (void)active;
}

void smart_home_alarm_on_rain_status(uint8_t floor_id, bool active) __attribute__((weak));
void smart_home_alarm_on_rain_status(uint8_t floor_id, bool active)
{
    (void)floor_id;
    (void)active;
}

void smart_home_alarm_on_help_status(uint8_t floor_id, bool active) __attribute__((weak));
void smart_home_alarm_on_help_status(uint8_t floor_id, bool active)
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
        case RC_DEVICE_MAIN_POWER:
            d->value = d->power_on ? 1 : 0;
            lv_snprintf(d->value_text, sizeof(d->value_text),
                d->power_on ? "已开启" : "已关闭");
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
    d->servo_open_angle = 180;
    d->servo_close_angle = 0;
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

static bool rain_hanger_lock_active_locked(void)
{
    return s_model.rain_status_floor_valid[2] && s_model.rain_floor_status[2] != 0;
}

static bool is_rain_locked_hanger(const rc_device_t *d)
{
    return d && (strcmp(d->id, "floor2_hanger") == 0 ||
                 strcmp(d->id, "floor3_hanger") == 0);
}

static void refresh_all_main_power_locked(void)
{
    rc_device_t *all = find_device("floor_all_main_power");
    if (!all) return;

    bool any_known = false;
    bool all_on = true;
    for (uint16_t i = 0; i < s_model.device_count; i++) {
        rc_device_t *d = &s_model.devices[i];
        if (d->type != RC_DEVICE_MAIN_POWER || d->floor == RC_FLOOR_ALL) {
            continue;
        }
        if (d->connected) {
            any_known = true;
            if (!d->power_on) {
                all_on = false;
            }
        }
    }

    all->connected = any_known;
    all->power_on = any_known && all_on;
    refresh_device_value(all);
}

void device_model_init(void)
{
    if (s_model_initialized) {
        return;
    }
    s_model_initialized = true;

    /* 创建互斥锁(静态分配,避免内存分配失败) */
    s_model_mutex = xSemaphoreCreateMutexStatic(&s_model_mutex_buffer);

    memset(&s_model, 0, sizeof(s_model));
    s_model.mqtt_state = MQTT_STATE_DISCONNECTED;
    s_model.wifi_state = WIFI_STATE_IDLE;
    snprintf(s_model.mqtt_broker, sizeof(s_model.mqtt_broker), "mqtt://8.134.167.240");
    snprintf(s_model.weather_location, sizeof(s_model.weather_location), "%s", "广州");

    /*                    floor   type            id                      name       ctrl  floor_id  cmd_type           gpio_idx */
    add_device(RC_FLOOR_ALL, RC_DEVICE_MAIN_POWER, "floor_all_main_power", "\xE6\x80\xBB\xE5\xBC\x80\xE5\x85\xB3", true, 0, IOT_CMD_SET_MAIN_POWER, 0); /* 总开关 */
    add_device(RC_FLOOR_1, RC_DEVICE_MAIN_POWER, "floor1_main_power", "\xE4\xB8\x80\xE6\xA5\xBC\xE6\x80\xBB\xE5\xBC\x80\xE5\x85\xB3", true, 1, IOT_CMD_SET_MAIN_POWER, 0); /* 一楼总开关 */
    add_device(RC_FLOOR_1, RC_DEVICE_DOOR,   "floor1_gate",         "\xE5\xA4\xA7\xE9\x97\xA8",     true, 1, IOT_CMD_SET_SERVO, 6);   /* 大门 */
    add_device(RC_FLOOR_1, RC_DEVICE_LIGHT,  "floor1_hall_light",   "\xE5\xA4\xA7\xE5\x8E\x85\xE7\x81\xAF", true, 1, IOT_CMD_SET_LIGHT, 0); /* 大厅灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_MAIN_POWER, "floor2_main_power", "\xE4\xBA\x8C\xE6\xA5\xBC\xE6\x80\xBB\xE5\xBC\x80\xE5\x85\xB3", true, 2, IOT_CMD_SET_MAIN_POWER, 0); /* 二楼总开关 */
    add_device(RC_FLOOR_2, RC_DEVICE_RGB_LIGHT, "floor2_master_light", "\xE5\x8D\xA7\xE5\xAE\xA4\xE7\x81\xAF", true, 2, IOT_CMD_SET_RGB_LIGHT, 0); /* 卧室灯(RGB, 由WS2812B驱动) */
    add_device(RC_FLOOR_2, RC_DEVICE_LIGHT,  "floor2_living_light", "\xE5\xAE\xA2\xE5\x8E\x85\xE7\x81\xAF", true, 2, IOT_CMD_SET_LIGHT, 1); /* 客厅灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_LIGHT,  "floor2_toilet_light", "\xE5\x8E\x95\xE6\x89\x80\xE7\x81\xAF", true, 2, IOT_CMD_SET_LIGHT, 2); /* 厕所灯 */
    add_device(RC_FLOOR_2, RC_DEVICE_FAN,    "floor2_fan",          "\xE9\xA3\x8E\xE6\x89\x87",     true, 2, IOT_CMD_SET_RELAY, 0);   /* 风扇 */
    add_device(RC_FLOOR_2, RC_DEVICE_WINDOW, "floor2_hanger",       "\xE4\xBA\x8C\xE6\xA5\xBC\xE6\x99\xBE\xE8\xA1\xA3\xE6\x9D\x86", true, 2, IOT_CMD_SET_SERVO, 8); /* 二楼晾衣杆 */
    add_device(RC_FLOOR_3, RC_DEVICE_MAIN_POWER, "floor3_main_power", "\xE4\xB8\x89\xE6\xA5\xBC\xE6\x80\xBB\xE5\xBC\x80\xE5\x85\xB3", true, 3, IOT_CMD_SET_MAIN_POWER, 0); /* 三楼总开关 */
    add_device(RC_FLOOR_3, RC_DEVICE_LIGHT,  "floor3_balcony_light","\xE9\x98\xB3\xE5\x8F\xB0\xE7\x81\xAF", true, 3, IOT_CMD_SET_LIGHT, 0); /* 阳台灯 */
    add_device(RC_FLOOR_3, RC_DEVICE_WINDOW, "floor3_right_skylight","\xE5\x8F\xB3\xE5\xA4\xA9\xE7\xAA\x97", true, 3, IOT_CMD_SET_SERVO, 6); /* 右天窗 */
    add_device(RC_FLOOR_3, RC_DEVICE_WINDOW, "floor3_left_skylight","\xE5\xB7\xA6\xE5\xA4\xA9\xE7\xAA\x97", true, 3, IOT_CMD_SET_SERVO, 7); /* 左天窗 */
    add_device(RC_FLOOR_3, RC_DEVICE_WINDOW, "floor3_hanger",       "\xE4\xB8\x89\xE6\xA5\xBC\xE6\x99\xBE\xE8\xA1\xA3\xE6\x9D\x86", true, 3, IOT_CMD_SET_SERVO, 8); /* 三楼晾衣杆 */

    rc_device_t *gate = find_device("floor1_gate");
    if (gate) {
        gate->servo_open_angle = 135;
        gate->servo_close_angle = 0;
    }

    /* 3F 天窗机械限位: GPIO8右天窗 90开/180关；GPIO11左天窗 90开/0关 */
    for (uint16_t i = 0; i < s_model.device_count; i++) {
        rc_device_t *d = &s_model.devices[i];
        if (d->floor == RC_FLOOR_3 && d->type == RC_DEVICE_WINDOW &&
            d->cmd_type == IOT_CMD_SET_SERVO) {
            if (strcmp(d->id, "floor3_right_skylight") == 0) {
                d->servo_open_angle = 90;
                d->servo_close_angle = 180;
            } else if (strcmp(d->id, "floor3_left_skylight") == 0) {
                d->servo_open_angle = 90;
                d->servo_close_angle = 0;
            }
        }
    }

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
        case RC_FLOOR_ALL: return "全部";
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
        case RC_DEVICE_MAIN_POWER: return "总开关";
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
        case RC_DEVICE_MAIN_POWER: return ICON_POWER;
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

static void apply_power_state_locked(rc_device_t *d, bool on)
{
    if (!d) return;

    d->connected = true;
    d->power_on = on;

    if (d->type == RC_DEVICE_RGB_LIGHT) {
        if (on && d->red == 0 && d->green == 0 && d->blue == 0) {
            d->red = 255;
            d->green = 160;
            d->blue = 80;
        }
        d->brightness = on ? 100 : 0;
    } else if (d->cmd_type == IOT_CMD_SET_SERVO) {
        d->value = on ? d->servo_open_angle : d->servo_close_angle;
    } else {
        d->value = on ? 1 : 0;
    }

    refresh_device_value(d);
}

static void apply_all_main_power_locked(bool on)
{
    for (uint16_t i = 0; i < s_model.device_count; i++) {
        rc_device_t *d = &s_model.devices[i];
        if (d->type == RC_DEVICE_MAIN_POWER) {
            apply_power_state_locked(d, on);
        }
    }
    refresh_all_main_power_locked();
}

static void set_power_internal(uint16_t index, bool on, bool publish_mqtt)
{
    if (index >= s_model.device_count) return;
    model_lock();
    rc_device_t *d = &s_model.devices[index];
    if (!d->controllable) { model_unlock(); return; }

    if (on && is_rain_locked_hanger(d) && rain_hanger_lock_active_locked()) {
        uint8_t floor_id = d->floor_id;
        uint8_t cmd_type = d->cmd_type;
        uint8_t gpio_index = d->gpio_index;
        uint8_t value = d->servo_close_angle;
        d->connected = true;
        d->power_on = false;
        d->value = value;
        refresh_device_value(d);
        publish_update();
        model_unlock();
        if (publish_mqtt) {
            mqtt_send_command(floor_id, cmd_type, gpio_index, value);
        }
        return;
    }

    if (publish_mqtt) {
        uint8_t floor_id = d->floor_id;
        uint8_t cmd_type = d->cmd_type;
        uint8_t gpio_index = d->gpio_index;
        uint8_t value = on ? 1 : 0;
        uint8_t r = d->red;
        uint8_t g = d->green;
        uint8_t b = d->blue;
        uint8_t brightness = d->brightness;
        uint8_t effect = d->effect;

        if (cmd_type == IOT_CMD_SET_MAIN_POWER && floor_id == 0) {
            apply_all_main_power_locked(on);
        } else {
            apply_power_state_locked(d, on);
            if (cmd_type == IOT_CMD_SET_MAIN_POWER) {
                refresh_all_main_power_locked();
            }
        }

        if (cmd_type == IOT_CMD_SET_RGB_LIGHT) {
            r = d->red;
            g = d->green;
            b = d->blue;
            brightness = d->brightness;
            effect = d->effect;
        } else if (cmd_type == IOT_CMD_SET_SERVO) {
            value = d->value;
        }

        publish_update();
        model_unlock();

        if (cmd_type == IOT_CMD_SET_RGB_LIGHT) {
            mqtt_send_rgb_light(floor_id, gpio_index, r, g, b, brightness, effect, 0);
        } else if (cmd_type == IOT_CMD_SET_MAIN_POWER) {
            if (floor_id == 0) {
                mqtt_send_all_main_power(on);
            } else {
                mqtt_send_main_power(floor_id, on);
            }
        } else {
            mqtt_send_command(floor_id, cmd_type, gpio_index, value);
        }
        return;
    }

    if (d->cmd_type == IOT_CMD_SET_MAIN_POWER && d->floor_id == 0) {
        apply_all_main_power_locked(on);
    } else {
        apply_power_state_locked(d, on);
        if (d->cmd_type == IOT_CMD_SET_MAIN_POWER) {
            refresh_all_main_power_locked();
        }
    }
    publish_update();
    model_unlock();
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
    model_lock();
    rc_device_t *d = find_device(device_id);
    if (!d) { model_unlock(); return; }
    d->power_on = power_on;
    d->value = value;
    d->connected = true;
    s_model.gateway_connected = true;
    refresh_device_value(d);
    publish_update();
    model_unlock();
}

void device_model_apply_command_ack(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value)
{
    bool changed = false;
    uint8_t canonical_gpio_index = gpio_index;
    if (cmd_type == IOT_CMD_SET_SERVO && canonical_gpio_index < 6) {
        canonical_gpio_index += 6;
    }
    model_lock();
    if (floor_id >= 1 && floor_id <= 3) {
        s_model.controller_online[floor_id - 1] = true;
    }

    for (uint16_t i = 0; i < s_model.device_count; i++) {
        rc_device_t *d = &s_model.devices[i];
        if (d->floor_id != floor_id || d->cmd_type != cmd_type || d->gpio_index != canonical_gpio_index) {
            continue;
        }
        d->connected = true;
        d->power_on = (d->cmd_type == IOT_CMD_SET_SERVO) ?
            (value == d->servo_open_angle) : (value != 0);
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
        refresh_all_main_power_locked();
        publish_update();
    }
    model_unlock();
}

void device_model_apply_rgb_ack(uint8_t floor_id, uint8_t index, uint8_t red, uint8_t green,
                                uint8_t blue, uint8_t brightness, uint8_t effect)
{
    bool changed = false;
    model_lock();
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
    model_unlock();
}

void device_model_apply_heartbeat(const iot_heartbeat_v2_packet_t *heartbeat)
{
    if (!heartbeat || heartbeat->device_id < 1 || heartbeat->device_id > 3) {
        return;
    }

    model_lock();
    uint8_t floor_id = heartbeat->device_id;
    uint8_t floor_idx = floor_id - 1;
    s_model.controller_online[floor_idx] = heartbeat->device_status != 0;
    s_model.gateway_connected = true;

    for (uint16_t i = 0; i < s_model.device_count; i++) {
        rc_device_t *d = &s_model.devices[i];
        if (d->floor_id != floor_id) {
            continue;
        }

        /* RGB 灯跳过 V2 心跳匹配, 由 V3 扩展字段(device_model_apply_heartbeat_v3)处理 */
        if (d->cmd_type == IOT_CMD_SET_RGB_LIGHT) {
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
                /* 从机心跳上报的是 servo_angle_enum_t 枚举值(0~4), 需映射回角度 */
                uint8_t servo_enum = heartbeat->servos[servo_index];
                static const uint8_t servo_angle_map_45[5] = {0, 45, 90, 135, 180};  /* 一楼/二楼: 45度分档 */
                static const uint8_t servo_angle_map_3f_right_skylight[5] = {0, 25, 90, 135, 180};
                static const uint8_t servo_angle_map_3f_left_skylight[5] = {0, 90, 90, 90, 90};
                static const uint8_t servo_angle_map_3f_hanger[5] = {0, 25, 90, 135, 180};
                const uint8_t *map = servo_angle_map_45;
                if (floor_id == 3) {
                    if (servo_index == 0) {
                        map = servo_angle_map_3f_right_skylight;
                    } else if (servo_index == 1) {
                        map = servo_angle_map_3f_left_skylight;
                    } else {
                        map = servo_angle_map_3f_hanger;
                    }
                }
                value = (servo_enum < 5) ? map[servo_enum] : 0;
                known = true;
            }
        } else if (d->cmd_type == IOT_CMD_SET_MAIN_POWER) {
            value = heartbeat->main_power_status;
            known = true;
        }

        if (known) {
            d->connected = true;
            d->power_on = (d->cmd_type == IOT_CMD_SET_SERVO) ?
                (value == d->servo_open_angle) : (value != 0);
            d->value = value;
            refresh_device_value(d);
        }
    }

    if (floor_id == 2) {
        s_model.rain_floor_valid[floor_idx] = false;
        s_model.rain_status_floor_valid[floor_idx] = false;
    }
    if (floor_id == 3 && heartbeat->sensor_count > 0) {
        /* 仅三楼保留雨滴传感器数据 */
        s_model.rain_floor_value[floor_idx] = heartbeat->rain_mv;
        s_model.rain_floor_valid[floor_idx] = true;
        s_model.rain_floor_status[floor_idx] = heartbeat->rain_status;
        s_model.rain_status_floor_valid[floor_idx] = true;
        smart_home_alarm_on_rain_status(floor_id, heartbeat->rain_status != 0);
        /* 全局字段: 仅作为兼容性保留, 取告警最严重的楼层值 */
        if (heartbeat->rain_mv > s_model.rain_value || !s_model.rain_valid) {
            s_model.rain_value = heartbeat->rain_mv;
            s_model.rain_valid = true;
        }
    }
    if (heartbeat->smoke_mv != 0 || heartbeat->fire_status != 0) {
        s_model.smoke_floor_value[floor_idx] = heartbeat->smoke_mv;
        s_model.smoke_floor_valid[floor_idx] = true;
        s_model.fire_floor_status[floor_idx] = heartbeat->fire_status;
        s_model.fire_floor_valid[floor_idx] = true;
        s_model.flame_floor_value[floor_idx] = heartbeat->fire_status;
        s_model.flame_floor_valid[floor_idx] = true;
        /* 全局字段: 取告警最严重的楼层值 */
        if (heartbeat->smoke_mv > s_model.smoke_value || !s_model.smoke_valid) {
            s_model.smoke_value = heartbeat->smoke_mv;
            s_model.smoke_valid = true;
        }
        if (heartbeat->fire_status > s_model.flame_value || !s_model.flame_valid) {
            s_model.flame_value = heartbeat->fire_status;
            s_model.flame_valid = true;
        }
        smart_home_alarm_on_fire_status(floor_id, heartbeat->fire_status >= 2);
    }
    s_model.help_floor_valid[floor_idx] = false;

    s_model.sensor_rx_count++;
    refresh_all_main_power_locked();
    publish_update();
    model_unlock();
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
        case IOT_SCENE_BRIGHT: return "明亮";
        default: return "未启用";
    }
}

void device_model_set_scene(uint8_t scene_id)
{
    model_lock();
    s_model.current_scene = scene_id;
    s_model.scene_seq++;
    lv_snprintf(s_model.scene_name, sizeof(s_model.scene_name), "%s", device_model_scene_name(scene_id));
    publish_update();
    model_unlock();
    ui_event_publish(UI_EVENT_SCENE_CHANGED);
}

void device_model_set_mqtt_state(mqtt_state_t state)
{
    model_lock();
    s_model.mqtt_state = state;
    if (state == MQTT_STATE_CONNECTED) {
        s_model.gateway_connected = true;
    }
    publish_update();
    model_unlock();
}

void device_model_set_device_online(const char *device_id, bool online)
{
    model_lock();
    rc_device_t *d = find_device(device_id);
    if (!d) { model_unlock(); return; }
    d->connected = online;
    publish_update();
    model_unlock();
}

void device_model_update_wifi_state(wifi_state_t state, const char *ssid)
{
    model_lock();
    s_model.wifi_state = state;
    if (ssid) {
        strncpy(s_model.wifi_ssid, ssid, WIFI_SSID_MAX - 1);
    }
    publish_update();
    model_unlock();
}

void device_model_update_wifi_aps(const wifi_ap_t *aps, uint16_t count)
{
    model_lock();
    if (count > WIFI_AP_MAX) count = WIFI_AP_MAX;
    if (aps && count > 0) {
        memcpy(s_model.wifi_aps, aps, count * sizeof(wifi_ap_t));
    } else {
        memset(s_model.wifi_aps, 0, sizeof(s_model.wifi_aps));
        count = 0;
    }
    s_model.wifi_ap_count = count;
    publish_update();
    model_unlock();
}

void device_model_update_sensors(float temp, float humi, uint16_t pm25,
                                  uint16_t rain, uint16_t flame, uint16_t smoke)
{
    model_lock();
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
    model_unlock();
}

void device_model_update_weather(const char *location, float temp, uint8_t humidity,
                                  float precipitation_mm, float wind_speed,
                                  uint16_t weather_code, float pm25, uint16_t aqi,
                                  bool air_quality_valid)
{
    model_lock();
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
    model_unlock();
}

void device_model_update_sensor_value(uint8_t floor_id, uint8_t sensor_type, uint16_t value)
{
    model_lock();
    uint8_t idx = 0;
    if (floor_id >= 1 && floor_id <= 3) {
        idx = floor_id - 1;
        s_model.controller_online[idx] = true;
    }

    if ((sensor_type == IOT_SENSOR_RAIN_MV || sensor_type == IOT_SENSOR_RAIN_STATUS) &&
        floor_id != 3) {
        model_unlock();
        return;
    }
    if (sensor_type == IOT_SENSOR_HELP_STATUS) {
        model_unlock();
        return;
    }

    switch (sensor_type) {
        case IOT_SENSOR_FLAME_MV:
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
                smart_home_alarm_on_rain_status(floor_id, value != 0);
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
        default:
            model_unlock();
            return;
    }

    s_model.sensor_rx_count++;
    publish_update();
    model_unlock();
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

bool device_model_get_flame_sensor_mv(uint8_t floor_id, uint16_t *value)
{
    return device_model_get_smoke_value(floor_id, value);
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
    if (s_model.rain_floor_valid[2]) count++;
    if (s_model.smoke_floor_valid[2]) count++;
    if (s_model.fire_floor_valid[2]) count++;
    return count;
}

void device_model_set_controller_online(uint8_t floor_id, bool online)
{
    model_lock();
    if (floor_id >= 1 && floor_id <= 3) {
        s_model.controller_online[floor_id - 1] = online;
        publish_update();
    }
    model_unlock();
}

void device_model_set_mqtt_stats(uint32_t rx_count, uint32_t last_seen_sec)
{
    model_lock();
    s_model.mqtt_rx_count = rx_count;
    s_model.mqtt_last_seen_sec = last_seen_sec;
    publish_update();
    model_unlock();
}

void device_model_set_floor_runtime_offline(uint8_t floor_id)
{
    if (floor_id < 1 || floor_id > 3) return;
    model_lock();
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
    model_unlock();
}

void device_model_reset_runtime_data(void)
{
    model_lock();
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
    model_unlock();
}
