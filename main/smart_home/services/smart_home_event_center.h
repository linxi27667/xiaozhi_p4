#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SH_EVENT_DEVICE_ONLINE = 0,
    SH_EVENT_DEVICE_OFFLINE,
    SH_EVENT_COMMAND_ACK,
    SH_EVENT_COMMAND_TIMEOUT,
    SH_EVENT_SENSOR_WARNING,
    SH_EVENT_FIRE_ALARM,
    SH_EVENT_RAIN_ALARM,
    SH_EVENT_HELP_ALARM,
    SH_EVENT_RULE_TRIGGERED,
    SH_EVENT_NETWORK_RECOVERED,
} smart_home_event_type_t;

typedef struct smart_home_event {
    smart_home_event_type_t type;
    uint8_t floor_id;
    uint8_t severity;
    uint16_t seq;
    int64_t time_ms;
    char message[96];
} smart_home_event_t;

void smart_home_event_center_init(void);
void smart_home_event_post(smart_home_event_type_t type, uint8_t floor_id, uint8_t severity, uint16_t seq, const char *message);
uint16_t smart_home_event_count(void);
bool smart_home_event_at(uint16_t index, smart_home_event_t *out);
void smart_home_event_center_clear(void);

#ifdef __cplusplus
}
#endif
