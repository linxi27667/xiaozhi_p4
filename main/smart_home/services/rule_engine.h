#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "smart_home_event_center.h"

void rule_engine_init(void);
void rule_engine_on_event(const smart_home_event_t *event);

#ifdef __cplusplus
}
#endif
