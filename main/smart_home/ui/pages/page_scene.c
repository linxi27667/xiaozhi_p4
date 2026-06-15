#include "page_scene.h"

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

static const char *TAG = "PAGE_SCENE";

typedef struct {
    lv_obj_t *state;
    bool deleted;
} scene_ctx_t;

typedef struct {
    uint8_t id;
    const char *icon;
    const char *zh;
    const char *en;
    const char *desc_zh;
    const char *desc_en;
} scene_item_t;

static const scene_item_t s_scenes[] = {
    { IOT_SCENE_HOME,  ICON_HOME, "回家", "Home", "大厅灯与主卧灯渐亮", "Hall and master light on" },
    { IOT_SCENE_SLEEP, ICON_MOON, "睡眠", "Sleep", "暖暗光并关闭多余设备", "Dim light and quiet devices" },
    { IOT_SCENE_MOVIE, ICON_STAR, "观影", "Movie", "低亮蓝色氛围", "Low blue ambience" },
    { IOT_SCENE_NIGHT, ICON_LIGHTBULB, "起夜", "Night", "主卧低亮，厕所灯开启", "Low light plus toilet light" },
    { IOT_SCENE_RAIN, ICON_DROP, "雨天收衣", "Rain", "收回晾衣杆并蓝色提示", "Retract racks and blue hint" },
    { IOT_SCENE_AWAY, ICON_LOCK, "离家", "Away", "关闭灯光和低压输出", "Turn outputs off" },
    { IOT_SCENE_FIRE, ICON_FIRE, "火警演示", "Fire Demo", "灯光警示并打开照明", "Warning lights and all lights on" },
};

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

static void refresh_scene(scene_ctx_t *ctx)
{
    if (!ctx || ctx->deleted || !ctx->state) return;
    const mqtt_device_model_t *m = device_model_get();
    char buf[64];
    lv_snprintf(buf, sizeof(buf), "%s：%s", tr("当前场景", "Current"), m->scene_name);
    lv_label_set_text(ctx->state, buf);
}

static void on_scene_event(void *user_data)
{
    refresh_scene((scene_ctx_t *)user_data);
}

static void on_scene_click(lv_event_t *e)
{
    uint8_t scene_id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    mqtt_send_scene(scene_id);
}

static void scene_button(lv_obj_t *parent, int x, int y, const scene_item_t *item)
{
    lv_obj_t *btn = panel(parent, x, y, 240, 96);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, on_scene_click, LV_EVENT_CLICKED, (void *)(uintptr_t)item->id);
    ui_apply_press_feedback(btn, UI_COLOR_ACCENT);

    lv_obj_t *ic = ui_create_icon(btn, item->icon, ui_font_icon(22), UI_COLOR_ACCENT);
    lv_obj_set_pos(ic, 18, 18);
    label(btn, tr(item->zh, item->en), 18, UI_COLOR_TEXT_STRONG, 56, 16, 116);
    lv_obj_t *desc = label(btn, tr(item->desc_zh, item->desc_en), 11, UI_COLOR_TEXT_SEC, 56, 48, 156);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
}

static void page_delete(lv_event_t *e)
{
    scene_ctx_t *ctx = (scene_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_scene_event, ctx);
    ui_event_unsubscribe(UI_EVENT_SCENE_CHANGED, on_scene_event, ctx);
    lv_free(ctx);
}

lv_obj_t *page_scene_create(lv_obj_t *parent)
{
    scene_ctx_t *ctx = lv_malloc(sizeof(scene_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(page, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(page, page_delete, LV_EVENT_DELETE, ctx);

    label(page, tr("场景", "Scenes"), 24, UI_COLOR_TEXT_STRONG, 32, 24, 120);
    ctx->state = label(page, "", 13, UI_COLOR_TEXT_SEC, 32, 58, 220);

    for (uint32_t i = 0; i < sizeof(s_scenes) / sizeof(s_scenes[0]); i++) {
        int col = i % 3;
        int row = i / 3;
        scene_button(page, 32 + col * 270, 100 + row * 124, &s_scenes[i]);
    }

    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_scene_event, ctx);
    ui_event_subscribe(UI_EVENT_SCENE_CHANGED, on_scene_event, ctx);
    refresh_scene(ctx);

    ESP_LOGI(TAG, "Scene page created");
    return page;
}
