#include "wifi_compat.h"

#include "mqtt_device_model.h"
#include "ui_events.h"

#include <esp_log.h>

#include <cstring>

static const char* TAG = "SH_WIFI_COMPAT";
static wifi_mgr_connect_cb_t s_connect_cb = nullptr;

extern "C" void wifi_manager_init(void) {
    ESP_LOGI(TAG, "Using Xiaozhi board network manager; legacy WiFi manager is disabled");
}

extern "C" void wifi_manager_scan(wifi_mgr_scan_cb_t cb) {
    if (cb) {
        cb(nullptr, 0);
    }
}

extern "C" void wifi_manager_connect(const char* ssid, const char* password) {
    (void)ssid;
    (void)password;
    if (s_connect_cb) {
        s_connect_cb(device_model_get()->wifi_state == WIFI_STATE_CONNECTED);
    }
}

extern "C" void wifi_manager_disconnect(void) {
    device_model_update_wifi_state(WIFI_STATE_IDLE, nullptr);
    ui_event_publish(UI_EVENT_WIFI_CHANGED);
}

extern "C" void wifi_manager_auto_connect(void) {
}

extern "C" bool wifi_manager_is_connected(void) {
    return device_model_get()->wifi_state == WIFI_STATE_CONNECTED;
}

extern "C" const char* wifi_manager_get_ip(void) {
    return "";
}

extern "C" const char* wifi_manager_get_ssid(void) {
    const mqtt_device_model_t* m = device_model_get();
    return m->wifi_ssid[0] ? m->wifi_ssid : "";
}

extern "C" int8_t wifi_manager_get_rssi(void) {
    return 0;
}

extern "C" void wifi_manager_set_connect_cb(wifi_mgr_connect_cb_t cb) {
    s_connect_cb = cb;
}
