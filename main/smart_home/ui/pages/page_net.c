#include "page_net.h"
#include "ui_kit.h"
#include "ui_events.h"
#include "ui_styles.h"
#include "ui_theme.h"
#include "ui_font.h"
#include "mqtt_device_model.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>

static const char *TAG = "PAGE_NET";

/* ===== UTF-8 hex escapes for Chinese strings ===== */

/* Page title */
#define STR_TITLE_ZH        "\xE7\xBD\x91\xE7\xBB\x9C"                          /* 网络 */
#define STR_TITLE_EN        "Network"

/* Group titles */
#define STR_WIFI_ZH         "Wi-Fi"
#define STR_WIFI_EN         "Wi-Fi"
#define STR_MQTT_ZH         "MQTT"
#define STR_MQTT_EN         "MQTT"

/* Info row labels */
#define STR_SSID_ZH         "\xE7\xBD\x91\xE7\xBB\x9C\xE5\x90\x8D\xE7\xA7\xB0"  /* 网络名称 */
#define STR_SSID_EN         "SSID"
#define STR_BROKER_ZH       "Broker"
#define STR_BROKER_EN       "Broker"
#define STR_CLIENT_ZH       "Client ID"
#define STR_CLIENT_EN       "Client ID"
#define STR_STATE_ZH        "\xE7\x8A\xB6\xE6\x80\x81"                          /* 状态 */
#define STR_STATE_EN        "State"
#define STR_MSG_COUNT_ZH    "\xE6\xB6\x88\xE6\x81\xAF\xE6\x95\xB0"              /* 消息数 */
#define STR_MSG_COUNT_EN    "Messages"
#define STR_CTRL_ZH         "\xE6\x8E\xA7\xE5\x88\xB6\xE5\x99\xA8"              /* 控制器 */
#define STR_CTRL_EN         "Controllers"

/* Status texts */
#define STR_CONNECTED_ZH    "\xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5"              /* 已连接 */
#define STR_CONNECTED_EN    "Connected"
#define STR_DISCONNECTED_ZH "\xE6\x9C\xAA\xE8\xBF\x9E\xE6\x8E\xA5"              /* 未连接 */
#define STR_DISCONNECTED_EN "Disconnected"
#define STR_NOT_CONN_ZH     "\xE6\x9C\xAA\xE8\xBF\x9E\xE6\x8E\xA5"              /* 未连接 */
#define STR_NOT_CONN_EN     "Not Connected"
#define STR_ONLINE_ZH       "\xE5\x9C\xA8\xE7\xBA\xBF"                          /* 在线 */
#define STR_ONLINE_EN       "Online"
#define STR_OFFLINE_ZH      "\xE7\xA6\xBB\xE7\xBA\xBF"                          /* 离线 */
#define STR_OFFLINE_EN      "Offline"

/* Action button */
#define STR_REFRESH_ZH      "\xE5\x88\xB7\xE6\x96\xB0"                          /* 刷新 */
#define STR_REFRESH_EN      "Refresh"

#define MQTT_CLIENT_ID      "xiaozhi_p4_host"
#define MQTT_BROKER_DEFAULT "8.134.167.240:1883"

typedef struct {
    lv_obj_t *page;
    lv_obj_t *pills_row;       /* dynamic: rebuilt on update */
    lv_obj_t *wifi_ssid_val;   /* value label */
    lv_obj_t *mqtt_broker_val; /* value label */
    lv_obj_t *mqtt_state_val;  /* value label */
    lv_obj_t *mqtt_rx_val;     /* value label */
    lv_obj_t *ctrl_val;        /* value label */
    bool deleted;
} net_ctx_t;

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

/* ---- Flex row helper ---- */
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

/* ---- Create an info row: icon + label on left, value on right ---- */
/* The value label is created first (temporarily child of parent), then
 * ui_kit_setting_row reparents it into the row. */
static void make_info_row(lv_obj_t *parent, const char *icon,
    const char *label, const char *value, lv_color_t value_color,
    lv_obj_t **value_label_out)
{
    lv_obj_t *val = ui_create_label(parent, value ? value : "",
        ui_font_cn(13), value_color);

    ui_kit_setting_row(parent, icon, label, val);

    if (value_label_out) *value_label_out = val;
}

/* ---- Refresh status pills ---- */
static void refresh_pills(net_ctx_t *ctx)
{
    if (!ctx || !ctx->pills_row || ctx->deleted) return;
    lv_obj_clean(ctx->pills_row);

    const mqtt_device_model_t *m = device_model_get();
    char buf[80];

    /* WiFi pill: green "已连接" when connected, gray "未连接" otherwise */
    bool wifi_ok = (m->wifi_state == WIFI_STATE_CONNECTED);
    if (wifi_ok && m->wifi_ssid[0]) {
        lv_snprintf(buf, sizeof(buf), "%s %s",
            tr(STR_CONNECTED_ZH, STR_CONNECTED_EN), m->wifi_ssid);
    } else {
        lv_snprintf(buf, sizeof(buf), "%s",
            tr(STR_DISCONNECTED_ZH, STR_DISCONNECTED_EN));
    }
    ui_kit_status_pill(ctx->pills_row, buf,
        wifi_ok ? UI_COLOR_GREEN : UI_COLOR_TEXT_SEC,
        wifi_ok ? UI_COLOR_GREEN_SOFT : UI_COLOR_INPUT_BG);

    /* MQTT pill: green "已连接" or red "未连接" */
    bool mqtt_ok = (m->mqtt_state == MQTT_STATE_CONNECTED);
    ui_kit_status_pill(ctx->pills_row,
        mqtt_ok ? tr(STR_CONNECTED_ZH, STR_CONNECTED_EN)
                : tr(STR_DISCONNECTED_ZH, STR_DISCONNECTED_EN),
        mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED,
        mqtt_ok ? UI_COLOR_GREEN_SOFT : UI_COLOR_INPUT_BG);
}

/* ---- Refresh value labels ---- */
static void refresh_values(net_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;
    const mqtt_device_model_t *m = device_model_get();

    /* WiFi SSID */
    if (ctx->wifi_ssid_val) {
        if (m->wifi_state == WIFI_STATE_CONNECTED && m->wifi_ssid[0]) {
            lv_label_set_text(ctx->wifi_ssid_val, m->wifi_ssid);
            lv_obj_set_style_text_color(ctx->wifi_ssid_val, UI_COLOR_TEXT_STRONG, 0);
        } else {
            lv_label_set_text(ctx->wifi_ssid_val, tr(STR_NOT_CONN_ZH, STR_NOT_CONN_EN));
            lv_obj_set_style_text_color(ctx->wifi_ssid_val, UI_COLOR_TEXT_SEC, 0);
        }
    }

    /* MQTT broker */
    if (ctx->mqtt_broker_val) {
        lv_label_set_text(ctx->mqtt_broker_val,
            m->mqtt_broker[0] ? m->mqtt_broker : MQTT_BROKER_DEFAULT);
    }

    /* MQTT state */
    if (ctx->mqtt_state_val) {
        bool mqtt_ok = (m->mqtt_state == MQTT_STATE_CONNECTED);
        lv_label_set_text(ctx->mqtt_state_val,
            mqtt_ok ? tr(STR_CONNECTED_ZH, STR_CONNECTED_EN)
                    : tr(STR_DISCONNECTED_ZH, STR_DISCONNECTED_EN));
        lv_obj_set_style_text_color(ctx->mqtt_state_val,
            mqtt_ok ? UI_COLOR_GREEN : UI_COLOR_RED, 0);
    }

    /* MQTT rx count */
    if (ctx->mqtt_rx_val) {
        char buf[32];
        lv_snprintf(buf, sizeof(buf), "%lu", (unsigned long)m->mqtt_rx_count);
        lv_label_set_text(ctx->mqtt_rx_val, buf);
    }

    /* Controllers online (1F/2F/3F) */
    if (ctx->ctrl_val) {
        char buf[80];
        const char *s1 = m->controller_online[0] ? tr(STR_ONLINE_ZH, STR_ONLINE_EN)
                                                  : tr(STR_OFFLINE_ZH, STR_OFFLINE_EN);
        const char *s2 = m->controller_online[1] ? tr(STR_ONLINE_ZH, STR_ONLINE_EN)
                                                  : tr(STR_OFFLINE_ZH, STR_OFFLINE_EN);
        const char *s3 = m->controller_online[2] ? tr(STR_ONLINE_ZH, STR_ONLINE_EN)
                                                  : tr(STR_OFFLINE_ZH, STR_OFFLINE_EN);
        lv_snprintf(buf, sizeof(buf), "1F %s  2F %s  3F %s", s1, s2, s3);
        lv_label_set_text(ctx->ctrl_val, buf);
    }
}

/* ---- Refresh button: re-publish MODEL_UPDATED to trigger refresh ---- */
static void on_refresh(lv_event_t *e)
{
    (void)e;
    ui_event_publish(UI_EVENT_MODEL_UPDATED);
}

/* ---- Event callback: model/mqtt/wifi changed ---- */
static void on_event(void *user_data)
{
    net_ctx_t *ctx = (net_ctx_t *)user_data;
    if (!ctx || ctx->deleted) return;
    refresh_pills(ctx);
    refresh_values(ctx);
}

/* ---- Delete callback ---- */
static void on_delete(lv_event_t *e)
{
    net_ctx_t *ctx = (net_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_event, ctx);
    ui_event_unsubscribe(UI_EVENT_MQTT_CONNECTED, on_event, ctx);
    ui_event_unsubscribe(UI_EVENT_MQTT_DISCONNECTED, on_event, ctx);
    ui_event_unsubscribe(UI_EVENT_WIFI_CHANGED, on_event, ctx);
    lv_free(ctx);
}

/* ---- Page create ---- */
lv_obj_t *page_net_create(lv_obj_t *parent)
{
    net_ctx_t *ctx = lv_malloc(sizeof(net_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    ctx->page = ui_create_page(parent, on_delete, ctx);

    /* Title */
    ui_kit_page_title(ctx->page, ICON_WIFI,
        tr(STR_TITLE_ZH, STR_TITLE_EN), NULL);

    /* Status pills row (dynamic) */
    ctx->pills_row = make_flex_row(ctx->page, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(ctx->pills_row, 8, 0);

    /* Wi-Fi group */
    ui_kit_group_title(ctx->page, tr(STR_WIFI_ZH, STR_WIFI_EN));
    make_info_row(ctx->page, ICON_WIFI,
        tr(STR_SSID_ZH, STR_SSID_EN),
        tr(STR_NOT_CONN_ZH, STR_NOT_CONN_EN),
        UI_COLOR_TEXT_SEC, &ctx->wifi_ssid_val);

    /* MQTT group */
    ui_kit_group_title(ctx->page, tr(STR_MQTT_ZH, STR_MQTT_EN));
    make_info_row(ctx->page, ICON_SERVER,
        tr(STR_BROKER_ZH, STR_BROKER_EN),
        MQTT_BROKER_DEFAULT, UI_COLOR_TEXT_STRONG, &ctx->mqtt_broker_val);
    make_info_row(ctx->page, ICON_INFO,
        tr(STR_CLIENT_ZH, STR_CLIENT_EN),
        MQTT_CLIENT_ID, UI_COLOR_TEXT_STRONG, NULL);
    make_info_row(ctx->page, ICON_PLUG,
        tr(STR_STATE_ZH, STR_STATE_EN),
        tr(STR_DISCONNECTED_ZH, STR_DISCONNECTED_EN),
        UI_COLOR_RED, &ctx->mqtt_state_val);
    make_info_row(ctx->page, ICON_CHART,
        tr(STR_MSG_COUNT_ZH, STR_MSG_COUNT_EN),
        "0", UI_COLOR_TEXT_STRONG, &ctx->mqtt_rx_val);
    make_info_row(ctx->page, ICON_HOME,
        tr(STR_CTRL_ZH, STR_CTRL_EN),
        "1F -  2F -  3F -", UI_COLOR_TEXT_STRONG, &ctx->ctrl_val);

    /* Action row */
    lv_obj_t *action_row = make_flex_row(ctx->page, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(action_row, 8, 0);
    ui_kit_chip(action_row, tr(STR_REFRESH_ZH, STR_REFRESH_EN), false, on_refresh, NULL);

    /* Subscribe to events for dynamic refresh */
    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_event, ctx);
    ui_event_subscribe(UI_EVENT_MQTT_CONNECTED, on_event, ctx);
    ui_event_subscribe(UI_EVENT_MQTT_DISCONNECTED, on_event, ctx);
    ui_event_subscribe(UI_EVENT_WIFI_CHANGED, on_event, ctx);

    /* Initial refresh */
    refresh_pills(ctx);
    refresh_values(ctx);

    ESP_LOGI(TAG, "Network page created");
    return ctx->page;
}
