#include "smart_home_alarm_ui.h"

#include "ui_events.h"
#include "ui_font.h"
#include "ui_icons.h"
#include "ui_styles.h"
#include "ui_theme.h"

#include "lvgl.h"

static lv_obj_t *s_modal;
static uint8_t s_fire_floor_id;
static bool s_fire_active;
static bool s_fire_acknowledged;
static bool s_initialized;

static lv_obj_t *modal_label(lv_obj_t *parent, const char *text, int size,
    lv_color_t color, int x, int y, int w)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, ui_font_cn((uint8_t)size), 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_width(lbl, w);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    return lbl;
}

static lv_obj_t *modal_panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, UI_COLOR_RED, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static const char *floor_name(uint8_t floor_id)
{
    switch (floor_id) {
        case 1: return "一楼";
        case 2: return "二楼";
        case 3: return "三楼";
        default: return "未知楼层";
    }
}

static void close_modal(void)
{
    if (s_modal) {
        lv_obj_delete(s_modal);
        s_modal = NULL;
    }
}

static void on_alarm_ack(lv_event_t *e)
{
    (void)e;
    s_fire_acknowledged = true;
    close_modal();
}

static void show_fire_modal(void)
{
    if (!s_fire_active || s_fire_acknowledged) {
        close_modal();
        return;
    }

    if (s_modal) {
        return;
    }

    s_modal = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_modal);
    lv_obj_set_size(s_modal, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(s_modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_50, 0);
    lv_obj_clear_flag(s_modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = modal_panel(s_modal, 312, 160, 400, 250);
    lv_obj_t *icon = ui_create_icon(box, ICON_FIRE, ui_font_icon(30), UI_COLOR_RED);
    lv_obj_set_pos(icon, 28, 28);

    modal_label(box, "火灾警报", 24, UI_COLOR_RED, 76, 25, 260);

    char body[128];
    lv_snprintf(body, sizeof(body), "检测到%s发生火灾风险，请立即检查烟雾/火焰传感器和现场设备。", floor_name(s_fire_floor_id));
    modal_label(box, body, 16, UI_COLOR_TEXT_STRONG, 28, 84, 344);

    lv_obj_t *btn = lv_btn_create(box);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, 112, 178);
    lv_obj_set_size(btn, 176, 44);
    lv_obj_set_style_bg_color(btn, UI_COLOR_RED, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_add_event_cb(btn, on_alarm_ack, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_label = modal_label(btn, "解除警报", 16, lv_color_white(), 0, 11, 176);
    lv_obj_set_style_text_align(btn_label, LV_TEXT_ALIGN_CENTER, 0);
}

static void on_fire_alarm_event(void *user_data)
{
    (void)user_data;
    show_fire_modal();
}

void smart_home_alarm_ui_init(void)
{
    if (s_initialized) {
        return;
    }
    s_initialized = true;
    ui_event_subscribe(UI_EVENT_FIRE_ALARM, on_fire_alarm_event, NULL);
}

void smart_home_alarm_ui_set_fire(uint8_t floor_id, bool active)
{
    s_fire_floor_id = floor_id;
    if (!active) {
        s_fire_active = false;
        s_fire_acknowledged = false;
        ui_event_publish(UI_EVENT_FIRE_ALARM);
        return;
    }

    s_fire_active = true;
    ui_event_publish(UI_EVENT_FIRE_ALARM);
}
