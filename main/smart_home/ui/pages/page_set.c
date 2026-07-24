#include "page_set.h"
#include "ui_kit.h"
#include "ui_events.h"
#include "ui_styles.h"
#include "ui_theme.h"
#include "ui_font.h"
#include "mqtt_device_model.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "ui_manager.h"
#include "ui_device_settings.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>

static const char *TAG = "PAGE_SET";

/* ===== UTF-8 hex escapes for Chinese strings ===== */

/* Page title */
#define STR_TITLE_ZH        "\xE8\xAE\xBE\xE7\xBD\xAE"                          /* 设置 */
#define STR_TITLE_EN        "Settings"

/* Group titles */
#define STR_GENERAL_ZH      "\xE9\x80\x9A\xE7\x94\xA8"                          /* 通用 */
#define STR_GENERAL_EN      "General"
#define STR_ABOUT_ZH        "\xE5\x85\xB3\xE4\xBA\x8E"                          /* 关于 */
#define STR_ABOUT_EN        "About"

/* Setting row labels */
#define STR_NETWORK_ZH      "\xE7\xBD\x91\xE7\xBB\x9C"                          /* 网络 */
#define STR_NETWORK_EN      "Network"
#define STR_LANGUAGE_ZH     "\xE8\xAF\xAD\xE8\xA8\x80"                          /* 语言 */
#define STR_LANGUAGE_EN     "Language"
#define STR_BRIGHTNESS_ZH   "\xE4\xBA\xAE\xE5\xBA\xA6"                          /* 亮度 */
#define STR_BRIGHTNESS_EN   "Brightness"
#define STR_VOLUME_ZH       "\xE5\xA3\xB0\xE9\x9F\xB3"                          /* 声音 */
#define STR_VOLUME_EN       "Volume"
#define STR_FIRMWARE_ZH     "\xE5\x9B\xBA\xE4\xBB\xB6\xE7\x89\x88\xE6\x9C\xAC"  /* 固件版本 */
#define STR_FIRMWARE_EN     "Firmware"
#define STR_WEATHER_LOC_ZH  "\xE5\xA4\xA9\xE6\xB0\x94\xE4\xBD\x8D\xE7\xBD\xAE"  /* 天气位置 */
#define STR_WEATHER_LOC_EN  "Weather Location"
#define STR_DEVICE_ID_ZH    "\xE8\xAE\xBE\xE5\xA4\x87 ID"                       /* 设备 ID */
#define STR_DEVICE_ID_EN    "Device ID"

/* Language names */
#define STR_LANG_ZH_NAME    "\xE4\xB8\xAD\xE6\x96\x87"                          /* 中文 */
#define STR_LANG_EN_NAME    "English"

/* Weather location */
#define STR_NANJING_ZH      "\xE5\x8D\x97\xE4\xBA\xAC"                          /* 南京 */
#define STR_NANJING_EN      "Nanjing"

#define MQTT_BROKER_STR     "8.134.167.240:1883"
#define MQTT_CLIENT_ID      "xiaozhi_p4_host"
#define FIRMWARE_VERSION    "v1.0.0"

/* Cached hardware values for the current page instance. */
static int s_brightness = 80;
static int s_volume = 70;

typedef struct {
    lv_obj_t *page;
    lv_obj_t *brightness_val;
    lv_obj_t *volume_val;
    bool deleted;
} set_ctx_t;

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

/* ---- Click handlers ---- */

static void on_network_row(lv_event_t *e)
{
    (void)e;
    UI_Manager_Switch_Page(UI_PAGE_NET);
}

static void on_language_row(lv_event_t *e)
{
    (void)e;
    /* Toggle language; ui_manager subscribes to UI_EVENT_LANG_CHANGED
     * and will rebuild the current page automatically. */
    ui_lang_t cur = ui_i18n_get_lang();
    ui_i18n_set_lang(cur == UI_LANG_ZH ? UI_LANG_EN : UI_LANG_ZH);
}

static void on_brightness_changed(lv_event_t *e)
{
    set_ctx_t *ctx = (set_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || ctx->deleted || !ctx->brightness_val) return;
    lv_obj_t *slider = lv_event_get_target(e);
    s_brightness = (int)lv_slider_get_value(slider);
    char buf[12];
    lv_snprintf(buf, sizeof(buf), "%d%%", s_brightness);
    lv_label_set_text(ctx->brightness_val, buf);
    ui_device_settings_set_brightness(s_brightness, false);
}

static void on_brightness_released(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    s_brightness = (int)lv_slider_get_value(slider);
    ui_device_settings_set_brightness(s_brightness, true);
}

static void on_volume_changed(lv_event_t *e)
{
    set_ctx_t *ctx = (set_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || ctx->deleted || !ctx->volume_val) return;
    lv_obj_t *slider = lv_event_get_target(e);
    s_volume = (int)lv_slider_get_value(slider);
    char buf[12];
    lv_snprintf(buf, sizeof(buf), "%d%%", s_volume);
    lv_label_set_text(ctx->volume_val, buf);
}

static void on_volume_released(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    s_volume = (int)lv_slider_get_value(slider);
    ui_device_settings_set_volume(s_volume);
}

/* ---- Delete callback ---- */
static void on_delete(lv_event_t *e)
{
    set_ctx_t *ctx = (set_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    lv_free(ctx);
}

/* ---- Create a styled slider for setting rows ---- */
static lv_obj_t *make_slider(lv_obj_t *parent, int min_value, int value,
    lv_event_cb_t changed_cb, lv_event_cb_t released_cb, void *user_data)
{
    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_width(slider, 140);
    lv_slider_set_range(slider, min_value, 100);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, UI_COLOR_INPUT_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, UI_COLOR_ACCENT, LV_PART_KNOB);
    lv_obj_clear_flag(slider, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    if (changed_cb) lv_obj_add_event_cb(slider, changed_cb, LV_EVENT_VALUE_CHANGED, user_data);
    if (released_cb) lv_obj_add_event_cb(slider, released_cb, LV_EVENT_RELEASED, user_data);
    return slider;
}

/* ---- Create a slider + value label container as right_widget ---- */
static lv_obj_t *make_slider_group(lv_obj_t *parent, int min_value, int value,
    lv_obj_t **val_label_out, lv_event_cb_t changed_cb,
    lv_event_cb_t released_cb, void *user_data)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_width(cont, LV_SIZE_CONTENT);
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_pad_column(cont, 8, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    make_slider(cont, min_value, value, changed_cb, released_cb, user_data);

    char buf[12];
    lv_snprintf(buf, sizeof(buf), "%d%%", value);
    lv_obj_t *val = ui_create_label(cont, buf, ui_font_cn(13), UI_COLOR_TEXT_SEC);
    lv_obj_set_width(val, 40);
    if (val_label_out) *val_label_out = val;

    return cont;
}

/* ---- Page create ---- */
lv_obj_t *page_set_create(lv_obj_t *parent)
{
    s_brightness = ui_device_settings_get_brightness();
    s_volume = ui_device_settings_get_volume();

    set_ctx_t *ctx = lv_malloc(sizeof(set_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    ctx->page = ui_create_page(parent, on_delete, ctx);

    /* Title */
    ui_kit_page_title(ctx->page, ICON_SETTINGS,
        tr(STR_TITLE_ZH, STR_TITLE_EN), NULL);

    /* ===== General group ===== */
    ui_kit_group_title(ctx->page, tr(STR_GENERAL_ZH, STR_GENERAL_EN));

    /* Network row: right arrow, clickable -> switch to network page */
    lv_obj_t *net_arrow = ui_create_label(ctx->page, ICON_NEXT,
        ui_font_icon(14), UI_COLOR_TEXT_SEC);
    lv_obj_t *net_row = ui_kit_setting_row(ctx->page, ICON_WIFI,
        tr(STR_NETWORK_ZH, STR_NETWORK_EN), net_arrow);
    lv_obj_add_flag(net_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(net_row, on_network_row, LV_EVENT_CLICKED, NULL);
    ui_apply_press_feedback(net_row, UI_COLOR_ACCENT);

    /* Language row: current language label, clickable -> toggle language */
    lv_obj_t *lang_val = ui_create_label(ctx->page,
        ui_i18n_get_lang() == UI_LANG_ZH ? STR_LANG_ZH_NAME : STR_LANG_EN_NAME,
        ui_font_cn(13), UI_COLOR_TEXT_SEC);
    lv_obj_t *lang_row = ui_kit_setting_row(ctx->page, ICON_LANGUAGE,
        tr(STR_LANGUAGE_ZH, STR_LANGUAGE_EN), lang_val);
    lv_obj_add_flag(lang_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lang_row, on_language_row, LV_EVENT_CLICKED, NULL);
    ui_apply_press_feedback(lang_row, UI_COLOR_ACCENT);

    /* Brightness previews while dragging and persists once released. */
    lv_obj_t *bright_right = make_slider_group(ctx->page, 10, s_brightness,
        &ctx->brightness_val, on_brightness_changed, on_brightness_released, ctx);
    ui_kit_setting_row(ctx->page, ICON_LIGHTBULB,
        tr(STR_BRIGHTNESS_ZH, STR_BRIGHTNESS_EN), bright_right);

    /* Volume is applied and persisted once released. */
    lv_obj_t *vol_right = make_slider_group(ctx->page, 0, s_volume,
        &ctx->volume_val, on_volume_changed, on_volume_released, ctx);
    ui_kit_setting_row(ctx->page, ICON_VOLUME,
        tr(STR_VOLUME_ZH, STR_VOLUME_EN), vol_right);

    /* ===== About group ===== */
    ui_kit_group_title(ctx->page, tr(STR_ABOUT_ZH, STR_ABOUT_EN));

    /* Firmware version */
    lv_obj_t *fw_val = ui_create_label(ctx->page, FIRMWARE_VERSION,
        ui_font_cn(13), UI_COLOR_TEXT_SEC);
    ui_kit_setting_row(ctx->page, ICON_INFO,
        tr(STR_FIRMWARE_ZH, STR_FIRMWARE_EN), fw_val);

    /* MQTT Broker */
    lv_obj_t *broker_val = ui_create_label(ctx->page, MQTT_BROKER_STR,
        ui_font_cn(13), UI_COLOR_TEXT_SEC);
    ui_kit_setting_row(ctx->page, ICON_SERVER,
        "MQTT Broker", broker_val);

    /* Weather location */
    lv_obj_t *loc_val = ui_create_label(ctx->page,
        tr(STR_NANJING_ZH, STR_NANJING_EN),
        ui_font_cn(13), UI_COLOR_TEXT_SEC);
    ui_kit_setting_row(ctx->page, ICON_DROP,
        tr(STR_WEATHER_LOC_ZH, STR_WEATHER_LOC_EN), loc_val);

    /* Device ID */
    lv_obj_t *id_val = ui_create_label(ctx->page, MQTT_CLIENT_ID,
        ui_font_cn(13), UI_COLOR_TEXT_SEC);
    ui_kit_setting_row(ctx->page, ICON_INFO,
        tr(STR_DEVICE_ID_ZH, STR_DEVICE_ID_EN), id_val);

    ESP_LOGI(TAG, "Settings page created");
    return ctx->page;
}
