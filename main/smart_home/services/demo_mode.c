#include "demo_mode.h"
#include "mqtt_device_model.h"
#include "smart_home_event_center.h"
#include "rule_engine.h"

#include <esp_log.h>
#include <esp_timer.h>

static const char *TAG = "DEMO";
static bool s_enabled = false;
static int64_t s_start_ms = 0;

void demo_mode_set_enabled(bool enabled)
{
    s_enabled = enabled;
    if (enabled) {
        s_start_ms = esp_timer_get_time() / 1000;
        ESP_LOGI(TAG, "Demo mode enabled");
    } else {
        ESP_LOGI(TAG, "Demo mode disabled");
    }
}

bool demo_mode_is_enabled(void)
{
    return s_enabled;
}

void demo_mode_tick(void)
{
    if (!s_enabled) return;

    for (uint8_t f = 1; f <= 3; f++) {
        device_model_set_controller_online(f, true);
    }
}

void demo_mode_trigger_fire(uint8_t floor_id)
{
    if (!s_enabled || floor_id < 1 || floor_id > 3) return;
    device_model_update_sensor_value(floor_id, IOT_SENSOR_FIRE_STATUS, 2);
    smart_home_event_post(SH_EVENT_FIRE_ALARM, floor_id, 2, 0, "Fire alarm (demo)");
    rule_engine_on_event(&(smart_home_event_t){.type = SH_EVENT_FIRE_ALARM, .floor_id = floor_id, .severity = 2});
}

void demo_mode_trigger_rain(uint8_t floor_id)
{
    if (!s_enabled || floor_id < 1 || floor_id > 3) return;
    device_model_update_sensor_value(floor_id, IOT_SENSOR_RAIN_STATUS, 1);
    smart_home_event_post(SH_EVENT_RAIN_ALARM, floor_id, 1, 0, "Rain detected (demo)");
    rule_engine_on_event(&(smart_home_event_t){.type = SH_EVENT_RAIN_ALARM, .floor_id = floor_id, .severity = 1});
}

void demo_mode_set_controller_online(uint8_t floor_id, bool online)
{
    device_model_set_controller_online(floor_id, online);
}
