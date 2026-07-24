#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void gesture_ui_show(void);
void gesture_ui_hide(void);
bool gesture_ui_is_visible(void);

#ifdef __cplusplus
}
#endif
