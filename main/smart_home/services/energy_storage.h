#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool energy_storage_load(void);
bool energy_storage_save(void);
void energy_storage_schedule_save(void);
bool energy_storage_is_save_pending(void);

#ifdef __cplusplus
}
#endif
