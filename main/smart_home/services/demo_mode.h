#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

void demo_mode_set_enabled(bool enabled);
bool demo_mode_is_enabled(void);
void demo_mode_tick(void);
void demo_mode_trigger_fire(uint8_t floor_id);
void demo_mode_trigger_rain(uint8_t floor_id);
void demo_mode_set_controller_online(uint8_t floor_id, bool online);

#ifdef __cplusplus
}
#endif
