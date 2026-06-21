#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define WIFI_MGR_MAX_SCAN 20
#define WIFI_MGR_SSID_MAX 33
#define WIFI_MGR_PASS_MAX 64

typedef struct {
    char ssid[WIFI_MGR_SSID_MAX];
    int8_t rssi;
    uint8_t authmode;
} wifi_mgr_ap_t;

typedef void (*wifi_mgr_scan_cb_t)(const wifi_mgr_ap_t *aps, uint16_t count);
typedef void (*wifi_mgr_connect_cb_t)(bool success);

void wifi_manager_init(void);
void wifi_manager_scan(wifi_mgr_scan_cb_t cb);
void wifi_manager_connect(const char *ssid, const char *password);
void wifi_manager_disconnect(void);
void wifi_manager_auto_connect(void);

bool wifi_manager_is_connected(void);
const char *wifi_manager_get_ip(void);
const char *wifi_manager_get_ssid(void);
int8_t wifi_manager_get_rssi(void);

void wifi_manager_set_connect_cb(wifi_mgr_connect_cb_t cb);

#ifdef __cplusplus
}
#endif
