#include "page_ctrl.h"
#include "ui_kit.h"
#include "ui_events.h"
#include "ui_styles.h"
#include "ui_theme.h"
#include "ui_font.h"
#include "mqtt_device_model.h"
#include "mqtt_iot_protocol.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdint.h>
#include <string.h>

static const char *TAG = "PAGE_CTRL";

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

typedef struct {
    lv_obj_t *page;
    lv_obj_t *tab_row;       /* tab button container */
    lv_obj_t *devices_col;   /* device cards grid container (scrollable) */
    int active_floor;        /* 1, 2, or 3 */
    uint32_t last_hash;      /* for change detection */
    bool deleted;
} ctrl_ctx_t;

/* ---- Device name localization ---- */

static const char *device_name_en(const rc_device_t *d)
{
    if (!d) return "";
    if (strcmp(d->id, "floor1_gate") == 0) return "Gate";
    if (strcmp(d->id, "floor1_hall_light") == 0) return "Hall Light";
    if (strcmp(d->id, "floor2_fan") == 0) return "Fan";
    if (strcmp(d->id, "floor2_living_light") == 0) return "Living Light";
    if (strcmp(d->id, "floor2_toilet_light") == 0) return "Toilet Light";
    if (strcmp(d->id, "floor2_hanger") == 0) return "2F Rack";
    if (strcmp(d->id, "floor3_balcony_light") == 0) return "Balcony Light";
    if (strcmp(d->id, "floor3_left_skylight") == 0) return "Left Skylight";
    if (strcmp(d->id, "floor3_right_skylight") == 0) return "Right Skylight";
    if (strcmp(d->id, "floor3_hanger") == 0) return "3F Rack";
    return d->name;
}

static const char *device_name_local(const rc_device_t *d)
{
    if (!d) return "";
    if (ui_i18n_get_lang() == UI_LANG_ZH) return d->name;
    return device_name_en(d);
}

/* ---- Device status helpers ---- */

static const char *device_status_text(const rc_device_t *d)
{
    if (!d || !d->connected) return tr("\xE6\x9C\xAA\xE6\x8E\xA5\xE5\x85\xA5", "Offline");
    if (d->type == RC_DEVICE_DOOR || d->type == RC_DEVICE_WINDOW) {
        /* UTF-8: 已打开=E5B7B2E68993E5BC80, 已关闭=E5B7B2E585B3E997AD */
        return d->power_on ? tr("\xE5\xB7\xB2\xE6\x89\x93\xE5\xBC\x80", "Open")
                           : tr("\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD", "Closed");
    }
    if (d->type == RC_DEVICE_FAN) {
        /* UTF-8: 运行=E8BF90E8A18C, 停止=E5819CE6ADA2 */
        return d->power_on ? tr("\xE8\xBF\x90\xE8\xA1\x8C", "Running")
                           : tr("\xE5\x81\x9C\xE6\xAD\xA2", "Stopped");
    }
    /* LIGHT / RGB_LIGHT */
    /* UTF-8: 已开启=E5B7B2E5BC80E590AF, 已关闭=E5B7B2E585B3E997AD */
    return d->power_on ? tr("\xE5\xB7\xB2\xE5\xBC\x80\xE5\x90\xAF", "On")
                       : tr("\xE5\xB7\xB2\xE5\x85\xB3\xE9\x97\xAD", "Off");
}

static lv_color_t device_status_color(const rc_device_t *d)
{
    if (!d || !d->connected) return UI_COLOR_TEXT_SEC;
    return d->power_on ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC;
}

/* ---- State hash for change detection ---- */

static uint32_t floor_state_hash(int floor)
{
    uint32_t h = 2166136261u;
    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t *d = device_model_at(i);
        if (!d || !d->controllable) continue;
        if (d->floor != (rc_floor_t)floor) continue;
        if (d->type == RC_DEVICE_RGB_LIGHT) continue;
        h ^= ((uint32_t)d->power_on << 8) ^ ((uint32_t)d->connected << 7) ^ (uint32_t)i;
        h *= 16777619u;
    }
    return h;
}

/* ---- Device click handler ---- */

static void on_device_click(lv_event_t *e)
{
    uint16_t idx = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
    const rc_device_t *d = device_model_at(idx);
    if (d && d->controllable && d->connected) {
        device_model_toggle_device(idx);
    }
}

/* ---- Forward declarations ---- */

static void rebuild_tabs(ctrl_ctx_t *ctx);
static void rebuild_devices(ctrl_ctx_t *ctx);

/* ---- Tab click handler ---- */

static void on_tab_click(lv_event_t *e)
{
    ctrl_ctx_t *ctx = (ctrl_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || ctx->deleted) return;
    lv_obj_t *btn = lv_event_get_current_target(e);
    int floor = (int)(intptr_t)lv_obj_get_user_data(btn);
    if (floor < 1 || floor > 3) return;
    if (ctx->active_floor == floor) return;
    ctx->active_floor = floor;
    ctx->last_hash = 0;  /* force rebuild */
    rebuild_tabs(ctx);
    rebuild_devices(ctx);
}

/* ---- Tab builder ---- */

static lv_obj_t *create_tab_btn(lv_obj_t *parent, const char *text, bool active,
                                  int floor, ctrl_ctx_t *ctx)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_height(btn, 36);
    lv_obj_set_width(btn, 0);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_style_bg_color(btn, active ? UI_COLOR_ACCENT : UI_COLOR_INPUT_BG, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 18, 0);
    lv_obj_set_style_pad_hor(btn, 16, 0);
    lv_obj_set_style_pad_ver(btn, 8, 0);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(btn, (void *)(intptr_t)floor);

    ui_create_label(btn, text, ui_font_cn(14),
                    active ? lv_color_white() : UI_COLOR_TEXT_SEC);

    lv_obj_add_event_cb(btn, on_tab_click, LV_EVENT_CLICKED, ctx);
    ui_apply_press_feedback(btn, UI_COLOR_ACCENT);
    return btn;
}

static void rebuild_tabs(ctrl_ctx_t *ctx)
{
    if (!ctx || !ctx->tab_row) return;
    lv_obj_clean(ctx->tab_row);

    /* UTF-8: 一楼=E4B880E6A5BC, 二楼=E4BA8CE6A5BC, 三楼=E4B889E6A5BC */
    const char *names_zh[] = { "\xE4\xB8\x80\xE6\xA5\xBC", "\xE4\xBA\x8C\xE6\xA5\xBC", "\xE4\xB8\x89\xE6\xA5\xBC" };
    const char *names_en[] = { "1F", "2F", "3F" };

    for (int i = 0; i < 3; i++) {
        int floor = i + 1;
        bool active = (ctx->active_floor == floor);
        create_tab_btn(ctx->tab_row, tr(names_zh[i], names_en[i]),
                       active, floor, ctx);
    }
}

/* ---- Device cards builder ---- */

static void rebuild_devices(ctrl_ctx_t *ctx)
{
    if (!ctx || !ctx->devices_col || ctx->deleted) return;
    lv_obj_clean(ctx->devices_col);

    int floor = ctx->active_floor;
    uint16_t count = 0;

    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t *d = device_model_at(i);
        if (!d || !d->controllable) continue;
        if (d->floor != (rc_floor_t)floor) continue;
        /* RGB_LIGHT (master bedroom light) is exclusive to the lighting page */
        if (d->type == RC_DEVICE_RGB_LIGHT) continue;

        const char *icon = device_model_type_icon(d->type);
        const char *name = device_name_local(d);
        const char *status = device_status_text(d);
        lv_color_t color = device_status_color(d);

        lv_obj_t *card = ui_kit_device_card(ctx->devices_col, icon, name, status, color,
                                            d->connected, on_device_click,
                                            (void *)(uintptr_t)i);
        lv_obj_set_width(card, lv_pct(31));
        lv_obj_set_style_min_height(card, 112, 0);
        count++;
    }

    if (count == 0) {
        /* UTF-8: 暂无设备=E69A82E697A0E8AEBEE5A487 */
        ui_kit_group_title(ctx->devices_col, tr("\xE6\x9A\x82\xE6\x97\xA0\xE8\xAE\xBE\xE5\xA4\x87", "No devices"));
    }
}

/* ---- Update ---- */

static void update_ctrl(ctrl_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;
    uint32_t h = floor_state_hash(ctx->active_floor);
    if (h != ctx->last_hash) {
        ctx->last_hash = h;
        rebuild_devices(ctx);
    }
}

static void on_model_updated(void *user_data)
{
    update_ctrl((ctrl_ctx_t *)user_data);
}

/* ---- Lifecycle ---- */

static void on_delete(lv_event_t *e)
{
    ctrl_ctx_t *ctx = (ctrl_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    lv_free(ctx);
}

/* ---- Flex helpers ---- */

static lv_obj_t *make_flex_row(lv_obj_t *parent, lv_flex_align_t main_align)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

/* ---- Create ---- */

lv_obj_t *page_ctrl_create(lv_obj_t *parent)
{
    ctrl_ctx_t *ctx = lv_malloc(sizeof(ctrl_ctx_t));
    memset(ctx, 0, sizeof(*ctx));
    ctx->active_floor = 1;

    ctx->page = ui_create_page(parent, on_delete, ctx);
    /* Fill parent height so devices column can scroll */
    lv_obj_set_height(ctx->page, lv_pct(100));

    /* Page title */
    ui_kit_page_title(ctx->page, ICON_CONTROL,
                      /* UTF-8: 设备控制=E8AEBEE5A484E68EA7E588B6 */
                      tr("\xE8\xAE\xBE\xE5\xA4\x87\xE6\x8E\xA7\xE5\x88\xB6", "Device Control"),
                      /* UTF-8: 点击设备切换开关=E782B9E587BBE8AEBEE5A484E58887E68DA2E5BC80E585B3 */
                      tr("\xE7\x82\xB9\xE5\x87\xBB\xE8\xAE\xBE\xE5\xA4\x87\xE5\x88\x87\xE6\x8D\xA2\xE5\xBC\x80\xE5\x85\xB3", "Tap device to toggle"));

    /* Floor tab row */
    ctx->tab_row = make_flex_row(ctx->page, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_obj_set_style_pad_column(ctx->tab_row, 8, 0);
    rebuild_tabs(ctx);

    /* Devices grid (scrollable, fills remaining space) */
    ctx->devices_col = lv_obj_create(ctx->page);
    lv_obj_remove_style_all(ctx->devices_col);
    lv_obj_set_width(ctx->devices_col, lv_pct(100));
    lv_obj_set_flex_grow(ctx->devices_col, 1);
    lv_obj_set_style_pad_all(ctx->devices_col, 0, 0);
    lv_obj_set_style_pad_row(ctx->devices_col, 8, 0);
    lv_obj_set_style_pad_column(ctx->devices_col, 10, 0);
    lv_obj_set_layout(ctx->devices_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ctx->devices_col, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ctx->devices_col, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(ctx->devices_col, LV_SCROLLBAR_MODE_AUTO);

    /* Initial build + subscribe */
    rebuild_devices(ctx);
    ctx->last_hash = floor_state_hash(ctx->active_floor);
    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);

    ESP_LOGI(TAG, "Device control page created (floor=%d)", ctx->active_floor);
    return ctx->page;
}
