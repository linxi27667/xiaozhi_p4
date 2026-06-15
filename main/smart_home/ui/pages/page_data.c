#include "page_data.h"
#include "ui_events.h"
#include "ui_styles.h"
#include "ui_font.h"
#include "mqtt_device_model.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "ui_brand.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdint.h>
#include <string.h>
#include <time.h>

static const char *TAG = "PAGE_DATA";
static int32_t s_x_scale = 256;

static int sx(int value)
{
    return (int)(((int64_t)value * s_x_scale + 128) / 256);
}

typedef struct {
    lv_obj_t *page;
    int active_floor;
    lv_obj_t *lbl_online;
    lv_obj_t *lbl_sensor_online;
    lv_obj_t *lbl_scene;
    lv_obj_t *lbl_top_mqtt;
    lv_obj_t *dot_mqtt_card;
    lv_obj_t *lbl_mqtt_card_state;
    lv_obj_t *lbl_mqtt_rx;
    lv_obj_t *lbl_mqtt_seen;
    lv_obj_t *dot_ctrl[3];
    lv_obj_t *lbl_ctrl_state[3];
    lv_obj_t *lbl_temp;
    lv_obj_t *lbl_humi;
    lv_obj_t *lbl_pm;
    lv_obj_t *lbl_rain2;
    lv_obj_t *lbl_rain3;
    lv_obj_t *lbl_fire;
    lv_obj_t *lbl_window;
    lv_obj_t *lbl_f1_gate;
    lv_obj_t *lbl_f1_hall;
    lv_obj_t *lbl_f2_living;
    lv_obj_t *lbl_f2_toilet;
    lv_obj_t *lbl_f2_master;
    lv_obj_t *lbl_f2_rain;
    lv_obj_t *lbl_f2_hanger_type;
    lv_obj_t *lbl_f2_hanger;
    lv_obj_t *lbl_f3_balcony;
    lv_obj_t *lbl_f3_window_type;
    lv_obj_t *lbl_f3_window;
    lv_obj_t *lbl_f3_hanger_type;
    lv_obj_t *lbl_f3_hanger;
    lv_obj_t *lbl_f3_rain_type;
    lv_obj_t *lbl_f3_fire_type;
    lv_obj_t *lbl_f3_fire;
    lv_obj_t *card_f1_gate;
    lv_obj_t *card_f1_hall;
    lv_obj_t *card_f2_living;
    lv_obj_t *card_f2_toilet;
    lv_obj_t *card_f2_master;
    lv_obj_t *card_f2_hanger;
    lv_obj_t *card_f3_balcony;
    lv_obj_t *card_f3_window;
    lv_obj_t *card_f3_hanger;
    bool deleted;
} data_ctx_t;

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

static const rc_device_t *device_by_id(const char *id);
static bool rain_text(uint8_t floor_id, char *buf, size_t buf_size);
static bool flame_text(uint8_t floor_id, char *buf, size_t buf_size);

static lv_obj_t *panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, sx(x), y);
    lv_obj_set_size(obj, sx(w), h);
    lv_obj_set_style_bg_color(obj, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(obj, UI_COLOR_CARD_SOFT, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_grad_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, UI_COLOR_BORDER, 0);
    lv_obj_set_style_shadow_width(obj, 10, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(obj, UI_COLOR_SHADOW, 0);
    lv_obj_set_style_shadow_offset_y(obj, 3, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *cn_label(lv_obj_t *parent, const char *text, int size, lv_color_t color,
    int x, int y, int w)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text ? text : "");
    lv_obj_set_style_text_font(lbl, ui_font_cn((uint8_t)size), 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_pos(lbl, sx(x), y);
    if (w > 0) {
        lv_obj_set_width(lbl, sx(w));
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    }
    return lbl;
}

static void icon_text(lv_obj_t *parent, const char *icon, lv_color_t color,
    int x, int y, int icon_size)
{
    lv_obj_t *lbl = ui_create_icon(parent, icon, ui_font_icon((uint8_t)icon_size), color);
    lv_obj_set_pos(lbl, sx(x), y);
}

static lv_obj_t *status_dot(lv_obj_t *parent, int x, int y, lv_color_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_pos(dot, sx(x), y);
    lv_obj_set_size(dot, sx(7), 7);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return dot;
}

static void top_status(lv_obj_t *page, data_ctx_t *ctx)
{
    const mqtt_device_model_t *m = device_model_get();
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char tm_buf[16];
    lv_snprintf(tm_buf, sizeof(tm_buf), "%02d:%02d", t ? t->tm_hour : 9, t ? t->tm_min : 41);

    cn_label(page, tr("智慧家庭控制面板", "Smart Home Console"), 20,
        UI_COLOR_TEXT_STRONG, 28, 18, 206);
    ui_brand_create(page, 254, 10, 330, 38);
    bool mqtt_ok = (m->mqtt_state == MQTT_STATE_CONNECTED);
    ctx->lbl_top_mqtt = cn_label(page, mqtt_ok ? tr("MQTT：已连接", "MQTT: Online") :
        tr("MQTT：未连接", "MQTT: Offline"), 12,
        mqtt_ok ? UI_COLOR_TEXT : UI_COLOR_RED, 612, 21, 132);
    cn_label(page, tm_buf, 20, UI_COLOR_TEXT_STRONG, 772, 16, 72);
    icon_text(page, ICON_WIFI, UI_COLOR_TEXT_STRONG, 868, 17, 18);
}

static void controller_card(lv_obj_t *page, data_ctx_t *ctx, int idx, int x, const char *title, bool online)
{
    lv_obj_t *c = panel(page, x, 58, 210, 52);
    lv_obj_set_style_shadow_width(c, 0, 0);
    icon_text(c, ICON_SERVER, UI_COLOR_TEXT_SEC, 16, 10, 24);
    cn_label(c, title, 13, UI_COLOR_TEXT_STRONG, 56, 12, 124);
    lv_obj_t *dot = status_dot(c, 58, 36, online ? UI_COLOR_GREEN : UI_COLOR_RED);
    lv_obj_t *state = cn_label(c, online ? tr("在线", "Online") : tr("离线", "Offline"),
        12, online ? UI_COLOR_GREEN : UI_COLOR_RED, 70, 32, 60);
    if (idx >= 0 && idx < 3) {
        ctx->dot_ctrl[idx] = dot;
        ctx->lbl_ctrl_state[idx] = state;
    }
}

static void summary_card(lv_obj_t *page, data_ctx_t *ctx)
{
    lv_obj_t *c = panel(page, 708, 58, 238, 72);
    lv_obj_set_style_shadow_width(c, 0, 0);
    cn_label(c, tr("系统摘要", "Summary"), 13, UI_COLOR_TEXT_STRONG, 16, 9, 90);

    lv_obj_t *sep = lv_obj_create(c);
    lv_obj_remove_style_all(sep);
    lv_obj_set_pos(sep, 118, 34);
    lv_obj_set_size(sep, 1, 26);
    lv_obj_set_style_bg_color(sep, UI_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *l1 = cn_label(c, tr("在线设备", "Devices"), 10, UI_COLOR_TEXT_SEC, 14, 35, 96);
    lv_obj_t *l2 = cn_label(c, tr("在线传感器", "Sensors"), 10, UI_COLOR_TEXT_SEC, 128, 35, 96);
    ctx->lbl_online = cn_label(c, "0", 20, UI_COLOR_TEXT_STRONG, 14, 49, 96);
    ctx->lbl_sensor_online = cn_label(c, "0", 20, UI_COLOR_GREEN, 128, 49, 96);
    lv_obj_set_style_text_align(l1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(l2, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(ctx->lbl_online, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(ctx->lbl_sensor_online, LV_TEXT_ALIGN_CENTER, 0);
}

static void scene_card(lv_obj_t *page, data_ctx_t *ctx)
{
    const mqtt_device_model_t *m = device_model_get();
    lv_obj_t *c = panel(page, 708, 136, 238, 44);
    lv_obj_set_style_shadow_width(c, 0, 0);
    icon_text(c, ICON_STAR, UI_COLOR_ACCENT, 14, 12, 15);
    cn_label(c, tr("当前场景", "Scene"), 11, UI_COLOR_TEXT_SEC, 38, 8, 68);
    ctx->lbl_scene = cn_label(c, m->scene_name, 13, UI_COLOR_TEXT_STRONG, 112, 8, 100);
}

static void create_overview(data_ctx_t *ctx, lv_obj_t *page);
static void update_data(data_ctx_t *ctx);

static void on_floor_tab_clicked(lv_event_t *e)
{
    data_ctx_t *ctx = (data_ctx_t *)lv_event_get_user_data(e);
    int floor = (int)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (!ctx || !ctx->page || floor < 0 || floor > 3) return;
    if (ctx->active_floor == floor) return;
    ctx->active_floor = floor;
    lv_obj_clean(ctx->page);
    create_overview(ctx, ctx->page);
    update_data(ctx);
}

static void floor_tabs(data_ctx_t *ctx, lv_obj_t *page)
{
    const char *tabs_zh[] = { "总览", "一楼", "二楼", "三楼" };
    const char *tabs_en[] = { "All", "1F", "2F", "3F" };
    int xs[] = { 28, 116, 204, 292 };
    for (int i = 0; i < 4; i++) {
        bool active = ctx->active_floor == i;
        cn_label(page, ui_i18n_get_lang() == UI_LANG_ZH ? tabs_zh[i] : tabs_en[i],
            14, active ? UI_COLOR_ACCENT : UI_COLOR_TEXT_SEC, xs[i], 126, 54);
        lv_obj_t *hit = lv_obj_create(page);
        lv_obj_remove_style_all(hit);
        lv_obj_set_pos(hit, xs[i] - 12, 116);
        lv_obj_set_size(hit, 76, 34);
        lv_obj_set_user_data(hit, (void *)(uintptr_t)i);
        lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(hit, on_floor_tab_clicked, LV_EVENT_CLICKED, ctx);
    }
    lv_obj_t *line = lv_obj_create(page);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, xs[ctx->active_floor] - 2, 150);
    lv_obj_set_size(line, 54, 3);
    lv_obj_set_style_bg_color(line, UI_COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(line, 2, 0);
}

static lv_obj_t *device_tile(lv_obj_t *parent, int x, int y, int w, int h,
    const char *icon, lv_color_t accent, const char *name, const char *type,
    const char *value, bool active, lv_obj_t **type_out, lv_obj_t **value_out)
{
    lv_obj_t *c = panel(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(c, 0, 0);
    if (active) {
        lv_obj_set_style_bg_color(c, lv_color_mix(accent, UI_COLOR_CARD, 12), 0);
        lv_obj_set_style_border_color(c, lv_color_mix(accent, UI_COLOR_BORDER, 42), 0);
    }
    icon_text(c, icon, active ? accent : UI_COLOR_TEXT_SEC, 14, h > 56 ? 17 : 12, h > 56 ? 24 : 18);
    cn_label(c, name, w < 120 ? 11 : 13, UI_COLOR_TEXT_STRONG, h > 56 ? 44 : 42, 11, w - 52);
    lv_obj_t *type_lbl = cn_label(c, type, 10, UI_COLOR_TEXT_SEC, h > 56 ? 44 : 42, h > 56 ? 34 : 24, w - 52);
    lv_obj_t *value_lbl = cn_label(c, value, h > 56 ? 11 : 10, active ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC,
        h > 56 ? 44 : w - 58, h > 56 ? 57 : 24, h > 56 ? w - 52 : 52);
    if (type_out) *type_out = type_lbl;
    if (value_out) *value_out = value_lbl;
    return c;
}

static void metric(lv_obj_t *parent, int x, const char *icon, lv_color_t accent,
    const char *value, const char *name, lv_obj_t **out)
{
    lv_obj_t *c = panel(parent, x, 16, 66, 50);
    lv_obj_set_style_shadow_width(c, 0, 0);
    icon_text(c, icon, accent, 8, 9, 14);
    lv_obj_t *v = cn_label(c, value, 12, UI_COLOR_TEXT_STRONG, 26, 8, 38);
    cn_label(c, name, 9, UI_COLOR_TEXT_SEC, 10, 30, 52);
    if (out) *out = v;
}

static bool rain_text(uint8_t floor_id, char *buf, size_t buf_size)
{
    uint16_t value = 0;
    if (!device_model_get_rain_value(floor_id, &value)) {
        lv_snprintf(buf, buf_size, "%s", tr("—", "—"));
        return false;
    }
    lv_snprintf(buf, buf_size, "%u%%", (unsigned)value);
    return true;
}

static bool flame_text(uint8_t floor_id, char *buf, size_t buf_size)
{
    uint8_t status = 0;
    if (!device_model_get_fire_status(floor_id, &status)) {
        lv_snprintf(buf, buf_size, "%s", tr("—", "—"));
        return false;
    }
    lv_snprintf(buf, buf_size, "%s", status >= 2 ? tr("告警", "Alert") : tr("正常", "OK"));
    return true;
}

static void __attribute__((unused)) scene_chip(lv_obj_t *parent, int x, const char *icon, const char *name,
    const char *state, bool active)
{
    lv_obj_t *c = panel(parent, x, 34, 86, 42);
    lv_obj_set_style_shadow_width(c, 0, 0);
    lv_obj_set_style_bg_color(c, active ? UI_COLOR_GREEN_SOFT : UI_COLOR_CARD_SOFT, 0);
    icon_text(c, icon, active ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC, 10, 10, 16);
    cn_label(c, name, 10, UI_COLOR_TEXT_STRONG, 32, 8, 50);
    cn_label(c, state, 9, active ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC, 32, 25, 50);
}

static void mqtt_card(lv_obj_t *page, data_ctx_t *ctx)
{
    const mqtt_device_model_t *m = device_model_get();
    bool mqtt_ok = (m->mqtt_state == MQTT_STATE_CONNECTED);
    lv_obj_t *c = panel(page, 644, 454, 302, 96);
    icon_text(c, ICON_PLUG, UI_COLOR_GREEN, 14, 14, 16);
    cn_label(c, tr("MQTT 状态", "MQTT Status"), 13, UI_COLOR_TEXT_STRONG, 40, 12, 90);
    cn_label(c, m->mqtt_broker, 10, UI_COLOR_TEXT_SEC, 136, 14, 150);
    ctx->dot_mqtt_card = status_dot(c, 42, 39, mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED);
    ctx->lbl_mqtt_card_state = cn_label(c, mqtt_ok ? tr("已连接", "Connected") : tr("未连接", "Offline"),
        11, mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED, 56, 35, 70);
    cn_label(c, tr("消息", "RX"), 10, UI_COLOR_TEXT_SEC, 136, 35, 40);
    ctx->lbl_mqtt_rx = cn_label(c, "0", 12, UI_COLOR_TEXT_STRONG, 176, 34, 50);
    cn_label(c, tr("延迟", "Seen"), 10, UI_COLOR_TEXT_SEC, 232, 35, 40);
    ctx->lbl_mqtt_seen = cn_label(c, "0s", 12, UI_COLOR_TEXT_STRONG, 266, 34, 32);
}

static void create_overview(data_ctx_t *ctx, lv_obj_t *page)
{
    const rc_device_t *d_gate = device_by_id("floor1_gate");
    const rc_device_t *d_hall = device_by_id("floor1_hall_light");
    const rc_device_t *d_living = device_by_id("floor2_living_light");
    const rc_device_t *d_toilet = device_by_id("floor2_toilet_light");
    const rc_device_t *d_master = device_by_id("floor2_master_light");
    const rc_device_t *d_hanger2 = device_by_id("floor2_hanger");
    const rc_device_t *d_balcony = device_by_id("floor3_balcony_light");
    const rc_device_t *d_window = device_by_id("floor3_left_skylight");
    const rc_device_t *d_hanger3 = device_by_id("floor3_hanger");
    const mqtt_device_model_t *m = device_model_get();

    top_status(page, ctx);
    controller_card(page, ctx, 0, 28, tr("一楼控制器", "1F Controller"), m->controller_online[0]);
    controller_card(page, ctx, 1, 248, tr("二楼控制器", "2F Controller"), m->controller_online[1]);
    controller_card(page, ctx, 2, 468, tr("三楼控制器", "3F Controller"), m->controller_online[2]);
    summary_card(page, ctx);
    scene_card(page, ctx);
    floor_tabs(ctx, page);

    /* Floor 1 */
    lv_obj_t *f1 = panel(page, 24, 160, 250, 270);
    cn_label(f1, tr("一楼设备", "1F Devices"), 14, UI_COLOR_TEXT_STRONG, 16, 12, 120);
    bool gate_on = d_gate && d_gate->connected && d_gate->power_on;
    ctx->card_f1_gate = device_tile(f1, 12, 42, 206, 82, ICON_HOME, UI_COLOR_TEXT_STRONG,
        tr("大门", "Gate"), tr("门磁", "Door"),
        d_gate && d_gate->connected ? (gate_on ? tr("已打开", "Open") : tr("已关闭", "Closed")) : tr("未接入", "N/A"),
        gate_on, NULL, &ctx->lbl_f1_gate);
    bool hall_on = d_hall && d_hall->connected && d_hall->power_on;
    ctx->card_f1_hall = device_tile(f1, 12, 138, 206, 82, ICON_LIGHTBULB, UI_COLOR_BLUE,
        tr("大厅灯", "Hall Light"), tr("吸顶灯", "Light"),
        d_hall && d_hall->connected ? (hall_on ? tr("已开启", "On") : tr("已关闭", "Off")) : tr("未接入", "N/A"),
        hall_on, NULL, &ctx->lbl_f1_hall);
    if (ctx->active_floor != 0 && ctx->active_floor != 1) lv_obj_set_style_opa(f1, LV_OPA_40, 0);

    /* Floor 2 */
    lv_obj_t *f2 = panel(page, 286, 160, 342, 270);
    cn_label(f2, tr("二楼设备", "2F Devices"), 14, UI_COLOR_TEXT_STRONG, 16, 12, 120);
    bool living_on = d_living && d_living->connected && d_living->power_on;
    ctx->card_f2_living = device_tile(f2, 12, 42, 100, 82, ICON_LIGHTBULB, UI_COLOR_ORANGE,
        tr("客厅灯", "Living"), tr("吸顶灯", "Light"),
        d_living && d_living->connected ? (living_on ? tr("已开启", "On") : tr("已关闭", "Off")) : tr("未接入", "N/A"),
        living_on, NULL, &ctx->lbl_f2_living);
    bool toilet_on = d_toilet && d_toilet->connected && d_toilet->power_on;
    ctx->card_f2_toilet = device_tile(f2, 122, 42, 100, 82, ICON_LIGHTBULB, UI_COLOR_TEXT_SEC,
        tr("厕所灯", "Toilet"), tr("吸顶灯", "Light"),
        d_toilet && d_toilet->connected ? (toilet_on ? tr("已开启", "On") : tr("已关闭", "Off")) : tr("未接入", "N/A"),
        toilet_on, NULL, &ctx->lbl_f2_toilet);
    bool master_on = d_master && d_master->connected && d_master->power_on;
    ctx->card_f2_master = device_tile(f2, 232, 42, 98, 82, ICON_LIGHTBULB, UI_COLOR_ORANGE,
        tr("主卧灯", "Master"), tr("氛围灯", "RGB"),
        d_master && d_master->connected ? (master_on ? d_master->value_text : tr("已关闭", "Off")) : tr("未接入", "N/A"),
        master_on, NULL, &ctx->lbl_f2_master);
    device_tile(f2, 12, 142, 116, 82, ICON_DROP, UI_COLOR_BLUE,
        tr("雨滴传感器", "Rain"), tr("雨滴值", "Value"), tr("未接入", "N/A"), false, NULL, &ctx->lbl_f2_rain);
    bool hanger2_on = d_hanger2 && d_hanger2->connected && d_hanger2->power_on;
    ctx->card_f2_hanger = device_tile(f2, 140, 142, 190, 82, ICON_HANGER, UI_COLOR_ACCENT,
        tr("二楼晾衣杆", "2F Rack"),
        d_hanger2 && d_hanger2->connected ? (hanger2_on ? tr("状态：已伸出", "Extended") : tr("状态：已收起", "Retracted")) : tr("未接入", "N/A"),
        d_hanger2 && d_hanger2->connected ? (hanger2_on ? tr("自动模式", "Auto") : tr("已关闭", "Off")) : tr("未接入", "N/A"),
        hanger2_on, &ctx->lbl_f2_hanger_type, &ctx->lbl_f2_hanger);
    if (ctx->active_floor != 0 && ctx->active_floor != 2) lv_obj_set_style_opa(f2, LV_OPA_40, 0);

    /* Floor 3 */
    lv_obj_t *f3 = panel(page, 640, 160, 306, 270);
    cn_label(f3, tr("三楼设备", "3F Devices"), 14, UI_COLOR_TEXT_STRONG, 16, 12, 120);
    bool balcony_on = d_balcony && d_balcony->connected && d_balcony->power_on;
    ctx->card_f3_balcony = device_tile(f3, 12, 42, 136, 76, ICON_LIGHTBULB, UI_COLOR_ORANGE,
        tr("阳台灯", "Balcony"), tr("吸顶灯", "Light"),
        d_balcony && d_balcony->connected ? (balcony_on ? tr("已开启", "On") : tr("已关闭", "Off")) : tr("未接入", "N/A"),
        balcony_on, NULL, &ctx->lbl_f3_balcony);
    bool window_on = d_window && d_window->connected && d_window->power_on;
    ctx->card_f3_window = device_tile(f3, 158, 42, 136, 76, ICON_WINDOW, UI_COLOR_TEXT_SEC,
        tr("天窗", "Skylight"),
        d_window && d_window->connected ? (window_on ? tr("开度：70%", "Open 70%") : tr("开度：0%", "Open 0%")) : tr("未接入", "N/A"),
        d_window && d_window->connected ? (window_on ? tr("已开启", "On") : tr("已关闭", "Closed")) : tr("未接入", "N/A"),
        window_on, &ctx->lbl_f3_window_type, &ctx->lbl_f3_window);
    bool hanger3_on = d_hanger3 && d_hanger3->connected && d_hanger3->power_on;
    ctx->card_f3_hanger = device_tile(f3, 12, 130, 136, 76, ICON_HANGER, UI_COLOR_ACCENT,
        tr("三楼晾衣杆", "3F Rack"),
        d_hanger3 && d_hanger3->connected ? (hanger3_on ? tr("状态：已伸出", "Extended") : tr("状态：已收起", "Retracted")) : tr("未接入", "N/A"),
        d_hanger3 && d_hanger3->connected ? (hanger3_on ? tr("自动模式", "Auto") : tr("已关闭", "Off")) : tr("未接入", "N/A"),
        hanger3_on, &ctx->lbl_f3_hanger_type, &ctx->lbl_f3_hanger);
    char buf[32];
    bool rain3_valid = device_model_get_rain_value(3, NULL);
    if (rain3_valid) {
        uint16_t value = 0;
        device_model_get_rain_value(3, &value);
        lv_snprintf(buf, sizeof(buf), "%s%u%%", tr("雨滴值：", "Value "), (unsigned)value);
    } else {
        lv_snprintf(buf, sizeof(buf), "%s", tr("未接入", "N/A"));
    }
    device_tile(f3, 158, 130, 136, 76, ICON_DROP, UI_COLOR_BLUE,
        tr("雨滴传感器", "Rain"), buf,
        rain3_valid ? tr("已上报", "Reported") : tr("未接入", "N/A"), false,
        &ctx->lbl_f3_rain_type, NULL);
    uint8_t flame3 = 0;
    bool flame3_valid = device_model_get_fire_status(3, &flame3);
    if (flame3_valid) lv_snprintf(buf, sizeof(buf), "%s", flame3 >= 2 ? tr("已触发", "Active") : tr("未触发", "Clear"));
    else lv_snprintf(buf, sizeof(buf), "%s", tr("未接入", "N/A"));
    device_tile(f3, 12, 216, 282, 42, ICON_FIRE, UI_COLOR_RED,
        tr("火灾传感器", "Fire Sensor"), buf,
        flame3_valid ? (flame3 >= 2 ? tr("告警", "Alert") : tr("正常", "OK")) : tr("未接入", "N/A"),
        flame3_valid && flame3 >= 2,
        &ctx->lbl_f3_fire_type, &ctx->lbl_f3_fire);
    if (ctx->active_floor != 0 && ctx->active_floor != 3) lv_obj_set_style_opa(f3, LV_OPA_40, 0);

    /* Environment metrics */
    cn_label(page, tr("环境与系统实时数据", "Environment & System"), 13,
        UI_COLOR_TEXT_STRONG, 24, 454, 190);
    lv_obj_t *metrics = panel(page, 24, 476, 604, 74);
    lv_obj_set_style_shadow_width(metrics, 0, 0);
    if (m->temperature_valid) lv_snprintf(buf, sizeof(buf), "%.1f℃", (double)m->temperature);
    else lv_snprintf(buf, sizeof(buf), "%s", tr("—", "—"));
    metric(metrics, 10, ICON_TEMP, UI_COLOR_ACCENT, buf, tr("温度", "Temp"), &ctx->lbl_temp);
    if (m->humidity_valid) lv_snprintf(buf, sizeof(buf), "%.0f%%", (double)m->humidity);
    else lv_snprintf(buf, sizeof(buf), "%s", tr("—", "—"));
    metric(metrics, 82, ICON_DROP, UI_COLOR_BLUE, buf, tr("湿度", "Humi"), &ctx->lbl_humi);
    if (m->pm25_valid) lv_snprintf(buf, sizeof(buf), "%u", (unsigned)m->pm25);
    else lv_snprintf(buf, sizeof(buf), "%s", tr("—", "—"));
    metric(metrics, 154, ICON_LEAF, UI_COLOR_GREEN, buf, tr("PM2.5", "PM2.5"), &ctx->lbl_pm);
    rain_text(2, buf, sizeof(buf));
    metric(metrics, 226, ICON_DROP, UI_COLOR_BLUE, buf, tr("二楼雨滴", "2F Rain"), &ctx->lbl_rain2);
    rain_text(3, buf, sizeof(buf));
    metric(metrics, 298, ICON_DROP, UI_COLOR_BLUE, buf, tr("三楼雨滴", "3F Rain"), &ctx->lbl_rain3);
    flame_text(3, buf, sizeof(buf));
    metric(metrics, 370, ICON_FIRE, UI_COLOR_RED, buf, tr("火警", "Fire"), &ctx->lbl_fire);
    metric(metrics, 442, ICON_WINDOW, UI_COLOR_BLUE, tr("—", "—"), tr("天窗", "Window"), &ctx->lbl_window);
    metric(metrics, 514, ICON_BOLT, UI_COLOR_TEXT_SEC, "—", tr("功率", "Power"), NULL);

    /* MQTT status card */
    mqtt_card(page, ctx);
}

static const rc_device_t *device_by_id(const char *id)
{
    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t *d = device_model_at(i);
        if (d && strcmp(d->id, id) == 0) return d;
    }
    return NULL;
}

static void set_state_label(lv_obj_t *lbl, bool connected, bool on)
{
    if (!lbl) return;
    if (!connected) {
        lv_label_set_text(lbl, tr("未接入", "N/A"));
        lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_SEC, 0);
        return;
    }
    lv_label_set_text(lbl, on ? tr("已开启", "On") : tr("已关闭", "Off"));
    lv_obj_set_style_text_color(lbl, on ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC, 0);
}

static void set_card_state(lv_obj_t *card, lv_color_t accent, bool connected, bool on)
{
    if (!card) return;
    if (!connected) {
        lv_obj_set_style_bg_color(card, UI_COLOR_CARD, 0);
        lv_obj_set_style_border_color(card, UI_COLOR_BORDER, 0);
        return;
    }
    lv_obj_set_style_bg_color(card, on ? lv_color_mix(accent, UI_COLOR_CARD, 12) : UI_COLOR_CARD, 0);
    lv_obj_set_style_border_color(card, on ? lv_color_mix(accent, UI_COLOR_BORDER, 42) : UI_COLOR_BORDER, 0);
}

static void set_text_color(lv_obj_t *lbl, const char *text, lv_color_t color)
{
    if (!lbl) return;
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
}

static void update_data(data_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;
    const mqtt_device_model_t *m = device_model_get();
    char buf[32];
    bool mqtt_ok = (m->mqtt_state == MQTT_STATE_CONNECTED);

    if (ctx->lbl_top_mqtt) {
        lv_label_set_text(ctx->lbl_top_mqtt, mqtt_ok ? tr("MQTT：已连接", "MQTT: Online") :
            tr("MQTT：未连接", "MQTT: Offline"));
        lv_obj_set_style_text_color(ctx->lbl_top_mqtt, mqtt_ok ? UI_COLOR_TEXT : UI_COLOR_RED, 0);
    }
    if (ctx->dot_mqtt_card) {
        lv_obj_set_style_bg_color(ctx->dot_mqtt_card, mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED, 0);
    }
    if (ctx->lbl_mqtt_card_state) {
        lv_label_set_text(ctx->lbl_mqtt_card_state, mqtt_ok ? tr("已连接", "Connected") :
            tr("未连接", "Offline"));
        lv_obj_set_style_text_color(ctx->lbl_mqtt_card_state, mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED, 0);
    }
    for (int i = 0; i < 3; i++) {
        bool online = m->controller_online[i];
        if (ctx->dot_ctrl[i]) {
            lv_obj_set_style_bg_color(ctx->dot_ctrl[i], online ? UI_COLOR_GREEN : UI_COLOR_RED, 0);
        }
        if (ctx->lbl_ctrl_state[i]) {
            lv_label_set_text(ctx->lbl_ctrl_state[i], online ? tr("在线", "Online") :
                tr("离线", "Offline"));
            lv_obj_set_style_text_color(ctx->lbl_ctrl_state[i], online ? UI_COLOR_GREEN : UI_COLOR_RED, 0);
        }
    }

    if (ctx->lbl_online) {
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)device_model_connected_count());
        lv_label_set_text(ctx->lbl_online, buf);
    }
    if (ctx->lbl_sensor_online) {
        uint16_t sensors = device_model_sensor_online_count();
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)sensors);
        lv_label_set_text(ctx->lbl_sensor_online, buf);
    }
    if (ctx->lbl_scene) {
        lv_label_set_text(ctx->lbl_scene, m->scene_name);
        lv_obj_set_style_text_color(ctx->lbl_scene,
            m->current_scene == IOT_SCENE_NONE ? UI_COLOR_TEXT_SEC : UI_COLOR_ACCENT, 0);
    }
    if (ctx->lbl_mqtt_rx) {
        lv_snprintf(buf, sizeof(buf), "%lu", (unsigned long)m->mqtt_rx_count);
        lv_label_set_text(ctx->lbl_mqtt_rx, buf);
    }
    if (ctx->lbl_mqtt_seen) {
        if (m->mqtt_rx_count > 0) {
            lv_snprintf(buf, sizeof(buf), "%lus", (unsigned long)m->mqtt_last_seen_sec);
        } else {
            lv_snprintf(buf, sizeof(buf), "%s", tr("—", "—"));
        }
        lv_label_set_text(ctx->lbl_mqtt_seen, buf);
    }
    if (ctx->lbl_temp) {
        if (m->temperature_valid) lv_snprintf(buf, sizeof(buf), "%.1f℃", (double)m->temperature);
        else lv_snprintf(buf, sizeof(buf), "%s", tr("—", "—"));
        set_text_color(ctx->lbl_temp, buf, m->temperature_valid ? UI_COLOR_TEXT_STRONG : UI_COLOR_TEXT_SEC);
    }
    if (ctx->lbl_humi) {
        if (m->humidity_valid) lv_snprintf(buf, sizeof(buf), "%.0f%%", (double)m->humidity);
        else lv_snprintf(buf, sizeof(buf), "%s", tr("—", "—"));
        set_text_color(ctx->lbl_humi, buf, m->humidity_valid ? UI_COLOR_TEXT_STRONG : UI_COLOR_TEXT_SEC);
    }
    if (ctx->lbl_pm) {
        if (m->pm25_valid) lv_snprintf(buf, sizeof(buf), "%u", (unsigned)m->pm25);
        else lv_snprintf(buf, sizeof(buf), "%s", tr("—", "—"));
        set_text_color(ctx->lbl_pm, buf, m->pm25_valid ? UI_COLOR_TEXT_STRONG : UI_COLOR_TEXT_SEC);
    }
    if (ctx->lbl_rain2) {
        bool valid = rain_text(2, buf, sizeof(buf));
        set_text_color(ctx->lbl_rain2, buf, valid ? UI_COLOR_TEXT_STRONG : UI_COLOR_TEXT_SEC);
    }
    if (ctx->lbl_rain3) {
        bool valid = rain_text(3, buf, sizeof(buf));
        set_text_color(ctx->lbl_rain3, buf, valid ? UI_COLOR_TEXT_STRONG : UI_COLOR_TEXT_SEC);
    }
    if (ctx->lbl_fire) {
        bool valid = flame_text(3, buf, sizeof(buf));
        set_text_color(ctx->lbl_fire, buf, valid ? UI_COLOR_TEXT_STRONG : UI_COLOR_TEXT_SEC);
    }

    const rc_device_t *d = device_by_id("floor1_gate");
    if (d) {
        set_state_label(ctx->lbl_f1_gate, d->connected, d->power_on);
        set_card_state(ctx->card_f1_gate, UI_COLOR_TEXT_STRONG, d->connected, d->power_on);
    }
    d = device_by_id("floor1_hall_light");
    if (d) {
        set_state_label(ctx->lbl_f1_hall, d->connected, d->power_on);
        set_card_state(ctx->card_f1_hall, UI_COLOR_BLUE, d->connected, d->power_on);
    }
    d = device_by_id("floor2_living_light");
    if (d) {
        set_state_label(ctx->lbl_f2_living, d->connected, d->power_on);
        set_card_state(ctx->card_f2_living, UI_COLOR_ORANGE, d->connected, d->power_on);
    }
    d = device_by_id("floor2_toilet_light");
    if (d) {
        set_state_label(ctx->lbl_f2_toilet, d->connected, d->power_on);
        set_card_state(ctx->card_f2_toilet, UI_COLOR_ORANGE, d->connected, d->power_on);
    }
    d = device_by_id("floor2_master_light");
    if (d) {
        if (ctx->lbl_f2_master) {
            lv_label_set_text(ctx->lbl_f2_master, d->connected ?
                (d->power_on ? d->value_text : tr("已关闭", "Off")) : tr("未接入", "N/A"));
            lv_obj_set_style_text_color(ctx->lbl_f2_master,
                d->connected && d->power_on ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC, 0);
        }
        set_card_state(ctx->card_f2_master, UI_COLOR_ORANGE, d->connected, d->power_on);
    }
    d = device_by_id("floor3_balcony_light");
    if (d) {
        set_state_label(ctx->lbl_f3_balcony, d->connected, d->power_on);
        set_card_state(ctx->card_f3_balcony, UI_COLOR_ORANGE, d->connected, d->power_on);
    }

    bool rain2_valid = rain_text(2, buf, sizeof(buf));
    set_text_color(ctx->lbl_f2_rain, buf, rain2_valid ? UI_COLOR_TEXT_STRONG : UI_COLOR_TEXT_SEC);
    uint16_t rain3 = 0;
    if (device_model_get_rain_value(3, &rain3)) {
        lv_snprintf(buf, sizeof(buf), "%s%u%%", tr("雨滴值：", "Value "), (unsigned)rain3);
        set_text_color(ctx->lbl_f3_rain_type, buf, UI_COLOR_TEXT_SEC);
    } else {
        set_text_color(ctx->lbl_f3_rain_type, tr("未接入", "N/A"), UI_COLOR_TEXT_SEC);
    }

    d = device_by_id("floor2_hanger");
    if (d) {
        set_card_state(ctx->card_f2_hanger, UI_COLOR_ACCENT, d->connected, d->power_on);
        if (d->connected) {
            set_text_color(ctx->lbl_f2_hanger_type, d->power_on ? tr("状态：已伸出", "Extended") :
                tr("状态：已收起", "Retracted"), UI_COLOR_TEXT_SEC);
            set_text_color(ctx->lbl_f2_hanger, d->power_on ? tr("自动模式", "Auto") :
                tr("已关闭", "Off"), d->power_on ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC);
        } else {
            set_text_color(ctx->lbl_f2_hanger_type, tr("未接入", "N/A"), UI_COLOR_TEXT_SEC);
            set_text_color(ctx->lbl_f2_hanger, tr("未接入", "N/A"), UI_COLOR_TEXT_SEC);
        }
    }
    d = device_by_id("floor3_left_skylight");
    if (d) {
        set_card_state(ctx->card_f3_window, UI_COLOR_BLUE, d->connected, d->power_on);
        if (d->connected) {
            lv_snprintf(buf, sizeof(buf), "%s%u%%", tr("开度：", "Open "), d->power_on ? 70u : 0u);
            set_text_color(ctx->lbl_f3_window_type, buf, UI_COLOR_TEXT_SEC);
            set_text_color(ctx->lbl_f3_window, d->power_on ? tr("已开启", "On") :
                tr("已关闭", "Closed"), d->power_on ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC);
        } else {
            set_text_color(ctx->lbl_f3_window_type, tr("未接入", "N/A"), UI_COLOR_TEXT_SEC);
            set_text_color(ctx->lbl_f3_window, tr("未接入", "N/A"), UI_COLOR_TEXT_SEC);
        }
    }
    d = device_by_id("floor3_hanger");
    if (d) {
        set_card_state(ctx->card_f3_hanger, UI_COLOR_ACCENT, d->connected, d->power_on);
        if (d->connected) {
            set_text_color(ctx->lbl_f3_hanger_type, d->power_on ? tr("状态：已伸出", "Extended") :
                tr("状态：已收起", "Retracted"), UI_COLOR_TEXT_SEC);
            set_text_color(ctx->lbl_f3_hanger, d->power_on ? tr("自动模式", "Auto") :
                tr("已关闭", "Off"), d->power_on ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC);
        } else {
            set_text_color(ctx->lbl_f3_hanger_type, tr("未接入", "N/A"), UI_COLOR_TEXT_SEC);
            set_text_color(ctx->lbl_f3_hanger, tr("未接入", "N/A"), UI_COLOR_TEXT_SEC);
        }
    }
    uint8_t flame3_value = 0;
    if (device_model_get_fire_status(3, &flame3_value)) {
        lv_snprintf(buf, sizeof(buf), "%s", flame3_value >= 2 ? tr("已触发", "Active") : tr("未触发", "Clear"));
        set_text_color(ctx->lbl_f3_fire_type, buf, UI_COLOR_TEXT_SEC);
        set_text_color(ctx->lbl_f3_fire, flame3_value >= 2 ? tr("告警", "Alert") :
            tr("正常", "OK"), flame3_value >= 2 ? UI_COLOR_RED : UI_COLOR_GREEN);
    } else {
        set_text_color(ctx->lbl_f3_fire_type, tr("未接入", "N/A"), UI_COLOR_TEXT_SEC);
        set_text_color(ctx->lbl_f3_fire, tr("未接入", "N/A"), UI_COLOR_TEXT_SEC);
    }
}

static void on_model_updated(void *user_data)
{
    update_data((data_ctx_t *)user_data);
}

static void page_data_delete(lv_event_t *e)
{
    data_ctx_t *ctx = (data_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    lv_free(ctx);
}

lv_obj_t *page_data_create(lv_obj_t *parent)
{
    data_ctx_t *ctx = lv_malloc(sizeof(data_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(page, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(page, page_data_delete, LV_EVENT_DELETE, ctx);
    ctx->page = page;
    ctx->active_floor = 0;
    int32_t parent_w = lv_obj_get_width(parent);
    if (parent_w <= 1) parent_w = LV_HOR_RES;
    s_x_scale = parent_w < 960 ? (int32_t)((int64_t)parent_w * 256 / 960) : 256;

    create_overview(ctx, page);
    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    update_data(ctx);

    ESP_LOGI(TAG, "Data overview page created");
    return page;
}
