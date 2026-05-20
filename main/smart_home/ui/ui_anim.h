#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef enum {
    UI_ANIM_EASE_OUT_QUART = 0,
    UI_ANIM_EASE_IN_OUT_CUBIC,
    UI_ANIM_EASE_OUT_BACK,
    UI_ANIM_EASE_OUT_ELASTIC,
    UI_ANIM_EASE_IN_QUAD,
    UI_ANIM_EASE_OUT_CIRC,
} ui_anim_ease_t;

void UI_Anim_Slide_In(lv_obj_t *obj, int32_t target_x, uint32_t duration);
void UI_Anim_Slide_Out(lv_obj_t *obj, int32_t target_x, uint32_t duration,
                       lv_anim_completed_cb_t on_ready, void *user_data);
void UI_Anim_Fade_In(lv_obj_t *obj, uint32_t duration);
void UI_Anim_Fade_Out(lv_obj_t *obj, uint32_t duration);
void UI_Anim_Scale_In(lv_obj_t *obj, uint32_t duration);
void UI_Anim_Scale_Out(lv_obj_t *obj, uint32_t duration);
void UI_Anim_Press_Effect(lv_obj_t *obj);
void UI_Anim_Release_Effect(lv_obj_t *obj);
void UI_Anim_Value_Animate(lv_obj_t *obj, int32_t start, int32_t end,
                           uint32_t duration, lv_anim_exec_xcb_t exec_cb);
void UI_Anim_Custom_Ease(lv_obj_t *obj, int32_t start_val, int32_t end_val,
                         uint32_t duration, lv_anim_exec_xcb_t exec_cb,
                         ui_anim_ease_t ease, lv_anim_completed_cb_t on_ready,
                         void *user_data);

int32_t UI_Anim_Ease_Out_Quart(int32_t t);
int32_t UI_Anim_Ease_In_Out_Cubic(int32_t t);
int32_t UI_Anim_Ease_Out_Back(int32_t t);

#ifdef __cplusplus
}
#endif
