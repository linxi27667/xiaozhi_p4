#include "smart_home_event_center.h"
#include "ui_events.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *TAG = "EVT_CENTER";

#define SH_EVENT_HISTORY_MAX 32
static smart_home_event_t s_history[SH_EVENT_HISTORY_MAX];
static uint16_t s_write_index;
static uint16_t s_count;

void smart_home_event_center_init(void)
{
    memset(s_history, 0, sizeof(s_history));
    s_write_index = 0;
    s_count = 0;
    ESP_LOGI(TAG, "Event center initialized");
}

void smart_home_event_post(smart_home_event_type_t type, uint8_t floor_id, uint8_t severity, uint16_t seq, const char *message)
{
    smart_home_event_t *evt = &s_history[s_write_index % SH_EVENT_HISTORY_MAX];
    memset(evt, 0, sizeof(*evt));
    evt->type = type;
    evt->floor_id = floor_id;
    evt->severity = severity;
    evt->seq = seq;
    evt->time_ms = esp_timer_get_time() / 1000;
    if (message) {
        strncpy(evt->message, message, sizeof(evt->message) - 1);
    }

    s_write_index++;
    if (s_count < SH_EVENT_HISTORY_MAX) s_count++;

    ESP_LOGI(TAG, "Event: type=%d floor=%u sev=%u msg=%s",
             type, floor_id, severity, message ? message : "");

    ui_event_publish(UI_EVENT_MODEL_UPDATED);
}

uint16_t smart_home_event_count(void)
{
    return s_count;
}

bool smart_home_event_at(uint16_t index, smart_home_event_t *out)
{
    if (index >= s_count || !out) return false;
    if (s_count < SH_EVENT_HISTORY_MAX) {
        *out = s_history[index];
    } else {
        uint16_t start = s_write_index % SH_EVENT_HISTORY_MAX;
        uint16_t actual = (start + index) % SH_EVENT_HISTORY_MAX;
        *out = s_history[actual];
    }
    return true;
}

void smart_home_event_center_clear(void)
{
    memset(s_history, 0, sizeof(s_history));
    s_write_index = 0;
    s_count = 0;
    ESP_LOGI(TAG, "Event history cleared");
    ui_event_publish(UI_EVENT_MODEL_UPDATED);
}
