#include "page_env.h"

#include "mqtt_device_model.h"
#include "ui_events.h"
#include "ui_font.h"
#include "ui_i18n.h"
#include "ui_icons.h"
#include "ui_kit.h"
#include "ui_styles.h"
#include "ui_theme.h"

#include "esp_log.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const char *TAG = "PAGE_ENV";

/* ===== UTF-8 hex escapes for Chinese strings ===== */

/* Page title */
#define STR_TITLE_ZH      "\xE7\x8E\xAF\xE5\xA2\x83\xE7\x9B\x91\xE6\xB5\x8B"  /* 环境监测 */
#define STR_TITLE_EN      "Environment"

/* Group titles */
#define STR_GROUP_INDOOR_ZH   "\xE5\xAE\xA4\xE5\x86\x85\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8"  /* 室内传感器 */
#define STR_GROUP_INDOOR_EN   "Indoor Sensors"
#define STR_GROUP_OUTDOOR_ZH  "\xE5\xAE\xA4\xE5\xA4\x96\xE5\xA4\xA9\xE6\xB0\x94"              /* 室外天气 */
#define STR_GROUP_OUTDOOR_EN  "Outdoor Weather"

/* Status pill texts */
#define STR_SENSORS_ONLINE_ZH "\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8 %u \xE9\xA1\xB9\xE5\x9C\xA8\xE7\xBA\xBF"  /* 传感器 N 项在线 */
#define STR_SENSORS_ONLINE_EN "%u sensors online"
#define STR_UPDATED_NOW_ZH    "\xE5\x88\x9A\xE5\x88\x9A\xE6\x9B\xB4\xE6\x96\xB0"  /* 刚刚更新 */
#define STR_UPDATED_NOW_EN    "Updated now"
#define STR_UPDATED_MIN_ZH    "%u \xE5\x88\x86\xE9\x92\x9F\xE5\x89\x8D"  /* N 分钟前 */
#define STR_UPDATED_MIN_EN    "%u min ago"

/* Indoor sensor labels */
#define STR_2F_RAIN_VAL_ZH    "\xE4\xBA\x8C\xE6\xA5\xBC\xE9\x9B\xA8\xE6\xBB\xB4\xE5\x80\xBC"  /* 二楼雨滴值 */
#define STR_2F_RAIN_VAL_EN    "2F Rain Value"
#define STR_2F_RAIN_STA_ZH    "\xE4\xBA\x8C\xE6\xA5\xBC\xE9\x9B\xA8\xE6\xBB\xB4\xE7\x8A\xB6\xE6\x80\x81"  /* 二楼雨滴状态 */
#define STR_2F_RAIN_STA_EN    "2F Rain Status"
#define STR_3F_RAIN_VAL_ZH    "\xE4\xB8\x89\xE6\xA5\xBC\xE9\x9B\xA8\xE6\xBB\xB4\xE5\x80\xBC"  /* 三楼雨滴值 */
#define STR_3F_RAIN_VAL_EN    "3F Rain Value"
#define STR_3F_RAIN_STA_ZH    "\xE4\xB8\x89\xE6\xA5\xBC\xE9\x9B\xA8\xE6\xBB\xB4\xE7\x8A\xB6\xE6\x80\x81"  /* 三楼雨滴状态 */
#define STR_3F_RAIN_STA_EN    "3F Rain Status"
#define STR_3F_SMOKE_ZH       "\xE4\xB8\x89\xE6\xA5\xBC\xE7\x83\x9F\xE9\x9B\xBE"  /* 三楼烟雾 */
#define STR_3F_SMOKE_EN       "3F Smoke"
#define STR_3F_FIRE_ZH        "\xE4\xB8\x89\xE6\xA5\xBC\xE7\x81\xAB\xE7\x81\xBE"  /* 三楼火灾 */
#define STR_3F_FIRE_EN        "3F Fire"
#define STR_3F_HELP_ZH        "\xE4\xB8\x89\xE6\xA5\xBC\xE6\xB1\x82\xE5\x8A\xA9"  /* 三楼求助 */
#define STR_3F_HELP_EN        "3F Help"

/* Outdoor weather labels */
#define STR_TEMP_ZH       "\xE6\xB8\xA9\xE5\xBA\xA6"      /* 温度 */
#define STR_TEMP_EN       "Temperature"
#define STR_HUMI_ZH       "\xE6\xB9\xBF\xE5\xBA\xA6"      /* 湿度 */
#define STR_HUMI_EN       "Humidity"
#define STR_WEATHER_ZH    "\xE5\xA4\xA9\xE6\xB0\x94"      /* 天气 */
#define STR_WEATHER_EN    "Weather"
#define STR_PRECIP_ZH     "\xE9\x99\x8D\xE6\xB0\xB4"      /* 降水 */
#define STR_PRECIP_EN     "Precipitation"
#define STR_WIND_ZH       "\xE9\xA3\x8E\xE9\x80\x9F"      /* 风速 */
#define STR_WIND_EN       "Wind Speed"

/* Status text */
#define STR_NORMAL_ZH     "\xE6\xAD\xA3\xE5\xB8\xB8"      /* 正常 */
#define STR_NORMAL_EN     "Normal"
#define STR_ALARM_ZH      "\xE5\x91\x8A\xE8\xAD\xA6"      /* 告警 */
#define STR_ALARM_EN      "Alarm"
#define STR_WET_ZH        "\xE4\xB8\x8B\xE9\x9B\xA8"      /* 下雨 */
#define STR_WET_EN        "Rain"
#define STR_DRY_ZH        "\xE5\xB9\xB2\xE7\x87\xA5"      /* 干燥 */
#define STR_DRY_EN        "Dry"
#define STR_HELP_ON_ZH    "\xE6\xB1\x82\xE5\x8A\xA9"      /* 求助 */
#define STR_HELP_ON_EN    "Help"

/* Weather code names (WMO) */
#define STR_WC_CLEAR_ZH      "\xE6\x99\xB4"              /* 晴 */
#define STR_WC_CLEAR_EN      "Clear"
#define STR_WC_CLOUDY_ZH     "\xE5\xA4\x9A\xE4\xBA\x91"  /* 多云 */
#define STR_WC_CLOUDY_EN     "Cloudy"
#define STR_WC_FOG_ZH        "\xE9\x9B\xBE"              /* 雾 */
#define STR_WC_FOG_EN        "Fog"
#define STR_WC_RAIN_ZH       "\xE9\x9B\xA8"              /* 雨 */
#define STR_WC_RAIN_EN       "Rain"
#define STR_WC_SNOW_ZH       "\xE9\x9B\xAA"              /* 雪 */
#define STR_WC_SNOW_EN       "Snow"
#define STR_WC_SHOWERS_ZH    "\xE9\x98\xB5\xE9\x9B\xA8"  /* 阵雨 */
#define STR_WC_SHOWERS_EN    "Showers"
#define STR_WC_THUNDER_ZH    "\xE9\x9B\xB7\xE6\x9A\xB4"  /* 雷暴 */
#define STR_WC_THUNDER_EN    "Thunderstorm"
#define STR_WC_UNKNOWN_ZH    "\xE6\x9C\xAA\xE7\x9F\xA5"  /* 未知 */
#define STR_WC_UNKNOWN_EN    "Unknown"

/* Unit strings (UTF-8) */
#define UNIT_DEG_C    "\xC2\xB0""C"           /* °C */
#define UNIT_UG_M3    "\xCE\xBCg/m\xC2\xB3"   /* μg/m³ */

#define METRIC_CARD_W_PCT  47

typedef struct {
    lv_obj_t *page;
    lv_obj_t *pill_row;       /* dynamic: rebuilt on update */
    lv_obj_t *indoor_wrap;    /* dynamic: rebuilt on update */
    lv_obj_t *outdoor_wrap;   /* dynamic: rebuilt on update */
    bool deleted;
} env_ctx_t;

static const char *tr(const char *zh, const char *en)
{
    return ui_i18n_get_lang() == UI_LANG_ZH ? zh : en;
}

/* WMO weather code to text mapping (per task spec) */
static const char *weather_code_name(uint16_t code)
{
    bool zh = (ui_i18n_get_lang() == UI_LANG_ZH);
    if (code == 0)                       return zh ? STR_WC_CLEAR_ZH   : STR_WC_CLEAR_EN;
    if (code >= 1 && code <= 3)          return zh ? STR_WC_CLOUDY_ZH  : STR_WC_CLOUDY_EN;
    if (code >= 45 && code <= 48)        return zh ? STR_WC_FOG_ZH     : STR_WC_FOG_EN;
    if (code >= 51 && code <= 67)        return zh ? STR_WC_RAIN_ZH    : STR_WC_RAIN_EN;
    if (code >= 71 && code <= 77)        return zh ? STR_WC_SNOW_ZH    : STR_WC_SNOW_EN;
    if (code >= 80 && code <= 82)        return zh ? STR_WC_SHOWERS_ZH : STR_WC_SHOWERS_EN;
    if (code >= 95 && code <= 99)        return zh ? STR_WC_THUNDER_ZH : STR_WC_THUNDER_EN;
    return zh ? STR_WC_UNKNOWN_ZH : STR_WC_UNKNOWN_EN;
}

/* Create a flex row container (no wrap) for pills */
static lv_obj_t *create_row(lv_obj_t *parent, int gap)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row, gap, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

/* Create a flex row wrap container for metric cards (3 per row) */
static lv_obj_t *create_wrap(lv_obj_t *parent, int gap)
{
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_remove_style_all(wrap);
    lv_obj_set_width(wrap, lv_pct(100));
    lv_obj_set_height(wrap, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(wrap, 0, 0);
    lv_obj_set_layout(wrap, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(wrap, gap, 0);
    lv_obj_set_style_pad_row(wrap, gap, 0);
    lv_obj_clear_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);
    return wrap;
}

static lv_obj_t *create_col(lv_obj_t *parent)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, 0);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_style_pad_row(col, 8, 0);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    return col;
}

/* Add a metric card to a wrap container with fixed percentage width */
static void add_metric_card(lv_obj_t *wrap, const char *icon,
    const char *label, const char *value, const char *unit, bool valid)
{
    lv_obj_t *card = ui_kit_metric_card(wrap, icon, label, value, unit, valid);
    lv_obj_set_width(card, lv_pct(METRIC_CARD_W_PCT));
    lv_obj_set_flex_grow(card, 0);
}

static void refresh_pills(env_ctx_t *ctx)
{
    if (!ctx || !ctx->pill_row || ctx->deleted) return;
    lv_obj_clean(ctx->pill_row);

    const mqtt_device_model_t *m = device_model_get();
    char buf[64];

    /* Sensor online count pill (green, always shown) */
    uint16_t online = device_model_sensor_online_count();
    lv_snprintf(buf, sizeof(buf),
        tr(STR_SENSORS_ONLINE_ZH, STR_SENSORS_ONLINE_EN), (unsigned)online);
    ui_kit_status_pill(ctx->pill_row, buf, UI_COLOR_GREEN, UI_COLOR_GREEN_SOFT);

    /* Weather update time pill (only when weather_valid) */
    if (m->weather_valid) {
        uint32_t now = (uint32_t)(lv_tick_get() / 1000U);
        uint32_t age = (now >= m->weather_last_update_s) ? (now - m->weather_last_update_s) : 0;
        if (age < 60) {
            lv_snprintf(buf, sizeof(buf), "%s",
                tr(STR_UPDATED_NOW_ZH, STR_UPDATED_NOW_EN));
        } else {
            lv_snprintf(buf, sizeof(buf),
                tr(STR_UPDATED_MIN_ZH, STR_UPDATED_MIN_EN),
                (unsigned)(age / 60));
        }
        ui_kit_status_pill(ctx->pill_row, buf, UI_COLOR_BLUE, UI_COLOR_ACCENT_SOFT);
    }
}

static void refresh_indoor(env_ctx_t *ctx)
{
    if (!ctx || !ctx->indoor_wrap || ctx->deleted) return;
    lv_obj_clean(ctx->indoor_wrap);

    char buf[32];
    uint16_t value = 0;
    uint8_t status = 0;
    bool valid = false;

    /* 2F rain value (mV) */
    valid = device_model_get_rain_value(2, &value);
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)value);
    add_metric_card(ctx->indoor_wrap, ICON_DROP,
        tr(STR_2F_RAIN_VAL_ZH, STR_2F_RAIN_VAL_EN),
        buf, "mV", valid);

    /* 2F rain status (下雨/干燥) */
    valid = device_model_get_rain_status(2, &status);
    add_metric_card(ctx->indoor_wrap, ICON_UMBRELLA,
        tr(STR_2F_RAIN_STA_ZH, STR_2F_RAIN_STA_EN),
        status ? tr(STR_WET_ZH, STR_WET_EN) : tr(STR_DRY_ZH, STR_DRY_EN),
        "", valid);

    /* 3F rain value (mV) */
    valid = device_model_get_rain_value(3, &value);
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)value);
    add_metric_card(ctx->indoor_wrap, ICON_DROP,
        tr(STR_3F_RAIN_VAL_ZH, STR_3F_RAIN_VAL_EN),
        buf, "mV", valid);

    /* 3F rain status (下雨/干燥) */
    valid = device_model_get_rain_status(3, &status);
    add_metric_card(ctx->indoor_wrap, ICON_UMBRELLA,
        tr(STR_3F_RAIN_STA_ZH, STR_3F_RAIN_STA_EN),
        status ? tr(STR_WET_ZH, STR_WET_EN) : tr(STR_DRY_ZH, STR_DRY_EN),
        "", valid);

    /* 3F smoke (mV) */
    valid = device_model_get_smoke_value(3, &value);
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)value);
    add_metric_card(ctx->indoor_wrap, ICON_FOG,
        tr(STR_3F_SMOKE_ZH, STR_3F_SMOKE_EN),
        buf, "mV", valid);

    /* 3F fire (告警/正常) */
    valid = device_model_get_fire_status(3, &status);
    add_metric_card(ctx->indoor_wrap, ICON_FIRE,
        tr(STR_3F_FIRE_ZH, STR_3F_FIRE_EN),
        status ? tr(STR_ALARM_ZH, STR_ALARM_EN) : tr(STR_NORMAL_ZH, STR_NORMAL_EN),
        "", valid);

    /* 3F help (求助/正常) */
    valid = device_model_get_help_status(3, &status);
    add_metric_card(ctx->indoor_wrap, ICON_SHIELD,
        tr(STR_3F_HELP_ZH, STR_3F_HELP_EN),
        status ? tr(STR_HELP_ON_ZH, STR_HELP_ON_EN) : tr(STR_NORMAL_ZH, STR_NORMAL_EN),
        "", valid);
}

static void refresh_outdoor(env_ctx_t *ctx)
{
    if (!ctx || !ctx->outdoor_wrap || ctx->deleted) return;
    lv_obj_clean(ctx->outdoor_wrap);

    const mqtt_device_model_t *m = device_model_get();
    char buf[32];
    bool wvalid = m->weather_valid;
    bool avalid = m->air_quality_valid;

    /* Temperature (°C, %.1f) */
    lv_snprintf(buf, sizeof(buf), "%.1f", (double)m->outdoor_temp);
    add_metric_card(ctx->outdoor_wrap, ICON_TEMP,
        tr(STR_TEMP_ZH, STR_TEMP_EN),
        buf, UNIT_DEG_C, wvalid);

    /* Humidity (%, %u) */
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)m->outdoor_humidity);
    add_metric_card(ctx->outdoor_wrap, ICON_DROP,
        tr(STR_HUMI_ZH, STR_HUMI_EN),
        buf, "%", wvalid);

    /* Weather (code mapped to text) */
    add_metric_card(ctx->outdoor_wrap, ICON_LEAF,
        tr(STR_WEATHER_ZH, STR_WEATHER_EN),
        weather_code_name(m->weather_code), "", wvalid);

    /* Precipitation (mm, %.1f) */
    lv_snprintf(buf, sizeof(buf), "%.1f", (double)m->precipitation_mm);
    add_metric_card(ctx->outdoor_wrap, ICON_UMBRELLA,
        tr(STR_PRECIP_ZH, STR_PRECIP_EN),
        buf, "mm", wvalid);

    /* Wind speed (m/s, %.1f) */
    lv_snprintf(buf, sizeof(buf), "%.1f", (double)m->wind_speed);
    add_metric_card(ctx->outdoor_wrap, ICON_AIR,
        tr(STR_WIND_ZH, STR_WIND_EN),
        buf, "m/s", wvalid);

    /* PM2.5 (μg/m³, %.1f) - uses air_quality_valid */
    lv_snprintf(buf, sizeof(buf), "%.1f", (double)m->pm25_outdoor);
    add_metric_card(ctx->outdoor_wrap, ICON_FOG,
        "PM2.5", buf, UNIT_UG_M3, avalid);

    /* AQI (%u) - uses air_quality_valid */
    lv_snprintf(buf, sizeof(buf), "%u", (unsigned)m->aqi);
    add_metric_card(ctx->outdoor_wrap, ICON_SHIELD,
        "AQI", buf, "", avalid);
}

static void refresh_env(env_ctx_t *ctx)
{
    if (!ctx || ctx->deleted) return;
    refresh_pills(ctx);
    refresh_indoor(ctx);
    refresh_outdoor(ctx);
}

static void on_model_updated(void *user_data)
{
    refresh_env((env_ctx_t *)user_data);
}

static void on_delete(lv_event_t *e)
{
    env_ctx_t *ctx = (env_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ctx->deleted = true;
    ui_event_unsubscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    lv_free(ctx);
}

lv_obj_t *page_env_create(lv_obj_t *parent)
{
    env_ctx_t *ctx = lv_malloc(sizeof(env_ctx_t));
    memset(ctx, 0, sizeof(*ctx));

    lv_obj_t *page = ui_create_page(parent, on_delete, ctx);
    ctx->page = page;

    /* Page title: icon + title (no subtitle per spec) */
    ui_kit_page_title(page, ICON_DROP,
        tr(STR_TITLE_ZH, STR_TITLE_EN), NULL);

    /* Status pills row (sensor online + weather update time) */
    ctx->pill_row = create_row(page, 8);

    lv_obj_t *body = create_row(page, 14);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *left = create_col(body);
    lv_obj_t *right = create_col(body);

    ui_kit_group_title(left, tr(STR_GROUP_INDOOR_ZH, STR_GROUP_INDOOR_EN));
    ctx->indoor_wrap = create_wrap(left, 10);

    ui_kit_group_title(right, tr(STR_GROUP_OUTDOOR_ZH, STR_GROUP_OUTDOOR_EN));
    ctx->outdoor_wrap = create_wrap(right, 10);

    ui_event_subscribe(UI_EVENT_MODEL_UPDATED, on_model_updated, ctx);
    refresh_env(ctx);

    ESP_LOGI(TAG, "Environment page created");
    return page;
}
