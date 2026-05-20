#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

LV_FONT_DECLARE(ui_font_cn_16);
LV_FONT_DECLARE(ui_font_cn_20);
LV_FONT_DECLARE(ui_font_cn_30);
LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_awesome_20_4);
LV_FONT_DECLARE(font_awesome_30_4);

#define UI_FONT_14  &ui_font_cn_16
#define UI_FONT_12  &ui_font_cn_16
#define UI_FONT_16  &ui_font_cn_16
#define UI_FONT_20  &ui_font_cn_20
#define UI_FONT_24  &ui_font_cn_30

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
