#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int ui_device_settings_get_brightness(void);
void ui_device_settings_set_brightness(int brightness, bool permanent);
int ui_device_settings_get_volume(void);
void ui_device_settings_set_volume(int volume);

#ifdef __cplusplus
}
#endif
