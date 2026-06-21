#include "page_scene.h"

#include "../../services/auto_mode.h"
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

static const char *TAG = "PAGE_SCENE";

typedef enum {
    SCENE_ACTION_SCENE = 0,
    SCENE_ACTION_AUTO,
} scene_action_t;

typedef struct {
    uint8_t id;
    scene_action_t action;
    const char *icon;
    const char *asset;
    const char *zh;
    const char *en;
    const char *desc_zh;
    const char *desc_en;
    uint32_t accent_hex;
} scene_item_t;

static const scene_item_t s_scenes[] = {
    { IOT_SCENE_HOME,  SCENE_ACTION_SCENE,  ICON_HOME,      "scene_home.png",
      "\xE5\x9B\x9E\xE5\xAE\xB6", "Home",
      "\xE5\xA4\xA7\xE5\x8E\x85\xE7\x81\xAF+\xE4\xB8\xBB\xE5\x8D\xA7\xE6\x9A\x96\xE5\x85\x89", "Hall + warm RGB",
      0x2F6BFF },
    { IOT_SCENE_SLEEP, SCENE_ACTION_SCENE,  ICON_MOON,      "scene_sleep.png",
      "\xE7\x9D\xA1\xE7\x9C\xA0", "Sleep",
      "\xE5\x85\xB3\xE9\x97\xAD\xE4\xB8\xBB\xE7\x81\xAF+\xE4\xBD\x8E\xE4\xBA\xAE\xE5\xA4\x9C\xE7\x81\xAF", "Main lights off",
      0x3B82F6 },
    { IOT_SCENE_MOVIE, SCENE_ACTION_SCENE,  ICON_STAR,      "scene_movie.png",
      "\xE8\xA7\x82\xE5\xBD\xB1", "Movie",
      "\xE5\x85\xB3\xE9\x97\xAD\xE4\xB8\xBB\xE7\x81\xAF+\xE5\xBD\xB1\xE9\x99\xA2\xE8\x93\x9D", "Cinema blue",
      0x6554FF },
    { IOT_SCENE_NIGHT, SCENE_ACTION_SCENE,  ICON_LIGHTBULB, "scene_night.png",
      "\xE8\xB5\xB7\xE5\xA4\x9C", "Night",
      "\xE5\xBC\x80\xE5\x90\xAF\xE5\x8E\x95\xE6\x89\x80\xE7\x81\xAF+\xE6\xA9\x99\xE8\x89\xB2\xE5\xBC\xB1\xE5\x85\x89", "Bathroom + dim RGB",
      0xF59E0B },
    { IOT_SCENE_BRIGHT, SCENE_ACTION_SCENE,  ICON_SUN,       "scene_lights.png",
      "\xE6\x98\x8E\xE4\xBA\xAE", "Bright",
      "\xE5\xBC\x80\xE5\x90\xAF\xE5\xB8\xB8\xE7\x94\xA8\xE7\x81\xAF\xE5\x85\x89", "Common lights on",
      0xF59E0B },
    { IOT_SCENE_AWAY,  SCENE_ACTION_SCENE,  ICON_LOCK,      "scene_away.png",
      "\xE7\xA6\xBB\xE5\xAE\xB6", "Away",
      "\xE5\x85\xA8\xE5\xB1\x8B\xE7\x81\xAF\xE5\x85\x89+\xE8\xBE\x93\xE5\x87\xBA\xE5\x85\xB3\xE9\x97\xAD", "All outputs off",
      0x6554FF },
    { IOT_SCENE_FIRE,  SCENE_ACTION_SCENE,  ICON_FIRE,      "scene_fire.png",
      "\xE7\x81\xAB\xE7\x81\xBE", "Fire",
      "\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8\xE8\xA7\xA6\xE5\x8F\x91\xE5\x9C\xBA\xE6\x99\xAF", "Sensor-triggered",
      0xC62828 },
    { IOT_SCENE_RAIN,  SCENE_ACTION_SCENE,  ICON_WATER,     "scene_rain.png",
      "\xE9\x9B\xA8\xE5\xA4\xA9", "Rain",
      "\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8\xE8\xA7\xA6\xE5\x8F\x91\xE5\x9C\xBA\xE6\x99\xAF", "Sensor-triggered",
      0x3B82F6 },
    /* Auto mode toggle (id=0xFF, not a real scene) */
    { 0xFF,            SCENE_ACTION_AUTO,   ICON_SETTINGS,  NULL,
      "\xE8\x87\xAA\xE5\x8A\xA8", "Auto",
      "\xE6\x99\xBA\xE8\x83\xBD\xE8\x87\xAA\xE5\x8A\xA8\xE6\x8E\xA7\xE5\x88\xB6", "Smart auto control",
      0x10B981 },
};

#define SCENE_COUNT (sizeof(s_scenes) / sizeof(s_scenes[0]))

typedef struct {
    lv_obj_t *card;
    uint8_t scene_id;
    scene_action_t action;
} scene_card_info_t;

typedef struct {
    lv_obj_t *page;
    lv_obj_t *current_scene_label;  /* top-right badge showing active scene */
    scene_card_info_t cards[SCENE_COUNT];
    bool deleted;
} scene_ctx_t;

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

static lv_color_t scene_accent(const scene_item_t *item)
{
    return lv_color_hex(item ? item->accent_hex : 0x2F6BFF);
}

static lv_obj_t *create_row(lv_obj_t *parent, int gap)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row, gap, 0);
    lv_obj_set_style_pad_row(row, gap, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

static void add_scene_image(lv_obj_t *parent, const scene_item_t *item)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 48, 48);
    lv_color_t accent = scene_accent(item);
    lv_obj_set_style_bg_color(box, lv_color_mix(accent, UI_COLOR_CARD, 18), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 10, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *img = ui_asset_image_create(box, item->asset);
    if (img) {
        lv_obj_center(img);
        return;
    }

    lv_obj_t *ico = ui_create_icon(box, item->icon, ui_font_icon(23), accent);
    lv_obj_center(ico);
}

static void send_bright_mode(void)
{
    mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_ON);
    mqtt_send_rgb_light(2, 0, 255, 210, 150, 58, IOT_LIGHT_EFFECT_STATIC, 20);
}

static void on_scene_click(lv_event_t *e)
{
    const scene_item_t *item = (const scene_item_t *)lv_event_get_user_data(e);
    if (!item) return;

    if (item->action == SCENE_ACTION_AUTO) {
        auto_mode_set_global(!auto_mode_get_global());
    } else {
        mqtt_send_scene(item->id);
        /* Bright scene also sends broadcast to turn lights on */
        if (item->id == IOT_SCENE_BRIGHT) {
            send_bright_mode();
        }
    }
}

static void set_card_active(lv_obj_t *card, lv_color_t accent, bool active)
{
    if (!card) return;
    lv_obj_set_style_border_color(card, active ? accent : UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(card, active ? 2 : 1, 0);
    lv_obj_set_style_bg_color(card, active ? lv_color_mix(accent, UI_COLOR_CARD, 12) : UI_COLOR_CARD, 0);
}

static void refresh_scene(scene_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;
    const mqtt_device_model_t *m = device_model_get();

    for (uint32_t i = 0; i < SCENE_COUNT; i++) {
        const scene_item_t *item = &s_scenes[i];
        bool active = false;
        if (item->action == SCENE_ACTION_SCENE) {
            active = (m->current_scene == item->id);
        } else if (item->action == SCENE_ACTION_AUTO) {
            active = auto_mode_get_global();
        }
        set_card_active(ctx->cards[i].card, scene_accent(item), active);
    }

    /* Update current scene badge */
    if (ctx->current_scene_label) {
        const char *name = device_model_scene_name(m->current_scene);
        lv_label_set_text(ctx->current_scene_label, name);
    }
}

static void on_model_updated(void *user_data)
{
    refresh_scene((scene_ctx_t *)user_data);
}

static void on_scene_changed(void *user_data)
{
    refresh_scene((scene_ctx_t *)user_data);
}

static void on_delete(lv_event_t *e)
{
    scene_ctx_t *ctx = (scene_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    ui_event_unsubscribe(UI_EVENT_SCENE_CHANGED, on_scene_changed, ctx);
    lv_free(ctx);
}

static lv_obj_t *create_scene_card(lv_obj_t *grid, scene_ctx_t *ctx, uint32_t index)
{
    const scene_item_t *item = &s_scenes[index];

    lv_obj_t *card = ui_create_card(grid);
    lv_obj_set_width(card, lv_pct(31));
    lv_obj_set_style_min_height(card, 116, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_column(card, 12, 0);
    lv_obj_set_style_bg_color(card, UI_COLOR_CARD, 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, on_scene_click, LV_EVENT_CLICKED, (void *)item);
    ui_apply_press_feedback(card, scene_accent(item));

    add_scene_image(card, item);

    lv_obj_t *col = lv_obj_create(card);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, 0);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, 5, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name = ui_create_label(col, tr(item->zh, item->en), ui_font_cn(19), UI_COLOR_TEXT_STRONG);
    lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(name, lv_pct(100));

    lv_obj_t *desc = ui_create_label(col, tr(item->desc_zh, item->desc_en), ui_font_cn(13), UI_COLOR_TEXT_SEC);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc, lv_pct(100));

    ctx->cards[index].card = card;
    ctx->cards[index].scene_id = item->id;
    ctx->cards[index].action = item->action;
    return card;
}

lv_obj_t *page_scene_create(lv_obj_t *parent)
{
    scene_ctx_t *ctx = lv_malloc(sizeof(scene_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    lv_obj_t *page = ui_create_page(parent, on_delete, ctx);
    ctx->page = page;

    /* Title row with current scene badge on the right */
    lv_obj_t *title_row = lv_obj_create(page);
    lv_obj_remove_style_all(title_row);
    lv_obj_set_width(title_row, lv_pct(100));
    lv_obj_set_height(title_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_layout(title_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    /* Left: title (grows to fill) */
    lv_obj_t *title_left = lv_obj_create(title_row);
    lv_obj_remove_style_all(title_left);
    lv_obj_set_width(title_left, 0);
    lv_obj_set_flex_grow(title_left, 1);
    lv_obj_set_height(title_left, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(title_left, 0, 0);
    lv_obj_clear_flag(title_left, LV_OBJ_FLAG_SCROLLABLE);

    ui_kit_page_title(title_left, ICON_STAR,
        tr("\xE5\x9C\xBA\xE6\x99\xAF\xE6\xA8\xA1\xE5\xBC\x8F", "Scene Mode"),
        tr("\xE6\x89\x8B\xE5\x8A\xA8\xE5\x9C\xBA\xE6\x99\xAF\xE5\xBF\xAB\xE6\x8D\xB7\xE5\x88\x87\xE6\x8D\xA2", "Quick scene switching"));

    /* Right: current scene badge */
    lv_obj_t *badge = lv_obj_create(title_row);
    lv_obj_remove_style_all(badge);
    lv_obj_set_height(badge, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(badge, UI_COLOR_ACCENT_SOFT, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 12, 0);
    lv_obj_set_style_pad_hor(badge, 12, 0);
    lv_obj_set_style_pad_ver(badge, 4, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    ctx->current_scene_label = ui_create_label(badge, "未启用",
        ui_font_cn(13), UI_COLOR_ACCENT);

    /* UTF-8: 手动场景 = E6898BE58AA8E59CBAE699AF */
    ui_kit_group_title(page, tr("\xE6\x89\x8B\xE5\x8A\xA8\xE5\x9C\xBA\xE6\x99\xAF", "Manual Scenes"));
    lv_obj_t *grid = create_row(page, 12);
    for (uint32_t i = 0; i < SCENE_COUNT; i++) {
        create_scene_card(grid, ctx, i);
    }

    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    ui_event_subscribe(UI_EVENT_SCENE_CHANGED, on_scene_changed, ctx);
    refresh_scene(ctx);

    ESP_LOGI(TAG, "Scene page created");
    return page;
}
