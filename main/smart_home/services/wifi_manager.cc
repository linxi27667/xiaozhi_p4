#include "wifi_manager.h"

#include "mqtt_device_model.h"
#include "ui_events.h"

#include <esp_log.h>

#include <cstring>

static const char* TAG = "SH_WIFI_COMPAT";
static wifi_mgr_connect_cb_t s_connect_cb = nullptr;

extern "C" void wifi_manager_init(void) {
    device_model_update_wifi_state(WIFI_STATE_CONNECTED, "xiaozhi-network");
    ESP_LOGI(TAG, "Using Xiaozhi board network manager; legacy WiFi manager is disabled");
}

extern "C" void wifi_manager_scan(wifi_mgr_scan_cb_t cb) {
    device_model_update_wifi_state(WIFI_STATE_SCANNING, nullptr);
    if (cb) {
        cb(nullptr, 0);
    }
    device_model_update_wifi_state(WIFI_STATE_CONNECTED, "xiaozhi-network");
}

extern "C" void wifi_manager_connect(const char* ssid, const char* password) {
    (void)password;
    device_model_update_wifi_state(WIFI_STATE_CONNECTED, ssid && ssid[0] ? ssid : "xiaozhi-network");
    ui_event_publish(UI_EVENT_WIFI_CHANGED);
    if (s_connect_cb) {
        s_connect_cb(true);
    }
}

extern "C" void wifi_manager_disconnect(void) {
    device_model_update_wifi_state(WIFI_STATE_IDLE, nullptr);
    ui_event_publish(UI_EVENT_WIFI_CHANGED);
}

extern "C" void wifi_manager_auto_connect(void) {
    device_model_update_wifi_state(WIFI_STATE_CONNECTED, "xiaozhi-network");
    ui_event_publish(UI_EVENT_WIFI_CHANGED);
}

extern "C" bool wifi_manager_is_connected(void) {
    return true;
}

extern "C" const char* wifi_manager_get_ip(void) {
    return "";
}

extern "C" const char* wifi_manager_get_ssid(void) {
    return "xiaozhi-network";
}

extern "C" int8_t wifi_manager_get_rssi(void) {
    return 0;
}

extern "C" void wifi_manager_set_connect_cb(wifi_mgr_connect_cb_t cb) {
    s_connect_cb = cb;
}
