#include "page_net.h"
#include "ui_events.h"
#include "ui_styles.h"
#include "ui_font.h"
#include "mqtt_device_model.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "../../services/wifi_manager.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>
#include <time.h>

static const char *TAG = "PAGE_NET";

typedef struct {
    lv_obj_t *wifi_status;
    lv_obj_t *disconnect_btn;
    lv_obj_t *mqtt_dot;
    lv_obj_t *mqtt_state;
    lv_obj_t *mqtt_rx;
    lv_obj_t *wifi_list;
    lv_obj_t *modal;
    lv_obj_t *ssid_ta;
    lv_obj_t *pwd_ta;
    lv_obj_t *keyboard;
    lv_obj_t *active_ta;
    int last_wifi_count;
    wifi_state_t last_wifi_state;
    char last_wifi_ssid[WIFI_SSID_MAX];
    bool deleted;
} net_ctx_t;

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

static lv_obj_t *status_dot(lv_obj_t *parent, int x, int y, lv_color_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_pos(dot, x, y);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return dot;
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

static lv_obj_t *text_button(lv_obj_t *parent, int x, int y, int w, const char *text,
    lv_color_t bg, lv_color_t fg, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, 36);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_mix(bg, UI_COLOR_BORDER, 70), 0);
    ui_apply_press_feedback(btn, bg);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *lbl = cn_label(btn, text, 13, fg, 0, 9, w);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    return btn;
}

static void set_button_hidden(lv_obj_t *btn, bool hidden)
{
    if (!btn) return;
    if (hidden) lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);
}

static void close_wifi_modal(net_ctx_t *ctx)
{
    if (!ctx) return;
    if (ctx->modal) {
        lv_obj_delete(ctx->modal);
        ctx->modal = NULL;
    }
    ctx->ssid_ta = NULL;
    ctx->pwd_ta = NULL;
    ctx->keyboard = NULL;
    ctx->active_ta = NULL;
}

static void on_modal_cancel(lv_event_t *e)
{
    close_wifi_modal((net_ctx_t *)lv_event_get_user_data(e));
}

static void on_wifi_connect(lv_event_t *e)
{
    net_ctx_t *ctx = (net_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    wifi_manager_connect(
        ctx->ssid_ta ? lv_textarea_get_text(ctx->ssid_ta) : "",
        ctx->pwd_ta ? lv_textarea_get_text(ctx->pwd_ta) : "");
    close_wifi_modal(ctx);
}

static void on_wifi_disconnect(lv_event_t *e)
{
    (void)e;
    wifi_manager_disconnect();
}

static void on_textarea_focus(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    net_ctx_t *ctx = (net_ctx_t *)lv_event_get_user_data(e);
    if (ctx) ctx->active_ta = ta;
}

static void on_matrix_key(lv_event_t *e)
{
    net_ctx_t *ctx = (net_ctx_t *)lv_event_get_user_data(e);
    lv_obj_t *kb = lv_event_get_target(e);
    if (!ctx || !ctx->active_ta) return;
    uint32_t id = lv_buttonmatrix_get_selected_button(kb);
    const char *txt = lv_buttonmatrix_get_button_text(kb, id);
    if (!txt) return;
    if (strcmp(txt, "Bksp") == 0) {
        lv_textarea_delete_char(ctx->active_ta);
    } else if (strcmp(txt, "Space") == 0) {
        lv_textarea_add_char(ctx->active_ta, ' ');
    } else if (strcmp(txt, "Clear") == 0) {
        lv_textarea_set_text(ctx->active_ta, "");
    } else {
        lv_textarea_add_text(ctx->active_ta, txt);
    }
}

static lv_obj_t *matrix_keyboard(net_ctx_t *ctx, lv_obj_t *parent, int x, int y)
{
    static const char *keys[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
        "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
        "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
        "z", "x", "c", "v", "b", "n", "m", ".", "-", "\n",
        "Clear", "Bksp", "Space", ""
    };

    lv_obj_t *kb = lv_buttonmatrix_create(parent);
    lv_buttonmatrix_set_map(kb, keys);
    lv_obj_set_pos(kb, x, y);
    lv_obj_set_size(kb, 648, 220);
    lv_obj_set_style_text_font(kb, ui_font_cn(15), 0);
    lv_obj_set_style_bg_color(kb, UI_COLOR_CARD_SOFT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(kb, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(kb, UI_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_radius(kb, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(kb, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(kb, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(kb, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(kb, UI_COLOR_INPUT_BG, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, UI_COLOR_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 8, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, UI_COLOR_TEXT_STRONG, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, UI_COLOR_ACCENT, (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(kb, on_matrix_key, LV_EVENT_VALUE_CHANGED, ctx);
    return kb;
}

static void open_wifi_modal(net_ctx_t *ctx, const char *ssid)
{
    if (!ctx || !ssid) return;
    close_wifi_modal(ctx);

    ctx->modal = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(ctx->modal);
    lv_obj_set_size(ctx->modal, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(ctx->modal, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_color(ctx->modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->modal, LV_OPA_30, 0);
    lv_obj_clear_flag(ctx->modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = panel(ctx->modal, 160, 40, 704, 520, 14);
    icon_text(box, ICON_WIFI, UI_COLOR_ACCENT, 28, 24, 20);
    cn_label(box, tr("连接网络", "Connect Network"), 20, UI_COLOR_TEXT_STRONG, 62, 20, 150);
    cn_label(box, tr("网络名称", "SSID"), 12, UI_COLOR_TEXT_SEC, 28, 60, 90);

    ctx->ssid_ta = lv_textarea_create(box);
    lv_obj_set_pos(ctx->ssid_ta, 28, 82);
    lv_obj_set_size(ctx->ssid_ta, 648, 44);
    lv_textarea_set_one_line(ctx->ssid_ta, true);
    lv_textarea_set_text(ctx->ssid_ta, ssid);
    lv_textarea_set_placeholder_text(ctx->ssid_ta, tr("输入 Wi-Fi 名称", "Enter Wi-Fi SSID"));
    lv_obj_set_style_text_font(ctx->ssid_ta, ui_font_cn(14), 0);
    lv_obj_set_style_text_color(ctx->ssid_ta, UI_COLOR_TEXT_STRONG, 0);
    lv_obj_set_style_text_color(ctx->ssid_ta, UI_COLOR_TEXT_SEC, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_radius(ctx->ssid_ta, 8, 0);
    lv_obj_set_style_border_width(ctx->ssid_ta, 1, 0);
    lv_obj_set_style_border_color(ctx->ssid_ta, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_color(ctx->ssid_ta, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(ctx->ssid_ta, UI_COLOR_INPUT_BG, 0);
    lv_obj_add_event_cb(ctx->ssid_ta, on_textarea_focus, LV_EVENT_FOCUSED, ctx);

    cn_label(box, tr("密码", "Password"), 12, UI_COLOR_TEXT_SEC, 28, 132, 80);
    ctx->pwd_ta = lv_textarea_create(box);
    lv_obj_set_pos(ctx->pwd_ta, 28, 154);
    lv_obj_set_size(ctx->pwd_ta, 648, 44);
    lv_textarea_set_one_line(ctx->pwd_ta, true);
    lv_textarea_set_password_mode(ctx->pwd_ta, true);
    lv_textarea_set_placeholder_text(ctx->pwd_ta, tr("输入 Wi-Fi 密码", "Enter Wi-Fi password"));
    lv_obj_set_style_text_font(ctx->pwd_ta, ui_font_cn(14), 0);
    lv_obj_set_style_text_color(ctx->pwd_ta, UI_COLOR_TEXT_STRONG, 0);
    lv_obj_set_style_text_color(ctx->pwd_ta, UI_COLOR_TEXT_SEC, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_radius(ctx->pwd_ta, 8, 0);
    lv_obj_set_style_border_width(ctx->pwd_ta, 1, 0);
    lv_obj_set_style_border_color(ctx->pwd_ta, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_color(ctx->pwd_ta, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(ctx->pwd_ta, UI_COLOR_INPUT_BG, 0);
    lv_obj_add_event_cb(ctx->pwd_ta, on_textarea_focus, LV_EVENT_FOCUSED, ctx);

    text_button(box, 372, 214, 132, tr("取消", "Cancel"), UI_COLOR_INPUT_BG,
        UI_COLOR_TEXT_SEC, on_modal_cancel, ctx);
    text_button(box, 520, 214, 156, tr("连接", "Connect"), UI_COLOR_ACCENT,
        lv_color_white(), on_wifi_connect, ctx);

    ctx->active_ta = ctx->pwd_ta;
    ctx->keyboard = matrix_keyboard(ctx, box, 28, 270);
}

static void on_ap_clicked(lv_event_t *e)
{
    net_ctx_t *ctx = (net_ctx_t *)lv_event_get_user_data(e);
    const char *ssid = (const char *)lv_obj_get_user_data(lv_event_get_target(e));
    if (ctx && ssid) open_wifi_modal(ctx, ssid);
}

static void on_scan_done(const wifi_mgr_ap_t *mgr_aps, uint16_t count)
{
    ESP_LOGI(TAG, "WiFi scan done, %u APs", (unsigned)count);
    wifi_ap_t aps[WIFI_AP_MAX];
    uint16_t n = (count > WIFI_AP_MAX) ? WIFI_AP_MAX : count;
    for (uint16_t i = 0; i < n; i++) {
        strncpy(aps[i].ssid, mgr_aps[i].ssid, WIFI_SSID_MAX - 1);
        aps[i].ssid[WIFI_SSID_MAX - 1] = '\0';
        aps[i].rssi = mgr_aps[i].rssi;
        aps[i].encrypted = (mgr_aps[i].authmode != 0);
    }
    device_model_update_wifi_aps(n > 0 ? aps : NULL, n);
}

static void on_scan(lv_event_t *e)
{
    (void)e;
    device_model_update_wifi_state(WIFI_STATE_SCANNING, NULL);
    wifi_manager_scan(on_scan_done);
}

static void on_manual_add(lv_event_t *e)
{
    open_wifi_modal((net_ctx_t *)lv_event_get_user_data(e), "");
}

static void wifi_row(net_ctx_t *ctx, lv_obj_t *parent, int y, const char *ssid,
    int8_t rssi, bool encrypted, bool connected)
{
    lv_obj_t *row = panel(parent, 0, y, 520, 56, 12);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_bg_color(row, connected ? UI_COLOR_ACCENT_SOFT : UI_COLOR_CARD, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(row, (void *)ssid);
    lv_obj_add_event_cb(row, on_ap_clicked, LV_EVENT_CLICKED, ctx);
    ui_apply_press_feedback(row, UI_COLOR_ACCENT);

    icon_text(row, ICON_WIFI, connected ? UI_COLOR_ACCENT : UI_COLOR_TEXT_SEC, 20, 19, 16);
    cn_label(row, ssid, 14, UI_COLOR_TEXT_STRONG, 54, 17, 210);
    if (encrypted) icon_text(row, ICON_LOCK, UI_COLOR_TEXT_SEC, 374, 20, 13);
    /* Signal strength indicator */
    lv_color_t sig_color = (rssi > -50) ? UI_COLOR_GREEN : (rssi > -70) ? UI_COLOR_ORANGE : UI_COLOR_RED;
    icon_text(row, ICON_WIFI, sig_color, 466, 20, 14);
    if (connected) {
        cn_label(row, tr("已连接", "Connected"), 11, UI_COLOR_GREEN, 284, 19, 70);
    }
}

static void rebuild_wifi_list(net_ctx_t *ctx)
{
    if (!ctx || !ctx->wifi_list) return;
    lv_obj_clean(ctx->wifi_list);

    const mqtt_device_model_t *m = device_model_get();
    int y = 0;
    for (int i = 0; i < m->wifi_ap_count && i < WIFI_AP_MAX; i++) {
        bool connected = (m->wifi_state == WIFI_STATE_CONNECTED) &&
            strcmp(m->wifi_ssid, m->wifi_aps[i].ssid) == 0;
        wifi_row(ctx, ctx->wifi_list, y, m->wifi_aps[i].ssid,
            m->wifi_aps[i].rssi, m->wifi_aps[i].encrypted, connected);
        y += 64;
    }
    if (m->wifi_ap_count == 0) {
        cn_label(ctx->wifi_list, tr("点击\"重新扫描\"搜索附近网络", "Click \"Scan\" to find networks"),
            13, UI_COLOR_TEXT_SEC, 20, 20, 400);
    }
}

static void update_network(net_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;
    const mqtt_device_model_t *m = device_model_get();
    bool mqtt_ok = (m->mqtt_state == MQTT_STATE_CONNECTED);

    if (ctx->mqtt_state) {
        lv_label_set_text(ctx->mqtt_state, mqtt_ok ?
            tr("连接状态：已连接", "Link: Connected") :
            tr("连接状态：未连接", "Link: Offline"));
        lv_obj_set_style_text_color(ctx->mqtt_state,
            mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED, 0);
    }
    if (ctx->mqtt_dot) {
        lv_obj_set_style_bg_color(ctx->mqtt_dot, mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED, 0);
    }
    if (ctx->mqtt_rx) {
        char buf[32];
        lv_snprintf(buf, sizeof(buf), "%lu", (unsigned long)m->mqtt_rx_count);
        lv_label_set_text(ctx->mqtt_rx, buf);
    }

    if (ctx->wifi_status) {
        char buf[96];
        switch (m->wifi_state) {
            case WIFI_STATE_CONNECTED:
                lv_snprintf(buf, sizeof(buf), "%s  %s", tr("当前状态：已连接", "Status: Connected"), m->wifi_ssid);
                lv_label_set_text(ctx->wifi_status, buf);
                lv_obj_set_style_text_color(ctx->wifi_status, UI_COLOR_GREEN, 0);
                set_button_hidden(ctx->disconnect_btn, false);
                break;
            case WIFI_STATE_SCANNING:
                lv_label_set_text(ctx->wifi_status, tr("当前状态：扫描中...", "Status: Scanning..."));
                lv_obj_set_style_text_color(ctx->wifi_status, UI_COLOR_ORANGE, 0);
                set_button_hidden(ctx->disconnect_btn, true);
                break;
            case WIFI_STATE_CONNECTING:
                lv_label_set_text(ctx->wifi_status, tr("当前状态：连接中...", "Status: Connecting..."));
                lv_obj_set_style_text_color(ctx->wifi_status, UI_COLOR_ORANGE, 0);
                set_button_hidden(ctx->disconnect_btn, true);
                break;
            case WIFI_STATE_FAILED:
                lv_label_set_text(ctx->wifi_status, tr("当前状态：连接失败", "Status: Failed"));
                lv_obj_set_style_text_color(ctx->wifi_status, UI_COLOR_RED, 0);
                set_button_hidden(ctx->disconnect_btn, true);
                break;
            default:
                lv_label_set_text(ctx->wifi_status, tr("当前状态：未连接", "Status: Disconnected"));
                lv_obj_set_style_text_color(ctx->wifi_status, UI_COLOR_RED, 0);
                set_button_hidden(ctx->disconnect_btn, true);
                break;
        }
    }

    bool changed = ctx->last_wifi_count != m->wifi_ap_count ||
        ctx->last_wifi_state != m->wifi_state ||
        strcmp(ctx->last_wifi_ssid, m->wifi_ssid) != 0;
    if (changed) {
        ctx->last_wifi_count = m->wifi_ap_count;
        ctx->last_wifi_state = m->wifi_state;
        lv_strlcpy(ctx->last_wifi_ssid, m->wifi_ssid, sizeof(ctx->last_wifi_ssid));
        rebuild_wifi_list(ctx);
    }
}

static void on_model_updated(void *user_data)
{
    update_network((net_ctx_t *)user_data);
}

static void page_net_delete(lv_event_t *e)
{
    net_ctx_t *ctx = (net_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    close_wifi_modal(ctx);
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    lv_free(ctx);
}

lv_obj_t *page_net_create(lv_obj_t *parent)
{
    net_ctx_t *ctx = lv_malloc(sizeof(net_ctx_t));
    memset(ctx, 0, sizeof(*ctx));
    ctx->last_wifi_count = -1;
    ctx->last_wifi_state = WIFI_STATE_FAILED;

    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(page, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(page, page_net_delete, LV_EVENT_DELETE, ctx);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char tm_buf[16];
    lv_snprintf(tm_buf, sizeof(tm_buf), "%02d:%02d", t ? t->tm_hour : 9, t ? t->tm_min : 41);

    cn_label(page, tr("网络连接", "Network"), 22, UI_COLOR_TEXT_STRONG, 32, 24, 130);
    cn_label(page, tm_buf, 16, UI_COLOR_TEXT_STRONG, 718, 28, 54);
    icon_text(page, ICON_WIFI, UI_COLOR_TEXT_STRONG, 784, 28, 16);
    ctx->wifi_status = cn_label(page, tr("当前状态：未连接", "Status: Disconnected"), 13, UI_COLOR_RED, 32, 70, 260);
    ctx->disconnect_btn = text_button(page, 608, 62, 120, tr("断开连接", "Disconnect"), UI_COLOR_INPUT_BG,
        UI_COLOR_RED, on_wifi_disconnect, ctx);
    set_button_hidden(ctx->disconnect_btn, true);
    text_button(page, 752, 62, 132, tr("重新扫描", "Scan"), UI_COLOR_ACCENT,
        lv_color_white(), on_scan, ctx);

    ctx->wifi_list = lv_obj_create(page);
    lv_obj_remove_style_all(ctx->wifi_list);
    lv_obj_set_pos(ctx->wifi_list, 32, 118);
    lv_obj_set_size(ctx->wifi_list, 520, 380);
    lv_obj_clear_flag(ctx->wifi_list, LV_OBJ_FLAG_SCROLLABLE);

    const mqtt_device_model_t *m = device_model_get();
    bool mqtt_ok = (m->mqtt_state == MQTT_STATE_CONNECTED);
    lv_obj_t *help = panel(page, 590, 118, 320, 230, 16);
    icon_text(help, ICON_PLUG, UI_COLOR_GREEN, 28, 26, 22);
    cn_label(help, tr("MQTT 数据链路", "MQTT Link"), 18, UI_COLOR_TEXT_STRONG, 64, 24, 150);
    cn_label(help, m->mqtt_broker, 12, UI_COLOR_TEXT_SEC, 28, 68, 260);
    ctx->mqtt_dot = status_dot(help, 30, 108, mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED);
    ctx->mqtt_state = cn_label(help, mqtt_ok ? tr("连接状态：已连接", "Link: Connected") :
        tr("连接状态：未连接", "Link: Offline"), 13,
        mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED, 46, 102, 160);
    cn_label(help, tr("消息数", "Messages"), 12, UI_COLOR_TEXT_SEC, 28, 144, 70);
    char buf[32];
    lv_snprintf(buf, sizeof(buf), "%lu", (unsigned long)m->mqtt_rx_count);
    ctx->mqtt_rx = cn_label(help, buf, 24, UI_COLOR_GREEN, 28, 166, 84);

    text_button(page, 32, 524, 520, tr("手动添加网络", "Add Network"), UI_COLOR_ACCENT,
        lv_color_white(), on_manual_add, ctx);

    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    update_network(ctx);

    ESP_LOGI(TAG, "Network page created");
    return page;
}
