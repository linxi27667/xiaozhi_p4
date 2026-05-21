#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void smart_home_alarm_ui_init(void);
void smart_home_alarm_ui_set_fire(uint8_t floor_id, bool active);

#ifdef __cplusplus
}
#endif
