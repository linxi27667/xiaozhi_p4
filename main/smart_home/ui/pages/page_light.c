#include "page_light.h"

#include "mqtt_device_model.h"
#include "mqtt_iot_protocol.h"
#include "ui_events.h"
#include "ui_font.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "ui_styles.h"
#include "../../services/xiaozhi_mqtt.h"

#include "esp_log.h"
#include "lvgl.h"
#include <stdint.h>
#include <string.h>

static const char *TAG = "PAGE_LIGHT";

typedef struct {
    lv_obj_t *page;
    lv_obj_t *state;
    lv_obj_t *brightness;
    lv_obj_t *sw;
    lv_obj_t *color_preview;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t effect;
    bool deleted;
} light_ctx_t;

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
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    }
    return lbl;
}

static lv_obj_t *icon(lv_obj_t *parent, const char *icon_text, lv_color_t color,
    int x, int y, int size)
{
    lv_obj_t *lbl = ui_create_icon(parent, icon_text, ui_font_icon((uint8_t)size), color);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}

static lv_obj_t *panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, UI_COLOR_BORDER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static const rc_device_t *master_light(void)
{
    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t *d = device_model_at(i);
        if (d && strcmp(d->id, "floor2_master_light") == 0) {
            return d;
        }
    }
    return NULL;
}

static void send_rgb(light_ctx_t *ctx, uint8_t brightness)
{
    if (!ctx) return;
    mqtt_send_rgb_light(2, 0, ctx->red, ctx->green, ctx->blue, brightness, ctx->effect, 16);
}

static void refresh_light(light_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;
    const rc_device_t *d = master_light();
    char buf[64];
    bool online = d && d->connected;
    bool on = d && d->power_on;

    if (d) {
        if (d->red || d->green || d->blue) {
            ctx->red = d->red;
            ctx->green = d->green;
            ctx->blue = d->blue;
        }
        ctx->effect = d->effect;
    }

    if (ctx->state) {
        lv_snprintf(buf, sizeof(buf), "%s / %s", online ? tr("在线", "Online") : tr("离线", "Offline"),
            on ? tr("已开启", "On") : tr("已关闭", "Off"));
        lv_label_set_text(ctx->state, buf);
        lv_obj_set_style_text_color(ctx->state, online ? UI_COLOR_GREEN : UI_COLOR_RED, 0);
    }
    if (ctx->brightness) {
        uint8_t br = d ? d->brightness : 0;
        lv_snprintf(buf, sizeof(buf), "%u%%", (unsigned)br);
        lv_label_set_text(ctx->brightness, buf);
    }
    if (ctx->sw) {
        if (on) lv_obj_add_state(ctx->sw, LV_STATE_CHECKED);
        else lv_obj_remove_state(ctx->sw, LV_STATE_CHECKED);
    }
    if (ctx->color_preview) {
        lv_obj_set_style_bg_color(ctx->color_preview, lv_color_make(ctx->red, ctx->green, ctx->blue), 0);
    }
}

static void on_model_updated(void *user_data)
{
    refresh_light((light_ctx_t *)user_data);
}

static void on_switch(lv_event_t *e)
{
    light_ctx_t *ctx = (light_ctx_t *)lv_event_get_user_data(e);
    bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    send_rgb(ctx, on ? 60 : 0);
}

static void on_brightness(lv_event_t *e)
{
    light_ctx_t *ctx = (light_ctx_t *)lv_event_get_user_data(e);
    uint8_t br = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    send_rgb(ctx, br);
}

static void on_color(lv_event_t *e)
{
    light_ctx_t *ctx = (light_ctx_t *)lv_obj_get_user_data(lv_event_get_target(e));
    uint32_t color = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    ctx->red = (color >> 16) & 0xFF;
    ctx->green = (color >> 8) & 0xFF;
    ctx->blue = color & 0xFF;
    send_rgb(ctx, 60);
}

static void on_effect(lv_event_t *e)
{
    light_ctx_t *ctx = (light_ctx_t *)lv_event_get_user_data(e);
    ctx->effect = (uint8_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    send_rgb(ctx, 60);
}

static lv_obj_t *color_button(light_ctx_t *ctx, lv_obj_t *parent, int x, uint32_t color)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, 90);
    lv_obj_set_size(btn, 48, 48);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_BORDER, 0);
    lv_obj_set_user_data(btn, ctx);
    lv_obj_add_event_cb(btn, on_color, LV_EVENT_CLICKED, (void *)(uintptr_t)color);
    ui_apply_press_feedback(btn, lv_color_hex(color));
    return btn;
}

static lv_obj_t *effect_button(light_ctx_t *ctx, lv_obj_t *parent, int x, const char *name, uint8_t effect)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, 178);
    lv_obj_set_size(btn, 116, 40);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_bg_color(btn, UI_COLOR_INPUT_BG, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_BORDER, 0);
    lv_obj_set_user_data(btn, (void *)(uintptr_t)effect);
    lv_obj_add_event_cb(btn, on_effect, LV_EVENT_CLICKED, ctx);
    ui_apply_press_feedback(btn, UI_COLOR_ACCENT);
    lv_obj_t *lbl = label(btn, name, 13, UI_COLOR_TEXT, 0, 11, 116);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    return btn;
}

static void page_delete(lv_event_t *e)
{
    light_ctx_t *ctx = (light_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    lv_free(ctx);
}

lv_obj_t *page_light_create(lv_obj_t *parent)
{
    light_ctx_t *ctx = lv_malloc(sizeof(light_ctx_t));
    memset(ctx, 0, sizeof(*ctx));
    ctx->red = 255;
    ctx->green = 160;
    ctx->blue = 80;

    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(page, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(page, page_delete, LV_EVENT_DELETE, ctx);
    ctx->page = page;

    label(page, tr("灯光", "Lighting"), 24, UI_COLOR_TEXT_STRONG, 32, 24, 120);
    label(page, tr("二楼主卧 WS2812B 氛围灯", "2F master WS2812B ambient light"), 13,
        UI_COLOR_TEXT_SEC, 32, 58, 280);

    lv_obj_t *main = panel(page, 32, 100, 860, 360);
    icon(main, ICON_STAR, UI_COLOR_ORANGE, 28, 26, 26);
    label(main, tr("主卧灯", "Master Light"), 22, UI_COLOR_TEXT_STRONG, 68, 24, 160);
    ctx->state = label(main, tr("离线 / 已关闭", "Offline / Off"), 13, UI_COLOR_TEXT_SEC, 70, 58, 180);

    ctx->sw = lv_switch_create(main);
    lv_obj_set_pos(ctx->sw, 760, 28);
    lv_obj_set_size(ctx->sw, 64, 34);
    lv_obj_add_event_cb(ctx->sw, on_switch, LV_EVENT_VALUE_CHANGED, ctx);

    ctx->color_preview = lv_obj_create(main);
    lv_obj_remove_style_all(ctx->color_preview);
    lv_obj_set_pos(ctx->color_preview, 28, 86);
    lv_obj_set_size(ctx->color_preview, 120, 120);
    lv_obj_set_style_radius(ctx->color_preview, 12, 0);
    lv_obj_set_style_bg_color(ctx->color_preview, lv_color_make(ctx->red, ctx->green, ctx->blue), 0);
    lv_obj_set_style_bg_opa(ctx->color_preview, LV_OPA_COVER, 0);

    label(main, tr("亮度", "Brightness"), 13, UI_COLOR_TEXT_SEC, 184, 92, 80);
    lv_obj_t *slider = lv_slider_create(main);
    lv_obj_set_pos(slider, 184, 122);
    lv_obj_set_size(slider, 420, 10);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 60, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_RELEASED, ctx);
    ctx->brightness = label(main, "60%", 14, UI_COLOR_TEXT_STRONG, 624, 115, 60);

    label(main, tr("颜色预设", "Colors"), 13, UI_COLOR_TEXT_SEC, 184, 154, 90);
    color_button(ctx, main, 184, 0xFFA052);
    color_button(ctx, main, 250, 0xFFFFFF);
    color_button(ctx, main, 316, 0x3B82F6);
    color_button(ctx, main, 382, 0x22C55E);
    color_button(ctx, main, 448, 0xEF4444);
    color_button(ctx, main, 514, 0x6554FF);

    label(main, tr("效果", "Effects"), 13, UI_COLOR_TEXT_SEC, 184, 236, 80);
    effect_button(ctx, main, 184, tr("静态", "Static"), IOT_LIGHT_EFFECT_STATIC);
    effect_button(ctx, main, 314, tr("呼吸", "Breathe"), IOT_LIGHT_EFFECT_BREATHE);
    effect_button(ctx, main, 444, tr("彩虹", "Rainbow"), IOT_LIGHT_EFFECT_RAINBOW);
    effect_button(ctx, main, 574, tr("警示", "Warning"), IOT_LIGHT_EFFECT_WARNING);

    label(page, tr("普通开关仍兼容“打开主卧灯/关闭主卧灯”语音命令。", "Plain on/off voice commands remain compatible."),
        12, UI_COLOR_TEXT_SEC, 36, 484, 420);

    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    refresh_light(ctx);

    ESP_LOGI(TAG, "Light page created");
    return page;
}
