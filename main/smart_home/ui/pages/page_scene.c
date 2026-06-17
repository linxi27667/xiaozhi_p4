#include "page_scene.h"

#include "mqtt_device_model.h"
#include "mqtt_iot_protocol.h"
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

/* ===== UTF-8 hex escapes for Chinese strings ===== */

/* Scene names */
#define SCN_HOME_ZH    "\xE5\x9B\x9E\xE5\xAE\xB6"                              /* 回家 */
#define SCN_SLEEP_ZH   "\xE7\x9D\xA1\xE7\x9C\xA0"                              /* 睡眠 */
#define SCN_MOVIE_ZH   "\xE8\xA7\x82\xE5\xBD\xB1"                              /* 观影 */
#define SCN_NIGHT_ZH   "\xE8\xB5\xB7\xE5\xA4\x9C"                              /* 起夜 */
#define SCN_RAIN_ZH    "\xE9\x9B\xA8\xE5\xA4\xA9\xE6\x94\xB6\xE8\xA1\xA3"      /* 雨天收衣 */
#define SCN_AWAY_ZH    "\xE7\xA6\xBB\xE5\xAE\xB6"                              /* 离家 */
#define SCN_FIRE_ZH    "\xE7\x81\xAB\xE8\xAD\xA6\xE6\xBC\x94\xE7\xA4\xBA"      /* 火警演示 */

/* Scene descriptions */
#define DESC_HOME_ZH   "\xE5\xBC\x80\xE7\x81\xAF+\xE6\x9A\x96\xE8\x89\xB2\xE6\xB0\x9B\xE5\x9B\xB4"      /* 开灯+暖色氛围 */
#define DESC_SLEEP_ZH  "\xE5\x85\xB3\xE7\x81\xAF+\xE8\x93\x9D\xE8\x89\xB2\xE5\xA4\x9C\xE7\x81\xAF"      /* 关灯+蓝色夜灯 */
#define DESC_MOVIE_ZH  "\xE5\x85\xB3\xE7\x81\xAF+\xE5\xBD\xB1\xE9\x99\xA2\xE8\x93\x9D"                  /* 关灯+影院蓝 */
#define DESC_NIGHT_ZH  "\xE5\xBC\x80\xE7\x81\xAF+\xE6\xA9\x99\xE8\x89\xB2\xE5\xBC\xB1\xE5\x85\x89"      /* 开灯+橙色弱光 */
#define DESC_RAIN_ZH   "\xE6\x94\xB6\xE6\x99\xBE\xE8\xA1\xA3\xE6\x9D\x86+\xE8\x93\x9D\xE8\x89\xB2\xE5\x91\xBC\xE5\x90\xB8"  /* 收晾衣杆+蓝色呼吸 */
#define DESC_AWAY_ZH   "\xE5\x85\xA8\xE5\x85\xB3"                                                      /* 全关 */
#define DESC_FIRE_ZH   "\xE7\xBA\xA2\xE8\x89\xB2\xE8\xAD\xA6\xE7\xA4\xBA+\xE5\x85\xA8\xE5\xBC\x80"      /* 红色警示+全开 */

/* Page title */
#define STR_TITLE_ZH      "\xE5\x9C\xBA\xE6\x99\xAF\xE6\xA8\xA1\xE5\xBC\x8F"  /* 场景模式 */
#define STR_TITLE_EN      "Scene Mode"

typedef struct {
    uint8_t id;
    const char *icon;
    const char *zh;
    const char *en;
    const char *desc_zh;
    const char *desc_en;
    bool danger;
} scene_item_t;

static const scene_item_t s_scenes[] = {
    { IOT_SCENE_HOME,  ICON_HOME,      SCN_HOME_ZH,  "Home",      DESC_HOME_ZH,  "Lights on, warm tone",    false },
    { IOT_SCENE_SLEEP, ICON_MOON,      SCN_SLEEP_ZH, "Sleep",     DESC_SLEEP_ZH, "Lights off, blue night",  false },
    { IOT_SCENE_MOVIE, ICON_STAR,      SCN_MOVIE_ZH, "Movie",     DESC_MOVIE_ZH, "Lights off, cinema blue", false },
    { IOT_SCENE_NIGHT, ICON_LIGHTBULB, SCN_NIGHT_ZH, "Night",     DESC_NIGHT_ZH, "Lights on, dim orange",   false },
    { IOT_SCENE_RAIN,  ICON_UMBRELLA,  SCN_RAIN_ZH,  "Rain",      DESC_RAIN_ZH,  "Retract racks, breathe",  false },
    { IOT_SCENE_AWAY,  ICON_LOCK,      SCN_AWAY_ZH,  "Away",      DESC_AWAY_ZH,  "All off",                 false },
    { IOT_SCENE_FIRE,  ICON_FIRE,      SCN_FIRE_ZH,  "Fire Demo", DESC_FIRE_ZH,  "Red alert, all on",       true  },
};

#define SCENE_COUNT (sizeof(s_scenes) / sizeof(s_scenes[0]))

typedef struct {
    lv_obj_t *card;
    uint8_t scene_id;
    bool danger;
} scene_card_info_t;

typedef struct {
    lv_obj_t *page;
    scene_card_info_t cards[SCENE_COUNT];
    bool deleted;
} scene_ctx_t;

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

static void on_scene_click(lv_event_t *e)
{
    uint8_t scene_id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    mqtt_send_scene(scene_id);
}

static void refresh_scene(scene_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;
    const mqtt_device_model_t *m = device_model_get();

    /* Update card active states by tweaking border/bg styles */
    for (uint32_t i = 0; i < SCENE_COUNT; i++) {
        lv_obj_t *card = ctx->cards[i].card;
        if (!card) continue;

        bool active = (m->current_scene == ctx->cards[i].scene_id);
        bool danger = ctx->cards[i].danger;
        lv_color_t accent = danger ? UI_COLOR_RED : UI_COLOR_ACCENT;

        if (active) {
            lv_obj_set_style_border_color(card, accent, 0);
            lv_obj_set_style_border_width(card, 2, 0);
            lv_obj_set_style_bg_color(card, lv_color_mix(accent, UI_COLOR_CARD, 12), 0);
        } else {
            lv_obj_set_style_border_color(card, UI_COLOR_BORDER, 0);
            lv_obj_set_style_border_width(card, 1, 0);
            lv_obj_set_style_bg_color(card, UI_COLOR_CARD, 0);
        }
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

static lv_obj_t *create_card_grid(lv_obj_t *parent, int gap)
{
    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_remove_style_all(grid);
    lv_obj_set_width(grid, lv_pct(100));
    lv_obj_set_height(grid, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(grid, gap, 0);
    lv_obj_set_style_pad_row(grid, gap, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    return grid;
}

static void create_scene_card(lv_obj_t *grid, scene_ctx_t *ctx, int index)
{
    const scene_item_t *item = &s_scenes[index];
    const mqtt_device_model_t *m = device_model_get();
    bool active = (m->current_scene == item->id);

    lv_obj_t *card = ui_kit_scene_card(grid,
        item->icon,
        tr(item->zh, item->en),
        tr(item->desc_zh, item->desc_en),
        active, item->danger,
        on_scene_click, (void *)(uintptr_t)item->id);

    lv_obj_set_width(card, lv_pct(31));
    lv_obj_set_style_min_height(card, 108, 0);

    ctx->cards[index].card = card;
    ctx->cards[index].scene_id = item->id;
    ctx->cards[index].danger = item->danger;
}

lv_obj_t *page_scene_create(lv_obj_t *parent)
{
    scene_ctx_t *ctx = lv_malloc(sizeof(scene_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    lv_obj_t *page = ui_create_page(parent, on_delete, ctx);
    ctx->page = page;

    /* Page title: icon + title (no subtitle per spec) */
    ui_kit_page_title(page, ICON_STAR,
        tr(STR_TITLE_ZH, STR_TITLE_EN), NULL);

    lv_obj_t *grid = create_card_grid(page, 12);
    for (int i = 0; i < 6; i++) {
        create_scene_card(grid, ctx, i);
    }

    /* Row 3: fire demo card (full width, danger=true) */
    const scene_item_t *fire = &s_scenes[6];
    const mqtt_device_model_t *m = device_model_get();
    bool fire_active = (m->current_scene == fire->id);
    lv_obj_t *fire_card = ui_kit_scene_card(page,
        fire->icon,
        tr(fire->zh, fire->en),
        tr(fire->desc_zh, fire->desc_en),
        fire_active, fire->danger,
        on_scene_click, (void *)(uintptr_t)fire->id);
    lv_obj_set_style_min_height(fire_card, 108, 0);
    ctx->cards[6].card = fire_card;
    ctx->cards[6].scene_id = fire->id;
    ctx->cards[6].danger = fire->danger;

    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    ui_event_subscribe(UI_EVENT_SCENE_CHANGED, on_scene_changed, ctx);
    refresh_scene(ctx);

    ESP_LOGI(TAG, "Scene page created");
    return page;
}
