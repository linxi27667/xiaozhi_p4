#include "ui_welcome_popup.h"

#include "mqtt_device_model.h"
#include "mqtt_iot_protocol.h"
#include "ui_events.h"
#include "ui_font.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "ui_styles.h"

#include "lvgl.h"
#include <stdbool.h>

static lv_obj_t *s_popup;
static uint32_t s_last_scene_seq;
static bool s_initialized;

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int size, lv_color_t color,
    int x, int y, int w)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text ? text : "");
    lv_obj_set_style_text_font(lbl, ui_font_cn((uint8_t)size), 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_pos(lbl, x, y);
    if (w > 0) {
        lv_obj_set_width(lbl, w);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    }
    return lbl;
}

static void close_popup(void)
{
    if (!s_popup) return;
    lv_obj_delete(s_popup);
    s_popup = NULL;
}

static void on_close(lv_event_t *e)
{
    (void)e;
    close_popup();
}

static void show_home_popup(void)
{
    close_popup();

    lv_obj_t *scr = lv_screen_active();
    s_popup = lv_obj_create(scr);
    lv_obj_remove_style_all(s_popup);
    lv_obj_set_size(s_popup, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(s_popup, LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(s_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_popup, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_popup, LV_OPA_20, 0);

    lv_obj_t *box = lv_obj_create(s_popup);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 360, 168);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_set_style_bg_color(box, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 10, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, UI_COLOR_BORDER, 0);
    lv_obj_set_style_shadow_width(box, 10, 0);
    lv_obj_set_style_shadow_opa(box, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(box, UI_COLOR_SHADOW, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ic = ui_create_icon(box, ICON_HOME, ui_font_icon(24), UI_COLOR_ACCENT);
    lv_obj_set_pos(ic, 24, 24);
    label(box, tr("欢迎回家", "Welcome Home"), 22, UI_COLOR_TEXT_STRONG, 64, 20, 160);
    label(box,
        tr("已为你打开大厅灯，并将主卧灯调为暖色氛围。", "Hall light is on and master light is set to warm ambience."),
        13, UI_COLOR_TEXT_SEC, 24, 66, 300);

    lv_obj_t *btn = lv_btn_create(box);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, 312, 16);
    lv_obj_set_size(btn, 32, 32);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_bg_color(btn, UI_COLOR_INPUT_BG, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(btn, on_close, LV_EVENT_CLICKED, NULL);
    lv_obj_t *x = label(btn, "X", 14, UI_COLOR_TEXT_SEC, 0, 8, 32);
    lv_obj_set_style_text_align(x, LV_TEXT_ALIGN_CENTER, 0);
}

static void on_scene_changed(void *user_data)
{
    (void)user_data;
    const mqtt_device_model_t *model = device_model_get();
    if (model->scene_seq == s_last_scene_seq) {
        return;
    }
    s_last_scene_seq = model->scene_seq;
    if (model->current_scene == IOT_SCENE_HOME) {
        show_home_popup();
    }
}

void ui_welcome_popup_init(void)
{
    if (s_initialized) {
        return;
    }
    s_initialized = true;
    ui_event_subscribe(UI_EVENT_SCENE_CHANGED, on_scene_changed, NULL);
}
