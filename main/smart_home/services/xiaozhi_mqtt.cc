#include "xiaozhi_mqtt.h"

#include "board.h"
#include "mqtt.h"
#include "mqtt_device_model.h"
#include "mqtt_iot_protocol.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <wifi_manager.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

static const char* TAG = "SH_MQTT";

static constexpr const char* kBrokerHost = "8.134.167.240";
static constexpr int kBrokerPort = 1883;
static constexpr const char* kClientId = "xiaozhi_p4_host";
static constexpr int kHeartbeatTimeoutSeconds = 60;

static std::unique_ptr<Mqtt> s_mqtt;
static bool s_connected = false;
static bool s_connecting = false;
static bool s_initialized = false;
static uint32_t s_mqtt_rx_count = 0;
static int64_t s_last_rx_time_s = 0;
static int64_t s_last_heartbeat[4] = {0};
static bool s_controller_online[4] = {false};
static int64_t s_last_network_wait_log_ms = 0;

static bool topic_starts_with(const std::string& topic, const char* prefix) {
    return topic.rfind(prefix, 0) == 0;
}

static void process_announce(const std::string& payload) {
    if (payload.size() < sizeof(iot_announce_v2_packet_t)) {
        return;
    }

    const auto* pkt = reinterpret_cast<const iot_announce_v2_packet_t*>(payload.data());
    char name[25] = {0};
    memcpy(name, pkt->device_name, sizeof(pkt->device_name));

    ESP_LOGI(TAG, "Announce: device_id=%u name=%s gpio_count=%u",
             pkt->device_id, name, pkt->gpio_index);

    if (pkt->device_id >= 1 && pkt->device_id <= 3) {
        device_model_set_controller_online(pkt->device_id, true);
        s_controller_online[pkt->device_id] = true;
        s_last_heartbeat[pkt->device_id] = esp_timer_get_time() / 1000000;
    }
}

static void process_heartbeat(const std::string& payload) {
    if (payload.size() < sizeof(iot_heartbeat_packet_t)) {
        return;
    }

    const auto* pkt = reinterpret_cast<const iot_heartbeat_packet_t*>(payload.data());
    ESP_LOGD(TAG, "Heartbeat: device_id=%u mac=%s", pkt->device_id, pkt->mac_str);

    if (pkt->device_id >= 1 && pkt->device_id <= 3) {
        device_model_set_controller_online(pkt->device_id, true);
        s_controller_online[pkt->device_id] = true;
        s_last_heartbeat[pkt->device_id] = esp_timer_get_time() / 1000000;
    }
}

static void process_response(const std::string& payload) {
    if (payload.size() < sizeof(iot_command_packet_t)) {
        return;
    }

    const auto* pkt = reinterpret_cast<const iot_command_packet_t*>(payload.data());
    ESP_LOGI(TAG, "Response: cmd=0x%02X dev=%u gpio=%u val=%u",
             pkt->command, pkt->device_id, pkt->gpio_index, pkt->value);

    if (pkt->device_id >= 1 && pkt->device_id <= 3) {
        device_model_set_controller_online(pkt->device_id, true);
    }

    for (uint16_t i = 0; i < device_model_count(); i++) {
        const rc_device_t* d = device_model_at(i);
        if (!d) {
            continue;
        }
        if (d->floor_id == pkt->device_id &&
            d->cmd_type == pkt->command &&
            d->gpio_index == pkt->gpio_index) {
            device_model_apply_power(i, pkt->value != 0);
            break;
        }
    }
}

static void process_sensor(const std::string& payload) {
    if (payload.size() < sizeof(iot_command_packet_t)) {
        return;
    }

    const auto* pkt = reinterpret_cast<const iot_command_packet_t*>(payload.data());
    if (pkt->command != IOT_CMD_SENSOR_REPORT) {
        return;
    }

    uint16_t sensor_val = static_cast<uint16_t>(pkt->value) |
                          (static_cast<uint16_t>(pkt->reserved[0]) << 8);
    ESP_LOGI(TAG, "Sensor: floor=%u type=%u val=%u",
             pkt->device_id, pkt->gpio_index, sensor_val);
    device_model_update_sensor_value(pkt->device_id, pkt->gpio_index, sensor_val);
}

static void publish_discover(void) {
    iot_command_packet_t discover = {};
    discover.command = IOT_CMD_DISCOVER;
    std::string payload(reinterpret_cast<const char*>(&discover), sizeof(discover));
    s_mqtt->Publish(MQTT_TOPIC_CMD_BROADCAST, payload, 0);
}

static void ensure_client_created(void) {
    if (s_mqtt) {
        return;
    }

    auto network = Board::GetInstance().GetNetwork();
    s_mqtt = network->CreateMqtt(1);
    s_mqtt->SetKeepAlive(90);

    s_mqtt->OnConnected([]() {
        s_connected = true;
        s_connecting = false;
        device_model_set_mqtt_state(MQTT_STATE_CONNECTED);

        s_mqtt->Subscribe("xiaozhi/iot/announce/#", 1);
        s_mqtt->Subscribe("xiaozhi/iot/heartbeat/#", 0);
        s_mqtt->Subscribe("xiaozhi/iot/resp/#", 1);
        s_mqtt->Subscribe("xiaozhi/iot/sensor/#", 0);
        publish_discover();

        ESP_LOGI(TAG, "Connected and subscribed to smart-home topics");
    });

    s_mqtt->OnDisconnected([]() {
        s_connected = false;
        s_connecting = false;
        memset(s_last_heartbeat, 0, sizeof(s_last_heartbeat));
        memset(s_controller_online, 0, sizeof(s_controller_online));
        device_model_set_mqtt_state(MQTT_STATE_DISCONNECTED);
        device_model_reset_runtime_data();
        ESP_LOGW(TAG, "Disconnected");
    });

    s_mqtt->OnError([](const std::string& error) {
        s_connecting = false;
        ESP_LOGW(TAG, "MQTT error: %s", error.c_str());
    });

    s_mqtt->OnMessage([](const std::string& topic, const std::string& payload) {
        s_mqtt_rx_count++;
        s_last_rx_time_s = esp_timer_get_time() / 1000000;
        device_model_set_mqtt_stats(s_mqtt_rx_count, 0);

        if (topic_starts_with(topic, MQTT_TOPIC_ANNOUNCE_PREFIX) ||
            topic == MQTT_TOPIC_ANNOUNCE) {
            process_announce(payload);
        } else if (topic_starts_with(topic, MQTT_TOPIC_HEARTBEAT_PREFIX)) {
            process_heartbeat(payload);
        } else if (topic_starts_with(topic, MQTT_TOPIC_RESP_PREFIX)) {
            process_response(payload);
        } else if (topic_starts_with(topic, MQTT_TOPIC_SENSOR_PREFIX)) {
            process_sensor(payload);
        }
    });
}

static bool network_ready(void) {
    auto& wifi = WifiManager::GetInstance();
    return wifi.IsInitialized() && wifi.IsConnected();
}

extern "C" void mqtt_client_init(void) {
    if (s_initialized) {
        return;
    }
    s_initialized = true;
    device_model_set_mqtt_state(MQTT_STATE_DISCONNECTED);
    ESP_LOGI(TAG, "Smart-home MQTT initialized");
}

extern "C" void mqtt_client_start(void) {
    if (s_connected || s_connecting) {
        return;
    }

    if (!network_ready()) {
        int64_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms - s_last_network_wait_log_ms > 15000) {
            s_last_network_wait_log_ms = now_ms;
            ESP_LOGI(TAG, "Waiting for WiFi before smart-home MQTT connect");
        }
        device_model_set_mqtt_state(MQTT_STATE_DISCONNECTED);
        return;
    }

    ensure_client_created();
    s_connecting = true;

    ESP_LOGI(TAG, "Connecting to mqtt://%s:%d", kBrokerHost, kBrokerPort);
    if (!s_mqtt->Connect(kBrokerHost, kBrokerPort, kClientId, "", "")) {
        ESP_LOGW(TAG, "Connect failed, code=%d", s_mqtt->GetLastError());
        s_connecting = false;
        device_model_set_mqtt_state(MQTT_STATE_FAILED);
    }
}

extern "C" void mqtt_client_stop(void) {
    if (!s_mqtt) {
        return;
    }
    s_mqtt->Disconnect();
    s_connected = false;
    s_connecting = false;
    device_model_set_mqtt_state(MQTT_STATE_DISCONNECTED);
}

extern "C" bool mqtt_client_is_connected(void) {
    return s_connected && s_mqtt && s_mqtt->IsConnected();
}

extern "C" void mqtt_client_poll(void) {
    if (!mqtt_client_is_connected()) {
        return;
    }

    int64_t now = esp_timer_get_time() / 1000000;
    if (s_last_rx_time_s > 0) {
        device_model_set_mqtt_stats(s_mqtt_rx_count, static_cast<uint32_t>(now - s_last_rx_time_s));
    }

    for (uint8_t floor_id = 1; floor_id <= 3; floor_id++) {
        if (s_controller_online[floor_id] &&
            s_last_heartbeat[floor_id] > 0 &&
            now - s_last_heartbeat[floor_id] > kHeartbeatTimeoutSeconds) {
            ESP_LOGW(TAG, "Floor %u heartbeat timeout", floor_id);
            s_controller_online[floor_id] = false;
            s_last_heartbeat[floor_id] = 0;
            device_model_set_floor_runtime_offline(floor_id);
        }
    }
}

extern "C" void mqtt_send_command(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value) {
    if (!mqtt_client_is_connected()) {
        ESP_LOGW(TAG, "Cannot send command: MQTT not connected");
        return;
    }

    iot_command_packet_t pkt = {};
    pkt.command = cmd_type;
    pkt.device_id = floor_id;
    pkt.gpio_index = gpio_index;
    pkt.value = value;

    char topic[48];
    snprintf(topic, sizeof(topic), "%s%u", MQTT_TOPIC_CMD_PREFIX, floor_id);

    std::string payload(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    s_mqtt->Publish(topic, payload, 0);
    ESP_LOGI(TAG, "CMD -> %s: cmd=0x%02X gpio=%u val=%u", topic, cmd_type, gpio_index, value);
}

extern "C" void mqtt_send_broadcast(uint8_t cmd_type) {
    if (!mqtt_client_is_connected()) {
        ESP_LOGW(TAG, "Cannot broadcast: MQTT not connected");
        return;
    }

    iot_command_packet_t pkt = {};
    pkt.command = cmd_type;

    std::string payload(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    s_mqtt->Publish(MQTT_TOPIC_CMD_BROADCAST, payload, 0);
    ESP_LOGI(TAG, "BROADCAST cmd=0x%02X", cmd_type);
}
