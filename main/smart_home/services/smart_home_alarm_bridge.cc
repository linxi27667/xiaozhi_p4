#include "application.h"
#include "assets/lang_config.h"
#include "rule_engine.h"
#include "smart_home_alarm_ui.h"
#include "smart_home_event_center.h"
#include "mqtt_iot_protocol.h"
#include "xiaozhi_mqtt.h"

#include <atomic>
#include <cstdio>

static std::atomic<uint8_t> s_fire_active_mask{0};
static std::atomic<uint8_t> s_rain_active_mask{0};
static std::atomic<uint8_t> s_help_active_mask{0};

static bool sensor_rising_edge(std::atomic<uint8_t>& mask, uint8_t floor_id, bool active)
{
    if (floor_id < 1 || floor_id > 3) {
        return false;
    }

    const uint8_t bit = static_cast<uint8_t>(1u << (floor_id - 1));
    uint8_t old_mask = mask.load();
    while (true) {
        uint8_t new_mask = active ? static_cast<uint8_t>(old_mask | bit)
                                  : static_cast<uint8_t>(old_mask & ~bit);
        if (mask.compare_exchange_weak(old_mask, new_mask)) {
            return active && ((old_mask & bit) == 0);
        }
    }
}

static bool sensor_state_changed(std::atomic<uint8_t>& mask, uint8_t floor_id, bool active)
{
    if (floor_id < 1 || floor_id > 3) {
        return false;
    }

    const uint8_t bit = static_cast<uint8_t>(1u << (floor_id - 1));
    uint8_t old_mask = mask.load();
    while (true) {
        uint8_t new_mask = active ? static_cast<uint8_t>(old_mask | bit)
                                  : static_cast<uint8_t>(old_mask & ~bit);
        if (new_mask == old_mask) {
            return false;
        }
        if (mask.compare_exchange_weak(old_mask, new_mask)) {
            return true;
        }
    }
}

static void post_rule_event(smart_home_event_type_t type, uint8_t floor_id,
                            uint8_t severity, const char *message)
{
    smart_home_event_post(type, floor_id, severity, 0, message);
    smart_home_event_t event = {};
    event.type = type;
    event.floor_id = floor_id;
    event.severity = severity;
    rule_engine_on_event(&event);
}

extern "C" void smart_home_alarm_on_fire_status(uint8_t floor_id, bool active)
{
    smart_home_alarm_ui_set_fire(floor_id, active);

    if (!sensor_rising_edge(s_fire_active_mask, floor_id, active)) {
        return;
    }

    post_rule_event(SH_EVENT_FIRE_ALARM, floor_id, 2, "Fire alarm");

    Application::GetInstance().Schedule([floor_id]() {
        char message[128];
        const char *floor = floor_id == 1 ? "一楼" : floor_id == 2 ? "二楼" : floor_id == 3 ? "三楼" : "未知楼层";
        snprintf(message, sizeof(message), "检测到%s发生火灾，请立即检查现场设备。", floor);
        auto& app = Application::GetInstance();
        app.Alert("火灾警报", message, "triangle_exclamation", Lang::Sounds::OGG_EXCLAMATION);
        app.WakeWordInvoke("fire_alarm");
    });
}

extern "C" void smart_home_alarm_on_rain_status(uint8_t floor_id, bool active)
{
    if (!sensor_state_changed(s_rain_active_mask, floor_id, active)) {
        return;
    }

    if (active) {
        post_rule_event(SH_EVENT_RAIN_ALARM, floor_id, 1, "Rain detected");
        mqtt_send_command(2, IOT_CMD_SET_SERVO, 8, 0);
        mqtt_send_command(3, IOT_CMD_SET_SERVO, 8, 0);
        mqtt_send_command(3, IOT_CMD_SET_SERVO, 6, 180);
        mqtt_send_command(3, IOT_CMD_SET_SERVO, 7, 0);
    } else {
        mqtt_send_command(2, IOT_CMD_SET_SERVO, 8, 180);
        /* 3F hanger stays collected until an explicit user command reopens it. */
    }
}

extern "C" void smart_home_alarm_on_help_status(uint8_t floor_id, bool active)
{
    if (sensor_rising_edge(s_help_active_mask, floor_id, active)) {
        post_rule_event(SH_EVENT_HELP_ALARM, floor_id, 2, "Help alarm");
    }
}
