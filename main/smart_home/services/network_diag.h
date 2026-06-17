#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NET_DIAG_BOOT = 0,
    NET_DIAG_HOST_WAIT,
    NET_DIAG_WIFI_READY,
    NET_DIAG_MQTT_CONNECTING,
    NET_DIAG_MQTT_READY,
    NET_DIAG_DEGRADED,
    NET_DIAG_OFFLINE_DEMO,
} network_diag_state_t;

typedef struct {
    network_diag_state_t state;
    uint32_t reconnect_count;
    int last_error;
    char last_error_text[64];
    int64_t last_error_ms;
    int64_t last_mqtt_rx_ms;
    bool offline_demo;
} network_diag_t;

void network_diag_init(void);
void network_diag_set_state(network_diag_state_t state);
void network_diag_set_error(int err, const char *text);
void network_diag_mark_mqtt_rx(void);
void network_diag_set_offline_demo(bool enabled);
const network_diag_t *network_diag_get(void);

#ifdef __cplusplus
}
#endif
