#include "page_set.h"
#include "ui_manager.h"
#include "ui_events.h"
#include "ui_styles.h"
#include "ui_font.h"
#include "mqtt_device_model.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>
#include <time.h>

static const char *TAG = "PAGE_SET";

typedef struct {
    lv_obj_t *network_value;
    lv_obj_t *brightness_slider;
    lv_obj_t *brightness_value;
    lv_obj_t *volume_value;
    lv_obj_t *chip_zh;
    lv_obj_t *chip_en;
    lv_obj_t *modal;
    bool deleted;
} set_ctx_t;

static int s_brightness = 80;
static int s_volume_level = 1;

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

static lv_obj_t *panel(lv_obj_t *parent, int x, int y, int w, int h, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, UI_COLOR_BORDER, 0);
    lv_obj_set_style_shadow_width(obj, 8, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_10, 0);
    lv_obj_set_style_shadow_color(obj, UI_COLOR_SHADOW, 0);
    lv_obj_set_style_shadow_offset_y(obj, 2, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *row_hit(lv_obj_t *parent, int y, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *hit = lv_obj_create(parent);
    lv_obj_remove_style_all(hit);
    lv_obj_set_pos(hit, 12, y);
    lv_obj_set_size(hit, 832, 50);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    if (cb) lv_obj_add_event_cb(hit, cb, LV_EVENT_CLICKED, user_data);
    return hit;
}

static lv_obj_t *setting_row(lv_obj_t *parent, int y, const char *icon, const char *name,
    const char *value, lv_color_t value_color, lv_event_cb_t cb, void *user_data)
{
    icon_text(parent, icon, UI_COLOR_TEXT_SEC, 26, y + 14, 15);
    cn_label(parent, name, 14, UI_COLOR_TEXT_STRONG, 66, y + 12, 130);
    lv_obj_t *val = cn_label(parent, value, 13, value_color, 342, y + 13, 250);
    icon_text(parent, ICON_NEXT, UI_COLOR_TEXT_SEC, 790, y + 15, 12);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 66, y + 48);
    lv_obj_set_size(line, 746, 1);
    lv_obj_set_style_bg_color(line, UI_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);

    if (cb) row_hit(parent, y, cb, user_data);
    return val;
}

static lv_obj_t *chip(lv_obj_t *parent, int x, int y, int w, const char *text,
    bool active, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *c = panel(parent, x, y, w, 30, 8);
    lv_obj_set_style_shadow_width(c, 0, 0);
    lv_obj_set_style_bg_color(c, active ? UI_COLOR_ACCENT : UI_COLOR_INPUT_BG, 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    if (cb) lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *lbl = cn_label(c, text, 12, active ? lv_color_white() : UI_COLOR_TEXT_SEC, 0, 7, w);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    return c;
}

static void apply_chip_state(lv_obj_t *chip_obj, bool active)
{
    if (!chip_obj) return;
    lv_obj_set_style_bg_color(chip_obj, active ? UI_COLOR_ACCENT : UI_COLOR_INPUT_BG, 0);
    lv_obj_t *lbl = lv_obj_get_child(chip_obj, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, active ? lv_color_white() : UI_COLOR_TEXT_SEC, 0);
}

static void refresh_language_chips(set_ctx_t *ctx)
{
    if (!ctx) return;
    bool zh = ui_i18n_get_lang() == UI_LANG_ZH;
    apply_chip_state(ctx->chip_zh, zh);
    apply_chip_state(ctx->chip_en, !zh);
}

static void update_network_value(set_ctx_t *ctx)
{
    if (!ctx || !ctx->network_value) return;
    const mqtt_device_model_t *m = device_model_get();
    char buf[80];
    if (m->wifi_state == WIFI_STATE_CONNECTED && m->wifi_ssid[0]) {
        lv_snprintf(buf, sizeof(buf), "%s  %s", tr("已连接", "Connected"), m->wifi_ssid);
        lv_label_set_text(ctx->network_value, buf);
        lv_obj_set_style_text_color(ctx->network_value, UI_COLOR_GREEN, 0);
    } else if (m->wifi_state == WIFI_STATE_SCANNING) {
        lv_label_set_text(ctx->network_value, tr("扫描中...", "Scanning..."));
        lv_obj_set_style_text_color(ctx->network_value, UI_COLOR_ORANGE, 0);
    } else {
        lv_label_set_text(ctx->network_value, tr("未连接", "Disconnected"));
        lv_obj_set_style_text_color(ctx->network_value, UI_COLOR_RED, 0);
    }
}

static void close_modal(set_ctx_t *ctx)
{
    if (!ctx || !ctx->modal) return;
    lv_obj_delete(ctx->modal);
    ctx->modal = NULL;
}

static void on_modal_close(lv_event_t *e)
{
    close_modal((set_ctx_t *)lv_event_get_user_data(e));
}

static void show_info_modal(set_ctx_t *ctx, const char *title, const char *body)
{
    if (!ctx) return;
    close_modal(ctx);

    ctx->modal = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(ctx->modal);
    lv_obj_set_size(ctx->modal, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(ctx->modal, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(ctx->modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->modal, LV_OPA_30, 0);
    lv_obj_clear_flag(ctx->modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = panel(ctx->modal, 322, 172, 380, 230, 14);
    icon_text(box, ICON_INFO, UI_COLOR_ACCENT, 28, 26, 20);
    cn_label(box, title, 20, UI_COLOR_TEXT_STRONG, 62, 22, 170);
    lv_obj_t *desc = cn_label(box, body, 13, UI_COLOR_TEXT_SEC, 28, 70, 320);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(desc, 86);

    lv_obj_t *btn = lv_btn_create(box);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, 226, 166);
    lv_obj_set_size(btn, 116, 36);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, on_modal_close, LV_EVENT_CLICKED, ctx);
    ui_apply_press_feedback(btn, UI_COLOR_ACCENT);
    lv_obj_t *lbl = cn_label(btn, tr("确定", "OK"), 13, lv_color_white(), 0, 9, 116);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
}

static void on_network_row(lv_event_t *e)
{
    (void)e;
    UI_Manager_Switch_Page(UI_PAGE_NET);
}

static void on_lang_zh(lv_event_t *e)
{
    set_ctx_t *ctx = (set_ctx_t *)lv_event_get_user_data(e);
    ui_i18n_set_lang(UI_LANG_ZH);
    refresh_language_chips(ctx);
    UI_Manager_Rebuild_Current();
}

static void on_lang_en(lv_event_t *e)
{
    set_ctx_t *ctx = (set_ctx_t *)lv_event_get_user_data(e);
    ui_i18n_set_lang(UI_LANG_EN);
    refresh_language_chips(ctx);
    UI_Manager_Rebuild_Current();
}

static void on_brightness_changed(lv_event_t *e)
{
    set_ctx_t *ctx = (set_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->brightness_slider || !ctx->brightness_value) return;
    s_brightness = (int)lv_slider_get_value(ctx->brightness_slider);
    char buf[12];
    lv_snprintf(buf, sizeof(buf), "%d%%", s_brightness);
    lv_label_set_text(ctx->brightness_value, buf);
}

static const char *volume_name(void)
{
    static const char *names_zh[] = { "静音", "中等", "高" };
    static const char *names_en[] = { "Mute", "Medium", "High" };
    return ui_i18n_get_lang() == UI_LANG_ZH ?
        names_zh[s_volume_level % 3] : names_en[s_volume_level % 3];
}

static void on_volume_row(lv_event_t *e)
{
    set_ctx_t *ctx = (set_ctx_t *)lv_event_get_user_data(e);
    s_volume_level = (s_volume_level + 1) % 3;
    if (ctx && ctx->volume_value) lv_label_set_text(ctx->volume_value, volume_name());
}

static void on_about_row(lv_event_t *e)
{
    show_info_modal((set_ctx_t *)lv_event_get_user_data(e), tr("关于设备", "About"),
        tr("智慧家庭控制面板基于 LVGL v9 构建，面向 ESP32-P4 主控与多楼层从机控制。",
           "Smart home control panel built with LVGL v9 for ESP32-P4 and multi-floor slave controllers."));
}

static void on_firmware_row(lv_event_t *e)
{
    show_info_modal((set_ctx_t *)lv_event_get_user_data(e), tr("固件版本", "Firmware"),
        tr("ESP32-P4 LVGL v9 固件，支持 WiFi、MQTT 通信与多楼层从机控制。",
           "ESP32-P4 LVGL v9 firmware with WiFi, MQTT communication and multi-floor slave control."));
}

static void on_model_updated(void *user_data)
{
    set_ctx_t *ctx = (set_ctx_t *)user_data;
    if (!ctx || ctx->deleted) return;
    update_network_value(ctx);
}

static void page_set_delete(lv_event_t *e)
{
    set_ctx_t *ctx = (set_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    close_modal(ctx);
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    lv_free(ctx);
}

lv_obj_t *page_set_create(lv_obj_t *parent)
{
    set_ctx_t *ctx = lv_malloc(sizeof(set_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(page, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(page, page_set_delete, LV_EVENT_DELETE, ctx);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char tm_buf[16];
    lv_snprintf(tm_buf, sizeof(tm_buf), "%02d:%02d", t ? t->tm_hour : 9, t ? t->tm_min : 41);

    cn_label(page, tr("设置", "Settings"), 22, UI_COLOR_TEXT_STRONG, 32, 24, 100);
    cn_label(page, tm_buf, 16, UI_COLOR_TEXT_STRONG, 816, 28, 54);
    icon_text(page, ICON_WIFI, UI_COLOR_TEXT_STRONG, 878, 28, 16);

    lv_obj_t *card = panel(page, 32, 74, 890, 450, 16);
    cn_label(card, tr("系统偏好", "System"), 18, UI_COLOR_TEXT_STRONG, 26, 22, 120);
    cn_label(card, tr("面板显示、语言和设备信息", "Display, language and device information"),
        12, UI_COLOR_TEXT_SEC, 26, 50, 260);

    ctx->network_value = setting_row(card, 88, ICON_WIFI, tr("网络设置", "Network"), tr("未连接", "Disconnected"),
        UI_COLOR_RED, on_network_row, ctx);
    setting_row(card, 146, ICON_GLOBE, tr("语言切换", "Language"), "", UI_COLOR_TEXT_SEC, NULL, NULL);
    ctx->chip_zh = chip(card, 342, 154, 78, "简体中文", true, on_lang_zh, ctx);
    ctx->chip_en = chip(card, 430, 154, 72, "English", false, on_lang_en, ctx);

    icon_text(card, ICON_SUN, UI_COLOR_TEXT_SEC, 26, 218, 15);
    cn_label(card, tr("屏幕亮度", "Brightness"), 14, UI_COLOR_TEXT_STRONG, 66, 216, 130);
    ctx->brightness_slider = lv_slider_create(card);
    lv_obj_set_pos(ctx->brightness_slider, 342, 224);
    lv_obj_set_size(ctx->brightness_slider, 220, 8);
    lv_slider_set_range(ctx->brightness_slider, 10, 100);
    lv_slider_set_value(ctx->brightness_slider, s_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ctx->brightness_slider, lv_color_hex(0xE6ECF5), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ctx->brightness_slider, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ctx->brightness_slider, UI_COLOR_ACCENT, LV_PART_KNOB);
    lv_obj_add_event_cb(ctx->brightness_slider, on_brightness_changed, LV_EVENT_VALUE_CHANGED, ctx);
    char brightness_buf[12];
    lv_snprintf(brightness_buf, sizeof(brightness_buf), "%d%%", s_brightness);
    ctx->brightness_value = cn_label(card, brightness_buf, 13, UI_COLOR_TEXT_SEC, 586, 216, 46);

    ctx->volume_value = setting_row(card, 260, ICON_VOLUME, tr("声音", "Sound"), volume_name(),
        UI_COLOR_TEXT_SEC, on_volume_row, ctx);
    setting_row(card, 318, ICON_INFO, tr("关于设备", "About"), "ESP32-P4 LVGL v9",
        UI_COLOR_TEXT_SEC, on_about_row, ctx);
    setting_row(card, 376, ICON_REFRESH, tr("固件版本", "Firmware"), "v1.0.0",
        UI_COLOR_TEXT_SEC, on_firmware_row, ctx);

    refresh_language_chips(ctx);
    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    update_network_value(ctx);

    ESP_LOGI(TAG, "Settings page created");
    return page;
}
