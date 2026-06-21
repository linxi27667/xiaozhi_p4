#include "page_light.h"

#include "mqtt_device_model.h"
#include "mqtt_iot_protocol.h"
#include "ui_asset_service.h"
#include "ui_events.h"
#include "ui_font.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "ui_kit.h"
#include "ui_styles.h"
#include "ui_theme.h"
#include "../../services/xiaozhi_mqtt.h"

#include "esp_log.h"
#include "lvgl.h"
#include <stdint.h>
#include <string.h>

static const char *TAG = "PAGE_LIGHT";

/* ===== UTF-8 hex escapes for Chinese strings ===== */

/* \xE4\xBA\x8C\xE6\xA5\xBC = 二楼 */
/* \xE4\xB8\xBB\xE5\x8D\xA7 = 主卧 */
/* \xE7\x81\xAF\xE5\xB8\xA6 = 灯带 */
#define STR_TITLE_ZH      "\xE4\xBA\x8C\xE6\xA5\xBC\xE4\xB8\xBB\xE5\x8D\xA7\xE7\x81\xAF\xE5\xB8\xA6"
#define STR_TITLE_EN      "Master Bedroom Strip"

/* \xE5\x9C\xA8\xE7\xBA\xBF = 在线 */
#define STR_ONLINE_ZH     "\xE5\x9C\xA8\xE7\xBA\xBF"
#define STR_ONLINE_EN     "Online"

/* \xE7\xA6\xBB\xE7\xBA\xBF = 离线 */
#define STR_OFFLINE_ZH    "\xE7\xA6\xBB\xE7\xBA\xBF"
#define STR_OFFLINE_EN    "Offline"

/* \xE4\xBA\xAE\xE5\xBA\xA6 = 亮度 */
#define STR_BRIGHTNESS_ZH "\xE4\xBA\xAE\xE5\xBA\xA6"
#define STR_BRIGHTNESS_EN "Brightness"

/* \xE6\x95\x88\xE6\x9E\x9C = 效果 */
#define STR_EFFECT_ZH     "\xE6\x95\x88\xE6\x9E\x9C"
#define STR_EFFECT_EN     "Effect"

/* \xE9\xA2\x9C\xE8\x89\xB2\xE9\xA2\x84\xE8\xAE\xBE = 颜色预设 */
#define STR_PRESET_ZH     "\xE9\xA2\x9C\xE8\x89\xB2\xE9\xA2\x84\xE8\xAE\xBE"
#define STR_PRESET_EN     "Presets"

/* \xE9\x9D\x99\xE6\x80\x81 = 静态 */
#define STR_STATIC_ZH     "\xE9\x9D\x99\xE6\x80\x81"
#define STR_STATIC_EN     "Static"

/* \xE5\x91\xBC\xE5\x90\xB8 = 呼吸 */
#define STR_BREATHE_ZH    "\xE5\x91\xBC\xE5\x90\xB8"
#define STR_BREATHE_EN    "Breathe"

/* \xE5\xBD\xA9\xE8\x99\xB9 = 彩虹 */
#define STR_RAINBOW_ZH    "\xE5\xBD\xA9\xE8\x99\xB9"
#define STR_RAINBOW_EN    "Rainbow"

/* \xE8\xAD\xA6\xE7\xA4\xBA = 警示 */
#define STR_WARNING_ZH    "\xE8\xAD\xA6\xE7\xA4\xBA"
#define STR_WARNING_EN    "Warning"

/* \xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89 = 自定义 */
#define STR_CUSTOM_ZH     "\xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89"
#define STR_CUSTOM_EN     "Custom"

#define NUM_EFFECTS   4
#define NUM_PRESETS   6  /* 5 color presets + 1 custom */

typedef struct {
    lv_obj_t *page;
    lv_obj_t *arc;             /* color wheel arc */
    lv_obj_t *preview;         /* center color preview circle */
    lv_obj_t *status_row;      /* container for status pill (rebuilt on update) */
    lv_obj_t *brightness_slider;
    lv_obj_t *brightness_label;
    lv_obj_t *effect_chips[NUM_EFFECTS];
    lv_obj_t *preset_blocks[NUM_PRESETS];

    /* Current state */
    uint8_t red, green, blue;
    uint8_t brightness;
    uint8_t effect;
    uint8_t active_preset;     /* 0-4 for color presets, 5 for custom */

    /* Device info */
    uint8_t dev_floor_id;
    uint8_t dev_gpio_index;
    bool dev_found;
    bool dev_connected;

    bool updating;   /* suppress callbacks during programmatic UI update */
    bool deleted;
} light_ctx_t;

/* Color presets: warm white, sunset, natural, cool white, night light */
typedef struct {
    uint8_t r, g, b;
} rgb_preset_t;

static const rgb_preset_t s_presets[5] = {
    { 255, 180, 100 },  /* 暖白 Warm white */
    { 255, 120, 60  },  /* 日落 Sunset */
    { 255, 230, 200 },  /* 自然 Natural */
    { 200, 230, 255 },  /* 冷白 Cool white */
    { 255, 80,  30  },  /* 夜灯 Night light */
};

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

/* ===== HSV -> RGB conversion (integer only) ===== */
/* h: 0-359, s: 0-100, v: 0-100 -> r,g,b: 0-255 */
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v,
                        uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) {
        uint8_t val = (uint8_t)((uint32_t)v * 255 / 100);
        *r = *g = *b = val;
        return;
    }

    uint32_t region = h / 60;             /* 0-5 */
    uint32_t rem = (h % 60) * 255 / 60;   /* 0-255, fractional part */

    /* Compute p, q, t in 0-100 scale (same as v) */
    uint32_t p100 = (uint32_t)v * (100 - s) / 100;
    uint32_t q100 = (uint32_t)v * (25500 - (uint32_t)s * rem) / 25500;
    uint32_t t100 = (uint32_t)v * (25500 - (uint32_t)s * (255 - rem)) / 25500;

    /* Convert to 0-255 scale */
    uint32_t v8 = (uint32_t)v * 255 / 100;
    uint32_t p8 = p100 * 255 / 100;
    uint32_t q8 = q100 * 255 / 100;
    uint32_t t8 = t100 * 255 / 100;

    switch (region) {
        case 0:  *r = (uint8_t)v8; *g = (uint8_t)t8; *b = (uint8_t)p8; break;
        case 1:  *r = (uint8_t)q8; *g = (uint8_t)v8; *b = (uint8_t)p8; break;
        case 2:  *r = (uint8_t)p8; *g = (uint8_t)v8; *b = (uint8_t)t8; break;
        case 3:  *r = (uint8_t)p8; *g = (uint8_t)q8; *b = (uint8_t)v8; break;
        case 4:  *r = (uint8_t)t8; *g = (uint8_t)p8; *b = (uint8_t)v8; break;
        default: *r = (uint8_t)v8; *g = (uint8_t)p8; *b = (uint8_t)q8; break;
    }
}

/* ===== RGB -> Hue conversion (integer only) ===== */
/* Returns hue 0-359 */
static uint16_t rgb_to_hue(uint8_t r, uint8_t g, uint8_t b)
{
    int32_t max = r; if (g > max) max = g; if (b > max) max = b;
    int32_t min = r; if (g < min) min = g; if (b < min) min = b;
    int32_t delta = max - min;

    if (delta == 0) return 0;

    int32_t h;
    if (max == r) {
        h = (g - b) * 60 / delta;
    } else if (max == g) {
        h = 120 + (b - r) * 60 / delta;
    } else {
        h = 240 + (r - g) * 60 / delta;
    }

    if (h < 0) h += 360;
    return (uint16_t)h;
}

/* ===== Find master RGB light device ===== */
static const rc_device_t *find_master_light(uint8_t *floor_id, uint8_t *gpio_index)
{
    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t *d = device_model_at(i);
        if (d && d->type == RC_DEVICE_RGB_LIGHT) {
            if (floor_id) *floor_id = d->floor_id;
            if (gpio_index) *gpio_index = d->gpio_index;
            return d;
        }
    }
    return NULL;
}

/* ===== Send current state via MQTT ===== */
static void send_current(light_ctx_t *ctx)
{
    if (!ctx || !ctx->dev_found) return;
    mqtt_send_rgb_light(ctx->dev_floor_id, ctx->dev_gpio_index,
                        ctx->red, ctx->green, ctx->blue,
                        ctx->brightness, ctx->effect, 50);
}

/* ===== Flex helpers ===== */
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

static lv_obj_t *make_flex_col(lv_obj_t *parent, lv_flex_align_t main_align)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, lv_pct(100));
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, main_align, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    return col;
}

/* ===== UI state helpers ===== */
static void chip_set_active(lv_obj_t *chip, bool active)
{
    if (!chip) return;
    lv_obj_set_style_bg_color(chip, active ? UI_COLOR_ACCENT : UI_COLOR_INPUT_BG, 0);
    lv_obj_t *lbl = lv_obj_get_child(chip, 0);
    if (lbl) {
        lv_obj_set_style_text_color(lbl, active ? lv_color_white() : UI_COLOR_TEXT_SEC, 0);
    }
}

static void preset_set_active(lv_obj_t *block, bool active)
{
    if (!block) return;
    lv_obj_set_style_border_width(block, active ? 3 : 1, 0);
    lv_obj_set_style_border_color(block, active ? UI_COLOR_ACCENT : UI_COLOR_BORDER, 0);
}

static void update_color_display(light_ctx_t *ctx)
{
    if (!ctx) return;
    lv_color_t color = lv_color_make(ctx->red, ctx->green, ctx->blue);
    if (ctx->preview) {
        lv_obj_set_style_bg_color(ctx->preview, color, 0);
    }
    if (ctx->arc) {
        lv_obj_set_style_bg_color(ctx->arc, color, LV_PART_KNOB);
        lv_obj_set_style_arc_color(ctx->arc, color, LV_PART_INDICATOR);
    }
}

static uint8_t find_matching_preset(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < 5; i++) {
        if (s_presets[i].r == r && s_presets[i].g == g && s_presets[i].b == b) {
            return (uint8_t)i;
        }
    }
    return 5;  /* custom */
}

static void update_preset_active(light_ctx_t *ctx)
{
    uint8_t matched = find_matching_preset(ctx->red, ctx->green, ctx->blue);
    ctx->active_preset = matched;
    for (int i = 0; i < NUM_PRESETS; i++) {
        preset_set_active(ctx->preset_blocks[i], i == matched);
    }
}

/* ===== Event handlers ===== */
static void on_arc_changed(lv_event_t *e)
{
    light_ctx_t *ctx = (light_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || ctx->updating || ctx->deleted) return;

    int16_t angle = lv_arc_get_value(ctx->arc);
    if (angle < 0) angle = 0;
    if (angle > 360) angle = 360;

    uint8_t r, g, b;
    hsv_to_rgb((uint16_t)angle, 100, 100, &r, &g, &b);
    ctx->red = r;
    ctx->green = g;
    ctx->blue = b;

    update_color_display(ctx);
    update_preset_active(ctx);
    send_current(ctx);
}

static void on_brightness_changed(lv_event_t *e)
{
    light_ctx_t *ctx = (light_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || ctx->updating || ctx->deleted) return;

    ctx->brightness = (uint8_t)lv_slider_get_value(ctx->brightness_slider);

    if (ctx->brightness_label) {
        char buf[16];
        lv_snprintf(buf, sizeof(buf), "%u%%", (unsigned)ctx->brightness);
        lv_label_set_text(ctx->brightness_label, buf);
    }

    send_current(ctx);
}

static void on_effect_click(lv_event_t *e)
{
    light_ctx_t *ctx = (light_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || ctx->updating || ctx->deleted) return;

    uint8_t new_effect = (uint8_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));
    ctx->effect = new_effect;

    for (int i = 0; i < NUM_EFFECTS; i++) {
        chip_set_active(ctx->effect_chips[i], i == new_effect);
    }

    send_current(ctx);
}

static void on_preset_click(lv_event_t *e)
{
    light_ctx_t *ctx = (light_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || ctx->updating || ctx->deleted) return;

    uint8_t idx = (uint8_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));

    if (idx < 5) {
        /* Color preset: set RGB */
        ctx->red = s_presets[idx].r;
        ctx->green = s_presets[idx].g;
        ctx->blue = s_presets[idx].b;

        /* Update arc position to match new color */
        uint16_t hue = rgb_to_hue(ctx->red, ctx->green, ctx->blue);
        ctx->updating = true;
        lv_arc_set_value(ctx->arc, (int16_t)hue);
        ctx->updating = false;

        update_color_display(ctx);
        update_preset_active(ctx);
        send_current(ctx);
    } else {
        /* Custom: just mark active, no color change */
        ctx->active_preset = 5;
        for (int i = 0; i < NUM_PRESETS; i++) {
            preset_set_active(ctx->preset_blocks[i], i == 5);
        }
    }
}

/* ===== Refresh from model ===== */
static void refresh_light(light_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;

    uint8_t floor_id = 2, gpio_index = 0;
    const rc_device_t *d = find_master_light(&floor_id, &gpio_index);

    ctx->dev_floor_id = floor_id;
    ctx->dev_gpio_index = gpio_index;
    ctx->dev_found = (d != NULL);
    ctx->dev_connected = d ? d->connected : false;

    /* Read state from device */
    if (d) {
        ctx->red = d->red;
        ctx->green = d->green;
        ctx->blue = d->blue;
        ctx->brightness = d->brightness;
        ctx->effect = d->effect;
    }

    /* Suppress callbacks during UI update */
    ctx->updating = true;

    /* Rebuild status pill */
    if (ctx->status_row) {
        lv_obj_clean(ctx->status_row);
        if (ctx->dev_connected) {
            ui_kit_status_pill(ctx->status_row,
                tr(STR_ONLINE_ZH, STR_ONLINE_EN),
                UI_COLOR_GREEN, UI_COLOR_GREEN_SOFT);
        } else {
            ui_kit_status_pill(ctx->status_row,
                tr(STR_OFFLINE_ZH, STR_OFFLINE_EN),
                UI_COLOR_TEXT_SEC, UI_COLOR_INPUT_BG);
        }
    }

    /* Update arc position from current RGB */
    if (ctx->arc) {
        uint16_t hue = rgb_to_hue(ctx->red, ctx->green, ctx->blue);
        lv_arc_set_value(ctx->arc, (int16_t)hue);
    }

    /* Update color display (preview + knob) */
    update_color_display(ctx);

    /* Update brightness slider + label */
    if (ctx->brightness_slider) {
        lv_slider_set_value(ctx->brightness_slider, ctx->brightness, LV_ANIM_OFF);
    }
    if (ctx->brightness_label) {
        char buf[16];
        lv_snprintf(buf, sizeof(buf), "%u%%", (unsigned)ctx->brightness);
        lv_label_set_text(ctx->brightness_label, buf);
    }

    /* Update effect chips */
    for (int i = 0; i < NUM_EFFECTS; i++) {
        chip_set_active(ctx->effect_chips[i], i == ctx->effect);
    }

    /* Update preset blocks */
    update_preset_active(ctx);

    ctx->updating = false;
}

static void on_model_updated(void *user_data)
{
    refresh_light((light_ctx_t *)user_data);
}

/* ===== Lifecycle ===== */
static void on_delete(lv_event_t *e)
{
    light_ctx_t *ctx = (light_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    lv_free(ctx);
}

/* ===== Preset block builder ===== */
static lv_obj_t *create_preset_block(lv_obj_t *parent, uint8_t idx,
                                       const rgb_preset_t *color,
                                       light_ctx_t *ctx)
{
    lv_obj_t *block = lv_obj_create(parent);
    lv_obj_remove_style_all(block);
    lv_obj_set_size(block, 40, 40);
    lv_obj_set_style_radius(block, 8, 0);
    lv_obj_set_style_border_width(block, 1, 0);
    lv_obj_set_style_border_color(block, UI_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(block, LV_OPA_COVER, 0);
    lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(block, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(block, (void *)(uintptr_t)idx);
    lv_obj_add_event_cb(block, on_preset_click, LV_EVENT_CLICKED, ctx);
    ui_apply_press_feedback(block, UI_COLOR_ACCENT);

    if (idx < 5 && color) {
        lv_obj_set_style_bg_color(block, lv_color_make(color->r, color->g, color->b), 0);
    } else {
        /* Custom: rainbow gradient to indicate "any color" */
        lv_obj_set_style_bg_color(block, lv_color_hex(0xFF3030), 0);
        lv_obj_set_style_bg_grad_color(block, lv_color_hex(0x3030FF), 0);
        lv_obj_set_style_bg_grad_dir(block, LV_GRAD_DIR_HOR, 0);
    }

    return block;
}

/* ===== Create ===== */
lv_obj_t *page_light_create(lv_obj_t *parent)
{
    light_ctx_t *ctx = lv_malloc(sizeof(light_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    /* Defaults (will be overwritten by refresh_light if device exists) */
    ctx->red = 255;
    ctx->green = 180;
    ctx->blue = 100;
    ctx->brightness = 60;
    ctx->effect = IOT_LIGHT_EFFECT_STATIC;
    ctx->active_preset = 0;

    ctx->page = ui_create_page(parent, on_delete, ctx);

    /* Title (subtitle = NULL per spec) */
    ui_kit_page_title(ctx->page, ICON_LIGHTBULB,
                      tr(STR_TITLE_ZH, STR_TITLE_EN), NULL);

    /* Main content: flex row (left = color wheel, right = controls) */
    lv_obj_t *main_row = make_flex_row(ctx->page, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(main_row, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(main_row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(main_row, UI_COLOR_CARD_SOFT, 0);
    lv_obj_set_style_bg_grad_dir(main_row, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(main_row, UI_CARD_RADIUS, 0);
    lv_obj_set_style_border_width(main_row, 1, 0);
    lv_obj_set_style_border_color(main_row, UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_all(main_row, 20, 0);
    lv_obj_set_style_pad_column(main_row, 20, 0);
    lv_obj_set_flex_align(main_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    ui_apply_card_shadow(main_row);

    /* ===== Left column: color wheel (fixed width 280) ===== */
    lv_obj_t *left_col = lv_obj_create(main_row);
    lv_obj_remove_style_all(left_col);
    lv_obj_set_width(left_col, 280);
    lv_obj_set_height(left_col, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(left_col, 0, 0);
    lv_obj_set_layout(left_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(left_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(left_col, LV_OBJ_FLAG_SCROLLABLE);

    /* Wheel container: no layout, fixed size, holds arc + preview overlay */
    lv_obj_t *wheel_box = lv_obj_create(left_col);
    lv_obj_remove_style_all(wheel_box);
    lv_obj_set_size(wheel_box, 240, 240);
    lv_obj_set_style_bg_color(wheel_box, UI_COLOR_ACCENT_SOFT, 0);
    lv_obj_set_style_bg_opa(wheel_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(wheel_box, 120, 0);
    lv_obj_clear_flag(wheel_box, LV_OBJ_FLAG_SCROLLABLE);

    /* Color wheel PNG background (from TF card), fallback to solid bg */
    lv_obj_t *wheel_bg = ui_asset_image_create(wheel_box, "color_wheel_220.png");
    if (wheel_bg) {
        lv_obj_center(wheel_bg);
    }

    /* Arc (color wheel): range 0-360 representing hue */
    ctx->arc = lv_arc_create(wheel_box);
    lv_obj_center(ctx->arc);
    lv_obj_set_size(ctx->arc, 220, 220);
    lv_arc_set_range(ctx->arc, 0, 360);
    lv_arc_set_bg_angles(ctx->arc, 0, 360);
    lv_arc_set_value(ctx->arc, 0);
    lv_obj_clear_flag(ctx->arc, LV_OBJ_FLAG_SCROLLABLE);

    /* Arc styling: transparent bg ring (PNG provides visual), indicator = current color */
    lv_obj_set_style_arc_width(ctx->arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ctx->arc, wheel_bg ? lv_color_white() : UI_COLOR_INPUT_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(ctx->arc, wheel_bg ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ctx->arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ctx->arc, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(ctx->arc, 6, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(ctx->arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(ctx->arc, 2, LV_PART_KNOB);
    lv_obj_set_style_border_color(ctx->arc, UI_COLOR_CARD, LV_PART_KNOB);
    lv_obj_set_style_radius(ctx->arc, LV_RADIUS_CIRCLE, LV_PART_KNOB);

    lv_obj_add_event_cb(ctx->arc, on_arc_changed, LV_EVENT_VALUE_CHANGED, ctx);

    /* Center preview circle (80x80, radius=40), floating overlay on arc */
    ctx->preview = lv_obj_create(wheel_box);
    lv_obj_remove_style_all(ctx->preview);
    lv_obj_set_size(ctx->preview, 80, 80);
    lv_obj_set_style_radius(ctx->preview, 40, 0);
    lv_obj_set_style_bg_color(ctx->preview, lv_color_make(ctx->red, ctx->green, ctx->blue), 0);
    lv_obj_set_style_bg_opa(ctx->preview, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ctx->preview, 2, 0);
    lv_obj_set_style_border_color(ctx->preview, UI_COLOR_CARD, 0);
    lv_obj_set_style_shadow_width(ctx->preview, 18, 0);
    lv_obj_set_style_shadow_opa(ctx->preview, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(ctx->preview, UI_COLOR_SHADOW, 0);
    lv_obj_add_flag(ctx->preview, LV_OBJ_FLAG_FLOATING);
    lv_obj_center(ctx->preview);
    lv_obj_clear_flag(ctx->preview, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* ===== Right column: controls (flex_grow 1) ===== */
    lv_obj_t *right_col = make_flex_col(main_row, LV_FLEX_ALIGN_START);
    lv_obj_set_width(right_col, 0);
    lv_obj_set_flex_grow(right_col, 1);
    lv_obj_set_style_pad_row(right_col, 12, 0);

    /* 1. Status row */
    ctx->status_row = make_flex_row(right_col, LV_FLEX_ALIGN_START);

    /* 2. Brightness group */
    ui_kit_group_title(right_col, tr(STR_BRIGHTNESS_ZH, STR_BRIGHTNESS_EN));
    lv_obj_t *bright_row = make_flex_row(right_col, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(bright_row, 10, 0);
    lv_obj_set_flex_align(bright_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ctx->brightness_slider = lv_slider_create(bright_row);
    lv_obj_set_width(ctx->brightness_slider, 0);
    lv_obj_set_flex_grow(ctx->brightness_slider, 1);
    lv_slider_set_range(ctx->brightness_slider, 0, 100);
    lv_slider_set_value(ctx->brightness_slider, ctx->brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ctx->brightness_slider, UI_COLOR_INPUT_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ctx->brightness_slider, UI_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ctx->brightness_slider, UI_COLOR_ACCENT, LV_PART_KNOB);
    lv_obj_add_event_cb(ctx->brightness_slider, on_brightness_changed,
                        LV_EVENT_VALUE_CHANGED, ctx);

    ctx->brightness_label = ui_create_label(bright_row, "60%",
        ui_font_cn(14), UI_COLOR_TEXT_STRONG);

    /* 3. Effect group */
    ui_kit_group_title(right_col, tr(STR_EFFECT_ZH, STR_EFFECT_EN));
    lv_obj_t *effect_row = lv_obj_create(right_col);
    lv_obj_remove_style_all(effect_row);
    lv_obj_set_width(effect_row, lv_pct(100));
    lv_obj_set_height(effect_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(effect_row, 0, 0);
    lv_obj_set_layout(effect_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(effect_row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(effect_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(effect_row, 8, 0);
    lv_obj_set_style_pad_row(effect_row, 8, 0);
    lv_obj_clear_flag(effect_row, LV_OBJ_FLAG_SCROLLABLE);

    static const uint8_t effect_values[NUM_EFFECTS] = {
        IOT_LIGHT_EFFECT_STATIC,
        IOT_LIGHT_EFFECT_BREATHE,
        IOT_LIGHT_EFFECT_RAINBOW,
        IOT_LIGHT_EFFECT_WARNING
    };
    const char *effect_zh[NUM_EFFECTS] = {
        STR_STATIC_ZH, STR_BREATHE_ZH, STR_RAINBOW_ZH, STR_WARNING_ZH
    };
    const char *effect_en[NUM_EFFECTS] = {
        STR_STATIC_EN, STR_BREATHE_EN, STR_RAINBOW_EN, STR_WARNING_EN
    };

    for (int i = 0; i < NUM_EFFECTS; i++) {
        bool active = (i == ctx->effect);
        ctx->effect_chips[i] = ui_kit_chip(effect_row,
            tr(effect_zh[i], effect_en[i]),
            active, on_effect_click, ctx);
        lv_obj_set_user_data(ctx->effect_chips[i], (void *)(uintptr_t)effect_values[i]);
    }

    /* 4. Preset group */
    ui_kit_group_title(right_col, tr(STR_PRESET_ZH, STR_PRESET_EN));
    lv_obj_t *preset_row = lv_obj_create(right_col);
    lv_obj_remove_style_all(preset_row);
    lv_obj_set_width(preset_row, lv_pct(100));
    lv_obj_set_height(preset_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(preset_row, 0, 0);
    lv_obj_set_layout(preset_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(preset_row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(preset_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(preset_row, 10, 0);
    lv_obj_set_style_pad_row(preset_row, 10, 0);
    lv_obj_clear_flag(preset_row, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 5; i++) {
        ctx->preset_blocks[i] = create_preset_block(preset_row, (uint8_t)i,
                                                      &s_presets[i], ctx);
    }
    /* Custom block (idx=5): rainbow gradient, no color change on click */
    ctx->preset_blocks[5] = create_preset_block(preset_row, 5, NULL, ctx);

    /* Initial refresh + subscribe */
    refresh_light(ctx);
    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);

    ESP_LOGI(TAG, "Light page created");
    return ctx->page;
}
