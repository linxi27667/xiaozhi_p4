#include "xiaozhi_mqtt.h"

#include "board.h"
#include "mqtt.h"
#include "mqtt_device_model.h"
#include "mqtt_iot_protocol.h"
#include "ui_events.h"

#include <esp_log.h>
#include <esp_timer.h>
#include "wifi_compat.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
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
/* mqtt_stats UI 刷新节流: 避免每条 MQTT 消息都触发 UI 全量刷新 */
static int64_t s_last_stats_publish_ms = 0;
static constexpr int64_t kStatsPublishIntervalMs = 5000;  /* 5秒最多刷新一次 */
static bool s_controller_online[4] = {false};
static int64_t s_last_network_wait_log_ms = 0;
static bool s_pending_scene_valid = false;
static uint8_t s_pending_scene_id = IOT_SCENE_NONE;
static bool s_flushing_pending_scene = false;

struct RgbBlinkState {
    bool active = false;
    bool light_on = false;
    uint8_t floor_id = 0;
    uint8_t index = 0;
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    uint8_t brightness = 0;
    uint8_t remaining_flashes = 0;
};

static std::mutex s_rgb_command_mutex;
static RgbBlinkState s_rgb_blink;

/* MAC->楼层映射表: 从机 announce/heartbeat 时注册, 用于 response topic 反查楼层 */
static char s_floor_mac[4][13] = {{0}};

static uint8_t canonical_servo_index(uint8_t gpio_index) {
    return (gpio_index < 6) ? (uint8_t)(gpio_index + 6) : gpio_index;
}

static bool rain_hanger_lock_active(void) {
    uint8_t rain_status = 0;
    return device_model_get_rain_status(3, &rain_status) && rain_status != 0;
}

static bool is_hanger_servo_command(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index) {
    return cmd_type == IOT_CMD_SET_SERVO &&
           (floor_id == 2 || floor_id == 3) &&
           canonical_servo_index(gpio_index) == 8;
}

static bool topic_starts_with(const std::string& topic, const char* prefix) {
    return topic.rfind(prefix, 0) == 0;
}

static void register_floor_mac(uint8_t floor_id, const char* mac_str) {
    if (floor_id >= 1 && floor_id <= 3 && mac_str) {
        strncpy(s_floor_mac[floor_id], mac_str, 12);
        s_floor_mac[floor_id][12] = '\0';
    }
}

static void process_announce(const std::string& topic, const std::string& payload) {
    if (payload.size() < sizeof(iot_announce_v2_packet_t)) {
        return;
    }

    const auto* pkt = reinterpret_cast<const iot_announce_v2_packet_t*>(payload.data());
    char name[25] = {0};
    memcpy(name, pkt->device_name, sizeof(pkt->device_name));

    ESP_LOGI(TAG, "Announce: device_id=%u name=%s gpio_count=%u",
             pkt->device_id, name, pkt->gpio_index);

    if (pkt->device_id >= 1 && pkt->device_id <= 3) {
        /* 从 topic 提取 MAC 并注册 */
        if (topic.size() > strlen(MQTT_TOPIC_ANNOUNCE_PREFIX)) {
            std::string mac = topic.substr(strlen(MQTT_TOPIC_ANNOUNCE_PREFIX));
            register_floor_mac(pkt->device_id, mac.c_str());
        }
        device_model_set_controller_online(pkt->device_id, true);
        s_controller_online[pkt->device_id] = true;
        s_last_heartbeat[pkt->device_id] = esp_timer_get_time() / 1000000;
    }
}

static void process_heartbeat(const std::string& topic, const std::string& payload) {
    /* 先检查 V2 心跳(小包), 再检查 V3(大包), 避免 V2 被误判为 V3 */
    if (payload.size() >= sizeof(iot_heartbeat_v2_packet_t)) {
        const auto* pkt = reinterpret_cast<const iot_heartbeat_v2_packet_t*>(payload.data());
        if (pkt->command == IOT_CMD_HEARTBEAT &&
            pkt->device_id >= 1 && pkt->device_id <= 3) {
            /* 从 topic 提取 MAC 并注册 */
            if (topic.size() > strlen(MQTT_TOPIC_HEARTBEAT_PREFIX)) {
                std::string mac = topic.substr(strlen(MQTT_TOPIC_HEARTBEAT_PREFIX));
                register_floor_mac(pkt->device_id, mac.c_str());
            }

            if (pkt->protocol_version == IOT_PROTOCOL_VERSION_V3 &&
                payload.size() >= sizeof(iot_heartbeat_v3_packet_t)) {
                /* V3 心跳: 含 RGB 扩展字段 */
                const auto* v3 = reinterpret_cast<const iot_heartbeat_v3_packet_t*>(payload.data());
                ESP_LOGD(TAG, "Heartbeat V3: device_id=%u rgb_count=%u scene=%u",
                         pkt->device_id, v3->rgb_count, v3->current_scene);
                device_model_apply_heartbeat_v3(v3);
            } else if (pkt->protocol_version == IOT_PROTOCOL_VERSION) {
                /* V2 心跳: 基础字段 */
                ESP_LOGD(TAG, "Heartbeat V2: device_id=%u name=%.*s mac=%s",
                         pkt->device_id, (int)sizeof(pkt->device_name), pkt->device_name, pkt->mac_str);
                device_model_apply_heartbeat(pkt);
            } else {
                /* 旧版 V1 心跳: 仅更新在线状态 */
                ESP_LOGD(TAG, "Heartbeat V1: device_id=%u", pkt->device_id);
                device_model_set_controller_online(pkt->device_id, true);
            }
            s_controller_online[pkt->device_id] = true;
            s_last_heartbeat[pkt->device_id] = esp_timer_get_time() / 1000000;
            return;
        }
    }

    if (payload.size() < sizeof(iot_heartbeat_packet_t)) {
        return;
    }

    const auto* pkt = reinterpret_cast<const iot_heartbeat_packet_t*>(payload.data());
    ESP_LOGD(TAG, "Heartbeat(legacy): device_id=%u mac=%s", pkt->device_id, pkt->mac_str);

    if (pkt->device_id >= 1 && pkt->device_id <= 3) {
        device_model_set_controller_online(pkt->device_id, true);
        s_controller_online[pkt->device_id] = true;
        s_last_heartbeat[pkt->device_id] = esp_timer_get_time() / 1000000;
    }
}

static uint8_t floor_id_from_response_topic(const std::string& topic) {
    if (!topic_starts_with(topic, MQTT_TOPIC_RESP_PREFIX)) {
        return 0;
    }
    /* 从 topic "xiaozhi/iot/resp/{mac}" 提取 MAC, 查 MAC->楼层映射表 */
    std::string mac = topic.substr(strlen(MQTT_TOPIC_RESP_PREFIX));
    for (uint8_t floor_id = 1; floor_id <= 3; floor_id++) {
        if (s_floor_mac[floor_id][0] != '\0' && mac == s_floor_mac[floor_id]) {
            return floor_id;
        }
    }
    return 0;
}

static void process_response(const std::string& topic, const std::string& payload) {
    if (payload.size() >= sizeof(iot_rgb_light_packet_t)) {
        const auto* rgb = reinterpret_cast<const iot_rgb_light_packet_t*>(payload.data());
        if (rgb->command == IOT_CMD_SET_RGB_LIGHT &&
            rgb->protocol_version == IOT_PROTOCOL_VERSION_V3 &&
            rgb->device_id >= 1 && rgb->device_id <= 3) {
            ESP_LOGI(TAG, "RGB response: dev=%u idx=%u rgb=(%u,%u,%u) br=%u effect=%u param=%u",
                     rgb->device_id, rgb->index, rgb->red, rgb->green, rgb->blue,
                     rgb->brightness, rgb->effect, rgb->speed);
            device_model_apply_rgb_ack(rgb->device_id, rgb->index, rgb->red, rgb->green,
                                       rgb->blue, rgb->brightness, rgb->effect);
            return;
        }
    }

    if (payload.size() < sizeof(iot_command_packet_t)) {
        return;
    }

    const auto* pkt = reinterpret_cast<const iot_command_packet_t*>(payload.data());
    ESP_LOGI(TAG, "Response: cmd=0x%02X dev=%u gpio=%u val=%u",
             pkt->command, pkt->device_id, pkt->gpio_index, pkt->value);

    uint8_t floor_id = pkt->device_id;
    if (floor_id == 0) {
        floor_id = floor_id_from_response_topic(topic);
    }
    if (floor_id >= 1 && floor_id <= 3) {
        device_model_apply_command_ack(floor_id, pkt->command, pkt->gpio_index, pkt->value);
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
    ESP_LOGD(TAG, "Sensor: floor=%u type=%u val=%u",
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

        /* 通知 UI MQTT 已连接(网络页实时刷新) */
        ui_event_publish(UI_EVENT_MQTT_CONNECTED);

        ESP_LOGI(TAG, "Connected and subscribed to smart-home topics");
    });

    s_mqtt->OnDisconnected([]() {
        s_connected = false;
        s_connecting = false;
        memset(s_last_heartbeat, 0, sizeof(s_last_heartbeat));
        memset(s_controller_online, 0, sizeof(s_controller_online));
        device_model_set_mqtt_state(MQTT_STATE_DISCONNECTED);
        device_model_reset_runtime_data();
        /* 通知 UI MQTT 已断开 */
        ui_event_publish(UI_EVENT_MQTT_DISCONNECTED);
        ESP_LOGW(TAG, "Disconnected");
    });

    s_mqtt->OnError([](const std::string& error) {
        s_connecting = false;
        ESP_LOGW(TAG, "MQTT error: %s", error.c_str());
    });

    s_mqtt->OnMessage([](const std::string& topic, const std::string& payload) {
        s_mqtt_rx_count++;
        s_last_rx_time_s = esp_timer_get_time() / 1000000;

        /* mqtt_stats 节流: 每 5 秒最多触发一次 UI 刷新,避免高频心跳导致 UI 卡顿 */
        int64_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms - s_last_stats_publish_ms >= kStatsPublishIntervalMs) {
            s_last_stats_publish_ms = now_ms;
            device_model_set_mqtt_stats(s_mqtt_rx_count, 0);
        }

        if (topic_starts_with(topic, MQTT_TOPIC_ANNOUNCE_PREFIX) ||
            topic == MQTT_TOPIC_ANNOUNCE) {
            process_announce(topic, payload);
        } else if (topic_starts_with(topic, MQTT_TOPIC_HEARTBEAT_PREFIX)) {
            process_heartbeat(topic, payload);
        } else if (topic_starts_with(topic, MQTT_TOPIC_RESP_PREFIX)) {
            process_response(topic, payload);
        } else if (topic_starts_with(topic, MQTT_TOPIC_SENSOR_PREFIX)) {
            process_sensor(payload);
        }
    });
}

static bool network_ready(void) {
    return wifi_manager_is_connected();
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

    if (s_pending_scene_valid) {
        uint8_t scene_id = s_pending_scene_id;
        s_pending_scene_valid = false;
        s_pending_scene_id = IOT_SCENE_NONE;
        ESP_LOGI(TAG, "Flush queued scene after MQTT connected: scene=%u", scene_id);
        s_flushing_pending_scene = true;
        mqtt_send_scene(scene_id);
        s_flushing_pending_scene = false;
    }

    int64_t now = esp_timer_get_time() / 1000000;
    /* poll 中也节流 mqtt_stats 更新,与 OnMessage 共享节流变量,避免重复刷新 */
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (s_last_rx_time_s > 0 &&
        now_ms - s_last_stats_publish_ms >= kStatsPublishIntervalMs) {
        s_last_stats_publish_ms = now_ms;
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

extern "C" bool mqtt_send_command(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value) {
    if (!mqtt_client_is_connected()) {
        ESP_LOGW(TAG, "Cannot send command: MQTT not connected");
        return false;
    }

    if (cmd_type == IOT_CMD_SET_SERVO) {
        gpio_index = canonical_servo_index(gpio_index);
    }
    if (is_hanger_servo_command(floor_id, cmd_type, gpio_index) &&
        value > 0 && rain_hanger_lock_active()) {
        ESP_LOGW(TAG, "Rain lock: force floor %u hanger closed, requested angle=%u",
                 floor_id, value);
        value = 0;
    }

    iot_command_packet_t pkt = {};
    pkt.command = cmd_type;
    pkt.device_id = floor_id;
    pkt.gpio_index = gpio_index;
    pkt.value = value;

    char topic[48];
    snprintf(topic, sizeof(topic), "%s%u", MQTT_TOPIC_CMD_PREFIX, floor_id);

    std::string payload(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    if (!s_mqtt->Publish(topic, payload, 0)) {
        ESP_LOGE(TAG, "Failed to publish command: topic=%s cmd=0x%02X gpio=%u val=%u",
                 topic, cmd_type, gpio_index, value);
        return false;
    }
    ESP_LOGI(TAG, "CMD -> %s: cmd=0x%02X gpio=%u val=%u", topic, cmd_type, gpio_index, value);
    return true;
}

extern "C" void mqtt_send_main_power(uint8_t floor_id, bool on) {
    if (!mqtt_client_is_connected()) {
        ESP_LOGW(TAG, "Cannot send main power: MQTT not connected");
        return;
    }
    if (floor_id < 1 || floor_id > 3) {
        ESP_LOGW(TAG, "Invalid main power floor: %u", floor_id);
        return;
    }

    iot_command_packet_t pkt = {};
    pkt.command = IOT_CMD_SET_MAIN_POWER;
    pkt.device_id = floor_id;
    pkt.value = on ? 1 : 0;

    char topic[48];
    snprintf(topic, sizeof(topic), "%s%u", MQTT_TOPIC_POWER_PREFIX, floor_id);

    std::string payload(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    s_mqtt->Publish(topic, payload, 1);
    ESP_LOGW(TAG, "MAIN_POWER -> %s: %s", topic, on ? "ON" : "OFF");
}

extern "C" void mqtt_send_all_main_power(bool on) {
    if (!mqtt_client_is_connected()) {
        ESP_LOGW(TAG, "Cannot broadcast main power: MQTT not connected");
        return;
    }

    iot_command_packet_t pkt = {};
    pkt.command = IOT_CMD_SET_MAIN_POWER;
    pkt.value = on ? 1 : 0;

    std::string payload(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    s_mqtt->Publish(MQTT_TOPIC_POWER_BROADCAST, payload, 1);
    ESP_LOGW(TAG, "MAIN_POWER -> broadcast: %s", on ? "ON" : "OFF");
}

extern "C" void mqtt_send_command_v3(uint8_t floor_id, uint8_t cmd_type, uint8_t gpio_index, uint8_t value, uint8_t source) {
    // For broadcast commands, use mqtt_send_broadcast; for others, delegate to mqtt_send_command
    (void)source;
    if (cmd_type >= 0x30 && cmd_type <= 0x34) {
        mqtt_send_broadcast(cmd_type);
    } else {
        mqtt_send_command(floor_id, cmd_type, gpio_index, value);
    }
}

extern "C" void mqtt_send_ambient_scene(uint8_t floor_id, uint8_t index, uint8_t ambient_scene) {
    // Map ambient scene presets to RGB light parameters
    switch (ambient_scene) {
        case IOT_AMBIENT_SCENE_OFF:
            mqtt_send_rgb_light(floor_id, index, 0, 0, 0, 0, IOT_LIGHT_EFFECT_STATIC, 0);
            break;
        case IOT_AMBIENT_SCENE_SLEEP:
            mqtt_send_rgb_light(floor_id, index, 24, 30, 96, 18, IOT_LIGHT_EFFECT_STATIC, 20);
            break;
        case IOT_AMBIENT_SCENE_RAIN:
            mqtt_send_rgb_light(floor_id, index, 0, 120, 255, 65, IOT_LIGHT_EFFECT_WARNING, 5);
            break;
        case IOT_AMBIENT_SCENE_WARNING:
            mqtt_send_rgb_light(floor_id, index, 255, 0, 0, 100, IOT_LIGHT_EFFECT_WARNING, 5);
            break;
        case IOT_AMBIENT_SCENE_WARM_HOME:
            mqtt_send_rgb_light(floor_id, index, 255, 166, 82, 45, IOT_LIGHT_EFFECT_STATIC, 0);
            break;
        default:
            break;
    }
    ESP_LOGI(TAG, "AMBIENT -> floor=%u idx=%u scene=%u", floor_id, index, ambient_scene);
}

static bool publish_rgb_light(uint8_t floor_id, uint8_t index, uint8_t red, uint8_t green,
                              uint8_t blue, uint8_t brightness, uint8_t effect, uint8_t speed) {
    if (!mqtt_client_is_connected()) {
        ESP_LOGW(TAG, "Cannot send RGB command: MQTT not connected");
        return false;
    }

    iot_rgb_light_packet_t pkt = {};
    pkt.command = IOT_CMD_SET_RGB_LIGHT;
    pkt.protocol_version = IOT_PROTOCOL_VERSION_V3;
    pkt.device_id = floor_id;
    pkt.index = index;
    pkt.red = red;
    pkt.green = green;
    pkt.blue = blue;
    pkt.brightness = brightness;
    pkt.effect = effect;
    pkt.speed = speed;

    char topic[48];
    snprintf(topic, sizeof(topic), "%s%u", MQTT_TOPIC_CMD_PREFIX, floor_id);

    std::string payload(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    if (!s_mqtt->Publish(topic, payload, 0)) {
        ESP_LOGE(TAG, "Failed to publish RGB command: topic=%s", topic);
        return false;
    }
    ESP_LOGI(TAG, "RGB -> %s: idx=%u rgb=(%u,%u,%u) br=%u effect=%u param=%u",
             topic, index, red, green, blue, brightness, effect, speed);
    return true;
}

extern "C" void mqtt_send_rgb_light(uint8_t floor_id, uint8_t index, uint8_t red, uint8_t green,
                                    uint8_t blue, uint8_t brightness, uint8_t effect, uint8_t speed) {
    std::lock_guard<std::mutex> lock(s_rgb_command_mutex);
    s_rgb_blink.active = false;
    publish_rgb_light(floor_id, index, red, green, blue, brightness, effect, speed);
}

extern "C" bool mqtt_start_rgb_blink(uint8_t floor_id, uint8_t index, uint8_t red, uint8_t green,
                                      uint8_t blue, uint8_t brightness, uint8_t blink_count) {
    if (brightness == 0 || blink_count == 0) {
        ESP_LOGW(TAG, "Cannot blink RGB light: brightness and blink_count must be non-zero");
        return false;
    }

    std::lock_guard<std::mutex> lock(s_rgb_command_mutex);
    if (!mqtt_client_is_connected()) {
        ESP_LOGW(TAG, "Cannot start RGB blink: MQTT not connected");
        return false;
    }

    s_rgb_blink.active = true;
    s_rgb_blink.light_on = true;
    s_rgb_blink.floor_id = floor_id;
    s_rgb_blink.index = index;
    s_rgb_blink.red = red;
    s_rgb_blink.green = green;
    s_rgb_blink.blue = blue;
    s_rgb_blink.brightness = brightness;
    s_rgb_blink.remaining_flashes = blink_count;

    ESP_LOGI(TAG, "RGB blink start: floor=%u idx=%u count=%u interval=250ms",
             floor_id, index, blink_count);
    bool published = publish_rgb_light(floor_id, index, red, green, blue, brightness,
                                       IOT_LIGHT_EFFECT_STATIC, 0);
    if (!published) {
        s_rgb_blink.active = false;
    }
    return published;
}

extern "C" void mqtt_rgb_blink_tick(void) {
    std::lock_guard<std::mutex> lock(s_rgb_command_mutex);
    if (!s_rgb_blink.active) {
        return;
    }
    if (!mqtt_client_is_connected()) {
        s_rgb_blink.active = false;
        ESP_LOGW(TAG, "RGB blink cancelled: MQTT disconnected");
        return;
    }

    if (s_rgb_blink.light_on) {
        s_rgb_blink.light_on = false;
        if (s_rgb_blink.remaining_flashes > 0) {
            s_rgb_blink.remaining_flashes--;
        }
        publish_rgb_light(s_rgb_blink.floor_id, s_rgb_blink.index,
                          s_rgb_blink.red, s_rgb_blink.green, s_rgb_blink.blue,
                          0, IOT_LIGHT_EFFECT_STATIC, 0);
        return;
    }

    s_rgb_blink.light_on = true;
    publish_rgb_light(s_rgb_blink.floor_id, s_rgb_blink.index,
                      s_rgb_blink.red, s_rgb_blink.green, s_rgb_blink.blue,
                      s_rgb_blink.brightness, IOT_LIGHT_EFFECT_STATIC, 0);
    if (s_rgb_blink.remaining_flashes == 0) {
        s_rgb_blink.active = false;
        ESP_LOGI(TAG, "RGB blink complete; target color remains on");
    }
}

extern "C" void mqtt_send_scene(uint8_t scene_id) {
    if (!s_flushing_pending_scene) {
        device_model_set_scene(scene_id);
    }

    if (!mqtt_client_is_connected()) {
        s_pending_scene_valid = true;
        s_pending_scene_id = scene_id;
        ESP_LOGW(TAG, "MQTT not connected, scene=%u applied locally and queued", scene_id);
        return;
    }

    s_pending_scene_valid = false;
    s_pending_scene_id = IOT_SCENE_NONE;

    iot_scene_packet_t pkt = {};
    pkt.command = IOT_CMD_SET_SCENE;
    pkt.protocol_version = IOT_PROTOCOL_VERSION_V3;
    pkt.scene_id = scene_id;
    pkt.source = 1;

    std::string payload(reinterpret_cast<const char*>(&pkt), sizeof(pkt));
    s_mqtt->Publish(MQTT_TOPIC_CMD_BROADCAST, payload, 0);

    switch (scene_id) {
        case IOT_SCENE_SLEEP:
            mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_OFF);
            mqtt_send_rgb_light(2, 0, 24, 30, 96, 18, IOT_LIGHT_EFFECT_STATIC, 20);
            mqtt_send_command(2, IOT_CMD_SET_LIGHT, 1, 0);
            mqtt_send_command(2, IOT_CMD_SET_LIGHT, 2, 0);
            mqtt_send_command(2, IOT_CMD_SET_RELAY, 0, 0);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 6, 180);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 7, 0);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 8, 0);
            break;
        case IOT_SCENE_MOVIE:
            mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_OFF);
            mqtt_send_rgb_light(2, 0, 52, 68, 210, 24, IOT_LIGHT_EFFECT_STATIC, 20);
            mqtt_send_command(2, IOT_CMD_SET_LIGHT, 1, 0);
            mqtt_send_command(2, IOT_CMD_SET_LIGHT, 2, 0);
            mqtt_send_command(2, IOT_CMD_SET_RELAY, 0, 0);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 6, 180);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 7, 0);
            break;
        case IOT_SCENE_NIGHT:
            mqtt_send_rgb_light(2, 0, 255, 154, 68, 8, IOT_LIGHT_EFFECT_STATIC, 10);
            mqtt_send_command(2, IOT_CMD_SET_LIGHT, 1, 0);
            mqtt_send_command(2, IOT_CMD_SET_LIGHT, 2, 1);
            mqtt_send_command(3, IOT_CMD_SET_LIGHT, 0, 0);
            break;
        case IOT_SCENE_FIRE:
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 6, 90);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 7, 90);
            mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_ON);
            mqtt_send_command(1, IOT_CMD_SET_SERVO, 6, 135);  // 一楼大门全开 135° 便于疏散
            mqtt_send_command(1, IOT_CMD_SET_LIGHT, 0, 1);
            mqtt_send_rgb_light(2, 0, 255, 0, 0, 100, IOT_LIGHT_EFFECT_WARNING, 5);
            mqtt_send_command(2, IOT_CMD_SET_LIGHT, 1, 1);
            mqtt_send_command(2, IOT_CMD_SET_LIGHT, 2, 1);
            break;
        case IOT_SCENE_RAIN:
            mqtt_send_ambient_scene(2, 0, IOT_AMBIENT_SCENE_RAIN);
            mqtt_send_command(2, IOT_CMD_SET_SERVO, 8, 0);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 8, 0);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 6, 180);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 7, 0);
            break;
        case IOT_SCENE_AWAY:
            mqtt_send_broadcast(IOT_CMD_BROADCAST_ALL_OFF);
            mqtt_send_command(1, IOT_CMD_SET_SERVO, 6, 0);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 6, 180);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 7, 0);
            mqtt_send_command(3, IOT_CMD_SET_SERVO, 8, 0);
            break;
        case IOT_SCENE_HOME:
            mqtt_send_all_main_power(true);
            mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_ON);
            mqtt_send_command(1, IOT_CMD_SET_SERVO, 6, 135);
            mqtt_send_command(1, IOT_CMD_SET_LIGHT, 0, 1);
            mqtt_send_command(2, IOT_CMD_SET_LIGHT, 1, 1);
            mqtt_send_command(2, IOT_CMD_SET_LIGHT, 2, 1);
            mqtt_send_command(3, IOT_CMD_SET_LIGHT, 0, 1);
            mqtt_send_rgb_light(2, 0, 255, 190, 120, 68, IOT_LIGHT_EFFECT_STATIC, 0);
            break;
        case IOT_SCENE_BRIGHT:
            mqtt_send_broadcast(IOT_CMD_BROADCAST_LIGHTS_ON);
            mqtt_send_rgb_light(2, 0, 255, 220, 170, 85, IOT_LIGHT_EFFECT_STATIC, 20);
            break;
        default:
            break;
    }

    ESP_LOGI(TAG, "SCENE -> broadcast: scene=%u", scene_id);
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
