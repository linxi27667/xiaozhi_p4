#include "rule_engine.h"
#include "auto_mode.h"
#include "xiaozhi_mqtt.h"
#include "mqtt_iot_protocol.h"
#include "mqtt_device_model.h"
#include "ui_events.h"

#include <esp_log.h>

static const char *TAG = "RULE";

void rule_engine_init(void)
{
    ESP_LOGI(TAG, "Rule engine initialized");
}

void rule_engine_on_event(const smart_home_event_t *event)
{
    if (!event) return;

    auto_mode_on_event(event);

    if (event->type == SH_EVENT_FIRE_ALARM) {
        ESP_LOGW(TAG, "Fire alarm floor=%u - triggering emergency response", event->floor_id);
    }

    if (event->type == SH_EVENT_RAIN_ALARM) {
        ESP_LOGI(TAG, "Rain alarm floor=%u - triggering rain response", event->floor_id);
    }

    if (event->type == SH_EVENT_HELP_ALARM) {
        ESP_LOGW(TAG, "Help alarm floor=%u - triggering alert", event->floor_id);
    }

    if (event->type == SH_EVENT_DEVICE_OFFLINE) {
        ESP_LOGW(TAG, "Device offline floor=%u - pausing auto control", event->floor_id);
    }
}
