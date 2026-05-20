#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui_theme.h"

lv_obj_t *ui_create_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color);
lv_obj_t *ui_create_icon(lv_obj_t *parent, const char *icon, const lv_font_t *font, lv_color_t color);
lv_obj_t *ui_create_dot(lv_obj_t *parent, lv_color_t color, int size);
lv_obj_t *ui_create_switch(lv_obj_t *parent, lv_color_t active_color);
lv_obj_t *ui_create_card(lv_obj_t *parent);
void ui_apply_card_shadow(lv_obj_t *card);
void ui_apply_soft_gradient(lv_obj_t *obj, lv_color_t base, lv_color_t grad);
void ui_apply_status_transition(lv_obj_t *obj);
void ui_apply_press_feedback(lv_obj_t *obj, lv_color_t accent);
void ui_fade_in(lv_obj_t *obj, uint32_t delay_ms);
void ui_fade_slide_in(lv_obj_t *obj, uint32_t delay_ms, int32_t from_y);
lv_obj_t *ui_create_page(lv_obj_t *parent, lv_event_cb_t delete_cb, void *user_data);
lv_obj_t *ui_create_title_row(lv_obj_t *parent, const char *icon, const char *title);

void ui_styles_init(void);

#ifdef __cplusplus
}
#endif
