#include "auto_mode.h"
#include "mqtt_device_model.h"
#include "xiaozhi_mqtt.h"
#include "mqtt_iot_protocol.h"
#include "smart_home_event_center.h"
#include "ui_events.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <nvs.h>
#include <string.h>
#include <time.h>

static const char *TAG = "AUTO_MODE";
static const char *NS = "auto_mode";

static auto_mode_state_t s_state;

void auto_mode_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    for (int i = 0; i < AUTO_TARGET_MAX; i++) {
        s_state.target_enabled[i] = true;
    }

    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        uint8_t global = 0;
        if (nvs_get_u8(h, "global", &global) == ESP_OK) {
            s_state.global_enabled = global != 0;
        }

        for (int i = 0; i < AUTO_TARGET_MAX; i++) {
            char key[8];
            snprintf(key, sizeof(key), "t%d", i);
            uint8_t val = 0;
            if (nvs_get_u8(h, key, &val) == ESP_OK) {
                s_state.target_enabled[i] = val != 0;
            }
        }
        nvs_close(h);
    }

    ESP_LOGI(TAG, "Auto mode initialized, global=%d", s_state.global_enabled);
}

static void auto_mode_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;

    nvs_set_u8(h, "global", s_state.global_enabled ? 1 : 0);
    for (int i = 0; i < AUTO_TARGET_MAX; i++) {
        char key[8];
        snprintf(key, sizeof(key), "t%d", i);
        nvs_set_u8(h, key, s_state.target_enabled[i] ? 1 : 0);
    }
    nvs_commit(h);
    nvs_close(h);
}

void auto_mode_set_global(bool enabled)
{
    s_state.global_enabled = enabled;
    auto_mode_save();
    ui_event_publish(UI_EVENT_MODEL_UPDATED);
    ESP_LOGI(TAG, "Global auto mode: %s", enabled ? "ON" : "OFF");
}

bool auto_mode_get_global(void)
{
    return s_state.global_enabled;
}

void auto_mode_set_target(auto_target_t target, bool enabled)
{
    if (target >= AUTO_TARGET_MAX) return;
    s_state.target_enabled[target] = enabled;
    auto_mode_save();
    ui_event_publish(UI_EVENT_MODEL_UPDATED);
}

bool auto_mode_get_target(auto_target_t target)
{
    if (target >= AUTO_TARGET_MAX) return false;
    return s_state.target_enabled[target];
}

bool auto_mode_is_effective(auto_target_t target)
{
    if (target >= AUTO_TARGET_MAX) return false;
    return s_state.global_enabled && s_state.target_enabled[target];
}

const auto_mode_state_t *auto_mode_get_state(void)
{
    return &s_state;
}

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static bool can_trigger(auto_target_t target)
{
    if (!auto_mode_is_effective(target)) return false;
    int64_t now = now_ms();
    if (now - s_state.last_trigger_ms[target] < 10000) return false;
    if (now - s_state.manual_override_ms[target] < 60000) return false;
    return true;
}

static void do_trigger(auto_target_t target, uint8_t floor_id, uint8_t cmd, uint8_t gpio, uint8_t value)
{
    s_state.trigger_count[target]++;
    s_state.last_trigger_ms[target] = now_ms();
    mqtt_send_command_v3(floor_id, cmd, gpio, value, IOT_SOURCE_RULE);
}

void auto_mode_on_event(const smart_home_event_t *event)
{
    if (!event) return;

    if (event->type == SH_EVENT_FIRE_ALARM) {
        mqtt_send_command_v3(event->floor_id, IOT_CMD_BROADCAST_ALL_OFF, 0, 0, IOT_SOURCE_RULE);
        mqtt_send_command_v3(2, IOT_CMD_SET_AMBIENT_SCENE, 0, IOT_AMBIENT_SCENE_WARNING, IOT_SOURCE_RULE);
        mqtt_send_command_v3(2, IOT_CMD_SET_AMBIENT_SCENE, 1, IOT_AMBIENT_SCENE_WARNING, IOT_SOURCE_RULE);
        return;
    }

    if (event->type == SH_EVENT_RAIN_ALARM) {
        if (can_trigger(AUTO_TARGET_FLOOR2_HANGER)) {
            do_trigger(AUTO_TARGET_FLOOR2_HANGER, 2, IOT_CMD_SET_SERVO, 6, 0);
        }
        if (can_trigger(AUTO_TARGET_FLOOR3_HANGER)) {
            do_trigger(AUTO_TARGET_FLOOR3_HANGER, 3, IOT_CMD_SET_SERVO, 8, 0);
        }
        if (can_trigger(AUTO_TARGET_FLOOR2_LIVING_AMBIENT)) {
            do_trigger(AUTO_TARGET_FLOOR2_LIVING_AMBIENT, 2, IOT_CMD_SET_AMBIENT_SCENE, 1, IOT_AMBIENT_SCENE_RAIN);
        }
        if (can_trigger(AUTO_TARGET_FLOOR3_SKYLIGHT)) {
            do_trigger(AUTO_TARGET_FLOOR3_SKYLIGHT, 3, IOT_CMD_SET_SERVO, 6, 0);
            do_trigger(AUTO_TARGET_FLOOR3_SKYLIGHT, 3, IOT_CMD_SET_SERVO, 7, 0);
        }
        return;
    }

    if (event->type == SH_EVENT_DEVICE_OFFLINE) {
        ESP_LOGW(TAG, "Device offline: floor=%d seq=%u msg=%s",
                 event->floor_id, event->seq, event->message);
        return;
    }

    if (event->type == SH_EVENT_HELP_ALARM) {
        if (can_trigger(AUTO_TARGET_FLOOR2_BEDROOM_AMBIENT)) {
            do_trigger(AUTO_TARGET_FLOOR2_BEDROOM_AMBIENT, event->floor_id, IOT_CMD_SET_AMBIENT_SCENE, 0, IOT_AMBIENT_SCENE_WARNING);
        }
        if (can_trigger(AUTO_TARGET_FLOOR2_LIVING_AMBIENT)) {
            do_trigger(AUTO_TARGET_FLOOR2_LIVING_AMBIENT, event->floor_id, IOT_CMD_SET_AMBIENT_SCENE, 1, IOT_AMBIENT_SCENE_WARNING);
        }
        return;
    }
}

static int8_t s_last_triggered_hour = -1;

void auto_mode_tick(void)
{
    if (!s_state.global_enabled) return;

    time_t t = time(NULL);
    struct tm *now_tm = localtime(&t);
    if (!now_tm) return;

    int8_t hour = (int8_t)now_tm->tm_hour;
    if (hour == s_last_triggered_hour) return;

    if (hour == 23 && can_trigger(AUTO_TARGET_FLOOR2_BEDROOM_AMBIENT)) {
        do_trigger(AUTO_TARGET_FLOOR2_BEDROOM_AMBIENT, 2, IOT_CMD_SET_AMBIENT_SCENE, 0, IOT_AMBIENT_SCENE_SLEEP);
        if (can_trigger(AUTO_TARGET_FLOOR2_LIVING_AMBIENT)) {
            do_trigger(AUTO_TARGET_FLOOR2_LIVING_AMBIENT, 2, IOT_CMD_SET_AMBIENT_SCENE, 1, IOT_AMBIENT_SCENE_OFF);
        }
        mqtt_send_command_v3(2, IOT_CMD_BROADCAST_LIGHTS_OFF, 0, 0, IOT_SOURCE_RULE);
        s_last_triggered_hour = hour;
    } else if (hour == 18 && can_trigger(AUTO_TARGET_FLOOR2_LIVING_AMBIENT)) {
        do_trigger(AUTO_TARGET_FLOOR2_LIVING_AMBIENT, 2, IOT_CMD_SET_AMBIENT_SCENE, 1, IOT_AMBIENT_SCENE_WARM_HOME);
        s_last_triggered_hour = hour;
    }
}

void auto_mode_mark_manual_override(auto_target_t target)
{
    if (target >= AUTO_TARGET_MAX) return;
    s_state.manual_override_ms[target] = now_ms();
    ESP_LOGI(TAG, "Manual override set for target %d, auto paused 60s", target);
}

auto_target_t auto_target_from_device(const char *device_id)
{
    if (!device_id) return AUTO_TARGET_MAX;
    if (strcmp(device_id, "floor2_master_ambient") == 0) return AUTO_TARGET_FLOOR2_BEDROOM_AMBIENT;
    if (strcmp(device_id, "floor2_living_ambient") == 0) return AUTO_TARGET_FLOOR2_LIVING_AMBIENT;
    if (strcmp(device_id, "floor2_fan") == 0) return AUTO_TARGET_FLOOR2_FAN;
    if (strcmp(device_id, "floor2_hanger") == 0) return AUTO_TARGET_FLOOR2_HANGER;
    if (strcmp(device_id, "floor3_left_skylight") == 0 || strcmp(device_id, "floor3_right_skylight") == 0) return AUTO_TARGET_FLOOR3_SKYLIGHT;
    if (strcmp(device_id, "floor3_hanger") == 0) return AUTO_TARGET_FLOOR3_HANGER;
    return AUTO_TARGET_MAX;
}
