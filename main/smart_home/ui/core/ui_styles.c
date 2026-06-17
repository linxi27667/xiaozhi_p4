#include "ui_styles.h"
#include "ui_theme.h"
#include "ui_font.h"
#include <string.h>

static lv_style_transition_dsc_t s_fast_transition;
static lv_style_transition_dsc_t s_state_transition;
static bool s_transition_ready;

static void opa_anim_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void translate_y_anim_cb(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t *)obj, v, 0);
}

static void press_feedback_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_obj_set_style_transform_scale(obj, 246, 0);
        lv_obj_set_style_border_width(obj, 2, 0);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_transform_scale(obj, 256, 0);
        lv_obj_set_style_border_width(obj, 1, 0);
    }
}

lv_obj_t *ui_create_label(lv_obj_t *parent, const char *text,
    const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text ? text : "");
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    return lbl;
}

lv_obj_t *ui_create_icon(lv_obj_t *parent, const char *icon,
    const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, icon ? icon : "");
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    return lbl;
}

lv_obj_t *ui_create_dot(lv_obj_t *parent, lv_color_t color, int size)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, size, size);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return dot;
}

lv_obj_t *ui_create_switch(lv_obj_t *parent, lv_color_t active_color)
{
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_set_style_bg_color(sw, UI_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, active_color,
        (lv_style_selector_t)LV_PART_INDICATOR | (lv_style_selector_t)LV_STATE_CHECKED);
    return sw;
}

lv_obj_t *ui_create_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(card, UI_COLOR_CARD_SOFT, 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_grad_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(card, 16, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(card, UI_COLOR_SHADOW, 0);
    lv_obj_set_style_shadow_offset_y(card, 6, 0);
    lv_obj_set_style_pad_all(card, 18, 0);
    lv_obj_set_style_pad_gap(card, 8, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

void ui_apply_card_shadow(lv_obj_t *card)
{
    lv_obj_set_style_shadow_width(card, 16, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(card, UI_COLOR_SHADOW, 0);
    lv_obj_set_style_shadow_offset_y(card, 6, 0);
}

void ui_apply_soft_gradient(lv_obj_t *obj, lv_color_t base, lv_color_t grad)
{
    lv_obj_set_style_bg_color(obj, base, 0);
    lv_obj_set_style_bg_grad_color(obj, grad, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_grad_opa(obj, LV_OPA_COVER, 0);
}

void ui_apply_status_transition(lv_obj_t *obj)
{
    if (!obj || !s_transition_ready) return;
    lv_obj_set_style_transition(obj, &s_state_transition, 0);
}

void ui_apply_press_feedback(lv_obj_t *obj, lv_color_t accent)
{
    if (!obj) return;
    lv_obj_set_style_transform_scale(obj, 256, 0);
    lv_obj_set_style_border_color(obj, accent, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_mix(accent, UI_COLOR_CARD, 40), LV_STATE_PRESSED);
    if (s_transition_ready) lv_obj_set_style_transition(obj, &s_fast_transition, 0);
    lv_obj_add_event_cb(obj, press_feedback_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, press_feedback_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(obj, press_feedback_cb, LV_EVENT_PRESS_LOST, NULL);
}

void ui_fade_in(lv_obj_t *obj, uint32_t delay_ms)
{
    if (!obj) return;
    lv_obj_set_style_opa(obj, LV_OPA_TRANSP, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_duration(&a, 260);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, opa_anim_cb);
    lv_anim_start(&a);
}

void ui_fade_slide_in(lv_obj_t *obj, uint32_t delay_ms, int32_t from_y)
{
    if (!obj) return;
    ui_fade_in(obj, delay_ms);
    lv_obj_set_style_translate_y(obj, from_y, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, from_y, 0);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_duration(&a, 260);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, translate_y_anim_cb);
    lv_anim_start(&a);
}

lv_obj_t *ui_create_page(lv_obj_t *parent, lv_event_cb_t delete_cb, void *user_data)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_width(page, lv_pct(100));
    lv_obj_set_height(page, lv_pct(100));
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_hor(page, 18, 0);
    lv_obj_set_style_pad_top(page, 14, 0);
    lv_obj_set_style_pad_bottom(page, 16, 0);
    lv_obj_set_layout(page, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(page, 10, 0);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_AUTO);
    if (delete_cb && user_data) {
        lv_obj_add_event_cb(page, delete_cb, LV_EVENT_DELETE, user_data);
    }
    return page;
}

lv_obj_t *ui_create_title_row(lv_obj_t *parent, const char *icon,
    const char *title)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 34);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_icon(row, icon, ui_font_icon(20), UI_COLOR_ACCENT);
    ui_create_label(row, title, ui_font_cn(20), UI_COLOR_TEXT_STRONG);
    return row;
}

void ui_styles_init(void)
{
    static const lv_style_prop_t fast_props[] = {
        LV_STYLE_BG_COLOR,
        LV_STYLE_BORDER_COLOR,
        LV_STYLE_BORDER_WIDTH,
        LV_STYLE_TRANSFORM_SCALE_X,
        LV_STYLE_TRANSFORM_SCALE_Y,
        LV_STYLE_OPA,
        0
    };
    static const lv_style_prop_t state_props[] = {
        LV_STYLE_TEXT_COLOR,
        LV_STYLE_BG_COLOR,
        LV_STYLE_BORDER_COLOR,
        LV_STYLE_OPA,
        0
    };

    lv_style_transition_dsc_init(&s_fast_transition, fast_props,
        lv_anim_path_ease_out, 180, 0, NULL);
    lv_style_transition_dsc_init(&s_state_transition, state_props,
        lv_anim_path_ease_out, 260, 0, NULL);
    s_transition_ready = true;
}
