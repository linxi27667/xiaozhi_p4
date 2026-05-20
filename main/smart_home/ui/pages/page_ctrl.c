#include "page_ctrl.h"
#include "ui_events.h"
#include "ui_styles.h"
#include "ui_font.h"
#include "mqtt_device_model.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdint.h>
#include <string.h>

static const char *TAG = "PAGE_CTRL";

typedef struct {
    lv_obj_t *grid;
    uint32_t last_hash;
    bool deleted;
} ctrl_ctx_t;

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

static lv_obj_t *cn_label(lv_obj_t *parent, const char *text, int size, lv_color_t color,
    int x, int y, int w)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text ? text : "");
    lv_obj_set_style_text_font(lbl, ui_font_cn((uint8_t)size), 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_pos(lbl, x, y);
    if (w > 0) {
        lv_obj_set_width(lbl, w);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    }
    return lbl;
}

static void icon_text(lv_obj_t *parent, const char *icon, lv_color_t color,
    int x, int y, int icon_size)
{
    lv_obj_t *lbl = ui_create_icon(parent, icon, ui_font_icon((uint8_t)icon_size), color);
    lv_obj_set_pos(lbl, x, y);
}

static lv_obj_t *panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 16, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, UI_COLOR_BORDER, 0);
    lv_obj_set_style_shadow_width(obj, 5, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_10, 0);
    lv_obj_set_style_shadow_color(obj, UI_COLOR_SHADOW, 0);
    lv_obj_set_style_shadow_offset_y(obj, 2, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_color_t device_accent(const rc_device_t *d)
{
    if (!d) return UI_COLOR_TEXT_STRONG;
    switch (d->type) {
        case RC_DEVICE_LIGHT: return UI_COLOR_ORANGE;
        case RC_DEVICE_FAN: return UI_COLOR_BLUE;
        case RC_DEVICE_DOOR: return UI_COLOR_TEXT_STRONG;
        case RC_DEVICE_WINDOW: return UI_COLOR_BLUE;
        default: return UI_COLOR_TEXT_STRONG;
    }
}

static const char *device_icon(const rc_device_t *d)
{
    if (!d) return ICON_POWER;
    return device_model_type_icon(d->type);
}

static const char *device_kind(const rc_device_t *d)
{
    if (!d) return tr("设备", "Device");
    return device_model_type_name(d->type);
}

static const char *device_name_local(const rc_device_t *d)
{
    if (!d || ui_i18n_get_lang() == UI_LANG_ZH) return d ? d->name : "";
    if (strcmp(d->id, "floor1_gate") == 0) return "Gate";
    if (strcmp(d->id, "floor1_hall_light") == 0) return "Hall Light";
    if (strcmp(d->id, "floor2_fan") == 0) return "Fan";
    if (strcmp(d->id, "floor2_living_light") == 0) return "Living Light";
    if (strcmp(d->id, "floor2_toilet_light") == 0) return "Toilet Light";
    if (strcmp(d->id, "floor2_master_light") == 0) return "Master Light";
    if (strcmp(d->id, "floor2_hanger") == 0) return "2F Rack";
    if (strcmp(d->id, "floor3_balcony_light") == 0) return "Balcony Light";
    if (strcmp(d->id, "floor3_skylight") == 0) return "Skylight";
    if (strcmp(d->id, "floor3_hanger") == 0) return "3F Rack";
    return d->name;
}

static const char *floor_name_local(rc_floor_t floor)
{
    if (ui_i18n_get_lang() == UI_LANG_EN) {
        switch (floor) {
            case RC_FLOOR_1: return "1F";
            case RC_FLOOR_2: return "2F";
            case RC_FLOOR_3: return "3F";
            default: return "Unknown";
        }
    }
    return device_model_floor_name(floor);
}

static void on_device_click(lv_event_t *e)
{
    uint16_t idx = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
    device_model_toggle_device(idx);
}

static uint32_t state_hash(void)
{
    uint32_t h = 2166136261u;
    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t *d = device_model_at(i);
        if (!d || !d->controllable) continue;
        h ^= ((uint32_t)d->floor << 24) ^ ((uint32_t)d->type << 16) ^
             ((uint32_t)d->power_on << 8) ^ ((uint32_t)d->connected << 7) ^ i;
        h *= 16777619u;
    }
    return h;
}

static void floor_title(lv_obj_t *parent, int x, int y, const char *title)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_pos(dot, x, y + 8);
    lv_obj_set_size(dot, 7, 7);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    cn_label(parent, title, 14, UI_COLOR_TEXT_STRONG, x + 16, y, 80);
}

static void device_button(lv_obj_t *parent, int x, int y, const rc_device_t *d, uint16_t idx)
{
    lv_color_t accent = device_accent(d);
    bool on = d->power_on;
    bool conn = d->connected;
    lv_obj_t *btn = panel(parent, x, y, 146, 112);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, on_device_click, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);
    ui_apply_press_feedback(btn, accent);
    if (on && conn) {
        lv_obj_set_style_bg_color(btn, lv_color_mix(accent, UI_COLOR_CARD, 13), 0);
        lv_obj_set_style_border_color(btn, lv_color_mix(accent, UI_COLOR_BORDER, 42), 0);
    }
    if (!conn) {
        lv_obj_set_style_opa(btn, LV_OPA_80, 0);
    }

    icon_text(btn, device_icon(d), (on && conn) ? accent : UI_COLOR_TEXT_STRONG, 14, 14, 22);
    lv_obj_t *state = panel(btn, 108, 12, 26, 24);
    lv_obj_set_style_shadow_width(state, 0, 0);
    lv_obj_set_style_radius(state, 8, 0);
    lv_obj_set_style_bg_color(state, (on && conn) ? UI_COLOR_GREEN_SOFT : UI_COLOR_INPUT_BG, 0);
    lv_obj_set_style_border_color(state, (on && conn) ? UI_COLOR_GREEN : UI_COLOR_BORDER, 0);
    lv_obj_t *state_lbl = cn_label(state,
        conn ? (on ? tr("开", "On") : tr("关", "Off")) : "?",
        12, (on && conn) ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC, 0, 5, 26);
    lv_obj_set_style_text_align(state_lbl, LV_TEXT_ALIGN_CENTER, 0);
    cn_label(btn, device_name_local(d), ui_i18n_get_lang() == UI_LANG_ZH ? 20 : 16,
        UI_COLOR_TEXT_STRONG, 14, 42, 112);
    cn_label(btn, conn ? device_kind(d) : tr("待同步", "Sync"), 12, UI_COLOR_TEXT_SEC, 14, 72, 64);
    cn_label(btn, conn ? (on ? tr("已开启", "On") : tr("已关闭", "Off")) : tr("未接入", "N/A"),
        12, conn ? (on ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC) : UI_COLOR_TEXT_SEC, 14, 90, 70);
}

static void rebuild_controls(ctrl_ctx_t *ctx)
{
    if (!ctx || !ctx->grid) return;
    lv_obj_clean(ctx->grid);

    cn_label(ctx->grid, tr("控制", "Control"), 22, UI_COLOR_TEXT_STRONG, 20, 18, 90);
    cn_label(ctx->grid, tr("选择要控制的设备", "Select a device to control"), 13, UI_COLOR_TEXT_SEC, 92, 25, 190);

    int y = 70;
    for (int floor = RC_FLOOR_1; floor <= RC_FLOOR_3; floor++) {
        int count = 0;
        for (uint16_t i = 0; i < device_model_count(); i++) {
            const rc_device_t *d = device_model_at(i);
            if (d && d->controllable && d->floor == floor) count++;
        }
        if (count == 0) continue;

        floor_title(ctx->grid, 20, y, floor_name_local((rc_floor_t)floor));
        y += 30;

        int col = 0;
        for (uint16_t i = 0; i < device_model_count(); i++) {
            const rc_device_t *d = device_model_at(i);
            if (!d || !d->controllable || d->floor != floor) continue;
            device_button(ctx->grid, 20 + col * 160, y, d, i);
            col++;
        }
        y += 132;
    }
}

static void update_ctrl(ctrl_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;
    uint32_t h = state_hash();
    if (h != ctx->last_hash) {
        ctx->last_hash = h;
        rebuild_controls(ctx);
    }
}

static void on_model_updated(void *user_data)
{
    update_ctrl((ctrl_ctx_t *)user_data);
}

static void page_ctrl_delete(lv_event_t *e)
{
    ctrl_ctx_t *ctx = (ctrl_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    lv_free(ctx);
}

lv_obj_t *page_ctrl_create(lv_obj_t *parent)
{
    ctrl_ctx_t *ctx = lv_malloc(sizeof(ctrl_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(page, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(page, page_ctrl_delete, LV_EVENT_DELETE, ctx);

    ctx->grid = lv_obj_create(page);
    lv_obj_remove_style_all(ctx->grid);
    lv_obj_set_pos(ctx->grid, 0, 0);
    lv_obj_set_size(ctx->grid, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ctx->grid, LV_OBJ_FLAG_SCROLLABLE);

    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    update_ctrl(ctx);

    ESP_LOGI(TAG, "Device control page created");
    return page;
}
