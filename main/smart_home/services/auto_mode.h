#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "smart_home_event_center.h"

typedef enum {
    AUTO_TARGET_FLOOR2_BEDROOM_AMBIENT = 0,
    AUTO_TARGET_FLOOR2_LIVING_AMBIENT,
    AUTO_TARGET_FLOOR2_FAN,
    AUTO_TARGET_FLOOR2_HANGER,
    AUTO_TARGET_FLOOR3_SKYLIGHT,
    AUTO_TARGET_FLOOR3_HANGER,
    AUTO_TARGET_MAX,
} auto_target_t;

typedef struct {
    bool global_enabled;
    bool target_enabled[AUTO_TARGET_MAX];
    uint32_t trigger_count[AUTO_TARGET_MAX];
    int64_t last_trigger_ms[AUTO_TARGET_MAX];
    int64_t manual_override_ms[AUTO_TARGET_MAX];
} auto_mode_state_t;

void auto_mode_init(void);
void auto_mode_set_global(bool enabled);
bool auto_mode_get_global(void);
void auto_mode_set_target(auto_target_t target, bool enabled);
bool auto_mode_get_target(auto_target_t target);
bool auto_mode_is_effective(auto_target_t target);
const auto_mode_state_t *auto_mode_get_state(void);
void auto_mode_on_event(const smart_home_event_t *event);
void auto_mode_tick(void);
void auto_mode_mark_manual_override(auto_target_t target);
auto_target_t auto_target_from_device(const char *device_id);

#ifdef __cplusplus
}
#endif
