#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

LV_FONT_DECLARE(font_puhui_basic_16_4);
LV_FONT_DECLARE(font_puhui_basic_20_4);
LV_FONT_DECLARE(font_puhui_basic_30_4);
LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_awesome_20_4);
LV_FONT_DECLARE(font_awesome_30_4);

#define UI_FONT_14  &font_puhui_basic_16_4
#define UI_FONT_12  &font_puhui_basic_16_4
#define UI_FONT_16  &font_puhui_basic_16_4
#define UI_FONT_20  &font_puhui_basic_20_4
#define UI_FONT_24  &font_puhui_basic_30_4

#define UI_FONT_TITLE  UI_FONT_20
#define UI_FONT_LABEL  UI_FONT_14
#define UI_FONT_SMALL  UI_FONT_12
#define UI_FONT_VALUE  UI_FONT_24

void ui_font_init(void);
const lv_font_t *ui_font_cn(uint8_t size);
const lv_font_t *ui_font_icon(uint8_t size);

#ifdef __cplusplus
}
#endif
