#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef enum {
    UI_EVENT_MODEL_UPDATED = 0,
    UI_EVENT_MQTT_CONNECTED,
    UI_EVENT_MQTT_DISCONNECTED,
    UI_EVENT_WIFI_CHANGED,
    UI_EVENT_LANG_CHANGED,
    UI_EVENT_PAGE_SWITCHED,
    UI_EVENT_FIRE_ALARM,
    UI_EVENT_SCENE_CHANGED,
    UI_EVENT_COUNT
} ui_event_type_t;

#define UI_UNSUB_ANY ((void *)(intptr_t)-1)

typedef void (*ui_event_cb_t)(void *user_data);

void ui_events_init(void);
void ui_event_subscribe(ui_event_type_t evt, ui_event_cb_t cb, void *user_data);
void ui_event_unsubscribe(ui_event_type_t evt, ui_event_cb_t cb, void *user_data);
void ui_event_publish(ui_event_type_t evt);
void ui_events_dispatch_pending(void);

#ifdef __cplusplus
}
#endif
