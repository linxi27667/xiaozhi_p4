#include "page_data.h"
#include "ui_kit.h"
#include "ui_events.h"
#include "ui_styles.h"
#include "ui_theme.h"
#include "ui_font.h"
#include "mqtt_device_model.h"
#include "mqtt_iot_protocol.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "ui_asset_service.h"
#include "../../services/xiaozhi_mqtt.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdint.h>
#include <string.h>
#include <time.h>

static const char *TAG = "PAGE_DATA";

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

typedef struct {
    lv_obj_t *page;
    lv_obj_t *visual_row;  /* dynamic: rebuilt on update */
    lv_obj_t *pills_col;    /* dynamic: rebuilt on update */
    lv_obj_t *metrics_row;  /* dynamic: rebuilt on update */
    lv_obj_t *floors_row;   /* dynamic: rebuilt on update */
    bool deleted;
} data_ctx_t;

/* ---- Helpers ---- */

static bool any_light_on(void)
{
    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t *d = device_model_at(i);
        if (d && d->connected && d->power_on &&
            (d->type == RC_DEVICE_LIGHT || d->type == RC_DEVICE_RGB_LIGHT)) {
            return true;
        }
    }
    return false;
}

static bool is_safe(void)
{
    const mqtt_device_model_t *m = device_model_get();
    /* Check fire status on all floors (status >= 2 means alert) */
    for (int i = 0; i < 3; i++) {
        if (m->fire_floor_valid[i] && m->fire_floor_status[i] >= 2) return false;
        if (m->help_floor_valid[i] && m->help_floor_status[i] >= 1) return false;
    }
    return true;
}

static const char *scene_display_name(const mqtt_device_model_t *m)
{
    if (!m) return tr("\xE6\x9C\xAA\xE5\x90\xAF\xE7\x94\xA8", "Idle");
    if (m->current_scene == IOT_SCENE_FIRE && is_safe()) {
        return tr("\xE6\x9C\xAA\xE5\x90\xAF\xE7\x94\xA8", "Idle");
    }
    return device_model_scene_name(m->current_scene);
}

static void floor_stats(uint8_t floor, uint16_t *total, uint16_t *online)
{
    *total = 0;
    *online = 0;
    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t *d = device_model_at(i);
        if (d && d->floor == (rc_floor_t)floor) {
            (*total)++;
            if (d->connected) (*online)++;
        }
    }
}

static uint8_t controllers_online(void)
{
    const mqtt_device_model_t *m = device_model_get();
    uint8_t n = 0;
    for (int i = 0; i < 3; i++) {
        if (m->controller_online[i]) n++;
    }
    return n;
}

static const char *weather_desc(uint16_t code)
{
    if (code == 0) return tr("\xE6\x99\xB4", "Clear");
    if (code == 1 || code == 2 || code == 3) return tr("\xE5\xA4\x9A\xE4\xBA\x91", "Cloudy");
    if (code == 45 || code == 48) return tr("\xE6\x9C\x89\xE9\x9B\xBE", "Fog");
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return tr("\xE6\x9C\x89\xE9\x9B\xA8", "Rain");
    if (code >= 71 && code <= 77) return tr("\xE6\x9C\x89\xE9\x9B\xAA", "Snow");
    if (code >= 95) return tr("\xE9\x9B\xB7\xE9\x9B\xA8", "Storm");
    return tr("\xE5\xA4\xA9\xE6\xB0\x94", "Weather");
}

/* ---- Quick action handlers ---- */

static void on_lights_toggle(lv_event_t *e)
{
    (void)e;
    if (any_light_on()) {
        mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_OFF);
    } else {
        mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_ON);
    }
}

static void on_scene_home(lv_event_t *e)
{
    (void)e;
    mqtt_send_scene(IOT_SCENE_HOME);
}

static void on_scene_away(lv_event_t *e)
{
    (void)e;
    mqtt_send_scene(IOT_SCENE_AWAY);
}

static void on_security(lv_event_t *e)
{
    (void)e;
    mqtt_send_broadcast(IOT_CMD_BROADCAST_ALL_OFF);
}

static void on_scene_rain(lv_event_t *e)
{
    (void)e;
    mqtt_send_scene(IOT_SCENE_RAIN);
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

static lv_obj_t *make_flex_col(lv_obj_t *parent, lv_flex_align_t main_align)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, LV_SIZE_CONTENT);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    return col;
}

static lv_obj_t *make_card(lv_obj_t *parent, int min_h)
{
    lv_obj_t *card = ui_create_card(parent);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(card, min_h, 0);
    return card;
}

static void add_asset_or_icon(lv_obj_t *parent, const char *asset, const char *icon,
                              int box_w, int box_h, lv_color_t accent)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, box_w, box_h);
    lv_obj_set_style_bg_color(box, lv_color_mix(accent, UI_COLOR_CARD, 12), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *img = ui_asset_image_create(box, asset);
    if (img) {
        lv_obj_center(img);
        return;
    }

    lv_obj_t *ico = ui_create_icon(box, icon, ui_font_icon(28), accent);
    lv_obj_center(ico);
}

static lv_obj_t *create_action_button(lv_obj_t *parent, const char *icon, const char *text,
                                      lv_color_t accent, lv_event_cb_t on_click)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_width(btn, 0);
    lv_obj_set_height(btn, 56);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_style_bg_color(btn, lv_color_mix(accent, UI_COLOR_CARD, 12), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_mix(accent, UI_COLOR_BORDER, 28), 0);
    lv_obj_set_style_pad_hor(btn, 10, 0);
    lv_obj_set_style_pad_column(btn, 8, 0);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    if (on_click) lv_obj_add_event_cb(btn, on_click, LV_EVENT_CLICKED, NULL);
    ui_apply_press_feedback(btn, accent);

    ui_create_icon(btn, icon, ui_font_icon(18), accent);
    lv_obj_t *lbl = ui_create_label(btn, text, ui_font_cn(16), UI_COLOR_TEXT_STRONG);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    return btn;
}

/* ---- Dynamic section builders ---- */

static void build_pills(data_ctx_t *ctx)
{
    if (!ctx->pills_col) return;
    lv_obj_clean(ctx->pills_col);

    const mqtt_device_model_t *m = device_model_get();
    char buf[32];

    /* Top row: WiFi + MQTT + location */
    lv_obj_t *row_top = make_flex_row(ctx->pills_col, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row_top, 8, 0);

    bool wifi_ok = (m->wifi_state == WIFI_STATE_CONNECTED);
    ui_kit_status_pill(row_top,
                       wifi_ok ? tr("Wi-Fi \xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5", "Wi-Fi Online")
                               : (m->wifi_state == WIFI_STATE_CONNECTING ? tr("Wi-Fi \xE8\xBF\x9E\xE6\x8E\xA5\xE4\xB8\xAD", "Wi-Fi Connecting")
                                                                          : tr("Wi-Fi \xE6\x9C\xAA\xE8\xBF\x9E\xE6\x8E\xA5", "Wi-Fi Offline")),
                       wifi_ok ? UI_COLOR_GREEN : UI_COLOR_RED,
                       wifi_ok ? UI_COLOR_GREEN_SOFT : UI_COLOR_INPUT_BG);

    bool mqtt_ok = (m->mqtt_state == MQTT_STATE_CONNECTED);
    ui_kit_status_pill(row_top,
                       mqtt_ok ? tr("MQTT \xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5", "MQTT Online")
                               : tr("MQTT \xE6\x9C\xAA\xE8\xBF\x9E\xE6\x8E\xA5", "MQTT Offline"),
                       mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED,
                       mqtt_ok ? UI_COLOR_GREEN_SOFT : UI_COLOR_INPUT_BG);

    ui_kit_status_pill(row_top, tr("\xE5\xB9\xBF\xE5\xB7\x9E\xE5\xAE\xA4\xE5\xA4\x96", "Guangzhou Outdoor"),
                       UI_COLOR_ACCENT, UI_COLOR_ACCENT_SOFT);

    /* Bottom row: time + weather */
    lv_obj_t *row_bot = make_flex_row(ctx->pills_col, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row_bot, 8, 0);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) {
        lv_snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    } else {
        lv_snprintf(buf, sizeof(buf), "--:--");
    }
    ui_kit_status_pill(row_bot, buf, UI_COLOR_ACCENT, UI_COLOR_ACCENT_SOFT);

    if (m->weather_valid) {
        int temp_int = (int)(m->outdoor_temp + 0.5f);
        lv_snprintf(buf, sizeof(buf), "%s %d\xC2\xB0""C", weather_desc(m->weather_code), temp_int);
        ui_kit_status_pill(row_bot, buf, UI_COLOR_ORANGE, UI_COLOR_CARD_SOFT);
    } else {
        ui_kit_status_pill(row_bot, tr("\xE5\xA4\xA9\xE6\xB0\x94\xE5\x90\x8C\xE6\xAD\xA5\xE4\xB8\xAD", "Weather Syncing"),
                           UI_COLOR_ORANGE, UI_COLOR_CARD_SOFT);
    }
}

static void build_visual(data_ctx_t *ctx)
{
    if (!ctx->visual_row) return;
    lv_obj_clean(ctx->visual_row);

    const mqtt_device_model_t *m = device_model_get();
    char buf[48];

    lv_obj_t *home = make_card(ctx->visual_row, 122);
    lv_obj_set_width(home, 0);
    lv_obj_set_flex_grow(home, 2);
    lv_obj_set_style_pad_column(home, 16, 0);
    add_asset_or_icon(home, "overview_home.png", ICON_HOME, 168, 96, UI_COLOR_ORANGE);

    lv_obj_t *home_col = make_flex_col(home, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(home_col, 1);
    lv_obj_set_style_pad_row(home_col, 6, 0);
    ui_create_label(home_col, tr("\xE5\xB9\xBF\xE5\xB7\x9E \xC2\xB7 ESP32-P4 \xE4\xB8\xAD\xE6\x8E\xA7", "Guangzhou ESP32-P4 Hub"),
                    ui_font_cn(22), UI_COLOR_TEXT_STRONG);
    lv_snprintf(buf, sizeof(buf), "%s \xC2\xB7 %u/3 %s",
                scene_display_name(m),
                (unsigned)controllers_online(),
                tr("\xE6\x8E\xA7\xE5\x88\xB6\xE5\x99\xA8\xE5\x9C\xA8\xE7\xBA\xBF", "controllers online"));
    ui_create_label(home_col, buf, ui_font_cn(15), UI_COLOR_TEXT_SEC);
    ui_create_label(home_col,
                    is_safe() ? tr("\xE5\xAE\x89\xE9\x98\xB2\xE7\x8A\xB6\xE6\x80\x81\xEF\xBC\x9A\xE6\xAD\xA3\xE5\xB8\xB8", "Security: OK")
                              : tr("\xE5\xAE\x89\xE9\x98\xB2\xE7\x8A\xB6\xE6\x80\x81\xEF\xBC\x9A\xE5\x91\x8A\xE8\xAD\xA6", "Security: Alert"),
                    ui_font_cn(15), is_safe() ? UI_COLOR_GREEN : UI_COLOR_RED);

    lv_obj_t *weather = make_card(ctx->visual_row, 122);
    lv_obj_set_width(weather, 0);
    lv_obj_set_flex_grow(weather, 1);
    lv_obj_set_style_pad_column(weather, 14, 0);
    add_asset_or_icon(weather, "weather_guangzhou.png", ICON_LOCATION, 96, 76, UI_COLOR_ACCENT);

    lv_obj_t *weather_col = make_flex_col(weather, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(weather_col, 1);
    lv_obj_set_style_pad_row(weather_col, 4, 0);
    ui_create_label(weather_col, tr("\xE5\xB9\xBF\xE5\xB7\x9E\xE5\xAE\xA4\xE5\xA4\x96", "Guangzhou Outdoor"),
                    ui_font_cn(17), UI_COLOR_TEXT_STRONG);
    if (m->weather_valid) {
        int temp_int = (int)(m->outdoor_temp + 0.5f);
        lv_snprintf(buf, sizeof(buf), "%s  %d\xC2\xB0""C", weather_desc(m->weather_code), temp_int);
        ui_create_label(weather_col, buf, ui_font_cn(18), UI_COLOR_ACCENT);
        int rain_tenths = (int)(m->precipitation_mm * 10.0f + 0.5f);
        int pm25_int = (int)(m->pm25_outdoor + 0.5f);
        lv_snprintf(buf, sizeof(buf), "%u%%  %d.%dmm  PM2.5 %d",
                    (unsigned)m->outdoor_humidity, rain_tenths / 10, rain_tenths % 10, pm25_int);
        ui_create_label(weather_col, buf, ui_font_cn(13), UI_COLOR_TEXT_SEC);
    } else {
        ui_create_label(weather_col, tr("\xE5\xA4\xA9\xE6\xB0\x94\xE6\x9C\x8D\xE5\x8A\xA1\xE5\x90\x8C\xE6\xAD\xA5\xE4\xB8\xAD", "Weather syncing"),
                        ui_font_cn(15), UI_COLOR_TEXT_SEC);
    }
}

static void build_metrics(data_ctx_t *ctx)
{
    if (!ctx->metrics_row) return;
    lv_obj_clean(ctx->metrics_row);

    const mqtt_device_model_t *m = device_model_get();
    char buf[32];

    /* Scene state */
    const char *scene_name = scene_display_name(m);
    lv_obj_t *c1 = ui_kit_metric_card(ctx->metrics_row, ICON_STAR,
                                       tr("\xE5\x9C\xBA\xE6\x99\xAF\xE6\xA8\xA1\xE5\xBC\x8F", "Scene"),
                                       scene_name,
                                       "",
                                       true);
    lv_obj_set_width(c1, 0);
    lv_obj_set_flex_grow(c1, 1);

    /* Security state */
    bool safe = is_safe();
    lv_obj_t *c2 = ui_kit_metric_card(ctx->metrics_row, safe ? ICON_CHECK : ICON_FIRE,
                                       tr("\xE5\xAE\x89\xE9\x98\xB2\xE7\x8A\xB6\xE6\x80\x81", "Security"),
                                       safe ? tr("\xE6\xAD\xA3\xE5\xB8\xB8", "OK") : tr("\xE5\x91\x8A\xE8\xAD\xA6", "Alert"),
                                       "", true);
    lv_obj_set_width(c2, 0);
    lv_obj_set_flex_grow(c2, 1);

    /* Controller status */
    uint8_t ctrl = controllers_online();
    if (ctrl == 0) {
        /* All controllers offline - show waiting state */
        lv_obj_t *c3 = ui_kit_metric_card(ctx->metrics_row, ICON_SERVER,
                                           tr("\xE6\x8E\xA7\xE5\x88\xB6\xE5\x99\xA8", "Controllers"),
                                           "", "", false);
        lv_obj_set_width(c3, 0);
        lv_obj_set_flex_grow(c3, 1);
    } else {
        lv_snprintf(buf, sizeof(buf), "%u/3", (unsigned)ctrl);
        lv_obj_t *c3 = ui_kit_metric_card(ctx->metrics_row, ICON_SERVER,
                                           tr("\xE6\x8E\xA7\xE5\x88\xB6\xE5\x99\xA8", "Controllers"),
                                           buf, tr("\xE5\x9C\xA8\xE7\xBA\xBF", "online"), true);
        lv_obj_set_width(c3, 0);
        lv_obj_set_flex_grow(c3, 1);
    }

    /* Weather data source */
    if (m->weather_valid) {
        lv_obj_t *c4 = ui_kit_metric_card(ctx->metrics_row, ICON_LOCATION,
                                           tr("\xE5\xB9\xBF\xE5\xB7\x9E\xE5\xA4\xA9\xE6\xB0\x94", "Weather"),
                                           tr("\xE5\xB7\xB2\xE5\x90\x8C\xE6\xAD\xA5", "Synced"), "", true);
        lv_obj_set_width(c4, 0);
        lv_obj_set_flex_grow(c4, 1);
    } else {
        lv_obj_t *c4 = ui_kit_metric_card(ctx->metrics_row, ICON_LOCATION,
                                           tr("\xE5\xB9\xBF\xE5\xB7\x9E\xE5\xA4\xA9\xE6\xB0\x94", "Weather"),
                                           tr("\xE5\x90\x8C\xE6\xAD\xA5\xE4\xB8\xAD", "Syncing"), "", true);
        lv_obj_set_width(c4, 0);
        lv_obj_set_flex_grow(c4, 1);
    }
}

static void build_floors(data_ctx_t *ctx)
{
    if (!ctx->floors_row) return;
    lv_obj_clean(ctx->floors_row);

    const mqtt_device_model_t *m = device_model_get();
    /* UTF-8: 一楼=E4B880, 二楼=E4BA8C, 三楼=E4B889 */
    const char *names_zh[] = { "\xE4\xB8\x80\xE6\xA5\xBC", "\xE4\xBA\x8C\xE6\xA5\xBC", "\xE4\xB8\x89\xE6\xA5\xBC" };
    const char *names_en[] = { "1F", "2F", "3F" };
    const char *assets[] = { "floor_1.png", "floor_2.png", "floor_3.png" };

    for (int i = 0; i < 3; i++) {
        uint16_t total = 0, online = 0;
        floor_stats((uint8_t)(i + 1), &total, &online);
        bool ctrl_online = m->controller_online[i];

        char status_buf[40];
        lv_snprintf(status_buf, sizeof(status_buf), "%u %s \xC2\xB7 %u %s",
                    (unsigned)total, tr("\xE8\xAE\xBE\xE5\xA4\x87", "dev"),
                    (unsigned)online, tr("\xE5\x9C\xA8\xE7\xBA\xBF", "on"));

        lv_color_t color = (total > 0 && online == total) ? UI_COLOR_GREEN :
                           (online > 0 ? UI_COLOR_ORANGE : UI_COLOR_TEXT_SEC);

        lv_obj_t *card = make_card(ctx->floors_row, 92);
        lv_obj_set_width(card, 0);
        lv_obj_set_flex_grow(card, 1);
        lv_obj_set_style_pad_column(card, 12, 0);
        add_asset_or_icon(card, assets[i], ICON_HOME, 92, 64, color);

        lv_obj_t *col = make_flex_col(card, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_flex_grow(col, 1);
        lv_obj_set_style_pad_row(col, 4, 0);
        ui_create_label(col, tr(names_zh[i], names_en[i]), ui_font_cn(18), UI_COLOR_TEXT_STRONG);
        ui_create_label(col, status_buf, ui_font_cn(13), ctrl_online ? color : UI_COLOR_TEXT_SEC);
    }
}

/* ---- Update ---- */

static void update_data(data_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;
    build_pills(ctx);
    build_visual(ctx);
    build_metrics(ctx);
    build_floors(ctx);
}

static void on_model_updated(void *user_data)
{
    update_data((data_ctx_t *)user_data);
}

/* ---- Lifecycle ---- */

static void on_delete(lv_event_t *e)
{
    data_ctx_t *ctx = (data_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    lv_free(ctx);
}

/* ---- Create ---- */

lv_obj_t *page_data_create(lv_obj_t *parent)
{
    data_ctx_t *ctx = lv_malloc(sizeof(data_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    ctx->page = ui_create_page(parent, on_delete, ctx);

    /* Header: title + pills */
    lv_obj_t *header = make_flex_row(ctx->page, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(header, 12, 0);

    lv_obj_t *title = ui_kit_page_title(header, ICON_HOME,
                                        tr("\xE6\x99\xBA\xE8\x83\xBD\xE5\xAE\xB6\xE5\xB1\x85\xE6\x80\xBB\xE8\xA7\x88", "Smart Home Overview"),
                                        tr("\xE5\xB9\xBF\xE5\xB7\x9E \xC2\xB7 ESP32-P4 \xE4\xB8\xAD\xE6\x8E\xA7", "Guangzhou ESP32-P4 Hub"));
    lv_obj_set_width(title, 0);
    lv_obj_set_flex_grow(title, 1);

    /* Pills column (dynamic) */
    ctx->pills_col = make_flex_col(header, LV_FLEX_ALIGN_END);
    lv_obj_set_width(ctx->pills_col, 520);
    lv_obj_set_style_pad_row(ctx->pills_col, 4, 0);

    /* Visual summary with TF card assets */
    ctx->visual_row = make_flex_row(ctx->page, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_obj_set_style_pad_column(ctx->visual_row, 10, 0);

    /* Metrics section */
    ui_kit_group_title(ctx->page, tr("\xE5\xAE\xB6\xE5\xBA\xAD\xE6\xA6\x82\xE8\xA7\x88", "Overview"));
    ctx->metrics_row = make_flex_row(ctx->page, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_obj_set_style_pad_column(ctx->metrics_row, 8, 0);

    /* Floors section */
    ui_kit_group_title(ctx->page, tr("\xE6\xA5\xBC\xE5\xB1\x82\xE8\xAE\xBE\xE5\xA4\x87", "Floors"));
    ctx->floors_row = make_flex_row(ctx->page, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_obj_set_style_pad_column(ctx->floors_row, 8, 0);

    /* Quick actions */
    ui_kit_group_title(ctx->page, tr("\xE5\xBF\xAB\xE6\x8D\xB7\xE6\x93\x8D\xE4\xBD\x9C", "Quick Actions"));
    lv_obj_t *actions = lv_obj_create(ctx->page);
    lv_obj_remove_style_all(actions);
    lv_obj_set_width(actions, lv_pct(100));
    lv_obj_set_height(actions, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(actions, 0, 0);
    lv_obj_set_layout(actions, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(actions, 8, 0);
    lv_obj_set_style_pad_row(actions, 8, 0);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);

    create_action_button(actions, ICON_LIGHTBULB, tr("\xE5\x85\xA8\xE9\x83\xA8\xE7\x81\xAF\xE5\x85\x89", "All Lights"), UI_COLOR_ORANGE, on_lights_toggle);
    create_action_button(actions, ICON_HOME, tr("\xE5\x9B\x9E\xE5\xAE\xB6\xE6\xA8\xA1\xE5\xBC\x8F", "Home"), UI_COLOR_ACCENT, on_scene_home);
    create_action_button(actions, ICON_LOCK, tr("\xE7\xA6\xBB\xE5\xAE\xB6\xE6\xA8\xA1\xE5\xBC\x8F", "Away"), UI_COLOR_PURPLE, on_scene_away);
    create_action_button(actions, ICON_SHIELD, tr("\xE5\x85\xA8\xE5\xB1\x8B\xE5\xAE\x89\xE9\x98\xB2", "Security"), UI_COLOR_GREEN, on_security);
    create_action_button(actions, ICON_UMBRELLA, tr("\xE9\x9B\xA8\xE5\xA4\xA9\xE6\x94\xB6\xE8\xA1\xA3", "Rain"), UI_COLOR_BLUE, on_scene_rain);

    /* Initial populate + subscribe */
    update_data(ctx);
    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);

    ESP_LOGI(TAG, "Data overview page created");
    return ctx->page;
}
