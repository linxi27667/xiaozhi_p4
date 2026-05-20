#include "ui_i18n.h"
#include "ui_events.h"
#include "ui_font.h"
#include <string.h>

static ui_lang_t s_lang = UI_LANG_ZH;

static const char *s_zh[] = {
    [UI_KEY_TAB_DATA]  = "\xE6\x95\xB0\xE6\x8D\xAE",           /* 数据 */
    [UI_KEY_TAB_CTRL]  = "\xE6\x8E\xA7\xE5\x88\xB6",           /* 控制 */
    [UI_KEY_TAB_NET]   = "\xE7\xBD\x91\xE7\xBB\x9C",           /* 网络 */
    [UI_KEY_TAB_SET]   = "\xE8\xAE\xBE\xE7\xBD\xAE",           /* 设置 */
    [UI_KEY_STATUS_ONLINE]  = "\xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5",     /* 已连接 */
    [UI_KEY_STATUS_OFFLINE] = "\xE6\x9C\xAA\xE8\xBF\x9E\xE6\x8E\xA5",     /* 未连接 */
    [UI_KEY_BRAND]     = "\xE6\x99\xBA\xE8\x83\xBD\xE5\xAE\xB6\xE5\xB1\x85", /* 智能家居 */
    [UI_KEY_MODEL]     = "ESP32-P4",
    [UI_KEY_DATA_REALTIME] = "\xE8\xAE\xBE\xE5\xA4\x87\xE5\xAE\x9E\xE6\x97\xB6\xE6\x95\xB0\xE6\x8D\xAE", /* 设备实时数据 */
    [UI_KEY_SENSOR_LIGHT]  = "\xE5\x85\x89\xE6\x95\x8F\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8",   /* 光敏传感器 */
    [UI_KEY_SENSOR_SMOKE]  = "\xE7\x83\x9F\xE9\x9B\xBE\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8",   /* 烟雾传感器 */
    [UI_KEY_SENSOR_RAIN]   = "\xE9\x9B\xA8\xE6\xBB\xB4\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8",   /* 雨滴传感器 */
    [UI_KEY_DEVICE_COURTYARD] = "\xE5\xBA\xAD\xE9\x99\xA2\xE7\x81\xAF",  /* 庭院灯 */
    [UI_KEY_DEVICE_STRIP]     = "\xE7\x81\xAF\xE5\xB8\xA6",              /* 灯带 */
    [UI_KEY_DEVICE_GARDEN]    = "\xE8\x8F\x9C\xE7\x81\xAF",              /* 菜灯 */
    [UI_KEY_DEVICE_FAN]       = "\xE6\x8E\x92\xE6\xB0\x94\xE6\x89\x87",  /* 排气扇 */
    [UI_KEY_DEVICES]  = "\xE8\xAE\xBE\xE5\xA4\x87",   /* 设备 */
    [UI_KEY_SENSORS]  = "\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8", /* 传感器 */
    [UI_KEY_AUTO_MODE] = "\xE8\x87\xAA\xE5\x8A\xA8\xE6\xA8\xA1\xE5\xBC\x8F", /* 自动模式 */
    [UI_KEY_MANUAL]    = "\xE6\x89\x8B\xE5\x8A\xA8",   /* 手动 */
    [UI_KEY_ON]  = "\xE5\xBC\x80",  /* 开 */
    [UI_KEY_OFF] = "\xE5\x85\xB3",  /* 关 */
    [UI_KEY_NOT_CONNECTED] = "\xE6\x9C\xAA\xE6\x8E\xA5\xE5\x85\xA5", /* 未接入 */
    [UI_KEY_SET_TITLE]   = "\xE8\xAE\xBE\xE7\xBD\xAE",   /* 设置 */
    [UI_KEY_SET_LANGUAGE] = "\xE8\xAF\xAD\xE8\xA8\x80",  /* 语言 */
    [UI_KEY_SET_LANG_CURRENT] = "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87", /* 简体中文 */
    [UI_KEY_SET_WIFI]    = "WiFi",
    [UI_KEY_SET_WIFI_SCAN]      = "\xE6\x89\xAB\xE6\x8F\x8F",           /* 扫描 */
    [UI_KEY_SET_WIFI_SCANNING]  = "\xE6\x89\xAB\xE6\x8F\x8F\xE4\xB8\xAD...", /* 扫描中... */
    [UI_KEY_SET_WIFI_CONNECT]   = "\xE8\xBF\x9E\xE6\x8E\xA5",           /* 连接 */
    [UI_KEY_SET_WIFI_DISCONNECT] = "\xE6\x96\xAD\xE5\xBC\x80",          /* 断开 */
    [UI_KEY_SET_WIFI_CONNECTED]  = "\xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5",  /* 已连接 */
    [UI_KEY_SET_WIFI_PASSWORD]   = "\xE5\xAF\x86\xE7\xA0\x81",          /* 密码 */
    [UI_KEY_SET_WIFI_NO_AP]      = "\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0\xE7\xBD\x91\xE7\xBB\x9C", /* 未找到网络 */
    [UI_KEY_SET_WIFI_CONNECT_FAILED] = "\xE8\xBF\x9E\xE6\x8E\xA5\xE5\xA4\xB1\xE8\xB4\xA5", /* 连接失败 */
    [UI_KEY_SET_ABOUT]   = "\xE5\x85\xB3\xE4\xBA\x8E",   /* 关于 */
    [UI_KEY_SET_VERSION] = "\xE7\x89\x88\xE6\x9C\xAC",   /* 版本 */
    [UI_KEY_SET_RESET]   = "\xE6\x81\xA2\xE5\xA4\x8D\xE5\x87\xBA\xE5\x8E\x82", /* 恢复出厂 */
    [UI_KEY_SET_RESET_CONFIRM] = "\xE7\xA1\xAE\xE8\xAE\xA4\xE9\x87\x8D\xE7\xBD\xAE\xE6\x89\x80\xE6\x9C\x89\xE6\x95\xB0\xE6\x8D\xAE?", /* 确认重置所有数据? */
    [UI_KEY_CTRL_TITLE]  = "\xE6\x8E\xA7\xE5\x88\xB6\xE9\x9D\xA2\xE6\x9D\xBF", /* 控制面板 */
    [UI_KEY_CTRL_SCENE]  = "\xE5\x9C\xBA\xE6\x99\xAF\xE6\xA8\xA1\xE5\xBC\x8F", /* 场景模式 */
    [UI_KEY_CTRL_MORNING] = "\xE6\x97\xA9\xE5\xAE\x89",  /* 早安 */
    [UI_KEY_CTRL_NIGHT]   = "\xE6\x99\x9A\xE5\xAE\x89",  /* 晚安 */
    [UI_KEY_CTRL_AWAY]    = "\xE7\xA6\xBB\xE5\xAE\xB6",  /* 离家 */
    [UI_KEY_LUX]     = "lux",
    [UI_KEY_PPM]     = "ppm",
    [UI_KEY_PERCENT] = "%",
    [UI_KEY_GARDEN_LIGHTING] = "\xE7\x85\xA7\xE6\x98\x8E",   /* 照明 */
    [UI_KEY_GARDEN_DEVICES]  = "\xE8\xAE\xBE\xE5\xA4\x87",   /* 设备 */
    [UI_KEY_GARDEN_SENSORS]  = "\xE4\xBC\xA0\xE6\x84\x9F\xE5\x99\xA8", /* 传感器 */
    [UI_KEY_GARDEN_AUTO]     = "\xE8\x87\xAA\xE5\x8A\xA8\xE6\xA8\xA1\xE5\xBC\x8F", /* 自动模式 */
    [UI_KEY_GARDEN_CORRIDOR] = "\xE8\xB5\xB0\xE5\xBB\x8A\xE7\x81\xAF", /* 走廊灯 */
    [UI_KEY_GARDEN_STRIP]    = "\xE7\x81\xAF\xE5\xB8\xA6",   /* 灯带 */
    [UI_KEY_GARDEN_VEG]      = "\xE8\x8F\x9C\xE7\x81\xAF",   /* 菜灯 */
    [UI_KEY_GARDEN_FAN]      = "\xE6\x8E\x92\xE6\xB0\x94\xE6\x89\x87", /* 排气扇 */
    [UI_KEY_GARDEN_SERVO]    = "\xE6\x99\xBE\xE8\xA1\xA3\xE6\x9E\xB6", /* 晾衣架 */
    [UI_KEY_GARDEN_HUNG_OUT]    = "\xE5\xB7\xB2\xE6\x99\xBE\xE5\x87\xBA", /* 已晾出 */
    [UI_KEY_GARDEN_COLLECTED]   = "\xE5\xB7\xB2\xE6\x94\xB6\xE5\x9B\x9E", /* 已收回 */
    [UI_KEY_GARDEN_HANG]        = "\xE6\x99\xBE\xE5\x87\xBA",   /* 晾出 */
    [UI_KEY_GARDEN_COLLECT]     = "\xE6\x94\xB6\xE5\x9B\x9E",   /* 收回 */
    [UI_KEY_GARDEN_SOIL]        = "\xE5\x9C\x9F\xE5\xA3\xA4\xE6\xB9\xBF\xE5\xBA\xA6", /* 土壤湿度 */
    [UI_KEY_GARDEN_LIGHT]       = "\xE5\x85\x89\xE7\x85\xA7",   /* 光照 */
    [UI_KEY_GARDEN_RAIN]        = "\xE9\x9B\xA8\xE6\xBB\xB4",   /* 雨滴 */
    [UI_KEY_GARDEN_RAINING]     = "\xE4\xB8\x8B\xE9\x9B\xA8\xE4\xB8\xAD", /* 下雨中 */
    [UI_KEY_GARDEN_DRY]         = "\xE6\x9C\xAA\xE4\xB8\x8B\xE9\x9B\xA8", /* 未下雨 */
    [UI_KEY_GARDEN_AUTO_MODE]   = "\xE8\x87\xAA\xE5\x8A\xA8",   /* 自动 */
    [UI_KEY_GARDEN_MANUAL]      = "\xE6\x89\x8B\xE5\x8A\xA8",   /* 手动 */
};

static const char *s_en[] = {
    [UI_KEY_TAB_DATA]  = "Data",
    [UI_KEY_TAB_CTRL]  = "Control",
    [UI_KEY_TAB_NET]   = "Network",
    [UI_KEY_TAB_SET]   = "Settings",
    [UI_KEY_STATUS_ONLINE]  = "Online",
    [UI_KEY_STATUS_OFFLINE] = "Offline",
    [UI_KEY_BRAND]     = "Smart Home",
    [UI_KEY_MODEL]     = "ESP32-P4",
    [UI_KEY_DATA_REALTIME] = "Real-time Data",
    [UI_KEY_SENSOR_LIGHT]  = "Light Sensor",
    [UI_KEY_SENSOR_SMOKE]  = "Smoke Sensor",
    [UI_KEY_SENSOR_RAIN]   = "Rain Sensor",
    [UI_KEY_DEVICE_COURTYARD] = "Courtyard Light",
    [UI_KEY_DEVICE_STRIP]     = "Strip Light",
    [UI_KEY_DEVICE_GARDEN]    = "Garden Light",
    [UI_KEY_DEVICE_FAN]       = "Exhaust Fan",
    [UI_KEY_DEVICES]  = "Devices",
    [UI_KEY_SENSORS]  = "Sensors",
    [UI_KEY_AUTO_MODE] = "Auto Mode",
    [UI_KEY_MANUAL]    = "Manual",
    [UI_KEY_ON]  = "On",
    [UI_KEY_OFF] = "Off",
    [UI_KEY_NOT_CONNECTED] = "Not Connected",
    [UI_KEY_SET_TITLE]   = "Settings",
    [UI_KEY_SET_LANGUAGE] = "Language",
    [UI_KEY_SET_LANG_CURRENT] = "English",
    [UI_KEY_SET_WIFI]    = "WiFi",
    [UI_KEY_SET_WIFI_SCAN]      = "Scan",
    [UI_KEY_SET_WIFI_SCANNING]  = "Scanning...",
    [UI_KEY_SET_WIFI_CONNECT]   = "Connect",
    [UI_KEY_SET_WIFI_DISCONNECT] = "Disconnect",
    [UI_KEY_SET_WIFI_CONNECTED]  = "Connected",
    [UI_KEY_SET_WIFI_PASSWORD]   = "Password",
    [UI_KEY_SET_WIFI_NO_AP]      = "No networks found",
    [UI_KEY_SET_WIFI_CONNECT_FAILED] = "Connection failed",
    [UI_KEY_SET_ABOUT]   = "About",
    [UI_KEY_SET_VERSION] = "Version",
    [UI_KEY_SET_RESET]   = "Reset",
    [UI_KEY_SET_RESET_CONFIRM] = "Reset all data?",
    [UI_KEY_CTRL_TITLE]  = "Control Panel",
    [UI_KEY_CTRL_SCENE]  = "Scene Mode",
    [UI_KEY_CTRL_MORNING] = "Morning",
    [UI_KEY_CTRL_NIGHT]   = "Night",
    [UI_KEY_CTRL_AWAY]    = "Away",
    [UI_KEY_LUX]     = "lux",
    [UI_KEY_PPM]     = "ppm",
    [UI_KEY_PERCENT] = "%",
    [UI_KEY_GARDEN_LIGHTING] = "Lighting",
    [UI_KEY_GARDEN_DEVICES]  = "Devices",
    [UI_KEY_GARDEN_SENSORS]  = "Sensors",
    [UI_KEY_GARDEN_AUTO]     = "Auto Mode",
    [UI_KEY_GARDEN_CORRIDOR] = "Corridor Light",
    [UI_KEY_GARDEN_STRIP]    = "Strip Light",
    [UI_KEY_GARDEN_VEG]      = "Veg Light",
    [UI_KEY_GARDEN_FAN]      = "Exhaust Fan",
    [UI_KEY_GARDEN_SERVO]    = "Clothes Rack",
    [UI_KEY_GARDEN_HUNG_OUT]    = "Hung Out",
    [UI_KEY_GARDEN_COLLECTED]   = "Collected",
    [UI_KEY_GARDEN_HANG]        = "Hang Out",
    [UI_KEY_GARDEN_COLLECT]     = "Collect",
    [UI_KEY_GARDEN_SOIL]        = "Soil Moisture",
    [UI_KEY_GARDEN_LIGHT]       = "Light",
    [UI_KEY_GARDEN_RAIN]        = "Rain",
    [UI_KEY_GARDEN_RAINING]     = "Raining",
    [UI_KEY_GARDEN_DRY]         = "Dry",
    [UI_KEY_GARDEN_AUTO_MODE]   = "Auto",
    [UI_KEY_GARDEN_MANUAL]      = "Manual",
};

void ui_i18n_init(void)
{
    s_lang = UI_LANG_ZH;
}

const char *ui_i18n_get(ui_i18n_key_t key)
{
    if (key < 0 || key >= UI_KEY_COUNT) return "";
    return s_lang == UI_LANG_ZH ? s_zh[key] : s_en[key];
}

ui_lang_t ui_i18n_get_lang(void)
{
    return s_lang;
}

void ui_i18n_set_lang(ui_lang_t lang)
{
    s_lang = lang;
    ui_event_publish(UI_EVENT_LANG_CHANGED);
}

const lv_font_t *ui_i18n_font(const lv_font_t *fallback)
{
    uint8_t req_size = 14;
    if (fallback == UI_FONT_12) req_size = 12;
    else if (fallback == UI_FONT_14) req_size = 14;
    else if (fallback == UI_FONT_16) req_size = 16;
    else if (fallback == UI_FONT_20) req_size = 20;
    else if (fallback == UI_FONT_24) req_size = 24;

    if (s_lang == UI_LANG_ZH) {
        return ui_font_cn(req_size);
    }
    return fallback;
}
