#include "ui_anim.h"
#include <stdint.h>

#define ANIM_RES_SHIFT 10
#define ANIM_RESOLUTION (1L << ANIM_RES_SHIFT)

static int32_t Path_Ease_Out_Quart(const lv_anim_t *a)
{
    int32_t t = lv_map(a->act_time, 0, a->duration, 0, ANIM_RESOLUTION);
    int64_t t1 = (int64_t)t - ANIM_RESOLUTION;
    int64_t t1_sq = t1 * t1;
    int32_t eased = (int32_t)(-(t1_sq * t1_sq) / ((int64_t)ANIM_RESOLUTION * ANIM_RESOLUTION * ANIM_RESOLUTION) + ANIM_RESOLUTION);

    int64_t new_value = (int64_t)eased * (a->end_value - a->start_value);
    new_value = new_value >> ANIM_RES_SHIFT;
    new_value += a->start_value;
    return (int32_t)new_value;
}

static int32_t Path_Ease_In_Out_Cubic(const lv_anim_t *a)
{
    int32_t t = lv_map(a->act_time, 0, a->duration, 0, ANIM_RESOLUTION);
    int64_t tl = (int64_t)t;
    int32_t eased;
    if (t < ANIM_RESOLUTION / 2) {
        eased = (int32_t)(4 * tl * tl * tl / ((int64_t)ANIM_RESOLUTION * ANIM_RESOLUTION));
    } else {
        int64_t t1 = 2 * tl - ANIM_RESOLUTION;
        eased = (int32_t)(t1 * t1 * t1 / (4 * ANIM_RESOLUTION) + ANIM_RESOLUTION / 2);
    }

    int64_t new_value = (int64_t)eased * (a->end_value - a->start_value);
    new_value = new_value >> ANIM_RES_SHIFT;
    new_value += a->start_value;
    return (int32_t)new_value;
}

static int32_t Path_Ease_Out_Back(const lv_anim_t *a)
{
    int32_t t = lv_map(a->act_time, 0, a->duration, 0, ANIM_RESOLUTION);
    const int64_t s = 362;
    int64_t t1 = (int64_t)t - ANIM_RESOLUTION;
    int32_t eased = (int32_t)(t1 * t1 * ((s + 256) * t1 + s) / 65536 + ANIM_RESOLUTION);

    int64_t new_value = (int64_t)eased * (a->end_value - a->start_value);
    new_value = new_value >> ANIM_RES_SHIFT;
    new_value += a->start_value;
    return (int32_t)new_value;
}

int32_t UI_Anim_Ease_Out_Quart(int32_t t)
{
    int64_t t1 = (int64_t)t - 1024;
    int64_t t1_sq = t1 * t1;
    return (int32_t)(-(t1_sq * t1_sq) / ((int64_t)1024 * 1024 * 1024) + 1024);
}

int32_t UI_Anim_Ease_In_Out_Cubic(int32_t t)
{
    int64_t tl = (int64_t)t;
    if (t < 512) {
        return (int32_t)(4 * tl * tl * tl / ((int64_t)1024 * 1024));
    } else {
        int64_t t1 = 2 * tl - 1024;
        return (int32_t)(t1 * t1 * t1 / (4 * 1024) + 512);
    }
}

int32_t UI_Anim_Ease_Out_Back(int32_t t)
{
    const int64_t s = 362;
    int64_t t1 = (int64_t)t - 1024;
    return (int32_t)(t1 * t1 * ((s + 256) * t1 + s) / 65536 + 1024);
}

static lv_anim_path_cb_t Get_Path_Cb(ui_anim_ease_t ease)
{
    switch (ease) {
        case UI_ANIM_EASE_OUT_QUART:      return Path_Ease_Out_Quart;
        case UI_ANIM_EASE_IN_OUT_CUBIC:   return Path_Ease_In_Out_Cubic;
        case UI_ANIM_EASE_OUT_BACK:       return Path_Ease_Out_Back;
        case UI_ANIM_EASE_OUT_ELASTIC:    return lv_anim_path_ease_out;
        case UI_ANIM_EASE_IN_QUAD:        return lv_anim_path_ease_in;
        case UI_ANIM_EASE_OUT_CIRC:       return lv_anim_path_ease_out;
        default:                          return lv_anim_path_ease_out;
    }
}

static void Anim_Exec_Opa(void *var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void Anim_Exec_Scale(void *var, int32_t v)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)var, v, 0);
}

void UI_Anim_Slide_In(lv_obj_t *obj, int32_t target_x, uint32_t duration)
{
    lv_anim_del(obj, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, lv_obj_get_x(obj), target_x);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_path_cb(&a, Path_Ease_Out_Quart);
    lv_anim_start(&a);
}

void UI_Anim_Slide_Out(lv_obj_t *obj, int32_t target_x, uint32_t duration,
                       lv_anim_completed_cb_t on_ready, void *user_data)
{
    lv_anim_del(obj, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, lv_obj_get_x(obj), target_x);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_path_cb(&a, Path_Ease_In_Out_Cubic);
    lv_anim_set_completed_cb(&a, on_ready);
    lv_anim_set_user_data(&a, user_data);
    lv_anim_start(&a);
}

void UI_Anim_Fade_In(lv_obj_t *obj, uint32_t duration)
{
    lv_anim_del(obj, Anim_Exec_Opa);
    lv_obj_set_style_opa(obj, LV_OPA_TRANSP, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, Anim_Exec_Opa);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_path_cb(&a, Path_Ease_Out_Quart);
    lv_anim_start(&a);
}

void UI_Anim_Fade_Out(lv_obj_t *obj, uint32_t duration)
{
    lv_anim_del(obj, Anim_Exec_Opa);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, Anim_Exec_Opa);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_path_cb(&a, Path_Ease_In_Out_Cubic);
    lv_anim_start(&a);
}

void UI_Anim_Scale_In(lv_obj_t *obj, uint32_t duration)
{
    lv_anim_del(obj, Anim_Exec_Scale);
    lv_obj_set_style_transform_scale(obj, 128, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, Anim_Exec_Scale);
    lv_anim_set_values(&a, 128, 256);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_path_cb(&a, Path_Ease_Out_Back);
    lv_anim_start(&a);
}

void UI_Anim_Scale_Out(lv_obj_t *obj, uint32_t duration)
{
    lv_anim_del(obj, Anim_Exec_Scale);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, Anim_Exec_Scale);
    lv_anim_set_values(&a, 256, 128);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_path_cb(&a, Path_Ease_In_Out_Cubic);
    lv_anim_start(&a);
}

void UI_Anim_Press_Effect(lv_obj_t *obj)
{
    lv_anim_del(obj, Anim_Exec_Scale);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, Anim_Exec_Scale);
    lv_anim_set_values(&a, 256, 240);
    lv_anim_set_duration(&a, 80);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

void UI_Anim_Release_Effect(lv_obj_t *obj)
{
    lv_anim_del(obj, Anim_Exec_Scale);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, Anim_Exec_Scale);
    lv_anim_set_values(&a, 240, 256);
    lv_anim_set_duration(&a, 200);
    lv_anim_set_path_cb(&a, Path_Ease_Out_Back);
    lv_anim_start(&a);
}

void UI_Anim_Value_Animate(lv_obj_t *obj, int32_t start, int32_t end,
                           uint32_t duration, lv_anim_exec_xcb_t exec_cb)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, start, end);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_path_cb(&a, Path_Ease_Out_Quart);
    lv_anim_start(&a);
}

void UI_Anim_Custom_Ease(lv_obj_t *obj, int32_t start_val, int32_t end_val,
                         uint32_t duration, lv_anim_exec_xcb_t exec_cb,
                         ui_anim_ease_t ease, lv_anim_completed_cb_t on_ready,
                         void *user_data)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, start_val, end_val);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_path_cb(&a, Get_Path_Cb(ease));
    lv_anim_set_completed_cb(&a, on_ready);
    lv_anim_set_user_data(&a, user_data);
    lv_anim_start(&a);
}
