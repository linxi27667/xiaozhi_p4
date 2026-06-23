#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>

#define UI_ASSET_DRIVE_LETTER 'S'
#define UI_ASSET_ROOT "/sdcard/xiaozhi_ui"
#define UI_ASSET_OVERRIDE_MARKER "/sdcard/xiaozhi_ui/.use_tf_assets"

void ui_asset_service_init(void);
int ui_asset_service_preload_required(void);
bool ui_asset_available(const char *name);
const char *ui_asset_src(const char *name);
lv_obj_t *ui_asset_image_create(lv_obj_t *parent, const char *name);
void ui_asset_service_diagnose(void);

#ifdef __cplusplus
}
#endif
