#include "ui_events.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static const char *TAG = "UI_EVT";

static const char *s_event_names[] = {
    [UI_EVENT_MODEL_UPDATED]      = "MODEL_UPDATED",
    [UI_EVENT_MQTT_CONNECTED]     = "MQTT_CONNECTED",
    [UI_EVENT_MQTT_DISCONNECTED]  = "MQTT_DISCONNECTED",
    [UI_EVENT_WIFI_CHANGED]       = "WIFI_CHANGED",
    [UI_EVENT_LANG_CHANGED]       = "LANG_CHANGED",
    [UI_EVENT_PAGE_SWITCHED]      = "PAGE_SWITCHED",
    [UI_EVENT_FIRE_ALARM]         = "FIRE_ALARM",
};

#define MAX_SUBSCRIBERS 16

typedef struct {
    ui_event_cb_t cb;
    void *user_data;
} subscriber_t;

static EXT_RAM_BSS_ATTR struct {
    subscriber_t subs[MAX_SUBSCRIBERS];
    int count;
} s_events[UI_EVENT_COUNT];

static portMUX_TYPE s_event_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t s_pending_events;
static bool s_initialized;

void ui_events_init(void)
{
    portENTER_CRITICAL(&s_event_lock);
    if (!s_initialized) {
        s_pending_events = 0;
        s_initialized = true;
    }
    portEXIT_CRITICAL(&s_event_lock);
}

void ui_event_subscribe(ui_event_type_t evt, ui_event_cb_t cb, void *user_data)
{
    if (evt >= UI_EVENT_COUNT || !cb) return;
    portENTER_CRITICAL(&s_event_lock);
    for (int i = 0; i < s_events[evt].count; i++) {
        if (s_events[evt].subs[i].cb == cb &&
            s_events[evt].subs[i].user_data == user_data) {
            portEXIT_CRITICAL(&s_event_lock);
            return;
        }
    }
    if (s_events[evt].count >= MAX_SUBSCRIBERS) {
        portEXIT_CRITICAL(&s_event_lock);
        return;
    }
    s_events[evt].subs[s_events[evt].count].cb = cb;
    s_events[evt].subs[s_events[evt].count].user_data = user_data;
    s_events[evt].count++;
    portEXIT_CRITICAL(&s_event_lock);
}

void ui_event_unsubscribe(ui_event_type_t evt, ui_event_cb_t cb, void *user_data)
{
    if (evt >= UI_EVENT_COUNT) return;
    portENTER_CRITICAL(&s_event_lock);
    for (int i = 0; i < s_events[evt].count; i++) {
        bool cb_match = (cb == UI_UNSUB_ANY || s_events[evt].subs[i].cb == cb);
        bool ud_match = (user_data == UI_UNSUB_ANY || s_events[evt].subs[i].user_data == user_data);
        if (cb_match && ud_match) {
            s_events[evt].subs[i] = s_events[evt].subs[s_events[evt].count - 1];
            s_events[evt].count--;
            break;
        }
    }
    portEXIT_CRITICAL(&s_event_lock);
}

void ui_event_publish(ui_event_type_t evt)
{
    if (evt >= UI_EVENT_COUNT) return;
    portENTER_CRITICAL(&s_event_lock);
    s_pending_events |= (1u << evt);
    portEXIT_CRITICAL(&s_event_lock);
}

void ui_events_dispatch_pending(void)
{
    uint32_t pending = 0;
    portENTER_CRITICAL(&s_event_lock);
    pending = s_pending_events;
    s_pending_events = 0;
    portEXIT_CRITICAL(&s_event_lock);

    for (ui_event_type_t evt = 0; evt < UI_EVENT_COUNT; evt++) {
        if ((pending & (1u << evt)) == 0) continue;

        subscriber_t local[MAX_SUBSCRIBERS];
        int count = 0;

        portENTER_CRITICAL(&s_event_lock);
        count = s_events[evt].count;
        if (count > MAX_SUBSCRIBERS) count = MAX_SUBSCRIBERS;
        memcpy(local, s_events[evt].subs, sizeof(subscriber_t) * count);
        portEXIT_CRITICAL(&s_event_lock);

        ESP_LOGD(TAG, "dispatch %s (%d subs)", s_event_names[evt], count);
        for (int i = 0; i < count; i++) {
            if (local[i].cb) local[i].cb(local[i].user_data);
        }
    }
}
