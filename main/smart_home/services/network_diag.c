#include "network_diag.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *TAG = "NET_DIAG";
static network_diag_t s_diag;
static network_diag_state_t s_pre_demo_state = NET_DIAG_BOOT;

void network_diag_init(void)
{
    memset(&s_diag, 0, sizeof(s_diag));
    s_diag.state = NET_DIAG_BOOT;
}

void network_diag_set_state(network_diag_state_t state)
{
    s_diag.state = state;
    if (state == NET_DIAG_MQTT_CONNECTING) {
        s_diag.reconnect_count++;
    }
    if (state == NET_DIAG_MQTT_READY) {
        s_diag.last_error_text[0] = '\0';
    }
    ESP_LOGI(TAG, "State -> %d", state);
}

void network_diag_set_error(int err, const char *text)
{
    s_diag.last_error = err;
    if (text) {
        strncpy(s_diag.last_error_text, text, sizeof(s_diag.last_error_text) - 1);
        s_diag.last_error_text[sizeof(s_diag.last_error_text) - 1] = '\0';
    }
    s_diag.last_error_ms = esp_timer_get_time() / 1000;
}

void network_diag_mark_mqtt_rx(void)
{
    s_diag.last_mqtt_rx_ms = esp_timer_get_time() / 1000;
}

void network_diag_set_offline_demo(bool enabled)
{
    s_diag.offline_demo = enabled;
    if (enabled) {
        s_pre_demo_state = s_diag.state;
        s_diag.state = NET_DIAG_OFFLINE_DEMO;
    } else {
        s_diag.state = s_pre_demo_state;
    }
}

const network_diag_t *network_diag_get(void)
{
    return &s_diag;
}
